#include "files.h"
#include "../memory.h"
#include <assert.h>
#include <string.h>
#include <uv.h>

uv_fs_t *new_fs_request(DictuVM *vm, ObjFile *file) {
    assert(file->asyncApi != NULL);
    return ALLOCATE(vm, uv_fs_t, 1);
}

AsyncFsRequest *new_async_fs_request(DictuVM *vm, ObjFile *file,
                                     ObjFuture *future) {
    AsyncFsRequest *fr = ALLOCATE(vm, AsyncFsRequest, 1);
    fr->file = file;
    fr->vm = vm;
    fr->buffer = NULL;
    fr->bufferLen = 0;
    if (future) {
        fr->future = future;
    } else {
        fr->future = newFuture(vm);
        fr->future->controlled = true;
    }
    return fr;
}

void async_on_open(uv_fs_t *req) {
    AsyncFsRequest *fr = req->data;
    DictuVM *vm = fr->vm;
    if (req->result < 0) {
        fr->future->pending = false;
        fr->future->result =
            newResultError(vm, (char *)uv_strerror(req->result));
    } else {
        fr->future->pending = false;
        fr->file->asyncApi->ready = true;
        fr->file->asyncApi->fd = req->result;
        fr->future->result = newResultSuccess(vm, TRUE_VAL);
    }
    uv_fs_req_cleanup(req);
    FREE(vm, uv_fs_t, req);
    FREE(vm, AsyncFsRequest, fr);
}
void async_on_close(uv_fs_t *req) {
    assert(req->result >= 0);
    AsyncFsRequest *fr = req->data;
    DictuVM *vm = fr->vm;

    FREE(vm, AsyncFile, fr->file->asyncApi);
    fr->file->asyncApi = NULL;
    uv_fs_req_cleanup(req);
    FREE(vm, uv_fs_t, req);
    FREE(vm, AsyncFsRequest, fr);
}
void async_on_write(uv_fs_t *req) {
    AsyncFsRequest *fr = req->data;
    DictuVM *vm = fr->vm;
    fr->future->pending = false;
    if (req->result < 0) {
        fr->future->result =
            newResultError(vm, (char *)uv_strerror(req->result));
    } else {
        fr->future->result = newResultSuccess(vm, NUMBER_VAL(req->result));
    }
    FREE_ARRAY(vm, char, fr->buffer, fr->bufferLen);
    uv_fs_req_cleanup(req);
    FREE(vm, uv_fs_t, req);
    FREE(vm, AsyncFsRequest, fr);
}
void async_on_read(uv_fs_t *req) {
    AsyncFsRequest *fr = req->data;
    DictuVM *vm = fr->vm;
    if (req->result < 0) {
        FREE_ARRAY(vm, char, fr->buffer, fr->bufferLen);
        fr->future->pending = false;
        fr->future->result =
            newResultError(vm, (char *)uv_strerror(req->result));
    } else {
        printf("%zd\n", req->result);
        if (req->result == 1024) {
            AsyncFsRequest *newRequest =
                new_async_fs_request(vm, fr->file, fr->future);
            newRequest->bufferLen = fr->bufferLen + 1024;
            newRequest->buffer = GROW_ARRAY(vm, fr->buffer, char, fr->bufferLen,
                                            newRequest->bufferLen);
            uv_fs_t *new_req = new_fs_request(vm, fr->file);
            new_req->data = newRequest;
            uv_buf_t iov =
                uv_buf_init(newRequest->buffer + fr->bufferLen, 1024);
            uv_fs_read(uv_default_loop(), new_req, fr->file->asyncApi->fd, &iov,
                       1, fr->bufferLen, async_on_read);

        } else {
            ObjString *result = copyString(
                vm, fr->buffer,
                fr->bufferLen == 1024 ? req->result
                                      : fr->bufferLen - (1024 - req->result));
            FREE_ARRAY(vm, char, fr->buffer, fr->bufferLen);
            fr->future->result = newResultSuccess(vm, OBJ_VAL(result));
            fr->future->pending = false;
        }
    }
    uv_fs_req_cleanup(req);
    FREE(vm, uv_fs_t, req);
    FREE(vm, AsyncFsRequest, fr);
}

int file_modes_from_str(char *str) {
    if (strcmp(str, "r") == 0 || strcmp(str, "rb") == 0)
        return UV_FS_O_RDONLY;
    if (strcmp(str, "w") == 0 || strcmp(str, "wb") == 0)
        return UV_FS_O_WRONLY | UV_FS_O_TRUNC | UV_FS_O_CREAT;
    if (strcmp(str, "a") == 0)
        return UV_FS_O_WRONLY | UV_FS_O_APPEND | UV_FS_O_CREAT;
    if (strcmp(str, "r+") == 0)
        return UV_FS_O_RDWR;
    if (strcmp(str, "w+") == 0)
        return UV_FS_O_RDWR | UV_FS_O_TRUNC | UV_FS_O_CREAT;
    if (strcmp(str, "a+") == 0 || strcmp(str, "wb") == 0)
        return UV_FS_O_RDWR | UV_FS_O_APPEND | UV_FS_O_CREAT;

    assert(false);
}

