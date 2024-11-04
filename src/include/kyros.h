

#ifndef KYROS_H
#define KYROS_H
#include <stdbool.h>
#include <stdint.h>
#include <openssl/ssl.h>
#ifdef _WIN32
#define export __declspec(dllexport)
#else
#define export
#endif
/// @brief initialize kyros library, and its dependences like BoringSSL and libuv
export void kyros_init();

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

typedef enum {
  KYROS_SOCKET_DISABLE_CORK = 0,
  KYROS_SOCKET_MANUAL_CORK = 1,
  KYROS_SOCKET_AUTO_CORK = 2,
} kyros_socket_cork_behavior;

typedef struct {
    /// @brief error type 0 = no error, 1 = connecting error, 2 = tls error, 3 = exit code
    kyros_socket_error_type type;
    /// @brief integer code that represents the connection error, tls error or exit code
    uint32_t code;
    /// @brief null-terminated text code that represents the tls error, NULL if not available
    const char* code_s;
    /// @brief null-terminated error message with extra information about the error, NULL if not available 
    const char* message;
} kyros_socket_error;


typedef enum {
 KYROS_SOCKET_SOURCE_HOSTPORT = 0,
 KYROS_SOCKET_SOURCE_UNIXSOCKET = 1,
 KYROS_SOCKET_SOURCE_NAMEDPIPE = 2,
 KYROS_SOCKET_SOURCE_FD = 3,
 KYROS_SOCKET_SOURCE_SOCKET = 4,
 KYROS_SOCKET_SOURCE_DUPLEX_INTERFACE = 5,
} kyros_socket_source_type;

typedef enum {
    /// @brief use TCP communication
    KYROS_SOCKET_FD_TYPE_TCP = 0,
    /// @brief use UDP/DGRAM communication
    KYROS_SOCKET_FD_TYPE_UDP = 1,
    /// @brief use UNIXSOCKET communication
    KYROS_SOCKET_FD_TYPE_UNIXSOCKET = 2,
    /// @brief use a namedpipe as a socket source
    KYROS_SOCKET_FD_TYPE_NAMEDPIPE = 3,
    /// @brief use a file fd as socket source
    KYROS_SOCKET_FD_TYPE_FILE_FD = 4,
    /// @brief try to auto detect the fd type of the source
    KYROS_SOCKET_FD_TYPE_UNKNOWN = 5,
} kyros_socket_fd_type;

typedef enum {
    KYROS_SOCKET_IP_FAMILY_ANY = 0,
    KYROS_SOCKET_IP_FAMILY_IPV4 = 1,
    KYROS_SOCKET_IP_FAMILY_IPV6 = 2,
} kyros_socket_ip_family;

typedef struct {
    kyros_socket_source_type type;
    union {
        /// @brief available when the type is KYROS_SOCKET_SOURCE_UNIXSOCKET or KYROS_SOCKET_SOURCE_NAMEDPIPE 
        const char* path;
        /// @brief available when the type is KYROS_SOCKET_SOURCE_HOSTPORT
        struct {
            const char* host;
            uint16_t port;
            /// @brief Version of IP stack. The value 0 indicates that both IPv4 and IPv6 addresses are allowed.
            kyros_socket_ip_family family: 2;
            /// @brief enables SO_REUSEPORT
            bool reuse_port : 1;
            /// @brief if true connect using UDP instead of TCP
            bool use_udp: 1;
            /// @brief uses Happy Eyeballs algorithm to connect
            bool use_happy_eyeballs: 1;
        } host_port;
        struct {
            /// @brief available when the type is KYROS_SOCKET_SOURCE_FD
            uint64_t fd;
            /// @brief tells what type of FD we are trying to connect
            kyros_socket_fd_type type;
        } fd;
        /// @brief available when the type is KYROS_SOCKET_SOURCE_SOCKET, this is used to wrap any kyros_socket to TLS
        kyros_socket socket;
        /// @brief available when the type is KYROS_SOCKET_SOURCE_DUPLEX_INTERFACE, can be used to wrap TLS or use a arbitrary source of data to a socket
        // kyros_duplex_interface duplex;
    } value;
} kyros_socket_source;

typedef struct {
  /// @brief 0 = disable corking, 1 = enable manual corking, 2 = automatic cork/uncork
  kyros_socket_cork_behavior cork_behavior : 2; 
  /// @brief if true, automatic buffer and flush
  bool enable_write_buffer: 1;
  /// @brief if false, close the socket when readable side closes
  bool allow_half_open : 1;
  /// @brief pause readable side of the socket
  bool start_paused : 1;
  /// @brief passing true for noDelay or not passing an argument will disable Nagle's algorithm for the socket
  bool no_delay: 1;
  /// @brief enables keep-alive functionality on the socket
  bool keep_alive: 1;  
  /// @brief if set to a positive number, it sets the initial delay before the first keepalive probe is sent on an idle socket
  uint32_t keep_alive_initial_delay; 
  /// @brief sets the socket to timeout after timeout milliseconds of inactivity on the socket. 0 to disable it
  uint32_t timeout; 
  /// @brief enables TLS
  SSL_CTX* tls;
} kryos_socket_options;

typedef struct {
    /// @brief optional custom context that will be passed in the ondata, ontimeout, ondrain and onstatus callbacks
    void* ctx;
    /// @brief return true to keep reading, return false to close the socket (default is true if ondata is NULL and data will be discarted unless it is paused)
    bool (*ondata)(kyros_socket socket, void* ctx);
    /// @brief return false to keep socket alive, return true to close after timeout (default is true if ontimeout is NULL)
    bool (*ontimeout)(kyros_socket socket, void* ctx);
    /// @brief called when all the write data is flushed and the socket is writable again
    void (*ondrain)(kyros_socket socket, void* ctx);
    /// @brief track status changes, open, half-closed, closed, error
    void (*onstatus)(kyros_socket socket, kyros_socket_error error, void* ctx);
    /// @brief ref_count this handler can be shared with multiple sockets, this is used manage memory
    uint64_t ref_count;
} kyros_socket_handler;

/// @brief connect socket to a source, following specified options
kyros_socket kyros_socket_connect(kyros_socket_source source, kryos_socket_options options, kyros_socket_handler* handler);
#endif