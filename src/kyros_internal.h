#ifndef KYROS_INTERNAL_H
#define KYROS_INTERNAL_H
#include <assert.h>
#include <kyros.h>
#include <kyros_bitset.h>
#include <mimalloc.h>
#include <openssl/ssl.h>
#include <stdatomic.h>
#include <uv.h>

#define kyros_alloc mi_malloc
#define kyros_free mi_free
#define kyros_resize mi_realloc

#define TASK_HIVE_SIZE 64 // used on main thread only to defer tasks (global)
#define ASYNC_TASK_HIVE_SIZE 64 // used to signal tasks from another thread to main thread (per loop)

#define CONCAT_INNER(a, b) a##b
#define CONCAT(a, b) CONCAT_INNER(a, b)

/// @brief create a unique name using base + line number
#define UNIQUE_TMP_NAME(base) CONCAT(base, __LINE__)

/// @brief assert with message
#ifndef NDEBUG
#define m_assert(expr, msg)                                                        \
    ({                                                                             \
        if (!(expr)) {                                                             \
            fprintf(stderr, "assertion failed: %s at %s - %s:%d\n", msg, __FILE__, \
                __PRETTY_FUNCTION__, __LINE__);                                    \
            abort();                                                               \
        }                                                                          \
    })
#else
#define m_assert(expr, msg) ((void)0)
#endif

/// @brief spins until lock is acquired
static atomic_flag* kyros_spin_lock(atomic_flag* lock)
{
    while (atomic_flag_test_and_set_explicit(lock, memory_order_acquire)) {
        ;
    }
    return lock;
}
/// @brief release lock
static void kyros_spin_unlock(atomic_flag** ref)
{
    atomic_flag* lock = *ref;
    atomic_flag_clear_explicit(lock, memory_order_release);
}
/// @brief defer unlock to the end of the context
#define unlock __attribute__((cleanup(kyros_spin_unlock)))

#define kyros_lock(flag, block)                                                \
    ({                                                                         \
        unlock atomic_flag* UNIQUE_TMP_NAME(tmp_lock) = kyros_spin_lock(flag); \
        block;                                                                 \
    })

// 24 bytes struct
typedef struct kyros_task {
    void (*task)(void* ctx);
    void* ctx;
    struct kyros_task* next;
} kyros_task;

declare_kyros_bitset_n(TASK_HIVE_SIZE);
typedef struct {
    kyros_bitset_n(TASK_HIVE_SIZE) set;
    kyros_task tasks[TASK_HIVE_SIZE];
} kyros_tasks_hive;
// hives can have diferent sizes but for now both are only 64 in size until
// further analysis
typedef struct {
    kyros_bitset_n(ASYNC_TASK_HIVE_SIZE) set;
    kyros_task tasks[ASYNC_TASK_HIVE_SIZE];
} kyros_tasks_async_hive;

#define kyros_tasks_hive_empty \
    (kyros_tasks_hive) { .set = kyros_bitset_full_n(TASK_HIVE_SIZE) }

// the loop is not small in size but normally we have 1 loop per thread so its fine
typedef struct {
    uint64_t ref_count;
    // loop can be held alive in another thread
    _Atomic(uint64_t) aref_count;

    // used to before/after IO processing
    uv_check_t uv_check;
    uv_prepare_t uv_prepare;

    // used to defer tasks/callbacks to next tick
    kyros_task* task_queue;
    uv_async_t task_queue_signal;

    // used to wakeup and thread comunication
    atomic_flag lock;
    uv_async_t async_signal;
    kyros_task* async_task_queue;
    kyros_tasks_async_hive async_task_hive;
} kyros_loop_internal;

// we have exacly 4 ptr wide here to be used inside uv_handler_t reserved size
typedef struct {
    uint64_t ref_count;
    void (*task)(void* ctx);
    void* ctx;
    void* _reserved;
} kyros_timer_internal;

