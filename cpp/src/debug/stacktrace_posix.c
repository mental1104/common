#include "stacktrace_internal.h"

#ifndef _WIN32

#if defined(__APPLE__) && !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 700
#endif

#include <dlfcn.h>
#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(__linux__)
#include <sys/syscall.h>
#include <ucontext.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <pthread.h>
#include <ucontext.h>
#endif

#define ST_ALTSTACK_SIZE (64 * 1024)

struct st_sigaction_entry {
  int signo;
  struct sigaction old_action;
};

static struct st_sigaction_entry g_old_actions[16];
static size_t g_old_action_count = 0;
static stack_t g_altstack;
static stack_t g_prev_altstack;
static int g_altstack_installed = 0;
static volatile sig_atomic_t g_handling = 0;

static const char* st_signal_name(int signo) {
  switch (signo) {
    case SIGSEGV:
      return "SIGSEGV";
    case SIGBUS:
      return "SIGBUS";
    case SIGILL:
      return "SIGILL";
    case SIGFPE:
      return "SIGFPE";
    case SIGABRT:
      return "SIGABRT";
#ifdef SIGTRAP
    case SIGTRAP:
      return "SIGTRAP";
#endif
    default:
      return "SIGUNKNOWN";
  }
}

static uint64_t st_get_tid(void) {
#if defined(__linux__)
#ifdef SYS_gettid
  return (uint64_t)syscall(SYS_gettid);
#else
  return (uint64_t)getpid();
#endif
#elif defined(__APPLE__)
  return (uint64_t)(uintptr_t)pthread_self();
#else
  return (uint64_t)getpid();
#endif
}

static void st_get_regs_from_ucontext(void* uctx,
                                      uintptr_t* ip,
                                      uintptr_t* sp,
                                      uintptr_t* bp) {
  *ip = 0;
  *sp = 0;
  *bp = 0;
#if defined(__linux__)
  if (!uctx) {
    return;
  }
  {
    ucontext_t* ctx = (ucontext_t*)uctx;
#if defined(REG_RIP)
    *ip = (uintptr_t)ctx->uc_mcontext.gregs[REG_RIP];
#endif
#if defined(REG_RSP)
    *sp = (uintptr_t)ctx->uc_mcontext.gregs[REG_RSP];
#endif
#if defined(REG_RBP)
    *bp = (uintptr_t)ctx->uc_mcontext.gregs[REG_RBP];
#endif
  }
#elif defined(__APPLE__)
  if (!uctx) {
    return;
  }
  {
    ucontext_t* ctx = (ucontext_t*)uctx;
#if defined(__x86_64__)
    *ip = (uintptr_t)ctx->uc_mcontext->__ss.__rip;
    *sp = (uintptr_t)ctx->uc_mcontext->__ss.__rsp;
    *bp = (uintptr_t)ctx->uc_mcontext->__ss.__rbp;
#elif defined(__aarch64__) || defined(__arm64__)
    *ip = (uintptr_t)ctx->uc_mcontext->__ss.__pc;
    *sp = (uintptr_t)ctx->uc_mcontext->__ss.__sp;
    *bp = (uintptr_t)ctx->uc_mcontext->__ss.__fp;
#endif
  }
#else
  (void)uctx;
#endif
}

static const char* st_get_exe_path(void) {
  static char path[1024];
  static int initialized = 0;
  if (initialized) {
    return path;
  }
#if defined(__linux__)
  {
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len > 0) {
      path[len] = '\0';
      initialized = 1;
      return path;
    }
  }
#elif defined(__APPLE__)
  {
    uint32_t size = (uint32_t)sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
      path[sizeof(path) - 1] = '\0';
      initialized = 1;
      return path;
    }
  }
#endif
  strncpy(path, "unknown", sizeof(path) - 1);
  path[sizeof(path) - 1] = '\0';
  initialized = 1;
  return path;
}

static void st_resolve_module(uintptr_t pc, const char** module, uintptr_t* base) {
  Dl_info info;
  if (dladdr((void*)pc, &info) && info.dli_fname) {
    *module = info.dli_fname;
    *base = (uintptr_t)info.dli_fbase;
    return;
  }
  *module = st_get_exe_path();
  *base = 0;
}

