#ifndef DEBUG_STACKTRACE_H_
#define DEBUG_STACKTRACE_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct st_options_s {
  int enable;
  int max_frames;
  int use_altstack;
  int chain_previous;
  int exit_on_fatal;
  int output_fd;
  void (*write_cb)(const char* msg, size_t len, void* user);
  void* user;
  const char* symbolizer_path;
  int dump_maps;
  int format_kind;
  int emit_raw_frames;
  const struct st_formatter_s* formatter;
  void* formatter_user;
} st_options_t;

typedef enum st_format_kind_e {
  ST_FORMAT_JSON = 0,
  ST_FORMAT_PYTHON = 1
} st_format_kind_t;

typedef struct st_context_s {
  const char* event;
  const char* platform;
  uint64_t pid;
  uint64_t tid;
  int signal;
  const char* signal_name;
  uint32_t exception_code;
  const char* exception_name;
  uintptr_t fault_address;
  uintptr_t ip;
  uintptr_t sp;
  uintptr_t bp;
} st_context_t;

typedef struct st_frame_s {
  int index;
  uintptr_t pc;
  const char* function;
  const char* file;
  int line;
  int column;
  const char* module;
} st_frame_t;

typedef void (*st_write_fn)(const char* msg, size_t len);

typedef struct st_formatter_s {
  void (*on_header)(const st_context_t* ctx, st_write_fn write, void* user);
  void (*on_frame_raw)(int index, uintptr_t pc, st_write_fn write, void* user);
  void (*on_frame)(const st_frame_t* frame, st_write_fn write, void* user);
  void (*on_maps_line)(const char* line, st_write_fn write, void* user);
  void (*on_footer)(const st_context_t* ctx, int exit_code, st_write_fn write, void* user);
} st_formatter_t;

int st_init(const st_options_t* opt);
void st_shutdown(void);
int st_dump_current_thread(void);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DEBUG_STACKTRACE_H_
