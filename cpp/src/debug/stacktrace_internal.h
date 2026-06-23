#ifndef STACKTRACE_INTERNAL_H_
#define STACKTRACE_INTERNAL_H_

#include "mental1104/debug/stacktrace.h"

#include <stddef.h>
#include <stdint.h>

COMMON_EXTERN_C_BEGIN

#define ST_MAX_FRAMES_LIMIT 256

const st_options_t* st_get_options(void);
int st_is_initialized(void);
const char* st_get_symbolizer_path(void);
const char* st_platform_name(void);

void st_write(const char* msg, size_t len);
void st_write_str(const char* msg);
void st_write_char(char c);
void st_write_uint64(uint64_t value);
void st_write_hex_uintptr(uintptr_t value);
void st_write_json_escaped(const char* msg);
size_t st_safe_strlen(const char* msg);

void st_format_header(const st_context_t* ctx);
void st_format_frame_raw(int index, uintptr_t pc);
void st_format_frame(const st_frame_t* frame);
void st_format_maps_line(const char* line);
void st_format_footer(const st_context_t* ctx, int exit_code);

COMMON_EXTERN_C_END

#endif  // STACKTRACE_INTERNAL_H_
