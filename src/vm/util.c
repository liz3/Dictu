#include "util.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "memory.h"
#include "vm.h"

char *readFile(DictuVM *vm, const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char *buffer = ALLOCATE(vm, char, fileSize + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }

    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }

    buffer[bytesRead] = '\0';

    fclose(file);
    return buffer;
}

ObjString *dirname(DictuVM *vm, char *path, int len) {
    if (!len) {
        return copyString(vm, ".", 1);
    }

    char *sep = path + len;

    /* trailing slashes */
    while (sep != path) {
        if (0 == IS_DIR_SEPARATOR(*sep))
            break;
        sep--;
    }

    /* first found */
    while (sep != path) {
        if (IS_DIR_SEPARATOR(*sep))
            break;
        sep--;
    }

    /* trim again */
    while (sep != path) {
        if (0 == IS_DIR_SEPARATOR(*sep))
            break;
        sep--;
    }

    if (sep == path && !IS_DIR_SEPARATOR(*sep)) {
        return copyString(vm, ".", 1);
    }

    len = sep - path + 1;

    return copyString(vm, path, len);
}

bool resolvePath(char *directory, char *path, char *ret) {
    char buf[PATH_MAX];
    if (*path == DIR_SEPARATOR)
        snprintf(buf, PATH_MAX, "%s", path);
    else
        snprintf(buf, PATH_MAX, "%s%c%s", directory, DIR_SEPARATOR, path);

#ifdef _WIN32
    _fullpath(ret, buf, PATH_MAX);
#else
    if (realpath(buf, ret) == NULL) {
        return false;
    }
#endif

    return true;
}

ObjString *getDirectory(DictuVM *vm, char *source) {
    // Slight workaround to ensure only .du files are the ones
    // attempted to be found.
    int len = strlen(source);
    if (vm->repl || len < 4 || source[len - 3] != '.') {
        source = "";
    }

    char res[PATH_MAX];
    if (!resolvePath(".", source, res)) {
        runtimeError(vm, "Unable to resolve path '%s'", source);
        exit(1);
    }

    if (vm->repl) {
        return copyString(vm, res, strlen(res));
    }

    return dirname(vm, res, strlen(res));
}

void defineNative(DictuVM *vm, Table *table, const char *name,
                  NativeFn function) {
    ObjNative *native = newNative(vm, function);
    push(vm, OBJ_VAL(native));
    ObjString *methodName = copyString(vm, name, strlen(name));
    push(vm, OBJ_VAL(methodName));
    tableSet(vm, table, methodName, OBJ_VAL(native));
    pop(vm);
    pop(vm);
}

void defineNativeProperty(DictuVM *vm, Table *table, const char *name,
                          Value value) {
    push(vm, value);
    ObjString *propertyName = copyString(vm, name, strlen(name));
    push(vm, OBJ_VAL(propertyName));
    tableSet(vm, table, propertyName, value);
    pop(vm);
    pop(vm);
}

bool isValidKey(Value value) {
    if (IS_NIL(value) || IS_BOOL(value) || IS_NUMBER(value) ||
        IS_STRING(value)) {
        return true;
    }

    return false;
}

Value boolNative(DictuVM *vm, int argCount, Value *args) {
    if (argCount != 0) {
        runtimeError(vm, "bool() takes no arguments (%d given).", argCount);
        return EMPTY_VAL;
    }

    return BOOL_VAL(!isFalsey(args[0]));
}
uint64_t getTimeMs() {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    unsigned long long millisecondsSinceEpoch =
        (unsigned long long)(tv.tv_sec) * 1000 +
        (unsigned long long)(tv.tv_usec) / 1000;
    return millisecondsSinceEpoch;
}
int msleep(uint32_t msec){
    struct timespec ts;
    int res;

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;
    res = nanosleep(&ts, &ts);
    return res;
}