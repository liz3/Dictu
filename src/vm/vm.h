#ifndef dictu_vm_h
#define dictu_vm_h

#include "compiler.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include <uv.h>

// TODO: Work out the maximum stack size at compilation time
#define STACK_MAX (64 * UINT8_COUNT)

typedef struct {
    ObjClosure *closure;
    uint8_t *ip;
    Value *slots;
} CallFrame;

typedef struct asyncContext {
    CallFrame *frames;
    int frameCount;
    int frameCapacity;
    ObjFuture *result;
    int breakFrame;
    Value stack[STACK_MAX];
    int stackSize;
    ObjUpvalue *openUpvalues;
    struct asyncContext *ref;
    int refs;
} AsyncContext;

typedef struct {
    uv_timer_t *handle;
    AsyncContext *context;
    bool timeout;
    int runCount;
} TaskTimer;

typedef struct {
    CallFrame *frame;
    ObjFuture *waitFor;
    AsyncContext *asyncContext;
} Task;

struct _vm {
    Compiler *compiler;
    uv_loop_t *uv_loop;
    Value *stack;
    Value *stackTop;
    bool repl;
    CallFrame *frames;
    AsyncContext *asyncContextInScope;
    AsyncContext **asyncContexts;
    Task **tasks;
    int taskCount;
    int asyncContextCount;
    int frameCount;
    int frameCapacity;
    int timerAmount;
    ObjModule *lastModule;
    Table modules;
    Table globals;
    Table constants;
    Table strings;
    Table numberMethods;
    Table boolMethods;
    Table nilMethods;
    Table stringMethods;
    Table listMethods;
    Table dictMethods;
    Table setMethods;
    Table fileMethods;
    Table futureMethods;
    Table classMethods;
    Table instanceMethods;
    Table resultMethods;
    Table enumMethods;
    ObjString *initString;
    ObjString *annotationString;
    ObjString *replVar;
    ObjUpvalue *openUpvalues;
    size_t bytesAllocated;
    size_t nextGC;
    Obj *objects;
    int grayCount;
    int grayCapacity;
    Obj **grayStack;
    int argc;
    char **argv;
};

#define OK 0
#define NOTOK -1

void push(DictuVM *vm, Value value);

Value peek(DictuVM *vm, int distance);

void runtimeError(DictuVM *vm, const char *format, ...);

Value pop(DictuVM *vm);

bool isFalsey(Value value);

ObjClosure *compileModuleToClosure(DictuVM *vm, char *name, char *source);

Value callFunction(DictuVM *vm, Value function, int argCount, Value *args);

Task *createTask(DictuVM *vm, bool prepend);
AsyncContext *copyVmState(DictuVM *vm);
void releaseAsyncContext(DictuVM *vm, AsyncContext *ctx);

void el_timer_cb(uv_timer_t *handle);

#endif