static int st_command_exists(const char* cmd) {
  if (!cmd || !cmd[0]) {
    return 0;
  }
  if (strchr(cmd, '/')) {
    return access(cmd, X_OK) == 0;
  }
  {
    const char* path = getenv("PATH");
    if (!path) {
      return 0;
    }
    const char* start = path;
    while (*start) {
      const char* end = strchr(start, ':');
      size_t len = end ? (size_t)(end - start) : strlen(start);
      if (len > 0 && len + 1 + strlen(cmd) < 1024) {
        char full[1024];
        memcpy(full, start, len);
        full[len] = '/';
        strcpy(full + len + 1, cmd);
        if (access(full, X_OK) == 0) {
          return 1;
        }
      }
      if (!end) {
        break;
      }
      start = end + 1;
    }
  }
  return 0;
}

enum st_symbolizer_kind {
  ST_SYMBOLIZER_NONE = 0,
  ST_SYMBOLIZER_LLVM,
  ST_SYMBOLIZER_ADDR2LINE,
  ST_SYMBOLIZER_ATOS,
  ST_SYMBOLIZER_XCRUN_ATOS
};

static enum st_symbolizer_kind st_detect_kind(const char* path) {
  if (!path) {
    return ST_SYMBOLIZER_NONE;
  }
  if (strstr(path, "llvm-symbolizer")) {
    return ST_SYMBOLIZER_LLVM;
  }
  if (strstr(path, "addr2line")) {
    return ST_SYMBOLIZER_ADDR2LINE;
  }
  if (strstr(path, "xcrun")) {
    return ST_SYMBOLIZER_XCRUN_ATOS;
  }
  if (strstr(path, "atos")) {
    return ST_SYMBOLIZER_ATOS;
  }
  return ST_SYMBOLIZER_LLVM;
}

static const char* st_select_symbolizer(enum st_symbolizer_kind* kind) {
  const char* configured = st_get_symbolizer_path();
  if (configured && configured[0]) {
    *kind = st_detect_kind(configured);
    return configured;
  }
  {
    const char* env = getenv("ST_SYMBOLIZER");
    if (env && env[0]) {
      *kind = st_detect_kind(env);
      return env;
    }
  }
#if defined(__APPLE__)
  if (st_command_exists("xcrun")) {
    *kind = ST_SYMBOLIZER_XCRUN_ATOS;
    return "xcrun";
  }
  if (st_command_exists("atos")) {
    *kind = ST_SYMBOLIZER_ATOS;
    return "atos";
  }
#else
  if (st_command_exists("llvm-symbolizer")) {
    *kind = ST_SYMBOLIZER_LLVM;
    return "llvm-symbolizer";
  }
  if (st_command_exists("addr2line")) {
    *kind = ST_SYMBOLIZER_ADDR2LINE;
    return "addr2line";
  }
#endif
  *kind = ST_SYMBOLIZER_NONE;
  return NULL;
}

static int st_run_command_capture(char* const argv[], char* output, size_t output_size) {
  int pipefd[2];
  if (pipe(pipefd) != 0) {
    return -1;
  }
  pid_t pid = fork();
  if (pid == 0) {
    close(pipefd[0]);
    dup2(pipefd[1], STDOUT_FILENO);
    {
      int devnull = open("/dev/null", O_WRONLY);
      if (devnull >= 0) {
        dup2(devnull, STDERR_FILENO);
        close(devnull);
      } else {
        dup2(pipefd[1], STDERR_FILENO);
      }
    }
    close(pipefd[1]);
    execvp(argv[0], argv);
    _Exit(127);
  }
  if (pid < 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    return -1;
  }
  close(pipefd[1]);
  size_t total = 0;
  while (total + 1 < output_size) {
    ssize_t n = read(pipefd[0], output + total, output_size - 1 - total);
    if (n <= 0) {
      break;
    }
    total += (size_t)n;
  }
  output[total] = '\0';
  close(pipefd[0]);
  {
    int status = 0;
    (void)waitpid(pid, &status, 0);
  }
  return total > 0 ? 0 : -1;
}

