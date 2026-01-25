#include "mental1104/debug/stacktrace.h"

#include <stddef.h>

#if defined(_MSC_VER)
#define ST_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define ST_NOINLINE __attribute__((noinline))
#else
#define ST_NOINLINE
#endif

static ST_NOINLINE void st_touch(volatile int* value) {
  if (*value == 0x7fffffff) {
    st_dump_current_thread();
  }
}

static ST_NOINLINE void st_crash_leaf(int offset) {
  volatile int* p = (int*)0x0;
  p[offset] = 42;
}

static ST_NOINLINE void st_crash_level4(int offset) {
  st_crash_leaf(offset);
}

static ST_NOINLINE void st_crash_level3(int offset) {
  volatile int v = offset + 3;
  st_crash_level4(offset);
  st_touch(&v);
}

static ST_NOINLINE void st_crash_level2(int offset) {
  volatile int v = offset + 2;
  st_crash_level3(offset);
  st_touch(&v);
}

static ST_NOINLINE void st_crash_level1(int offset) {
  volatile int v = offset + 1;
  st_crash_level2(offset);
  st_touch(&v);
}

int main(void) {
  st_options_t opt;
  opt.enable = 1;
  opt.max_frames = 64;
  opt.use_altstack = 1;
  opt.chain_previous = 0;
  opt.exit_on_fatal = 1;
  opt.output_fd = 2;
  opt.write_cb = NULL;
  opt.user = NULL;
  opt.symbolizer_path = NULL;
  opt.dump_maps = 1;
  opt.format_kind = ST_FORMAT_PYTHON;
  opt.emit_raw_frames = 1;
  opt.formatter = NULL;
  opt.formatter_user = NULL;

  st_init(&opt);
  st_dump_current_thread();

  st_crash_level1(1);

  return 0;
}
