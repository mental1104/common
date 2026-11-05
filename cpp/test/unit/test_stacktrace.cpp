#include <gtest/gtest.h>

#include <cstdint>

#include "mental1104/debug/stacktrace.h"

namespace {

static const char kHeaderMarker[] = "STACKTRACE_TEST_HEADER\n";
static const char kFooterMarker[] = "STACKTRACE_TEST_FOOTER\n";

static void st_test_on_header(const st_context_t* ctx, st_write_fn write, void* user) {
  (void)ctx;
  (void)user;
  write(kHeaderMarker, sizeof(kHeaderMarker) - 1);
}

static void st_test_on_frame_raw(int index, uintptr_t pc, st_write_fn write, void* user) {
  (void)index;
  (void)pc;
  (void)write;
  (void)user;
}

static void st_test_on_frame(const st_frame_t* frame, st_write_fn write, void* user) {
  (void)frame;
  (void)write;
  (void)user;
}

static void st_test_on_maps_line(const char* line, st_write_fn write, void* user) {
  (void)line;
  (void)write;
  (void)user;
}

static void st_test_on_footer(const st_context_t* ctx,
                              int exit_code,
                              st_write_fn write,
                              void* user) {
  (void)ctx;
  (void)exit_code;
  (void)user;
  write(kFooterMarker, sizeof(kFooterMarker) - 1);
}

static const st_formatter_t kTestFormatter = {
    st_test_on_header,
    st_test_on_frame_raw,
    st_test_on_frame,
    st_test_on_maps_line,
    st_test_on_footer};

static void st_force_segv() {
  volatile int* p = reinterpret_cast<volatile int*>(0x0);
  *p = 1;
}

static void st_crash_with_formatter() {
  st_options_t opt = {};
  opt.enable = 1;
  opt.max_frames = 8;
  opt.use_altstack = 1;
  opt.chain_previous = 0;
  opt.exit_on_fatal = 1;
  opt.output_fd = 2;
  opt.write_cb = NULL;
  opt.user = NULL;
  opt.symbolizer_path = NULL;
  opt.dump_maps = 0;
  opt.format_kind = ST_FORMAT_PYTHON;
  opt.emit_raw_frames = 0;
  opt.formatter = &kTestFormatter;
  opt.formatter_user = NULL;

  st_init(&opt);
  st_force_segv();
}

}  // namespace

TEST(StacktraceTest, CustomFormatterDeathTest) {
#if defined(_WIN32)
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
#endif
  EXPECT_DEATH(st_crash_with_formatter(),
               "STACKTRACE_TEST_HEADER(.|\\n)*STACKTRACE_TEST_FOOTER");
}