static void st_copy_trim(char* dst, size_t dst_size, const char* src, size_t len) {
  size_t start = 0;
  size_t end = len;
  while (start < len && (src[start] == ' ' || src[start] == '\t')) {
    start++;
  }
  while (end > start && (src[end - 1] == ' ' || src[end - 1] == '\t' || src[end - 1] == '\r' || src[end - 1] == '\n')) {
    end--;
  }
  if (dst_size == 0) {
    return;
  }
  if (end <= start) {
    dst[0] = '\0';
    return;
  }
  if (end - start >= dst_size) {
    end = start + dst_size - 1;
  }
  memcpy(dst, src + start, end - start);
  dst[end - start] = '\0';
}

static int st_parse_int_range(const char* start, const char* end) {
  int value = 0;
  const char* p = start;
  while (p < end && *p >= '0' && *p <= '9') {
    value = value * 10 + (*p - '0');
    p++;
  }
  return value;
}

static void st_parse_file_line(const char* text, char* file, size_t file_size, int* line, int* column) {
  size_t len = strlen(text);
  size_t i;
  const char* last = NULL;
  const char* second = NULL;
  *line = 0;
  *column = 0;
  if (file_size == 0) {
    return;
  }
  if (len == 0) {
    file[0] = '\0';
    return;
  }
  for (i = len; i > 0; --i) {
    if (text[i - 1] == ':') {
      if (!last) {
        last = text + (i - 1);
      } else {
        second = text + (i - 1);
        break;
      }
    }
  }
  if (!last) {
    st_copy_trim(file, file_size, text, len);
    return;
  }
  if (second) {
    st_copy_trim(file, file_size, text, (size_t)(second - text));
    *line = st_parse_int_range(second + 1, last);
    *column = st_parse_int_range(last + 1, text + len);
    return;
  }
  st_copy_trim(file, file_size, text, (size_t)(last - text));
  *line = st_parse_int_range(last + 1, text + len);
}

static void st_parse_llvm_output(const char* output,
                                 char* function,
                                 size_t function_size,
                                 char* file,
                                 size_t file_size,
                                 int* line,
                                 int* column) {
  const char* line1 = output;
  const char* end1 = strchr(line1, '\n');
  const char* line2 = NULL;
  const char* end2 = NULL;
  if (!end1) {
    end1 = line1 + strlen(line1);
  }
  st_copy_trim(function, function_size, line1, (size_t)(end1 - line1));
  if (*end1 == '\n') {
    line2 = end1 + 1;
  }
  if (line2) {
    end2 = strchr(line2, '\n');
    if (!end2) {
      end2 = line2 + strlen(line2);
    }
    {
      char loc[1024];
      st_copy_trim(loc, sizeof(loc), line2, (size_t)(end2 - line2));
      st_parse_file_line(loc, file, file_size, line, column);
    }
  } else {
    if (file_size > 0) {
      file[0] = '\0';
    }
    *line = 0;
    *column = 0;
  }
}

static void st_parse_addr2line_output(const char* output,
                                      char* function,
                                      size_t function_size,
                                      char* file,
                                      size_t file_size,
                                      int* line,
                                      int* column) {
  const char* at = strstr(output, " at ");
  if (at) {
    st_copy_trim(function, function_size, output, (size_t)(at - output));
    {
      const char* loc = at + 4;
      char locbuf[1024];
      st_copy_trim(locbuf, sizeof(locbuf), loc, strlen(loc));
      st_parse_file_line(locbuf, file, file_size, line, column);
    }
    return;
  }
  st_copy_trim(function, function_size, output, strlen(output));
  if (file_size > 0) {
    file[0] = '\0';
  }
  *line = 0;
  *column = 0;
}

static void st_parse_atos_output(const char* output,
                                 char* function,
                                 size_t function_size,
                                 char* file,
                                 size_t file_size,
                                 int* line) {
  const char* first_paren = strchr(output, '(');
  const char* last_paren = strrchr(output, '(');
  const char* last_close = strrchr(output, ')');
  if (first_paren) {
    st_copy_trim(function, function_size, output, (size_t)(first_paren - output));
  } else {
    st_copy_trim(function, function_size, output, strlen(output));
  }
  if (file_size > 0) {
    file[0] = '\0';
  }
  *line = 0;
  if (last_paren && last_close && last_close > last_paren) {
    char inner[1024];
    int col = 0;
    st_copy_trim(inner, sizeof(inner), last_paren + 1, (size_t)(last_close - last_paren - 1));
    st_parse_file_line(inner, file, file_size, line, &col);
  }
}

