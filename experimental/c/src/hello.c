#include <string.h>

#include "hello.h"

const char *get_world(const char *greeting) {
    if (greeting == NULL) {
        return NULL;
    }

    const char *needle = "world";
    const char *pos = strstr(greeting, needle);
    return pos != NULL ? pos : NULL;
}
