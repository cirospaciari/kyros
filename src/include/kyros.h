

#ifndef KYROS_H
#define KYROS_H
#include <stdbool.h>
#include <stdint.h>
#ifdef _WIN32
#define export __declspec(dllexport)
#else
#define export
#endif

typedef struct kyros_loop kyros_loop;
// kyros_socket is a tagged ptr but we keep it as uint64_t so the user dont try
// to tag the ptr again
typedef struct {
    uint64_t tagged_ptr;
} kyros_socket;
typedef struct kyros_timer kyros_timer;

export kyros_loop* kyros_loop_create(void* loop);
export kyros_loop* kyros_loop_default();
export uint32_t kyros_loop_run_once(kyros_loop* loop);
export uint32_t kyros_loop_run_forever(kyros_loop* loop);
export uint64_t kyros_loop_ref(kyros_loop* loop);
export void kyros_loop_atomic_ref(kyros_loop* loop);
export uint64_t kyros_loop_unref(kyros_loop* loop);
export bool kyros_loop_atomic_unref(kyros_loop* loop);
export void kyros_loop_stop(kyros_loop* loop);
export void kyros_loop_defer(kyros_loop* loop, void (*task)(void* ctx), void* ctx);
export void kyros_loop_atomic_defer(kyros_loop* loop, void (*task)(void* ctx),
    void* ctx);

///
/// Timer
///
export kyros_timer* kyros_loop_timer(kyros_loop* loop, void (*task)(void* ctx),
    void* ctx, uint64_t timeout, uint64_t repeat,
    bool keep_alive);
export void kyros_timer_set_callback(kyros_timer* timer, void (*task)(void* ctx),
    void* ctx);
export void kyros_timer_set_times(kyros_timer* timer, uint64_t timeout,
    uint64_t repeat);
export void kyros_timer_stop(kyros_timer* timer);
export uint64_t kyros_timer_ref(kyros_timer* timer);
export uint64_t kyros_timer_unref(kyros_timer* timer);
export void kyros_timer_keepalive_loop(kyros_timer* timer, bool keep_alive);

///
/// SOCKET
///

typedef enum {
    KYROS_SOCKET_ERROR_NO_ERROR = 0,
    KYROS_SOCKET_ERROR_CONNECTING_ERROR = 1,
    KYROS_SOCKET_ERROR_TLS_ERROR = 2,
    KYROS_SOCKET_ERROR_EXIT_CODE = 3,
} kyros_socket_error_type;

typedef struct {
    kyros_socket_error_type type;
    uint32_t code;
    const char* code_s;
    const char* message;
} kyros_socket_error;

typedef struct {
    // custom context
    void* ctx;
    // return true to keep reading, return false to close the socket (default is true if ondata is NULL and data will be discarted unless it is paused)
    bool (*ondata)(kyros_socket socket, void* ctx);
    // return false to keep socket alive, return true to close after timeout (default is true if ontimeout is NULL)
    bool (*ontimeout)(kyros_socket socket, void* ctx);
    void (*ondrain)(kyros_socket socket, void* ctx);
    // track status changes, open, half-closed, closed, error
    void (*onstatus)(kyros_socket socket, kyros_socket_error error, void* ctx);
    // ref_count this handler can be shared with multiple sockets
    uint64_t ref_count;
} kyros_socket_handler;
#endif