static Value writeFile(DictuVM *vm, int argCount, Value *args) {
    if (argCount != 1) {
        runtimeError(vm, "write() takes 1 argument (%d given)", argCount);
        return EMPTY_VAL;
    }

    if (!IS_STRING(args[1])) {
        runtimeError(vm, "write() argument must be a string");
        return EMPTY_VAL;
    }

    ObjFile *file = AS_FILE(args[0]);
    ObjString *string = AS_STRING(args[1]);

    if (strcmp(file->openType, "r") == 0 || strcmp(file->openType, "rb") == 0) {
        runtimeError(vm, "File is not writable!");
        return EMPTY_VAL;
    }

    int charsWrote = 0;

    if (file->asyncApi) {
        uv_fs_t *req = new_fs_request(vm, file);
        AsyncFsRequest *fr = new_async_fs_request(vm, file, NULL);
        req->data = fr;
        fr->buffer = ALLOCATE(vm, char, string->length);
        fr->bufferLen = string->length;
        memcpy(fr->buffer, string->chars, fr->bufferLen);
        uv_buf_t iov = uv_buf_init(fr->buffer, fr->bufferLen);
        uv_fs_write(uv_default_loop(), req, file->asyncApi->fd, &iov, 1, -1,
                    async_on_write);
        return OBJ_VAL(fr->future);
    }

    if (strcmp(file->openType, "wb") == 0) {
        charsWrote = fwrite(string->chars, 1, string->length, file->file);
    } else {
        charsWrote = fprintf(file->file, "%s", string->chars);
    }
    fflush(file->file);

    return NUMBER_VAL(charsWrote);
}

static Value writeLineFile(DictuVM *vm, int argCount, Value *args) {
    if (argCount != 1) {
        runtimeError(vm, "writeLine() takes 1 argument (%d given)", argCount);
        return EMPTY_VAL;
    }

    if (!IS_STRING(args[1])) {
        runtimeError(vm, "writeLine() argument must be a string");
        return EMPTY_VAL;
    }

    ObjFile *file = AS_FILE(args[0]);
    ObjString *string = AS_STRING(args[1]);

    if (strcmp(file->openType, "r") == 0) {
        runtimeError(vm, "File is not writable!");
        return EMPTY_VAL;
    }

    if (file->asyncApi) {
        uv_fs_t *req = new_fs_request(vm, file);
        AsyncFsRequest *fr = new_async_fs_request(vm, file, NULL);
        req->data = fr;
        fr->buffer = ALLOCATE(vm, char, string->length+1);
        fr->bufferLen = string->length+1;
        fr->buffer[string->length] = '\n';
        memcpy(fr->buffer, string->chars, fr->bufferLen-1);
        uv_buf_t iov = uv_buf_init(fr->buffer, fr->bufferLen);
        uv_fs_write(uv_default_loop(), req, file->asyncApi->fd, &iov, 1, -1,
                    async_on_write);
        return OBJ_VAL(fr->future);
    }


    int charsWrote = fprintf(file->file, "%s\n", string->chars);
    fflush(file->file);

    return NUMBER_VAL(charsWrote);
}

static Value readFullFile(DictuVM *vm, int argCount, Value *args) {
    if (argCount != 0) {
        runtimeError(vm, "read() takes no arguments (%d given)", argCount);
        return EMPTY_VAL;
    }

    ObjFile *file = AS_FILE(args[0]);

    if (file->asyncApi) {
        uv_fs_t *req = new_fs_request(vm, file);
        AsyncFsRequest *fr = new_async_fs_request(vm, file, NULL);
        req->data = fr;
        fr->buffer = ALLOCATE(vm, char, 1024);
        fr->bufferLen = 1024;
        uv_buf_t iov = uv_buf_init(fr->buffer, fr->bufferLen);
        uv_fs_read(uv_default_loop(), req, file->asyncApi->fd, &iov, 1, 0,
                   async_on_read);
        return OBJ_VAL(fr->future);
    }

    size_t currentPosition = ftell(file->file);
    // Calculate file size
    fseek(file->file, 0L, SEEK_END);
    size_t fileSize = ftell(file->file);
    fseek(file->file, currentPosition, SEEK_SET);

    char *buffer = ALLOCATE(vm, char, fileSize + 1);
    if (buffer == NULL) {
        runtimeError(vm, "Not enough memory to read \"%s\".\n", file->path);
        return EMPTY_VAL;
    }

    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file->file);
    if (bytesRead < fileSize && !feof(file->file)) {
        FREE_ARRAY(vm, char, buffer, fileSize + 1);
        runtimeError(vm, "Could not read file \"%s\".\n", file->path);
        return EMPTY_VAL;
    }

    if (bytesRead != fileSize) {
        buffer = SHRINK_ARRAY(vm, buffer, char, fileSize + 1, bytesRead + 1);
    }

    buffer[bytesRead] = '\0';

    return OBJ_VAL(takeString(vm, buffer, bytesRead));
}

