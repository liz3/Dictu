#include "future.h"

typedef struct {
    TaskTimer *timer;
    bool cancelled;
} TaskTimerAbstract;

#define AS_TASK_TIMER(v) ((TaskTimerAbstract *)AS_ABSTRACT(v)->data)

void release_uv_timer(uv_handle_t *handle) {
    FREE(vmFromUvHandle((uv_handle_t *)handle), uv_timer_t, handle);
}

void freeTaskTimerAbstract(DictuVM *vm, ObjAbstract *abstract) {
    TaskTimerAbstract *timer = (TaskTimerAbstract *)abstract->data;
    if (timer->timer->handle) {
        uv_close((uv_handle_t *)timer->timer->handle, release_uv_timer);
        timer->timer->handle = NULL;
    }
    if (timer->timer->context) {
        releaseAsyncContext(vm, timer->timer->context);
        timer->timer->context = NULL;
    }
    FREE(vm, TaskTimer, timer->timer);
    FREE(vm, TaskTimerAbstract, abstract->data);
}

char *taskTimerAbstractToString(ObjAbstract *abstract) {
    UNUSED(abstract);

    char *bufferString = malloc(sizeof(char) * 12);
    snprintf(bufferString, 12, "<TaskTimer>");
    return bufferString;
}

void grayTaskTimerAbstract(DictuVM *vm, ObjAbstract *abstract) {
    (void)vm;
    TaskTimerAbstract *entry = (TaskTimerAbstract *)abstract->data;

    if (entry == NULL)
        return;
}

static Value createNewFuture(DictuVM *vm, int argCount, Value *args) {
    UNUSED(args);
    UNUSED(argCount);
    ObjFuture *future = newFuture(vm);
    future->consumed = false;
    future->controlled = false;
    future->isAwait = false;
    return OBJ_VAL(future);
}

static Value cancelTimer(DictuVM *vm, int argCount, Value *args) {
    if (argCount > 0) {
        runtimeError(vm, "cancel() takes no arguments.");
        return EMPTY_VAL;
    }
    TaskTimerAbstract *tta = AS_TASK_TIMER(args[0]);
    if (tta->cancelled)
        return BOOL_VAL(false);
    uv_timer_stop(tta->timer->handle);
    if (!tta->timer->timeout || tta->timer->runCount == 0) {
        if(!tta->timer->timeout)
           tta->timer->context->refs--;
        vm->timerAmount--;
        if (tta->timer->handle) {
            uv_close((uv_handle_t *)tta->timer->handle,
                     release_uv_timer);
            tta->timer->handle = NULL;
        }
    }
    return BOOL_VAL(true);
}

static Value createTimer(DictuVM *vm, int argCount, Value *args, bool timeout) {
    if (argCount < 1 || (!IS_CLOSURE(args[0]) && IS_FUNCTION(args[0]))) {
        if (timeout)
            runtimeError(vm,
                         "runLater() first argument needs to be a callable.");
        else
            runtimeError(
                vm, "runInterval() first argument needs to be a callable.");
        return EMPTY_VAL;
    }
    if (argCount > 1 && !IS_NUMBER(args[1])) {
        if (timeout)
            runtimeError(vm, "runLater() second argument needs to a number.");
        else
            runtimeError(vm,
                         "runInterval() second argument needs to be a number.");
        return EMPTY_VAL;
    }
    ObjClosure *closure = NULL;
    ObjFunction *function;
    int interval = argCount > 1 ? AS_NUMBER(args[1]) : 1;
    if (interval < 1) {
        if (timeout)
            runtimeError(vm, "runLater() timeout needs to be >= 1.");
        else
            runtimeError(vm, "runInterval() interval value needs to be >= 1.");
        return EMPTY_VAL;
    }
    if (IS_CLOSURE(args[0])) {
        closure = AS_CLOSURE(args[0]);
        function = closure->function;

    } else {
        function = AS_FUNCTION(args[0]);
        push(vm, args[0]);
        closure = newClosure(vm, function);
        pop(vm);
    }

    ObjAbstract *abstract =
        newAbstract(vm, freeTaskTimerAbstract, taskTimerAbstractToString);
    push(vm, OBJ_VAL(abstract));
    TaskTimerAbstract *tta = ALLOCATE(vm, TaskTimerAbstract, 1);
    tta->cancelled = false;
    TaskTimer *timer = ALLOCATE(vm, TaskTimer, 1);
    uv_timer_t *handle = ALLOCATE(vm, uv_timer_t, 1);
    uv_timer_init(vm->uv_loop, handle);
    tta->timer = timer;
    AsyncContext *context = copyVmState(vm);
    timer->context = context;
    timer->timeout = timeout;
    timer->runCount = 0;
    vm->timerAmount++;
    context->breakFrame = context->frameCount;
    CallFrame *frame = &context->frames[context->frameCount++];

    frame->closure = closure;
    frame->ip = function->chunk.code;

    if (vm->asyncContextInScope) {
        context->ref = vm->asyncContextInScope;
        vm->asyncContextInScope->refs++;
    }
    context->stack[context->stackSize++] = args[1];
    timer->handle = handle;
    frame->slots = context->stack + (context->stackSize - 1);
    handle->data = timer;
    uv_timer_start(handle, el_timer_cb, interval, timeout ? 0 : interval);

    defineNative(vm, &abstract->values, "cancel", cancelTimer);
    abstract->data = tta;
    abstract->grayFunc = grayTaskTimerAbstract;
    pop(vm);

    return OBJ_VAL(abstract);
}

static Value createInterval(DictuVM *vm, int argCount, Value *args) {
    return createTimer(vm, argCount, args, false);
}

static Value runLater(DictuVM *vm, int argCount, Value *args) {
    return createTimer(vm, argCount, args, true);
}

Value createFutureModule(DictuVM *vm) {
    ObjString *name = copyString(vm, "Future", 6);
    push(vm, OBJ_VAL(name));
    ObjModule *module = newModule(vm, name);
    push(vm, OBJ_VAL(module));
    defineNative(vm, &module->values, "new", createNewFuture);
    defineNative(vm, &module->values, "runInterval", createInterval);
    defineNative(vm, &module->values, "runLater", runLater);

    pop(vm);
    pop(vm);

    return OBJ_VAL(module);
}