static inline kyros_loop_internal* kyros_get_internal_loop(kyros_loop* loop)
{
    return (kyros_loop_internal*)(((uv_loop_t*)loop)->data);
}
static inline kyros_timer_internal* kyros_get_internal_timer(kyros_timer* timer)
{
    return (kyros_timer_internal*)(&((uv_timer_t*)timer)->u.reserved[0]);
}

typedef enum {
    KYROS_SOCKET_STATE_CONNECTING = 0,
    KYROS_SOCKET_STATE_OPEN = 1,
    KYROS_SOCKET_STATE_SECURE = 2, // handshake ok (only available when over TLS)
    KYROS_SOCKET_STATE_READABLE_ENDED = 3,
    KYROS_SOCKET_STATE_WRITABLE_ENDED = 4,
    KYROS_SOCKET_STATE_CLOSED = 5,
} KYROS_SOCKET_STATUS;

// we will not have a single socket type but one for each tag this is only the common part
typedef struct {
    uint32_t ref_count; // u32 should be fine right?
    KYROS_SOCKET_STATUS status : 5; // up to 32 status, should be fine
    bool allow_half_open : 1; // really needed and useful, if false it will close the socket if the readable side closes
    bool paused : 1;
    bool client : 1;
    // usockets uses the uv_poll_t ptr + fd + poll_type
    // our solution tags the ptr instead of poll_type
    // and uses ref_count + flags with should be basically fd + poll_type in size
    // uv_poll_t data is the loop
} kyros_socket_internal;

typedef struct {
    uv_poll_t poll; // only pollable sockets have this because is 160 bytes
} kyros_socket_internal_poll;

typedef struct {
    uv_pipe_t pipe; // only pipe sockets have this because is 264 bytes
} kyros_socket_internal_pipe;

typedef struct {
    kyros_socket_internal socket;
    kyros_socket_internal_poll poll;
    kyros_socket_handler* handlers;
} kyros_socket_internal_tcp;

typedef struct {
    kyros_socket_internal_tcp tcp;
    SSL* ssl;
    SSL_CTX* ssl_ctx;
} kyros_socket_internal_tls;

typedef union {
    struct {
        uint8_t tag : 5; // up to 32 types but we can increase if needed up to 16bits
        uint64_t value : 59; // this can be as low as 48 bits if needed
    } v;
    kyros_socket ptr;
    uint64_t tagged_value;
} kyros_tagged_socket;

typedef enum {
    KYROS_SOCKET_TCP = 0,
    KYROS_SOCKET_UDP = 1,
    KYROS_SOCKET_TLS = 2,
    KYROS_SOCKET_QUIC = 3,
    KYROS_DUPLEX_INTERFACE = 4, // Not a socket but a generic interface that mimics Duplex stream
    KYROS_SOCKET_NAMED_PIPE = 5, // Windows name pipe
    KYROS_SOCKET_UPGRADED_DUPLEX = 6,
    KYROS_SOCKET_UPGRADED_TCP = 7, // TLS over another TCP source
    KYROS_SOCKET_UPGRADED_UDP = 8, // TLS over UDP
    KYROS_SOCKET_UPGRADED_NAMED_PIPE = 9, // TLS over Windows named pipe
    KYROS_SOCKET_TCP_LISTENER = 10,
    KYROS_SOCKET_TLS_LISTENER = 11,
    KYROS_SOCKET_UDP_LISTENER = 12,
    KYROS_SOCKET_UDP_TLS_LISTENER = 13,
    KYROS_SOCKET_QUIC_LISTENER = 14,
    KYROS_SOCKET_NAMED_PIPE_LISTENER = 15,
    KYROS_SOCKET_NAMED_PIPE_TLS_LISTENER = 16,
} kyros_socket_internal_tag;

static inline kyros_socket_internal_tag kyros_get_socket_internal_tag(kyros_socket socket)
{
    return ((kyros_tagged_socket) { .ptr = socket }).v.tag;
}

static inline kyros_socket_internal* kyros_get_socket_internal(kyros_socket socket)
{
    return (kyros_socket_internal*)((kyros_tagged_socket) { .ptr = socket }).v.value;
}

#endif