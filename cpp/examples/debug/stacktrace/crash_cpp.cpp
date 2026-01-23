#include "mental1104/debug/stacktrace.h"

#include <cstddef>

#if defined(_MSC_VER)
#define ST_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define ST_NOINLINE __attribute__((noinline))
#else
#define ST_NOINLINE
#endif

class StackTraceGuard {
 public:
  explicit StackTraceGuard(const st_options_t& opt) {
    st_init(&opt);
  }

  ~StackTraceGuard() {
    st_shutdown();
  }

  StackTraceGuard(const StackTraceGuard&) = delete;
  StackTraceGuard& operator=(const StackTraceGuard&) = delete;
};

class CrashChain {
 public:
  explicit CrashChain(int seed) : seed_(seed) {}

  void Run() {
    Step1(seed_);
  }

 private:
  int seed_;

  ST_NOINLINE void Touch(int value) {
    volatile int v = value;
    if (v == 0x7fffffff) {
      st_dump_current_thread();
    }
  }

  ST_NOINLINE void Step1(int value) {
    Step2(value + 1);
    Touch(value);
  }

  ST_NOINLINE void Step2(int value) {
    Step3(value + 1);
    Touch(value);
  }

  ST_NOINLINE void Step3(int value) {
    Step4(value + 1);
    Touch(value);
  }

  ST_NOINLINE void Step4(int value) {
    Crash(value);
  }

  ST_NOINLINE void Crash(int value) {
    volatile int* p = reinterpret_cast<int*>(0x0);
    p[value] = 7;
  }
};

int main() {
  st_options_t opt = {};
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

  StackTraceGuard guard(opt);
  st_dump_current_thread();

  CrashChain chain(1);
  chain.Run();

  return 0;
}
