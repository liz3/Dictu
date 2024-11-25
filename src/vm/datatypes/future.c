#include "future.h"

static Value setResolved(DictuVM *vm, int argCount, Value *args) {
    if (argCount != 1)
        return EMPTY_VAL;
    ObjFuture *future = AS_FUTURE(args[0]);
        if (future->controlled) {
      runtimeError(vm, "Cannot resolve a controlled future.");
      return EMPTY_VAL;
    }
    if (!future->pending)
        return EMPTY_VAL;
    future->pending = false;
    future->result = newResultSuccess(vm, args[1]);
    return args[0];
}

static Value setRejected(DictuVM *vm, int argCount, Value *args) {
    if (argCount != 1)
        return EMPTY_VAL;
      if(!IS_STRING(args[1])){
          runtimeError(vm, "Reject Value can only be a string.");
          return EMPTY_VAL;
      }
    ObjFuture *future = AS_FUTURE(args[0]);
    if (future->controlled) {
      runtimeError(vm, "Cannot reject a controlled future.");
      return EMPTY_VAL;
    }
    if (!future->pending)
        return EMPTY_VAL;
    future->pending = false;
    ObjString *str = AS_STRING(args[1]);
    future->result = newResultError(vm, str->chars);
    return args[0];
}

static Value then(DictuVM *vm, int argCount, Value *args) {
    if (argCount != 1)
        return EMPTY_VAL;
    ObjFuture *future = AS_FUTURE(args[0]);
    AsyncContext *context = copyVmState(vm);
    context->breakFrame = context->frameCount;
    CallFrame *frame = &context->frames[context->frameCount++];
    if (IS_FUNCTION(args[1])) {
        ObjFunction *function = AS_FUNCTION(args[1]);
        frame->closure = NULL;
        frame->ip = function->chunk.code;
    } else if (IS_CLOSURE(args[1])) {
        ObjClosure *closure = AS_CLOSURE(args[1]);
        frame->closure = closure;
        frame->ip = closure->function->chunk.code;
    }
    if(vm->asyncContextInScope){
      context->ref = vm->asyncContextInScope;
      vm->asyncContextInScope->refs++;
    }
    context->stack[context->stackSize++] = args[1];
    context->stack[context->stackSize++] = args[0];
    future->consumed = true;
    frame->slots = context->stack+(context->stackSize -1);
    Task* t = createTask(vm, true);
    t->asyncContext = context;
    t->waitFor = future;

    return args[0];
}


void declareFutureMethods(DictuVM *vm) {
    defineNative(vm, &vm->futureMethods, "resolve", setResolved);
    defineNative(vm, &vm->futureMethods, "reject", setRejected);
    defineNative(vm, &vm->futureMethods, "result", then);
}