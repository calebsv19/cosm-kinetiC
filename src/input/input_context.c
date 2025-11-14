#include "input/input_context.h"

#include <string.h>

void input_context_manager_init(InputContextManager *mgr) {
    if (!mgr) return;
    memset(mgr, 0, sizeof(*mgr));
    mgr->top = -1;
}

bool input_context_manager_push(InputContextManager *mgr,
                                const InputContext *ctx) {
    if (!mgr || !ctx) return false;
    if (mgr->top >= (INPUT_CONTEXT_STACK_CAPACITY - 1)) {
        return false;
    }
    mgr->top += 1;
    mgr->stack[mgr->top] = *ctx;
    return true;
}

bool input_context_manager_pop(InputContextManager *mgr) {
    if (!mgr || mgr->top < 0) return false;
    mgr->top -= 1;
    return true;
}

InputContext *input_context_manager_current(InputContextManager *mgr) {
    if (!mgr || mgr->top < 0) return NULL;
    return &mgr->stack[mgr->top];
}
