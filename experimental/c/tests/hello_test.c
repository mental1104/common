#include <assert.h>
#include <string.h>

#include "hello.h"

int main(void) {
    const char *result = get_world("hello world");
    assert(result != NULL);
    assert(strcmp(result, "world") == 0);

    assert(get_world("no match here") == NULL);
    return 0;
}
