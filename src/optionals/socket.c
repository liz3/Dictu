#include "socket.h"

#include <netinet/in.h>
#include <stdio.h>
#include <uv.h>

#ifdef _WIN32
#include "windowsapi.h"
#include <io.h>
#define setsockopt(S, LEVEL, OPTNAME, OPTVAL, OPTLEN)                          \
    setsockopt(S, LEVEL, OPTNAME, (char *)(OPTVAL), OPTLEN)

#ifndef __MINGW32__
// Fixes deprecation warning
unsigned long inet_addr_new(const char *cp) {
    unsigned long S_addr;
    inet_pton(AF_INET, cp, &S_addr);
    return S_addr;
}
#define inet_addr(cp) inet_addr_new(cp)
#endif

#define write(fd, buffer, count) _write(fd, buffer, count)
#define close(fd) closesocket(fd)
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

typedef struct {
    uv_tcp_t handle;
    Value connection_func;
    bool isServer;
    DictuVM *vm;
} AsyncSocket;

typedef struct {
    int socket;
    int socketFamily;   /* Address family, e.g., AF_INET */
    int socketType;     /* Socket type, e.g., SOCK_STREAM */
    int socketProtocol; /* Protocol type, usually 0 */
    AsyncSocket *asyncSocket;
} SocketData;

typedef struct {
    ObjFuture* future;
    SocketData* socket;
} AsyncWritePayload;

#define AS_SOCKET(v) ((SocketData *)AS_ABSTRACT(v)->data)

ObjAbstract *newSocket(DictuVM *vm, int sock, int socketFamily, int socketType,
                       int socketProtocol, AsyncSocket *asyncSocket);

void async_socket_on_close(uv_handle_t* handle) {
    SocketData *d = (SocketData *)handle->data;
    DictuVM *vm = d->asyncSocket->vm;
    // TODO close callback?
    FREE(vm, AsyncSocket, d->asyncSocket);
    d->asyncSocket = NULL;
     vm->asyncSockets -= 1;
}

void async_on_new_connection(uv_stream_t *server, int status) {
    if (status < 0) {
        return;
    }
    SocketData *d = (SocketData *)server->data;
    DictuVM *vm = d->asyncSocket->vm;
    ObjAbstract *newSock =
        newSocket(vm, 0, d->socketFamily, d->socketProtocol, 0,
                      ALLOCATE(vm, AsyncSocket, 1));
    AsyncSocket* newAsyncSocket = ((SocketData*)newSock->data)->asyncSocket;
     memset(newAsyncSocket, 0, sizeof(AsyncSocket));

    newAsyncSocket->connection_func = NIL_VAL;
    newAsyncSocket->vm = vm;
    uv_tcp_t *client = &newAsyncSocket->handle;
    client->data = (SocketData*)newSock->data;
    uv_tcp_init(d->asyncSocket->vm->uv_loop, client);

    if (uv_accept(server, (uv_stream_t *)client) == 0) {
        ObjList *list = newList(vm);
        push(vm, OBJ_VAL(list));
        push(vm, OBJ_VAL(newSock));
        writeValueArray(vm, &list->values, OBJ_VAL(newSock));
        pop(vm);
        // // IPv6 is 39 chars
        // char ip[40];
        // inet_ntop(d->socketFamily, &client.sin_addr, ip, 40);
        // ObjString *string = copyString(vm, ip, strlen(ip));

        // push(vm, OBJ_VAL(string));
        // writeValueArray(vm, &list->values, OBJ_VAL(string));
        // pop(vm);

        pop(vm);
        Value v = OBJ_VAL(list);
        callFunction(vm, d->asyncSocket->connection_func, 1, &v);
    } else {
        uv_close((uv_handle_t *)client, async_socket_on_close);
    }
}

