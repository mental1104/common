#include "stacktrace_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

static st_options_t g_options = {
  1,    /* enable */
  64,   /* max_frames */
  1,    /* use_altstack */
  0,    /* chain_previous */
  1,    /* exit_on_fatal */
  2,    /* output_fd */
  NULL, /* write_cb */
  NULL, /* user */
  NULL, /* symbolizer_path */
  1,    /* dump_maps */
  ST_FORMAT_JSON, /* format_kind */
  1,    /* emit_raw_frames */
  NULL, /* formatter */
  NULL  /* formatter_user */
};

static int g_initialized = 0;
static char* g_symbolizer_path = NULL;

int st_platform_init(void);
void st_platform_shutdown(void);
int st_platform_dump_current_thread(void);

static void st_free_symbolizer_path(void) {
  if (g_symbolizer_path) {
    free(g_symbolizer_path);
    g_symbolizer_path = NULL;
  }
}

static void st_copy_symbolizer_path(const char* path) {
  st_free_symbolizer_path();
  if (path && path[0]) {
    size_t len = strlen(path);
    g_symbolizer_path = (char*)malloc(len + 1);
    if (g_symbolizer_path) {
      memcpy(g_symbolizer_path, path, len + 1);
    }
  }
}

static st_options_t st_normalize_options(const st_options_t* opt) {
  st_options_t out = *opt;
  if (out.max_frames <= 0) {
    out.max_frames = 64;
  }
  if (out.max_frames > ST_MAX_FRAMES_LIMIT) {
    out.max_frames = ST_MAX_FRAMES_LIMIT;
  }
  if (out.output_fd < 0) {
    out.output_fd = 2;
  }
  if (out.format_kind != ST_FORMAT_JSON && out.format_kind != ST_FORMAT_PYTHON) {
    out.format_kind = ST_FORMAT_JSON;
  }
  out.emit_raw_frames = out.emit_raw_frames ? 1 : 0;
  return out;
}

static void st_apply_options(const st_options_t* opt) {
  if (!opt) {
    return;
  }
  g_options = st_normalize_options(opt);
  if (opt->symbolizer_path && opt->symbolizer_path[0]) {
    st_copy_symbolizer_path(opt->symbolizer_path);
  } else {
    const char* env = getenv("ST_SYMBOLIZER");
    if (env && env[0]) {
      st_copy_symbolizer_path(env);
    } else {
      st_free_symbolizer_path();
    }
  }
  g_options.symbolizer_path = g_symbolizer_path;
}

const st_options_t* st_get_options(void) {
  return &g_options;
}

int st_is_initialized(void) {
  return g_initialized;
}

const char* st_get_symbolizer_path(void) {
  return g_symbolizer_path;
}

const char* st_platform_name(void) {
#if defined(_WIN32)
  return "windows";
#elif defined(__APPLE__)
  return "darwin";
#elif defined(__linux__)
  return "linux";
#else
  return "posix";
#endif
}

size_t st_safe_strlen(const char* msg) {
  size_t len = 0;
  if (!msg) {
    return 0;
  }
  while (msg[len] != '\0') {
    len++;
  }
  return len;
}

void st_write(const char* msg, size_t len) {
  if (!msg || len == 0) {
    return;
  }
  if (g_options.write_cb) {
    g_options.write_cb(msg, len, g_options.user);
    return;
  }
#if defined(_WIN32)
  {
    int fd = g_options.output_fd >= 0 ? g_options.output_fd : 2;
    while (len > 0) {
      int chunk = len > (size_t)INT_MAX ? INT_MAX : (int)len;
      int written = _write(fd, msg, chunk);
      if (written <= 0) {
        break;
      }
      msg += (size_t)written;
      len -= (size_t)written;
    }
  }
#else
  {
    int fd = g_options.output_fd >= 0 ? g_options.output_fd : 2;
    while (len > 0) {
      ssize_t written = write(fd, msg, len);
      if (written <= 0) {
        break;
      }
      msg += (size_t)written;
      len -= (size_t)written;
    }
  }
#endif
}

void st_write_str(const char* msg) {
  st_write(msg, st_safe_strlen(msg));
}

void st_write_char(char c) {
  st_write(&c, 1);
}

void st_write_uint64(uint64_t value) {
  char buf[32];
  size_t len = 0;
  if (value == 0) {
    buf[len++] = '0';
  } else {
    char tmp[32];
    size_t pos = 0;
    while (value > 0 && pos < sizeof(tmp)) {
      tmp[pos++] = (char)('0' + (value % 10));
      value /= 10;
    }
    while (pos > 0) {
      buf[len++] = tmp[--pos];
    }
  }
  st_write(buf, len);
}

