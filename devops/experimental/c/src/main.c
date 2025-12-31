#include <stdio.h>

#include "hello.h"

int main(void) {
    const char *result = get_world("hello world");
    if (result == NULL) {
        return 1;
    }

    printf("%s\n", result);
    return 0;
}
