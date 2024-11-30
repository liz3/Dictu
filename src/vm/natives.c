#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <uv.h>

#include "common.h"
#include "memory.h"
#include "natives.h"
#include "object.h"
#include "util.h"
#include "value.h"
#include "vm.h"
#include "../optionals/optionals.h"

typedef struct {
    ObjFuture* future;
    char* buffer;
    int bufferLen;
    DictuVM* vm;
} AsyncInputPayload;

// Callback to allocate memory for the incoming data
void async_input_alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    UNUSED(handle);
    buf->base = (char *)malloc(suggested_size);
    buf->len = suggested_size;
}

void async_input_close(uv_handle_t* e){
    FREE(vmFromUvHandle(e), uv_pipe_t, e);
}
// Callback to handle incoming data from stdin
void async_input_read_stdin(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    AsyncInputPayload* p = stream->data;
    DictuVM* vm = p->vm;
    if (nread > 0) {
        p->buffer = p->buffer ? GROW_ARRAY(vm, p->buffer, char, p->bufferLen, p->bufferLen + nread) : ALLOCATE(vm, char, nread);
        memcpy(p->buffer + p->bufferLen, buf->base, nread);
        p->bufferLen += nread;
        if(p->buffer[p->bufferLen-1] == '\n'){
         ObjFuture* ft = p->future;
         ft->pending = false;
         ObjString* str = copyString(vm, p->buffer, p->bufferLen-1);
         ft->result = newResultSuccess(vm, OBJ_VAL(str));
         FREE_ARRAY(vm, char, p->buffer, p->bufferLen);
         FREE(vm, AsyncInputPayload, p);
         uv_close((uv_handle_t *)stream, async_input_close);
     }
 }
    // Free the memory allocated for the buffer
 if (buf->base) {
    free(buf->base);
}
}

// Native functions
static Value typeNative(DictuVM *vm, int argCount, Value *args) {
    if (argCount != 1) {
        runtimeError(vm, "type() takes 1 argument (%d given).", argCount);
        return EMPTY_VAL;
    }

    int length = 0;
    char *type = valueTypeToString(vm, args[0], &length);
    return OBJ_VAL(takeString(vm, type, length));
}

static Value setNative(DictuVM *vm, int argCount, Value *args) {
    ObjSet *set = newSet(vm);
    push(vm, OBJ_VAL(set));

    for (int i = 0; i < argCount; i++) {
        setInsert(vm, set, args[i]);
    }
    pop(vm);

    return OBJ_VAL(set);
}

static Value inputNative(DictuVM *vm, int argCount, Value *args) {
    if (argCount > 1) {
        runtimeError(vm, "input() takes either 0 or 1 arguments (%d given)", argCount);
        return EMPTY_VAL;
    }

    if (argCount != 0) {
        Value prompt = args[0];
        if (!IS_STRING(prompt)) {
            runtimeError(vm, "input() only takes a string argument");
            return EMPTY_VAL;
        }

        printf("%s", AS_CSTRING(prompt));
    }

    uint64_t currentSize = 128;
    char *line = ALLOCATE(vm, char, currentSize);

    if (line == NULL) {
        runtimeError(vm, "Memory error on input()!");
        return EMPTY_VAL;
    }

    int c = EOF;
    uint64_t length = 0;
    while ((c = getchar()) != '\n' && c != EOF) {
        line[length++] = (char) c;

        if (length + 1 == currentSize) {
            int oldSize = currentSize;
            currentSize = GROW_CAPACITY(currentSize);
            line = GROW_ARRAY(vm, line, char, oldSize, currentSize);

            if (line == NULL) {
                printf("Unable to allocate memory\n");
                exit(71);
            }
        }
    }

    // If length has changed, shrink
    if (length != currentSize) {
        line = SHRINK_ARRAY(vm, line, char, currentSize, length + 1);
    }

    line[length] = '\0';

    return OBJ_VAL(takeString(vm, line, length));
}

