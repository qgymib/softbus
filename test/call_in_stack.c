#include <string.h>
#include "test.h"
#include "call_in_stack.h"

static void _callback(void* arg)
{
    void** addr = (void**)arg;

    int var;
    *addr = &var;
}

TEST(call_in_stack)
{
    /* use valgrind to detect memory error */
    const size_t buffer_size = 1024;
    char* buffer = malloc(buffer_size);

    /* get variable address in stack */
    void* addr = NULL;
    bus__call_in_stack(buffer, buffer_size, _callback, &addr);

    /* if success, the variable address must in between buffer */
    ASSERT_GT_PTR(addr, buffer);
    ASSERT_LT_PTR(addr, buffer + buffer_size);

    free(buffer);
}