static Value readLineFile(DictuVM *vm, int argCount, Value *args) {
    if (argCount > 1) {
        runtimeError(vm, "readLine() takes at most 1 argument (%d given)",
                     argCount);
        return EMPTY_VAL;
    }

    int readLineBufferSize = 4096;

    if (argCount == 1) {
        if (!IS_NUMBER(args[1])) {
            runtimeError(vm, "readLine() argument must be a number");
            return EMPTY_VAL;
        }

        readLineBufferSize = AS_NUMBER(args[1]) + 1;
    }

#ifdef _WIN32
    char *line = ALLOCATE(vm, char, readLineBufferSize);
#else
    char line[readLineBufferSize];
#endif

    ObjFile *file = AS_FILE(args[0]);
    if (fgets(line, readLineBufferSize, file->file) != NULL) {
        int lineLength = strlen(line);
        // Remove newline char
        if (line[lineLength - 1] == '\n') {
            lineLength--;
            line[lineLength] = '\0';
        }
        return OBJ_VAL(copyString(vm, line, lineLength));
    }

#ifdef _WIN32
    FREE(vm, char, line);
#endif

    return NIL_VAL;
}

static Value seekFile(DictuVM *vm, int argCount, Value *args) {
    if (argCount != 1 && argCount != 2) {
        runtimeError(vm, "seek() takes 1 or 2 arguments (%d given)", argCount);
        return EMPTY_VAL;
    }

    int seekType = SEEK_SET;

    if (argCount == 2) {
        if (!IS_NUMBER(args[1]) || !IS_NUMBER(args[2])) {
            runtimeError(vm, "seek() arguments must be numbers");
            return EMPTY_VAL;
        }

        int seekTypeNum = AS_NUMBER(args[2]);

        switch (seekTypeNum) {
        case 0:
            seekType = SEEK_SET;
            break;
        case 1:
            seekType = SEEK_CUR;
            break;
        case 2:
            seekType = SEEK_END;
            break;
        default:
            seekType = SEEK_SET;
            break;
        }
    }

    if (!IS_NUMBER(args[1])) {
        runtimeError(vm, "seek() argument must be a number");
        return EMPTY_VAL;
    }

    int offset = AS_NUMBER(args[1]);
    ObjFile *file = AS_FILE(args[0]);

    if (offset != 0 && !strstr(file->openType, "b")) {
        runtimeError(vm, "seek() may not have non-zero offset if file is "
                         "opened in text mode");
        return EMPTY_VAL;
    }

    fseek(file->file, offset, seekType);

    return NIL_VAL;
}

static Value isReady(DictuVM *vm, int argCount, Value *args) {
    UNUSED(args);
    if (argCount > 0) {
        runtimeError(vm, "isReady() takes no argument, (%d) given.", argCount);
        return EMPTY_VAL;
    }
    ObjFile *file = AS_FILE(args[0]);
    if (!file->asyncApi) {
        runtimeError(vm, "File does not have the async api enabled.");
    }
    return OBJ_VAL(file->asyncApi->readyFuture);
}

void declareFileMethods(DictuVM *vm) {
    defineNative(vm, &vm->fileMethods, "write", writeFile);
    defineNative(vm, &vm->fileMethods, "writeLine", writeLineFile);
    defineNative(vm, &vm->fileMethods, "read", readFullFile);
    defineNative(vm, &vm->fileMethods, "readLine", readLineFile);
    defineNative(vm, &vm->fileMethods, "seek", seekFile);
    defineNative(vm, &vm->fileMethods, "isReady", isReady);
}

void openFile(DictuVM *vm, ObjFile *file) {
    if (file->asyncApi) {
        memset(file->asyncApi, 0, sizeof(AsyncFile)); // reset
        ObjFuture *readyFuture = newFuture(vm);
        readyFuture->controlled = true;
        file->asyncApi->readyFuture = readyFuture;
        uv_fs_t *open_req = new_fs_request(vm, file);
        open_req->data = new_async_fs_request(vm, file, readyFuture);
        uv_fs_open(vm->uv_loop, open_req, file->path,
                   file_modes_from_str(file->openType), S_IRUSR | S_IWUSR | S_IRGRP, async_on_open);
    } else {
        file->file = fopen(file->path, file->openType);

        if (file->file == NULL) {
            runtimeError(vm, "Unable to open file '%s'", file->path);
        }
    }
}
void closeFile(DictuVM *vm, ObjFile *file) {
    if (file->asyncApi && file->asyncApi->ready) {
        uv_fs_t *close_req = new_fs_request(vm, file);
        close_req->data = new_async_fs_request(vm, file, NULL);
        uv_fs_close(vm->uv_loop, close_req, file->asyncApi->fd, async_on_close);
    } else {
        fclose(file->file);
        file->file = NULL;
    }
}