static Value asyncInputNative(DictuVM *vm, int argCount, Value *args) {
    if (argCount > 1) {
        runtimeError(vm, "input() takes either 0 or 1 arguments (%d given)", argCount);
        return EMPTY_VAL;
    }

    if (argCount != 0) {
        Value prompt = args[0];
        if (!IS_STRING(prompt)) {
            runtimeError(vm, "input() only takes a string argument");
            return EMPTY_VAL;
        }

        printf("%s", AS_CSTRING(prompt));
        fflush(stdout);
    }

    uv_pipe_t* stdin_pipe = ALLOCATE(vm, uv_pipe_t, 1);
    uv_pipe_init(vm->uv_loop, stdin_pipe, 0);
    uv_pipe_open(stdin_pipe, STDIN_FILENO);
    ObjFuture* ft = newFuture(vm);
    AsyncInputPayload* pl = ALLOCATE(vm, AsyncInputPayload, 1);
    pl->future = ft;
    pl->vm =vm;
    pl->bufferLen = 0;
    pl->buffer = NULL;
    ft->controlled = true;
    stdin_pipe->data = pl;
    uv_read_start((uv_stream_t *)stdin_pipe, async_input_alloc_buffer, async_input_read_stdin);
    return OBJ_VAL(ft);
}

static Value printNative(DictuVM *vm, int argCount, Value *args) {
    UNUSED(vm);

    if (argCount == 0) {
        printf("\n");
        return NIL_VAL;
    }

    for (int i = 0; i < argCount; ++i) {
        printValue(args[i]);
        printf("\n");
    }

    return NIL_VAL;
}

static Value printErrorNative(DictuVM *vm, int argCount, Value *args) {
    UNUSED(vm);

    if (argCount == 0) {
        fprintf(stderr, "\n");
        return NIL_VAL;
    }

    for (int i = 0; i < argCount; ++i) {
        printValueError(args[i]);
        fprintf(stderr, "\n");
    }

    return NIL_VAL;
}

static Value assertNative(DictuVM *vm, int argCount, Value *args) {
    if (argCount != 1) {
        runtimeError(vm, "assert() takes 1 argument (%d given)", argCount);
        return EMPTY_VAL;
    }

    if (isFalsey(args[0])) {
        runtimeError(vm, "assert() was false!");
        return EMPTY_VAL;
    }

    return NIL_VAL;
}

static Value isDefinedNative(DictuVM *vm, int argCount, Value *args) {
    if (argCount != 1) {
        runtimeError(vm, "isDefined() takes 1 argument (%d given).", argCount);
        return EMPTY_VAL;
    }

    if (!IS_STRING(args[0])) {
        runtimeError(vm, "isDefined() only takes a string as an argument");
        return EMPTY_VAL;
    }

    ObjString *string = AS_STRING(args[0]);

    Value value;
    CallFrame *frame = &vm->frames[vm->frameCount - 1];
    if (tableGet(&frame->closure->function->module->values, string, &value))
       return TRUE_VAL;

    if (tableGet(&vm->globals, string, &value))
        return TRUE_VAL;

    bool _;

    if (findBuiltinModule(string->chars, string->length, &_) != -1)
        return TRUE_VAL;

    return FALSE_VAL;
}

static Value generateSuccessResult(DictuVM *vm, int argCount, Value *args) {
    if (argCount != 1) {
        runtimeError(vm, "Success() takes 1 argument (%d given).", argCount);
        return EMPTY_VAL;
    }

    return newResultSuccess(vm, args[0]);
}

static Value generateErrorResult(DictuVM *vm, int argCount, Value *args) {
    if (argCount != 1) {
        runtimeError(vm, "Error() takes 1 argument (%d given).", argCount);
        return EMPTY_VAL;
    }

    if (!IS_STRING(args[0])) {
        runtimeError(vm, "Error() only takes a string as an argument");
        return EMPTY_VAL;
    }

    return OBJ_VAL(newResult(vm, ERR, args[0]));
}

// End of natives

void defineAllNatives(DictuVM *vm) {
    char *nativeNames[] = {
            "input",
            "asyncInput",
            "type",
            "set",
            "print",
            "printError",
            "assert",
            "isDefined",
            "Success",
            "Error"
    };

    NativeFn nativeFunctions[] = {
            inputNative,
            asyncInputNative,
            typeNative,
            setNative,
            printNative,
            printErrorNative,
            assertNative,
            isDefinedNative,
            generateSuccessResult,
            generateErrorResult
    };

    for (uint8_t i = 0; i < sizeof(nativeNames) / sizeof(nativeNames[0]); ++i) {
        defineNative(vm, &vm->globals, nativeNames[i], nativeFunctions[i]);
    }
}
