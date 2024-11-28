#ifndef dictu_files_h
#define dictu_files_h

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "../common.h"
#include "../util.h"

typedef struct {
  ObjFuture* future;
  ObjFile* file;
  DictuVM* vm;
  char* buffer;
  size_t bufferLen;
} AsyncFsRequest;

void declareFileMethods(DictuVM *vm);

void openFile(DictuVM* vm, ObjFile* file);
void closeFile(DictuVM *vm, ObjFile *file);
void async_maybe_close_file(DictuVM *vm, ObjFile *file, bool release);

#endif //dictu_files_h
