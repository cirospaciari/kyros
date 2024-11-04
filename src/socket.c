#include <kyros.h>
#include <kyros_internal.h>


kyros_socket kyros_socket_connect(kyros_socket_source source, kryos_socket_options options, kyros_socket_handler* handler) {
    return (kyros_socket){ 0 };
}

SSL* kyros_socket_get_ssl(kyros_socket socket) {
    return NULL;
}


SSL_CTX* kyros_socket_get_ctx(kyros_socket socket) {
    return NULL;
}

void kyros_socket_pause(kyros_socket socket) {

}
void kyros_socket_resume(kyros_socket socket) {

}
bool kyros_socket_is_paused(kyros_socket socket) {
    return false;
}

uint64_t kyros_socket_flush(kyros_socket socket) {
    return 0;    
}

uint64_t kyros_socket_buffer_size(kyros_socket socket) {
    /// writable buffer size waiting to be flushed on drain event
    return 0;
}

void kyros_socket_ref(kyros_socket socket) {

}
void kyros_socket_unref(kyros_socket socket) {

}

void kyros_socket_write(kyros_socket socket, const char* buffer, uint64_t size, bool end) {

}

void kyros_socket_close(kyros_socket socket) {

}

void kyros_socket_keepalive_loop(kyros_socket socket, bool keep_alive) {

}

void kyros_socket_nodelay(kyros_socket socket, bool nodelay) {

}

void kyros_socket_keepalive(kyros_socket socket, bool nodelay) {

}
void kyros_socket_timeout(kyros_socket socket, uint32_t timeout) {

}

// kyros_socket_write2(socket, origin_socket, end); // end = true close the writable side of the socket
// kyros_socket_write3(socket, origin_duplex, end);
// kyros_socket_write4(socket, fd, end); // may optimize sendfile / SSL_sendfile (or not)

// kyros_socket_pipe(socket, dst_socket, end: bool);
// kyros_socket_pipe2(socket, dst_duplex, end: bool); //end = true close the writable side of the dst
// kyros_socket_pipe3(socket, fd, end: bool);
// kyros_socket_unpipe(socket);

//  kyros_socket_is_readable(socket) // readable side is open
//  kyros_socket_is_writable(socket) // writable side is open
//  kyros_socket_is_closed(socket)  // is closed
//  kyros_socket_is_secure(socket) // tls handshake ok
//  kyros_socket_is_open(socket) // opened/connected
//  kytos_socket_is_connecting(socket) // connecting
 // const state =  kyros_socket_get_state(socket);
 // state == .connecting
 // state == .open
 // state == .secure
 // state == .redable_ended,
 // state == .writeble_ended,
 // state == .closed