static void st_symbolize_one(enum st_symbolizer_kind kind,
                             const char* symbolizer,
                             const char* module,
                             uintptr_t base,
                             uintptr_t pc,
                             char* function,
                             size_t function_size,
                             char* file,
                             size_t file_size,
                             int* line,
                             int* column) {
  char addr_buf[32];
  char* argv[16];
  char output[2048];
  uintptr_t offset = base ? (pc - base) : pc;
  snprintf(addr_buf, sizeof(addr_buf), "0x%lx", (unsigned long)offset);
  function[0] = '\0';
  file[0] = '\0';
  *line = 0;
  *column = 0;

  if (kind == ST_SYMBOLIZER_LLVM) {
    argv[0] = (char*)symbolizer;
    argv[1] = "--obj";
    argv[2] = (char*)module;
    argv[3] = "--demangle";
    argv[4] = addr_buf;
    argv[5] = NULL;
    if (st_run_command_capture(argv, output, sizeof(output)) == 0) {
      st_parse_llvm_output(output, function, function_size, file, file_size, line, column);
    }
    return;
  }

  if (kind == ST_SYMBOLIZER_ADDR2LINE) {
    argv[0] = (char*)symbolizer;
    argv[1] = "-e";
    argv[2] = (char*)module;
    argv[3] = "-f";
    argv[4] = "-C";
    argv[5] = "-p";
    argv[6] = addr_buf;
    argv[7] = NULL;
    if (st_run_command_capture(argv, output, sizeof(output)) == 0) {
      st_parse_addr2line_output(output, function, function_size, file, file_size, line, column);
    }
    return;
  }

#if defined(__APPLE__)
  if (kind == ST_SYMBOLIZER_ATOS || kind == ST_SYMBOLIZER_XCRUN_ATOS) {
    char base_buf[32];
    char pc_buf[32];
    snprintf(base_buf, sizeof(base_buf), "0x%lx", (unsigned long)base);
    snprintf(pc_buf, sizeof(pc_buf), "0x%lx", (unsigned long)pc);
    if (kind == ST_SYMBOLIZER_XCRUN_ATOS) {
      argv[0] = (char*)symbolizer;
      argv[1] = "atos";
      argv[2] = "-o";
      argv[3] = (char*)module;
      argv[4] = "-l";
      argv[5] = base_buf;
      argv[6] = pc_buf;
      argv[7] = NULL;
    } else {
      argv[0] = (char*)symbolizer;
      argv[1] = "-o";
      argv[2] = (char*)module;
      argv[3] = "-l";
      argv[4] = base_buf;
      argv[5] = pc_buf;
      argv[6] = NULL;
    }
    if (st_run_command_capture(argv, output, sizeof(output)) == 0) {
      int line_out = 0;
      st_parse_atos_output(output, function, function_size, file, file_size, &line_out);
      *line = line_out;
      *column = 0;
    }
    return;
  }
#else
  (void)base;
  (void)pc;
#endif
}

static void st_dump_maps_linux(void) {
#if defined(__linux__)
  const st_options_t* opt = st_get_options();
  if (!opt || opt->dump_maps == 0) {
    return;
  }
  int fd = open("/proc/self/maps", O_RDONLY);
  if (fd < 0) {
    return;
  }
  {
    char buf[4096];
    char line[4096];
    size_t line_len = 0;
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
      ssize_t i;
      for (i = 0; i < n; ++i) {
        char c = buf[i];
        if (c == '\n') {
          line[line_len] = '\0';
          st_format_maps_line(line);
          line_len = 0;
        } else if (line_len + 1 < sizeof(line)) {
          line[line_len++] = c;
        }
      }
    }
    if (line_len > 0) {
      line[line_len] = '\0';
      st_format_maps_line(line);
    }
  }
  close(fd);
#endif
}

