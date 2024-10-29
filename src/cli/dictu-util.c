#include "../optionals/json/jsonBuilderLib.h"
#include "../vm/ast_event.h"
#include "../vm/chunk.h"
#include "../vm/compiler.h"
#include "../vm/util.h"
#include "../vm/vm.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *ast_to_string(AstEventCode code) {
    const char *list[] = {"AST_DECLARE_CONST",
                          "AST_DECLARE_VAR",
                          "AST_DECLARE_FUNCTION",
                          "AST_DECLARE_ARROW_FUNCTION",
                          "AST_DECLARE_CLASS",
                          "AST_DECLARE_TRAIT",
                          "AST_DECLARE_CLASS_ABSTRACT",
                          "AST_DECLARE_FUNCTION_PARAM",
                          "AST_NAME",
                          "AST_SET",
                          "AST_RETURN",
                          "AST_DECLARE_ENUM",
                          "AST_ENUM_VALUE",
                          "AST_BLOCK",
                          "AST_BLOCK_END",
                          "AST_IMPORT",
                          "AST_END",
                          "AST_BRANCH",
                          "AST_FOR_LOOP",
                          "AST_WHILE_LOOP",
                          "AST_FUNCTION_CALL",
                          "AST_USE_STATEMENT",
                          "AST_INSTANCE",
                            "AST_SUBSCRIPT",
    "AST_INDEX",
    "AST_SUBSCRIPT_END",
    "AST_INDEX_END"};
    return list[code];
}