void async_socket_alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    UNUSED(handle);
    buf->base = (char *)malloc(suggested_size);
    buf->len = suggested_size;
}
void async_socket_on_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {
    if (nread > 0) {
        SocketData *d = (SocketData *)client->data;
        DictuVM *vm = d->asyncSocket->vm;
        ObjString* str = copyString(vm, buf->base, nread);
        Value v = OBJ_VAL(str);
        callFunction(vm,  d->asyncSocket->connection_func, 1, &v);
    } else if (nread < 0) {
        // if (nread != UV_EOF) {
        //     fprintf(stderr, "Read error: %s\n", uv_err_name(nread));
        // }
        uv_close((uv_handle_t *)client, async_socket_on_close);
    }

    // Free the read buffer memory
    free(buf->base);
}
void async_socket_on_write(uv_write_t *req, int status) {
    AsyncWritePayload* payload = req->data;
    SocketData *d = payload->socket;
    DictuVM *vm = d->asyncSocket->vm;
    payload->future->pending = false;
    if(status < 0) {
        payload->future->result = newResultError(vm, (char*)uv_strerror(status));
    } else {
        payload->future->result = newResultSuccess(vm, TRUE_VAL);
    }

    FREE(vm, AsyncWritePayload, payload);
    FREE(vm, uv_write_t, req);
}

void async_socket_on_connect(uv_connect_t *req, int status) {
    AsyncWritePayload* payload = req->data;
    SocketData *d = payload->socket;
    DictuVM *vm = d->asyncSocket->vm;
    payload->future->pending = false;
    if(status < 0) {
        payload->future->result = newResultError(vm, (char*)uv_strerror(status));
    } else {
        payload->future->result = newResultSuccess(vm, TRUE_VAL);
    }

    FREE(vm, AsyncWritePayload, payload);
    FREE(vm, uv_connect_t, req);
}

static Value createSocket(DictuVM *vm, int argCount, Value *args) {
    if (argCount != 2) {
        runtimeError(vm, "create() takes 2 arguments (%d given)", argCount);
        return EMPTY_VAL;
    }

    if (!IS_NUMBER(args[0]) || !IS_NUMBER(args[1])) {
        runtimeError(vm, "create() arguments must be a numbers");
        return EMPTY_VAL;
    }

    int socketFamily = AS_NUMBER(args[0]);
    int socketType = AS_NUMBER(args[1]);

    int sock = socket(socketFamily, socketType, 0);
    if (sock == -1) {
        ERROR_RESULT;
    }

    ObjAbstract *s = newSocket(vm, sock, socketFamily, socketType, 0, NULL);
    return newResultSuccess(vm, OBJ_VAL(s));
}

static Value createAsyncSocket(DictuVM *vm, int argCount, Value *args) {
    if (argCount != 2) {
        runtimeError(vm, "createAsync() takes 2 arguments (%d given)",
                     argCount);
        return EMPTY_VAL;
    }

    if (!IS_NUMBER(args[0]) || !IS_NUMBER(args[1])) {
        runtimeError(vm, "createAsync() arguments must be a numbers");
        return EMPTY_VAL;
    }

    int socketFamily = AS_NUMBER(args[0]);
    int socketType = AS_NUMBER(args[1]);

    ObjAbstract *s = newSocket(vm, 0, socketFamily, socketType, 0,
                               ALLOCATE(vm, AsyncSocket, 1));
    SocketData *d = (SocketData *)s->data;
    memset(d->asyncSocket, 0, sizeof(AsyncSocket));
    d->asyncSocket->connection_func = NIL_VAL;
    d->asyncSocket->vm = vm;

    int ret =
        uv_tcp_init_ex(vm->uv_loop, &d->asyncSocket->handle, socketFamily);
    if (ret) {
        ERROR_RESULT;
    }
    d->asyncSocket->handle.data = d;

    return newResultSuccess(vm, OBJ_VAL(s));
}

static Value bindSocket(DictuVM *vm, int argCount, Value *args) {
    if (argCount != 2) {
        runtimeError(vm, "bind() takes 2 arguments (%d given)", argCount);
        return EMPTY_VAL;
    }

    if (!IS_STRING(args[1])) {
        runtimeError(vm, "host passed to bind() must be a string");
        return EMPTY_VAL;
    }

    if (!IS_NUMBER(args[2])) {
        runtimeError(vm, "port passed to bind() must be a number");
        return EMPTY_VAL;
    }

    SocketData *sock = AS_SOCKET(args[0]);
    char *host = AS_CSTRING(args[1]);
    int port = AS_NUMBER(args[2]);

    struct sockaddr_in server;

    server.sin_family = sock->socketFamily;
    server.sin_addr.s_addr = inet_addr(host);
    server.sin_port = htons(port);

    if (sock->asyncSocket) {
        if (uv_tcp_bind(&sock->asyncSocket->handle,
                        (const struct sockaddr *)&server, 0)) {
            ERROR_RESULT;
        }
    } else {
        if (bind(sock->socket, (struct sockaddr *)&server, sizeof(server)) <
            0) {
            ERROR_RESULT;
        }
    }

    return newResultSuccess(vm, NIL_VAL);
}