void st_write_hex_uintptr(uintptr_t value) {
  char buf[2 + sizeof(uintptr_t) * 2];
  size_t len = 0;
  buf[len++] = '0';
  buf[len++] = 'x';
  if (value == 0) {
    buf[len++] = '0';
  } else {
    char tmp[sizeof(uintptr_t) * 2];
    size_t pos = 0;
    while (value > 0 && pos < sizeof(tmp)) {
      unsigned int nibble = (unsigned int)(value & 0xF);
      tmp[pos++] = (char)(nibble < 10 ? ('0' + nibble) : ('a' + (nibble - 10)));
      value >>= 4;
    }
    while (pos > 0) {
      buf[len++] = tmp[--pos];
    }
  }
  st_write(buf, len);
}

void st_write_json_escaped(const char* msg) {
  const unsigned char* p = (const unsigned char*)msg;
  if (!msg) {
    st_write_str("null");
    return;
  }
  st_write_char('"');
  while (*p) {
    unsigned char c = *p++;
    switch (c) {
      case '"':
        st_write_str("\\\"");
        break;
      case '\\':
        st_write_str("\\\\");
        break;
      case '\b':
        st_write_str("\\b");
        break;
      case '\f':
        st_write_str("\\f");
        break;
      case '\n':
        st_write_str("\\n");
        break;
      case '\r':
        st_write_str("\\r");
        break;
      case '\t':
        st_write_str("\\t");
        break;
      default:
        if (c < 0x20 || c >= 0x7f) {
          char esc[6];
          static const char kHex[] = "0123456789abcdef";
          esc[0] = '\\';
          esc[1] = 'u';
          esc[2] = '0';
          esc[3] = '0';
          esc[4] = kHex[(c >> 4) & 0xF];
          esc[5] = kHex[c & 0xF];
          st_write(esc, sizeof(esc));
        } else {
          st_write_char((char)c);
        }
        break;
    }
  }
  st_write_char('"');
}

static void st_write_adapter(const char* msg, size_t len) {
  st_write(msg, len);
}

static void st_write_hex_json(uintptr_t value) {
  st_write_char('"');
  st_write_hex_uintptr(value);
  st_write_char('"');
}

static void st_json_on_header(const st_context_t* ctx, st_write_fn write, void* user) {
  (void)write;
  (void)user;
  st_write_str("{\"type\":\"header\",\"event\":");
  st_write_json_escaped(ctx && ctx->event ? ctx->event : "");
  st_write_str(",\"pid\":");
  st_write_uint64(ctx ? ctx->pid : 0);
  st_write_str(",\"tid\":");
  st_write_uint64(ctx ? ctx->tid : 0);
  st_write_str(",\"platform\":");
  st_write_json_escaped(ctx && ctx->platform ? ctx->platform : "");
  if (ctx && ctx->exception_name && ctx->exception_name[0]) {
    st_write_str(",\"exception_code\":");
    st_write_hex_json((uintptr_t)ctx->exception_code);
    st_write_str(",\"exception_name\":");
    st_write_json_escaped(ctx->exception_name);
  } else {
    st_write_str(",\"signal\":");
    st_write_uint64((uint64_t)(ctx ? ctx->signal : 0));
    st_write_str(",\"signal_name\":");
    st_write_json_escaped(ctx && ctx->signal_name ? ctx->signal_name : "");
  }
  st_write_str(",\"fault_address\":");
  if (ctx && ctx->fault_address != 0) {
    st_write_hex_json(ctx->fault_address);
  } else {
    st_write_str("null");
  }
  st_write_str(",\"ip\":");
  st_write_hex_json(ctx ? ctx->ip : 0);
  st_write_str(",\"sp\":");
  st_write_hex_json(ctx ? ctx->sp : 0);
  st_write_str(",\"bp\":");
  st_write_hex_json(ctx ? ctx->bp : 0);
  st_write_str("}\n");
}

static void st_json_on_frame_raw(int index, uintptr_t pc, st_write_fn write, void* user) {
  (void)write;
  (void)user;
  st_write_str("{\"type\":\"frame_raw\",\"frame_index\":");
  st_write_uint64((uint64_t)index);
  st_write_str(",\"pc\":");
  st_write_hex_json(pc);
  st_write_str("}\n");
}