static void st_dump_symbolized_frames(void* const* frames, int frame_count) {
  enum st_symbolizer_kind kind = ST_SYMBOLIZER_NONE;
  const char* symbolizer = st_select_symbolizer(&kind);
  int i;
  if (!symbolizer || kind == ST_SYMBOLIZER_NONE) {
    return;
  }
  for (i = 0; i < frame_count; ++i) {
    uintptr_t pc = (uintptr_t)frames[i];
    const char* module = NULL;
    uintptr_t base = 0;
    char function[512];
    char file[1024];
    int line = 0;
    int column = 0;
    st_frame_t frame;
    st_resolve_module(pc, &module, &base);
    if (!module) {
      module = "";
    }
    st_symbolize_one(kind, symbolizer, module, base, pc,
                     function, sizeof(function),
                     file, sizeof(file),
                     &line, &column);

    frame.index = i;
    frame.pc = pc;
    frame.function = function[0] ? function : "??";
    frame.file = file[0] ? file : "??";
    frame.line = line;
    frame.column = column;
    frame.module = module;
    st_format_frame(&frame);
  }
}

static void st_symbolize_child(void* const* frames,
                               int frame_count,
                               const st_context_t* ctx,
                               int exit_code) {
  st_dump_symbolized_frames(frames, frame_count);
  st_dump_maps_linux();
  st_format_footer(ctx, exit_code);
}

static void st_call_previous(int signo, siginfo_t* info, void* uctx) {
  size_t i;
  const st_options_t* opt = st_get_options();
  if (!opt || opt->chain_previous == 0) {
    return;
  }
  for (i = 0; i < g_old_action_count; ++i) {
    if (g_old_actions[i].signo == signo) {
      struct sigaction* old_action = &g_old_actions[i].old_action;
      if (old_action->sa_handler == SIG_IGN || old_action->sa_handler == SIG_DFL || old_action->sa_handler == NULL) {
        return;
      }
      if (old_action->sa_flags & SA_SIGINFO) {
        old_action->sa_sigaction(signo, info, uctx);
      } else {
        old_action->sa_handler(signo);
      }
      return;
    }
  }
}

static void st_signal_handler(int signo, siginfo_t* info, void* uctx) {
  const st_options_t* opt = st_get_options();
  void* frames[ST_MAX_FRAMES_LIMIT];
  int max_frames = opt ? opt->max_frames : 64;
  int frame_count = 0;
  uintptr_t ip = 0;
  uintptr_t sp = 0;
  uintptr_t bp = 0;
  uintptr_t fault_address = 0;
  int exit_code = 128 + signo;
  st_context_t ctx;

  if (g_handling) {
    _Exit(exit_code);
  }
  g_handling = 1;

  if (max_frames <= 0 || max_frames > ST_MAX_FRAMES_LIMIT) {
    max_frames = ST_MAX_FRAMES_LIMIT;
  }

  st_get_regs_from_ucontext(uctx, &ip, &sp, &bp);
  if (info && (signo == SIGSEGV || signo == SIGBUS)) {
    fault_address = (uintptr_t)info->si_addr;
  }

  ctx.event = "fatal";
  ctx.platform = st_platform_name();
  ctx.pid = (uint64_t)getpid();
  ctx.tid = st_get_tid();
  ctx.signal = signo;
  ctx.signal_name = st_signal_name(signo);
  ctx.exception_code = 0;
  ctx.exception_name = NULL;
  ctx.fault_address = fault_address;
  ctx.ip = ip;
  ctx.sp = sp;
  ctx.bp = bp;
  st_format_header(&ctx);

  frame_count = backtrace(frames, max_frames);
  {
    int i;
    for (i = 0; i < frame_count; ++i) {
      st_format_frame_raw(i, (uintptr_t)frames[i]);
    }
  }

  {
    pid_t pid = fork();
    if (pid == 0) {
      st_symbolize_child(frames, frame_count, &ctx, exit_code);
      _Exit(exit_code);
    }
    if (pid < 0) {
      st_format_footer(&ctx, exit_code);
    }
  }

  st_call_previous(signo, info, uctx);

  if (opt && opt->exit_on_fatal) {
    _Exit(exit_code);
  }
}