static Value listenSocket(DictuVM *vm, int argCount, Value *args) {
    SocketData *sock = AS_SOCKET(args[0]);

    if (argCount > 1 && !sock->asyncSocket) {
        runtimeError(vm, "listen() takes 0 or 1 arguments (%d given)",
                     argCount);
        return EMPTY_VAL;
    } else if (sock->asyncSocket && (argCount < 1 || argCount > 2)) {
        runtimeError(vm, "listen() takes 1 or 2 arguments (%d given)",
                     argCount);
        return EMPTY_VAL;
    }

    int backlog = SOMAXCONN;

    if ((!sock->asyncSocket && argCount == 1) ||
        (sock->asyncSocket && argCount == 2)) {

        if (!IS_NUMBER(args[sock->asyncSocket ? 2 : 1])) {
            runtimeError(vm, "listen() argument must be a number");
            return EMPTY_VAL;
        }
        backlog = AS_NUMBER(args[sock->asyncSocket ? 2 : 1]);
    }

    if (sock->asyncSocket) {
        if (!IS_CLOSURE(args[1])) {
            ERROR_RESULT;
        }
        sock->asyncSocket->isServer = true;
        sock->asyncSocket->connection_func = args[1];
        int r = uv_listen((uv_stream_t *)&sock->asyncSocket->handle, backlog,
                          async_on_new_connection);
        if (r) {
            ERROR_RESULT;
        }
    } else {
    if (listen(sock->socket, backlog) == -1) {
        ERROR_RESULT;
    }
    }

    return newResultSuccess(vm, NIL_VAL);
}

static Value acceptSocket(DictuVM *vm, int argCount, Value *args) {
    if (argCount != 0) {
        runtimeError(vm, "accept() takes no arguments (%d given)", argCount);
        return EMPTY_VAL;
    }

    SocketData *sock = AS_SOCKET(args[0]);

    struct sockaddr_in client;
    int c = sizeof(struct sockaddr_in);
    int newSockId =
        accept(sock->socket, (struct sockaddr *)&client, (socklen_t *)&c);

    if (newSockId < 0) {
        ERROR_RESULT;
    }

    ObjList *list = newList(vm);
    push(vm, OBJ_VAL(list));

    ObjAbstract *newSock = newSocket(vm, newSockId, sock->socketFamily,
                                     sock->socketProtocol, 0, NULL);

    push(vm, OBJ_VAL(newSock));
    writeValueArray(vm, &list->values, OBJ_VAL(newSock));
    pop(vm);

    // IPv6 is 39 chars
    char ip[40];
    inet_ntop(sock->socketFamily, &client.sin_addr, ip, 40);
    ObjString *string = copyString(vm, ip, strlen(ip));

    push(vm, OBJ_VAL(string));
    writeValueArray(vm, &list->values, OBJ_VAL(string));
    pop(vm);

    pop(vm);

    return newResultSuccess(vm, OBJ_VAL(list));
}

static Value writeSocket(DictuVM *vm, int argCount, Value *args) {
    if (argCount != 1) {
        runtimeError(vm, "write() takes 1 argument (%d given)", argCount);
        return EMPTY_VAL;
    }

    if (!IS_STRING(args[1])) {
        runtimeError(vm, "write() argument must be a string");
        return EMPTY_VAL;
    }

    SocketData *sock = AS_SOCKET(args[0]);
    ObjString *message = AS_STRING(args[1]);

    if(sock->asyncSocket){
        if(sock->asyncSocket->isServer){
            ERROR_RESULT;
        }
        uv_write_t* req = ALLOCATE(vm, uv_write_t, 1);
        uv_buf_t write_buf = uv_buf_init(message->chars, message->length);
        ObjFuture* ft = newFuture(vm);
        AsyncWritePayload* payload = ALLOCATE(vm, AsyncWritePayload, 1);
        payload->future = ft;
        payload->socket = sock;
        req->data = payload;
        ft->controlled = true;
        uv_write(req, (uv_stream_t*)&sock->asyncSocket->handle, &write_buf, 1, async_socket_on_write);
        return OBJ_VAL(ft);
    }

    int writeRet = write(sock->socket, message->chars, message->length);

    if (writeRet == -1) {
        ERROR_RESULT;
    }

    return newResultSuccess(vm, NUMBER_VAL(writeRet));
}