static void st_json_on_frame(const st_frame_t* frame, st_write_fn write, void* user) {
  (void)write;
  (void)user;
  st_write_str("{\"type\":\"frame\",\"frame_index\":");
  st_write_uint64((uint64_t)(frame ? frame->index : 0));
  st_write_str(",\"pc\":");
  st_write_hex_json(frame ? frame->pc : 0);
  st_write_str(",\"function\":");
  st_write_json_escaped(frame && frame->function && frame->function[0] ? frame->function : "??");
  st_write_str(",\"file\":");
  st_write_json_escaped(frame && frame->file && frame->file[0] ? frame->file : "??");
  st_write_str(",\"line\":");
  st_write_uint64((uint64_t)(frame ? frame->line : 0));
  st_write_str(",\"column\":");
  st_write_uint64((uint64_t)(frame ? frame->column : 0));
  st_write_str(",\"module\":");
  st_write_json_escaped(frame && frame->module && frame->module[0] ? frame->module : "??");
  st_write_str("}\n");
}

static void st_json_on_maps_line(const char* line, st_write_fn write, void* user) {
  (void)write;
  (void)user;
  st_write_str("{\"type\":\"maps\",\"line\":");
  st_write_json_escaped(line ? line : "");
  st_write_str("}\n");
}

static void st_json_on_footer(const st_context_t* ctx, int exit_code, st_write_fn write, void* user) {
  (void)write;
  (void)user;
  st_write_str("{\"type\":\"footer\"");
  if (ctx && ctx->exception_name && ctx->exception_name[0]) {
    st_write_str(",\"exception_code\":");
    st_write_hex_json((uintptr_t)ctx->exception_code);
  } else {
    st_write_str(",\"signal\":");
    st_write_uint64((uint64_t)(ctx ? ctx->signal : 0));
  }
  st_write_str(",\"exit_code\":");
  st_write_uint64((uint64_t)exit_code);
  st_write_str("}\n");
}

static void st_py_on_header(const st_context_t* ctx, st_write_fn write, void* user) {
  (void)write;
  (void)user;
  st_write_str("Stacktrace (event=");
  st_write_str(ctx && ctx->event ? ctx->event : "");
  st_write_str(", platform=");
  st_write_str(ctx && ctx->platform ? ctx->platform : "");
  st_write_str(", pid=");
  st_write_uint64(ctx ? ctx->pid : 0);
  st_write_str(", tid=");
  st_write_uint64(ctx ? ctx->tid : 0);
  if (ctx && ctx->exception_name && ctx->exception_name[0]) {
    st_write_str(", exception=");
    st_write_str(ctx->exception_name);
    st_write_char('(');
    st_write_hex_uintptr((uintptr_t)ctx->exception_code);
    st_write_char(')');
  } else {
    st_write_str(", signal=");
    st_write_str(ctx && ctx->signal_name ? ctx->signal_name : "UNKNOWN");
    st_write_char('(');
    st_write_uint64((uint64_t)(ctx ? ctx->signal : 0));
    st_write_char(')');
  }
  st_write_str(", fault=");
  if (ctx && ctx->fault_address) {
    st_write_hex_uintptr(ctx->fault_address);
  } else {
    st_write_str("null");
  }
  st_write_str(", ip=");
  st_write_hex_uintptr(ctx ? ctx->ip : 0);
  st_write_str(", sp=");
  st_write_hex_uintptr(ctx ? ctx->sp : 0);
  st_write_str(", bp=");
  st_write_hex_uintptr(ctx ? ctx->bp : 0);
  st_write_str(")\n");
}

static void st_py_on_frame_raw(int index, uintptr_t pc, st_write_fn write, void* user) {
  (void)write;
  (void)user;
  if (index == 0) {
    st_write_str("Raw stack (PCs):\n");
  }
  st_write_str("  #");
  st_write_uint64((uint64_t)index);
  st_write_str(" ");
  st_write_hex_uintptr(pc);
  st_write_str("\n");
}

static void st_py_on_frame(const st_frame_t* frame, st_write_fn write, void* user) {
  const char* file = (frame && frame->file && frame->file[0]) ? frame->file : "??";
  const char* func = (frame && frame->function && frame->function[0]) ? frame->function : "??";
  const char* module = (frame && frame->module && frame->module[0]) ? frame->module : "";
  (void)write;
  (void)user;
  if (frame && frame->index == 0) {
    st_write_str("Symbolized stack (most recent call first):\n");
  }
  st_write_str("  File \"");
  st_write_str(file);
  st_write_str("\", line ");
  st_write_uint64((uint64_t)(frame ? frame->line : 0));
  st_write_str(", in ");
  st_write_str(func);
  st_write_str("\n    pc=");
  st_write_hex_uintptr(frame ? frame->pc : 0);
  if (module[0]) {
    st_write_str(" module=");
    st_write_str(module);
  }
  st_write_str("\n");
}

