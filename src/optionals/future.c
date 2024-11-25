#include "future.h"

static Value createNewFuture(DictuVM *vm, int argCount, Value *args) {
  UNUSED(args);
  UNUSED(argCount);
  ObjFuture* future = newFuture(vm);
  future->consumed = false;
  future->controlled = false;
  future->isAwait = false;
  return OBJ_VAL(future);
}


Value createFutureModule(DictuVM *vm) {
    ObjString *name = copyString(vm, "Future", 6);
    push(vm, OBJ_VAL(name));
    ObjModule *module = newModule(vm, name);
    push(vm, OBJ_VAL(module));
    defineNative(vm, &module->values, "new", createNewFuture);

    pop(vm);
    pop(vm);

    return OBJ_VAL(module);
}