static Value recvSocket(DictuVM *vm, int argCount, Value *args) {
    if (argCount != 1) {
        runtimeError(vm, "recv() takes 1 argument (%d given)", argCount);
        return EMPTY_VAL;
    }
    SocketData *sock = AS_SOCKET(args[0]);

    if (!sock->asyncSocket && !IS_NUMBER(args[1])) {
        runtimeError(vm, "recv() argument must be a number");
        return EMPTY_VAL;
    }
    if (sock->asyncSocket && !IS_CLOSURE(args[1])) {
        runtimeError(vm, "recv() argument must be a callback");
        return EMPTY_VAL;
    }

    if(sock->asyncSocket){
        if(sock->asyncSocket->isServer) {
            runtimeError(vm, "recv() called on a server");
            return EMPTY_VAL;
        }
        sock->asyncSocket->connection_func = args[1];
        uv_read_start((uv_stream_t *)&sock->asyncSocket->handle, async_socket_alloc_buffer, async_socket_on_read);
        return newResultSuccess(vm, NIL_VAL);
    }

    int bufferSize = AS_NUMBER(args[1]) + 1;

    if (bufferSize < 1) {
        runtimeError(vm, "recv() argument must be greater than 1");
        return EMPTY_VAL;
    }

    char *buffer = ALLOCATE(vm, char, bufferSize);
    int readSize = recv(sock->socket, buffer, bufferSize - 1, 0);

    if (readSize == -1) {
        FREE_ARRAY(vm, char, buffer, bufferSize);
        ERROR_RESULT;
    }

    // Resize string
    if (readSize != bufferSize) {
        buffer = SHRINK_ARRAY(vm, buffer, char, bufferSize, readSize + 1);
    }

    buffer[readSize] = '\0';
    ObjString *rString = takeString(vm, buffer, readSize);

    return newResultSuccess(vm, OBJ_VAL(rString));
}

static Value connectSocket(DictuVM *vm, int argCount, Value *args) {
    if (argCount != 2) {
        runtimeError(vm, "connect() takes two arguments (%d given)", argCount);
        return EMPTY_VAL;
    }

    if (!IS_STRING(args[1])) {
        runtimeError(vm, "host passed to bind() must be a string");
        return EMPTY_VAL;
    }

    if (!IS_NUMBER(args[2])) {
        runtimeError(vm, "port passed to bind() must be a number");
        return EMPTY_VAL;
    }

    SocketData *sock = AS_SOCKET(args[0]);

    struct sockaddr_in server;

    server.sin_family = sock->socketFamily;
    server.sin_addr.s_addr = inet_addr(AS_CSTRING(args[1]));
    server.sin_port = htons(AS_NUMBER(args[2]));

    if(sock->asyncSocket) {
        if(sock->asyncSocket->isServer){
            ERROR_RESULT;
        }
        uv_connect_t* connect_req = ALLOCATE(vm, uv_connect_t, 1);
        AsyncWritePayload* payload = ALLOCATE(vm, AsyncWritePayload, 1);
        connect_req->data = payload;
        payload->socket = sock;
        payload->future = newFuture(vm);
        payload->future->controlled = true;
        uv_tcp_connect(connect_req, &sock->asyncSocket->handle, (const struct sockaddr *)&server, async_socket_on_connect);
        return OBJ_VAL(payload->future);
    }

    if (connect(sock->socket, (struct sockaddr *)&server, sizeof(server)) < 0) {
        ERROR_RESULT;
    }

    return newResultSuccess(vm, NIL_VAL);
}

static Value closeSocket(DictuVM *vm, int argCount, Value *args) {
    if (argCount != 0) {
        runtimeError(vm, "close() takes no arguments (%d given)", argCount);
        return EMPTY_VAL;
    }

    SocketData *sock = AS_SOCKET(args[0]);
    close(sock->socket);

    return NIL_VAL;
}

