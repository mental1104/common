#include "stacktrace_internal.h"

#ifdef _WIN32

#include <dbghelp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

static LPTOP_LEVEL_EXCEPTION_FILTER g_prev_filter = NULL;
static int g_sym_initialized = 0;

static const char* st_exception_name(DWORD code) {
  switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
      return "EXCEPTION_ACCESS_VIOLATION";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
      return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
    case EXCEPTION_BREAKPOINT:
      return "EXCEPTION_BREAKPOINT";
    case EXCEPTION_DATATYPE_MISALIGNMENT:
      return "EXCEPTION_DATATYPE_MISALIGNMENT";
    case EXCEPTION_FLT_DENORMAL_OPERAND:
      return "EXCEPTION_FLT_DENORMAL_OPERAND";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
      return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
    case EXCEPTION_FLT_INEXACT_RESULT:
      return "EXCEPTION_FLT_INEXACT_RESULT";
    case EXCEPTION_FLT_INVALID_OPERATION:
      return "EXCEPTION_FLT_INVALID_OPERATION";
    case EXCEPTION_FLT_OVERFLOW:
      return "EXCEPTION_FLT_OVERFLOW";
    case EXCEPTION_FLT_STACK_CHECK:
      return "EXCEPTION_FLT_STACK_CHECK";
    case EXCEPTION_FLT_UNDERFLOW:
      return "EXCEPTION_FLT_UNDERFLOW";
    case EXCEPTION_ILLEGAL_INSTRUCTION:
      return "EXCEPTION_ILLEGAL_INSTRUCTION";
    case EXCEPTION_IN_PAGE_ERROR:
      return "EXCEPTION_IN_PAGE_ERROR";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
      return "EXCEPTION_INT_DIVIDE_BY_ZERO";
    case EXCEPTION_INT_OVERFLOW:
      return "EXCEPTION_INT_OVERFLOW";
    case EXCEPTION_INVALID_DISPOSITION:
      return "EXCEPTION_INVALID_DISPOSITION";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
      return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
    case EXCEPTION_PRIV_INSTRUCTION:
      return "EXCEPTION_PRIV_INSTRUCTION";
    case EXCEPTION_SINGLE_STEP:
      return "EXCEPTION_SINGLE_STEP";
    case EXCEPTION_STACK_OVERFLOW:
      return "EXCEPTION_STACK_OVERFLOW";
    default:
      return "EXCEPTION_UNKNOWN";
  }
}

static void st_windows_init_symbols(void) {
  if (g_sym_initialized) {
    return;
  }
  SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
  if (SymInitialize(GetCurrentProcess(), NULL, TRUE)) {
    g_sym_initialized = 1;
  }
}

