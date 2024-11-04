#include "uv.h"
#include <kyros.h>
#include <stdio.h>

void task(void* ctx) {
    auto value = ((uint64_t)ctx);
    // printf("Task: %llu\n", value);
    if(++value < 1'000'000)
    kyros_loop_atomic_defer(kyros_loop_default(), task, (void*)value);
}

int main() {
    kyros_init();
    
    auto start = uv_now(uv_default_loop());
    auto loop = kyros_loop_default();
    kyros_loop_defer(loop, task, (void*)1);
    kyros_loop_run_forever(loop); 
    kyros_loop_unref(loop);
    uv_update_time(uv_default_loop());
    printf("time %llu", uv_now(uv_default_loop()) - start);

}