static char *readFileInternal(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char *buffer = malloc(sizeof(char) * (fileSize + 1));
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

typedef struct asEntry {
    struct asEntry *parent;
    json_value *current;
    AstEventCode type;
    int id;
} AstEntry;

typedef struct {
    AstEntry *entry;
    AstEntry *ref;
    int id;

} AstPrintInfo;

void astPrintValue(Value *value, AstPrintInfo *info) {
    UNUSED(info);
    if (IS_STRING(*value)) {
        ObjString *str = AS_STRING(*value);
        printf(" - [string]%s", str->chars);
    } else if (IS_NUMBER(*value)) {
        double v = AS_NUMBER(*value);
        printf(" - [number]%f", v);
    }
}

void printAstEmit(Compiler *compiler, AstEventCode code, void **args, int line,
                  void *userData) {
    AstPrintInfo *nf = (AstPrintInfo *)userData;
    int id = nf->id++;
    if (code == AST_BLOCK) {
        AstEntry *next = calloc(1, sizeof(AstEntry));
        next->id = id;
        next->current = json_array_new(0);
        next->parent = nf->entry;
        next->type = code;
        nf->entry = next;
        json_array_push(next->parent->current, next->current);
    } else if (code == AST_BLOCK_END || code == AST_SUBSCRIPT_END) {
        if (nf->entry->parent) {
            nf->entry = nf->entry->parent;
        }
    } else if (code == AST_DECLARE_CONST || code == AST_DECLARE_VAR ||
               code == AST_DECLARE_FUNCTION || code == AST_DECLARE_CLASS ||
               code == AST_DECLARE_TRAIT ||
               code == AST_DECLARE_CLASS_ABSTRACT || code == AST_DECLARE_ENUM) {
        AstEntry *next = calloc(1, sizeof(AstEntry));
        next->type = code;
        next->id = id;
        next->current = json_object_new(0);
        json_value *nameValue =
            json_string_new_length(*((int *)args[1]), (*((char **)args[0])));
        json_value *idValue = json_integer_new(id);
        json_value *codeValue = json_string_new(ast_to_string(code));
        json_value *lineValue = json_integer_new(line);
        json_object_push(next->current, "name", nameValue);
        json_object_push(next->current, "line", lineValue);
        json_object_push(next->current, "id", idValue);
        json_object_push(next->current, "code", codeValue);

        next->parent = nf->entry;
        nf->ref = next;
        json_array_push(nf->entry->current, next->current);

    } else if (code == AST_DECLARE_FUNCTION_PARAM) {
        json_value *o = json_object_new(0);
        json_value *codeValue = json_string_new(ast_to_string(code));
        json_value *lineValue = json_integer_new(line);

        json_value *nameValue =
            json_string_new_length(*((int *)args[1]), (*((char **)args[0])));
        json_object_push(o, "name", nameValue);
        json_object_push(o, "code", codeValue);
        json_object_push(o, "line", lineValue);
        if (nf->ref && nf->ref->type == AST_DECLARE_FUNCTION) {
            json_value *idValue = json_integer_new(nf->ref->id);
            json_object_push(o, "ref", idValue);
        }
        json_array_push(nf->entry->current, o);
    } else if (code == AST_FUNCTION_CALL) {
        json_value *o = json_object_new(0);
        json_value *codeValue = json_string_new(ast_to_string(code));
        json_value *lineValue = json_integer_new(line);
        json_object_push(o, "code", codeValue);
        json_object_push(o, "line", lineValue);
        json_value *nameValue =
            json_string_new_length(*((int *)args[1]), (*((char **)args[0])));
        if (args[2] != NULL) {
            json_object_push(o, "parent_name", nameValue);
            json_value *callee_name = json_string_new_length(
                *((int *)args[3]), (*((char **)args[2])));
            json_object_push(o, "name", callee_name);

        } else {
            json_object_push(o, "name", nameValue);
        }
        json_array_push(nf->entry->current, o);

    } else if (code == AST_USE_STATEMENT) {
        json_value *o = json_object_new(0);
        json_value *codeValue = json_string_new(ast_to_string(code));
        json_value *lineValue = json_integer_new(line);
        json_object_push(o, "code", codeValue);
        json_object_push(o, "line", lineValue);
        json_value *nameValue =
            json_string_new_length(*((int *)args[1]), (*((char **)args[0])));
        json_object_push(o, "name", nameValue);

        json_array_push(nf->entry->current, o);

    } else if(code == AST_SUBSCRIPT) {
        AstEntry *next = calloc(1, sizeof(AstEntry));
        next->id = id;
        next->current = json_array_new(0);
        next->parent = nf->entry;
        next->type = code;
        nf->entry = next;
          json_value *o = json_object_new(0);
        json_value *codeValue = json_string_new(ast_to_string(code));
        json_value *lineValue = json_integer_new(line);
        json_object_push(o, "code", codeValue);
        json_object_push(o, "line", lineValue);
        json_value *nameValue =
            json_string_new_length(*((int *)args[1]), (*((char **)args[0])));
        json_object_push(o, "name", nameValue);
        json_object_push(o, "body", next->current);
        json_array_push(next->parent->current, o);
    } else {
        json_value *o = json_object_new(0);
        json_value *codeValue = json_string_new(ast_to_string(code));
        json_value *lineValue = json_integer_new(line);
        json_object_push(o, "code", codeValue);
        json_object_push(o, "line", lineValue);
        json_array_push(nf->entry->current, o);
    }

    UNUSED(compiler);
    UNUSED(code);
    UNUSED(args);
    UNUSED(line);
    UNUSED(userData);
}
void printAstErrorAt(Parser *parser, LangToken *token, const char *message,
                     void *user_data) {
    UNUSED(parser);
    UNUSED(token);
    UNUSED(message);
    UNUSED(user_data);
}

void printAst(char *path) {
    char *moduleName = path;
    DictuVM *vm = dictuInitVM(false, 0, NULL); // todo check argc and argv args
    char *source = readFileInternal(path);
    AstPrintInfo info;
    AstEntry *root = calloc(1, sizeof(AstEntry));
    root->current = json_array_new(0);
    info.entry = root;
    ObjString *name = copyString(vm, moduleName, strlen(moduleName));
    push(vm, OBJ_VAL(name));
    ObjModule *module = newModule(vm, name);
    pop(vm);

    push(vm, OBJ_VAL(module));
    module->path = getDirectory(vm, moduleName);
    pop(vm);

    ObjFunction *function = compileExternal(vm, module, source, &printAstEmit,
                                            &printAstErrorAt, &info);
    if (function == NULL)
        return;
    char *buffer = malloc(json_measure(root->current));
    json_serialize(buffer, root->current);
    printf("%s\n", buffer);
}

int main(int argc, char **argv) {
    if (argc == 0) {
        return 1;
    }
    printAst(argv[1]);
    return 0;
}