static Value setSocketOpt(DictuVM *vm, int argCount, Value *args) {
    if (argCount != 2) {
        runtimeError(vm, "setsocketopt() takes 2 arguments (%d given)",
                     argCount);
        return EMPTY_VAL;
    }

    if (!IS_NUMBER(args[1]) || !IS_NUMBER(args[2])) {
        runtimeError(vm, "setsocketopt() arguments must be numbers");
        return EMPTY_VAL;
    }

    SocketData *sock = AS_SOCKET(args[0]);
    int level = AS_NUMBER(args[1]);
    int option = AS_NUMBER(args[2]);

    if (setsockopt(sock->socket, level, option, &(int){1}, sizeof(int)) == -1) {
        ERROR_RESULT;
    }

    return newResultSuccess(vm, NIL_VAL);
}

void freeSocket(DictuVM *vm, ObjAbstract *abstract) {
    FREE(vm, SocketData, abstract->data);
}

char *socketToString(ObjAbstract *abstract) {
    UNUSED(abstract);

    char *socketString = malloc(sizeof(char) * 9);
    snprintf(socketString, 9, "<Socket>");
    return socketString;
}

ObjAbstract *newSocket(DictuVM *vm, int sock, int socketFamily, int socketType,
                       int socketProtocol, AsyncSocket *asyncSocket) {
    ObjAbstract *abstract = newAbstract(vm, freeSocket, socketToString);
    push(vm, OBJ_VAL(abstract));

    SocketData *socket = ALLOCATE(vm, SocketData, 1);
    socket->socket = sock;
    socket->socketFamily = socketFamily;
    socket->socketType = socketType;
    socket->socketProtocol = socketProtocol;
    socket->asyncSocket = asyncSocket;
    if(asyncSocket)
         vm->asyncSockets += 1;

    abstract->data = socket;

    /**
     * Setup Socket object methods
     */
    defineNative(vm, &abstract->values, "bind", bindSocket);
    defineNative(vm, &abstract->values, "listen", listenSocket);
    defineNative(vm, &abstract->values, "accept", acceptSocket);
    defineNative(vm, &abstract->values, "write", writeSocket);
    defineNative(vm, &abstract->values, "recv", recvSocket);
    defineNative(vm, &abstract->values, "connect", connectSocket);
    defineNative(vm, &abstract->values, "close", closeSocket);
    defineNative(vm, &abstract->values, "setsockopt", setSocketOpt);
    pop(vm);

    return abstract;
}

#ifdef _WIN32
void cleanupSockets(void) {
    // Calls WSACleanup until an error occurs.
    // Avoids issues if WSAStartup is called multiple times.
    while (!WSACleanup())
        ;
}
#endif

Value createSocketModule(DictuVM *vm) {
#ifdef _WIN32
#include "windowsapi.h"

    atexit(cleanupSockets);
    WORD versionWanted = MAKEWORD(2, 2);
    WSADATA wsaData;
    WSAStartup(versionWanted, &wsaData);
#endif

    ObjString *name = copyString(vm, "Socket", 6);
    push(vm, OBJ_VAL(name));
    ObjModule *module = newModule(vm, name);
    push(vm, OBJ_VAL(module));

    /**
     * Define Socket methods
     */
    defineNative(vm, &module->values, "create", createSocket);
    defineNative(vm, &module->values, "asyncCreate", createAsyncSocket);

    /**
     * Define Socket properties
     */
    defineNativeProperty(vm, &module->values, "AF_INET", NUMBER_VAL(AF_INET));
    defineNativeProperty(vm, &module->values, "SOCK_STREAM",
                         NUMBER_VAL(SOCK_STREAM));
    defineNativeProperty(vm, &module->values, "SOCK_DGRAM",
                         NUMBER_VAL(SOCK_DGRAM));
    defineNativeProperty(vm, &module->values, "SOCK_RAW", NUMBER_VAL(SOCK_RAW));
    defineNativeProperty(vm, &module->values, "SOCK_SEQPACKET",
                         NUMBER_VAL(SOCK_SEQPACKET));
    defineNativeProperty(vm, &module->values, "SOL_SOCKET",
                         NUMBER_VAL(SOL_SOCKET));
    defineNativeProperty(vm, &module->values, "SO_REUSEADDR",
                         NUMBER_VAL(SO_REUSEADDR));
    defineNativeProperty(vm, &module->values, "SO_BROADCAST",
                         NUMBER_VAL(SO_BROADCAST));

    pop(vm);
    pop(vm);

    return OBJ_VAL(module);
}