static int st_capture_stack(CONTEXT* context, DWORD64* frames, int max_frames) {
  STACKFRAME64 frame;
  HANDLE process = GetCurrentProcess();
  HANDLE thread = GetCurrentThread();
  CONTEXT ctx = *context;
  DWORD machine = IMAGE_FILE_MACHINE_AMD64;
  int count = 0;

  memset(&frame, 0, sizeof(frame));
  frame.AddrPC.Offset = ctx.Rip;
  frame.AddrPC.Mode = AddrModeFlat;
  frame.AddrStack.Offset = ctx.Rsp;
  frame.AddrStack.Mode = AddrModeFlat;
  frame.AddrFrame.Offset = ctx.Rbp;
  frame.AddrFrame.Mode = AddrModeFlat;

  while (count < max_frames) {
    if (!StackWalk64(machine, process, thread, &frame, &ctx, NULL,
                     SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {
      break;
    }
    if (frame.AddrPC.Offset == 0) {
      break;
    }
    frames[count++] = frame.AddrPC.Offset;
  }
  return count;
}

static void st_symbolize_frame(DWORD64 pc,
                               char* function,
                               size_t function_size,
                               char* file,
                               size_t file_size,
                               DWORD* line,
                               char* module,
                               size_t module_size) {
  HANDLE process = GetCurrentProcess();
  DWORD64 displacement = 0;
  DWORD line_disp = 0;
  char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
  SYMBOL_INFO* sym = (SYMBOL_INFO*)buffer;
  IMAGEHLP_LINE64 line_info;
  IMAGEHLP_MODULE64 module_info;

  sym->SizeOfStruct = sizeof(SYMBOL_INFO);
  sym->MaxNameLen = MAX_SYM_NAME;
  function[0] = '\0';
  file[0] = '\0';
  module[0] = '\0';
  *line = 0;

  if (SymFromAddr(process, pc, &displacement, sym)) {
    strncpy(function, sym->Name, function_size - 1);
    function[function_size - 1] = '\0';
  }

  memset(&line_info, 0, sizeof(line_info));
  line_info.SizeOfStruct = sizeof(line_info);
  if (SymGetLineFromAddr64(process, pc, &line_disp, &line_info)) {
    strncpy(file, line_info.FileName, file_size - 1);
    file[file_size - 1] = '\0';
    *line = line_info.LineNumber;
  }

  memset(&module_info, 0, sizeof(module_info));
  module_info.SizeOfStruct = sizeof(module_info);
  if (SymGetModuleInfo64(process, pc, &module_info)) {
    strncpy(module, module_info.ImageName, module_size - 1);
    module[module_size - 1] = '\0';
  }
}

static LONG WINAPI st_exception_handler(EXCEPTION_POINTERS* ep) {
  const st_options_t* opt = st_get_options();
  DWORD64 frames[ST_MAX_FRAMES_LIMIT];
  int max_frames = opt ? opt->max_frames : 64;
  int frame_count = 0;
  DWORD code = ep->ExceptionRecord->ExceptionCode;
  uintptr_t ip = 0;
  uintptr_t sp = 0;
  uintptr_t bp = 0;
  uintptr_t fault_address = 0;
  st_context_t ctx;

  if (max_frames <= 0 || max_frames > ST_MAX_FRAMES_LIMIT) {
    max_frames = ST_MAX_FRAMES_LIMIT;
  }

  st_windows_init_symbols();

  if (ep->ContextRecord) {
    ip = (uintptr_t)ep->ContextRecord->Rip;
    sp = (uintptr_t)ep->ContextRecord->Rsp;
    bp = (uintptr_t)ep->ContextRecord->Rbp;
  }

  if (code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2) {
    fault_address = (uintptr_t)ep->ExceptionRecord->ExceptionInformation[1];
  }

  ctx.event = "fatal";
  ctx.platform = st_platform_name();
  ctx.pid = (uint64_t)GetCurrentProcessId();
  ctx.tid = (uint64_t)GetCurrentThreadId();
  ctx.signal = 0;
  ctx.signal_name = "";
  ctx.exception_code = (uint32_t)code;
  ctx.exception_name = st_exception_name(code);
  ctx.fault_address = fault_address;
  ctx.ip = ip;
  ctx.sp = sp;
  ctx.bp = bp;
  st_format_header(&ctx);

  if (ep->ContextRecord) {
    frame_count = st_capture_stack(ep->ContextRecord, frames, max_frames);
  }

  {
    int i;
    for (i = 0; i < frame_count; ++i) {
      st_format_frame_raw(i, (uintptr_t)frames[i]);
    }
  }

  {
    int i;
    for (i = 0; i < frame_count; ++i) {
      char function[512];
      char file[1024];
      char module[512];
      DWORD line = 0;
      st_frame_t frame;
      st_symbolize_frame(frames[i], function, sizeof(function), file, sizeof(file), &line,
                         module, sizeof(module));

      frame.index = i;
      frame.pc = (uintptr_t)frames[i];
      frame.function = function[0] ? function : "??";
      frame.file = file[0] ? file : "??";
      frame.line = (int)line;
      frame.column = 0;
      frame.module = module[0] ? module : "??";
      st_format_frame(&frame);
    }
  }

  st_format_footer(&ctx, (int)code);

  if (opt && opt->chain_previous && g_prev_filter && g_prev_filter != st_exception_handler) {
    LONG prev = g_prev_filter(ep);
    if (!opt->exit_on_fatal) {
      return prev;
    }
  }

  if (opt && opt->exit_on_fatal) {
    TerminateProcess(GetCurrentProcess(), code);
  }

  return EXCEPTION_EXECUTE_HANDLER;
}

int st_platform_init(void) {
  st_windows_init_symbols();
  g_prev_filter = SetUnhandledExceptionFilter(st_exception_handler);
  return 0;
}

void st_platform_shutdown(void) {
  SetUnhandledExceptionFilter(g_prev_filter);
  g_prev_filter = NULL;
  if (g_sym_initialized) {
    SymCleanup(GetCurrentProcess());
    g_sym_initialized = 0;
  }
}

int st_platform_dump_current_thread(void) {
  DWORD64 frames[ST_MAX_FRAMES_LIMIT];
  CONTEXT ctx;
  int max_frames = 64;
  int frame_count = 0;
  int i;
  st_context_t header;

  const st_options_t* opt = st_get_options();
  if (opt && opt->max_frames > 0 && opt->max_frames <= ST_MAX_FRAMES_LIMIT) {
    max_frames = opt->max_frames;
  }

  st_windows_init_symbols();

  memset(&ctx, 0, sizeof(ctx));
  RtlCaptureContext(&ctx);

  header.event = "manual";
  header.platform = st_platform_name();
  header.pid = (uint64_t)GetCurrentProcessId();
  header.tid = (uint64_t)GetCurrentThreadId();
  header.signal = 0;
  header.signal_name = "MANUAL";
  header.exception_code = 0;
  header.exception_name = NULL;
  header.fault_address = 0;
  header.ip = (uintptr_t)ctx.Rip;
  header.sp = (uintptr_t)ctx.Rsp;
  header.bp = (uintptr_t)ctx.Rbp;
  st_format_header(&header);

  frame_count = st_capture_stack(&ctx, frames, max_frames);
  for (i = 0; i < frame_count; ++i) {
    st_format_frame_raw(i, (uintptr_t)frames[i]);
  }

  for (i = 0; i < frame_count; ++i) {
    char function[512];
    char file[1024];
    char module[512];
    DWORD line = 0;
    st_frame_t frame;
    st_symbolize_frame(frames[i], function, sizeof(function), file, sizeof(file), &line,
                       module, sizeof(module));

    frame.index = i;
    frame.pc = (uintptr_t)frames[i];
    frame.function = function[0] ? function : "??";
    frame.file = file[0] ? file : "??";
    frame.line = (int)line;
    frame.column = 0;
    frame.module = module[0] ? module : "??";
    st_format_frame(&frame);
  }

  st_format_footer(&header, 0);

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