static void st_py_on_maps_line(const char* line, st_write_fn write, void* user) {
  (void)write;
  (void)user;
  st_write_str("  maps: ");
  st_write_str(line ? line : "");
  st_write_str("\n");
}

static void st_py_on_footer(const st_context_t* ctx, int exit_code, st_write_fn write, void* user) {
  (void)write;
  (void)user;
  st_write_str("Stacktrace end (exit_code=");
  st_write_uint64((uint64_t)exit_code);
  if (ctx && ctx->exception_name && ctx->exception_name[0]) {
    st_write_str(", exception=");
    st_write_str(ctx->exception_name);
    st_write_char('(');
    st_write_hex_uintptr((uintptr_t)ctx->exception_code);
    st_write_char(')');
  } else {
    st_write_str(", signal=");
    st_write_str(ctx && ctx->signal_name ? ctx->signal_name : "UNKNOWN");
    st_write_char('(');
    st_write_uint64((uint64_t)(ctx ? ctx->signal : 0));
    st_write_char(')');
  }
  st_write_str(")\n");
}

static const st_formatter_t k_json_formatter = {
  st_json_on_header,
  st_json_on_frame_raw,
  st_json_on_frame,
  st_json_on_maps_line,
  st_json_on_footer
};

static const st_formatter_t k_py_formatter = {
  st_py_on_header,
  st_py_on_frame_raw,
  st_py_on_frame,
  st_py_on_maps_line,
  st_py_on_footer
};

static const st_formatter_t* st_builtin_formatter(void) {
  if (g_options.format_kind == ST_FORMAT_PYTHON) {
    return &k_py_formatter;
  }
  return &k_json_formatter;
}

void st_format_header(const st_context_t* ctx) {
  const st_formatter_t* custom = g_options.formatter;
  const st_formatter_t* builtin = st_builtin_formatter();
  if (custom && custom->on_header) {
    custom->on_header(ctx, st_write_adapter, g_options.formatter_user);
    return;
  }
  if (builtin && builtin->on_header) {
    builtin->on_header(ctx, st_write_adapter, NULL);
  }
}

void st_format_frame_raw(int index, uintptr_t pc) {
  const st_formatter_t* custom = g_options.formatter;
  const st_formatter_t* builtin = st_builtin_formatter();
  if (g_options.emit_raw_frames == 0) {
    return;
  }
  if (custom && custom->on_frame_raw) {
    custom->on_frame_raw(index, pc, st_write_adapter, g_options.formatter_user);
    return;
  }
  if (builtin && builtin->on_frame_raw) {
    builtin->on_frame_raw(index, pc, st_write_adapter, NULL);
  }
}

void st_format_frame(const st_frame_t* frame) {
  const st_formatter_t* custom = g_options.formatter;
  const st_formatter_t* builtin = st_builtin_formatter();
  if (custom && custom->on_frame) {
    custom->on_frame(frame, st_write_adapter, g_options.formatter_user);
    return;
  }
  if (builtin && builtin->on_frame) {
    builtin->on_frame(frame, st_write_adapter, NULL);
  }
}

void st_format_maps_line(const char* line) {
  const st_formatter_t* custom = g_options.formatter;
  const st_formatter_t* builtin = st_builtin_formatter();
  if (custom && custom->on_maps_line) {
    custom->on_maps_line(line, st_write_adapter, g_options.formatter_user);
    return;
  }
  if (builtin && builtin->on_maps_line) {
    builtin->on_maps_line(line, st_write_adapter, NULL);
  }
}

void st_format_footer(const st_context_t* ctx, int exit_code) {
  const st_formatter_t* custom = g_options.formatter;
  const st_formatter_t* builtin = st_builtin_formatter();
  if (custom && custom->on_footer) {
    custom->on_footer(ctx, exit_code, st_write_adapter, g_options.formatter_user);
    return;
  }
  if (builtin && builtin->on_footer) {
    builtin->on_footer(ctx, exit_code, st_write_adapter, NULL);
  }
}

int st_init(const st_options_t* opt) {
  if (opt && opt->enable == 0) {
    st_shutdown();
    return 0;
  }
  if (g_initialized) {
    if (opt) {
      st_apply_options(opt);
    }
    return 0;
  }
  if (opt) {
    st_apply_options(opt);
  }
  if (st_platform_init() != 0) {
    return -1;
  }
  g_initialized = 1;
  return 0;
}

void st_shutdown(void) {
  if (!g_initialized) {
    return;
  }
  st_platform_shutdown();
  g_initialized = 0;
  st_free_symbolizer_path();
  g_options.symbolizer_path = NULL;
}

int st_dump_current_thread(void) {
  return st_platform_dump_current_thread();
}
