#include <assert.h>
#include <setjmp.h>
#include <stdint.h>
#include "call_in_stack.h"

#define ALIGN_SIZE(size, align)	\
	(((uintptr_t)(size) + ((uintptr_t)(align) - 1)) & ~((uintptr_t)(align) - 1))

/**
 * @brief Call `func(arg)` in stack and jump back to `orig`
 * @param[in] orig      Jumpback address
 * @param[in] longjmp   Jumpback function
 * @param[in] func      User defined function
 * @param[in] arg       User defined argument
 */
void bus__call_in_stack_asm(jmp_buf orig, void (*longjmp)(jmp_buf, int),
    void (*func)(void* arg), void* arg);

void bus__call_in_stack(void* stack, size_t size, void (*func)(void* arg), void* arg)
{
    assert(size > sizeof(jmp_buf) + sizeof(void*));

    /**
     * Calculate stack base address
     * The stack address must align to sizeof(void*)
     */
    void* base_addr = (char*)stack + size;
    void* align_addr = (void*)ALIGN_SIZE(base_addr, sizeof(void*));
    if (base_addr != align_addr)
    {
        base_addr = align_addr - sizeof(void*);
    }

    /* Save context */
    void* jump_addr = base_addr - sizeof(jmp_buf);
    if (setjmp(jump_addr) != 0)
    {
        return;
    }

    /* Switch stack and call function */
    bus__call_in_stack_asm(jump_addr, longjmp, func, arg);
}