int st_platform_init(void) {
  const st_options_t* opt = st_get_options();
  size_t i;
  static const int kSignals[] = {
    SIGSEGV,
#ifdef SIGBUS
    SIGBUS,
#endif
    SIGILL,
    SIGFPE,
    SIGABRT,
#ifdef SIGTRAP
    SIGTRAP,
#endif
  };

  g_old_action_count = 0;

  if (opt && opt->use_altstack) {
    size_t stack_size = ST_ALTSTACK_SIZE;
    if (stack_size < SIGSTKSZ) {
      stack_size = SIGSTKSZ;
    }
    memset(&g_altstack, 0, sizeof(g_altstack));
    g_altstack.ss_sp = malloc(stack_size);
    g_altstack.ss_size = stack_size;
    g_altstack.ss_flags = 0;
    if (g_altstack.ss_sp && sigaltstack(&g_altstack, &g_prev_altstack) == 0) {
      g_altstack_installed = 1;
    } else {
      if (g_altstack.ss_sp) {
        free(g_altstack.ss_sp);
        g_altstack.ss_sp = NULL;
      }
      g_altstack_installed = 0;
    }
  }

  for (i = 0; i < sizeof(kSignals) / sizeof(kSignals[0]); ++i) {
    int signo = kSignals[i];
    struct sigaction sa;
    struct sigaction old_sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = st_signal_handler;
    sa.sa_flags = SA_SIGINFO;
    if (g_altstack_installed) {
      sa.sa_flags |= SA_ONSTACK;
    }
    if (sigaction(signo, &sa, &old_sa) == 0) {
      g_old_actions[g_old_action_count].signo = signo;
      g_old_actions[g_old_action_count].old_action = old_sa;
      g_old_action_count++;
    }
  }

  return 0;
}

void st_platform_shutdown(void) {
  size_t i;
  for (i = 0; i < g_old_action_count; ++i) {
    sigaction(g_old_actions[i].signo, &g_old_actions[i].old_action, NULL);
  }
  g_old_action_count = 0;
  if (g_altstack_installed) {
    sigaltstack(&g_prev_altstack, NULL);
    free(g_altstack.ss_sp);
    g_altstack.ss_sp = NULL;
    g_altstack_installed = 0;
  }
}

int st_platform_dump_current_thread(void) {
  const st_options_t* opt = st_get_options();
  void* frames[ST_MAX_FRAMES_LIMIT];
  int max_frames = opt ? opt->max_frames : 64;
  int frame_count = 0;
  uintptr_t ip = 0;
  uintptr_t sp = 0;
  uintptr_t bp = 0;
  int exit_code = 0;
  st_context_t ctx;

  if (max_frames <= 0 || max_frames > ST_MAX_FRAMES_LIMIT) {
    max_frames = ST_MAX_FRAMES_LIMIT;
  }

  st_get_regs_from_ucontext(NULL, &ip, &sp, &bp);
#if defined(__linux__)
  {
    ucontext_t ctx;
    if (getcontext(&ctx) == 0) {
      st_get_regs_from_ucontext(&ctx, &ip, &sp, &bp);
    }
  }
#endif

  ctx.event = "manual";
  ctx.platform = st_platform_name();
  ctx.pid = (uint64_t)getpid();
  ctx.tid = st_get_tid();
  ctx.signal = 0;
  ctx.signal_name = "MANUAL";
  ctx.exception_code = 0;
  ctx.exception_name = NULL;
  ctx.fault_address = 0;
  ctx.ip = ip;
  ctx.sp = sp;
  ctx.bp = bp;
  st_format_header(&ctx);

  frame_count = backtrace(frames, max_frames);
  {
    int i;
    for (i = 0; i < frame_count; ++i) {
      st_format_frame_raw(i, (uintptr_t)frames[i]);
    }
  }

  {
    pid_t pid = fork();
    if (pid == 0) {
      st_symbolize_child(frames, frame_count, &ctx, exit_code);
      _Exit(exit_code);
    }
    if (pid > 0) {
      (void)waitpid(pid, NULL, 0);
    }
    if (pid < 0) {
      st_format_footer(&ctx, exit_code);
    }
  }

  return 0;
}

#else

int st_platform_init(void) {
  return 0;
}

void st_platform_shutdown(void) {
}

int st_platform_dump_current_thread(void) {
  return -1;
}

#endif
