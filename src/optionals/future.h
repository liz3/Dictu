#ifndef dictu_future_module_h
#define dictu_future_module_h

#include <stdlib.h>

#include "optionals.h"
#include "../vm/vm.h"


TaskTimer* createTaskTimer(DictuVM*vm, bool timeout);
Value createFutureModule(DictuVM *vm);

#endif //dictu_ffi_module_h
