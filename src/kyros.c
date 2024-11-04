
#include <kyros.h>
#include <kyros_internal.h>
#include <uv.h>
#include <openssl/ssl.h>
static inline void uv_init() {
  #ifdef KYROS_OVERRIDE_LIBUV_ALLOCATOR
  auto rc = uv_replace_allocator(kyros_alloc, kyros_resize, kyros_calloc,
                                kyros_free);
  if (rc != 0) {
    panic_fmt("uv_replace_allocator failed: %d", rc);
  }
  #endif
}

#ifdef KYROS_OVERRIDE_BORINGSSL_ALLOCATOR 

export void* OpenSSL_malloc(usize_t size) { 
    return kyros_alloc(size); 
}

export void OpenSSL_free(void* ptr) {
  // BoringSSL's free function should zero out memory before freeing it
  let len = kyros_usable_size(ptr);
  memset(ptr, 0, len);
  kyros_free(ptr);
}

export usize OpenSSL_get_size(void* ptr) { 
    return kyros_usable_size(ptr); 
}

#endif

static void kyros_do_not_optimize_away(void* p) {
    __asm__ volatile("" : : "g"(p) : "memory");
}

void kyros_init() {
    uv_init();
    CRYPTO_library_init();
    assert(boring.SSL_library_init() > 0);
    SSL_load_error_strings();
    ERR_load_BIO_strings();
    OpenSSL_add_all_algorithms();

    #ifdef KYROS_OVERRIDE_BORINGSSL_ALLOCATOR
    kyros_do_not_optimize_away(&OpenSSL_malloc);
    kyros_do_not_optimize_away(&OpenSSL_free);
    kyros_do_not_optimize_away(&OpenSSL_get_size);
    #endif
}