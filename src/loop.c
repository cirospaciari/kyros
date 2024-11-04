#include <kyros.h>
#include <kyros_bitset.h>
#include <kyros_internal.h>

#include <stdatomic.h>
#include <uv.h>

#include <stdint.h>
#include <stdio.h>
#include <time.h>

static kyros_tasks_hive tasks_hive = kyros_tasks_hive_empty;

static inline kyros_task* kyros_new_task()
{
    auto index = kyros_bitset_first_set_n(TASK_HIVE_SIZE, &tasks_hive.set);
    if (index == -1) {
        return (kyros_task*)kyros_alloc(sizeof(kyros_task));
    }
    auto new_task = &tasks_hive.tasks[index];
    kyros_bitset_unset_n(TASK_HIVE_SIZE, &tasks_hive.set, index);
    return new_task;
}

static inline int32_t kyros_task_index_of(kyros_task* task)
{
    const auto start = (uintptr_t)&tasks_hive.tasks[0];
    const auto end = (uintptr_t)&tasks_hive.tasks[TASK_HIVE_SIZE - 1];
    const auto value = (uintptr_t)task;

    if ((value >= start) && (value < end)) {
        return (value - start) / sizeof(uint64_t);
    }
    return -1;
}

static inline void kyros_free_task(kyros_task* task)
{
    auto index = kyros_task_index_of(task);
    if (index == -1) {
        kyros_free(task);
    } else {
        kyros_bitset_set_n(TASK_HIVE_SIZE, &tasks_hive.set, index);
    }
}

static inline kyros_task* kyros_loop_new_task(kyros_loop* loop)
{
    auto internal = kyros_get_internal_loop(loop);
    auto* async_task_set = &internal->async_task_hive.set;
    auto index = kyros_bitset_first_set_n(ASYNC_TASK_HIVE_SIZE, async_task_set);
    if (index == -1) {
        return (kyros_task*)kyros_alloc(sizeof(kyros_task));
    }
    auto new_task = &internal->async_task_hive.tasks[index];
    kyros_bitset_unset_n(ASYNC_TASK_HIVE_SIZE, async_task_set, index);
    return new_task;
}
static inline int32_t kyros_loop_task_index_of(kyros_loop_internal* internal, kyros_task* task)
{
    const auto start = (uintptr_t)&internal->async_task_hive.tasks[0];
    const auto end = (uintptr_t)&internal->async_task_hive.tasks[ASYNC_TASK_HIVE_SIZE - 1];
    const auto value = (uintptr_t)task;

    if ((value >= start) && (value < end)) {
        return (value - start) / sizeof(uint64_t);
    }
    return -1;
}
static inline void kyros_loop_free_task(kyros_loop* loop, kyros_task* task)
{
    auto internal = kyros_get_internal_loop(loop);
    auto index = kyros_loop_task_index_of(internal, task);
    if (index == -1) {
        kyros_free(task);
    } else {
        kyros_bitset_set_n(TASK_HIVE_SIZE, &internal->async_task_hive.set, index);
    }
}
// uv loop default is just static not thread_local
// when not in main thread dont use the default loop unless you wanna call kyros_loop_async_defer
static kyros_loop* default_loop = NULL;
static void kyros_loop_deinit(kyros_loop* loop);

static void kyros_loop_drain_tasks(kyros_loop* loop)
{
    auto internal = kyros_get_internal_loop(loop);
    if (!internal)
        return;
    kyros_task* task = internal->task_queue;
    if (task) {
        internal->task_queue = NULL;
        while (task) {
            auto next_task = task->next;
            task->task(task->ctx);
            kyros_free_task(task);
            task = next_task;
        }
        kyros_loop_unref(loop);
    }
}
static void kyros_loop_drain_async_tasks(kyros_loop* loop)
{
    auto internal = kyros_get_internal_loop(loop);
    if (!internal)
        return;

    kyros_task* task;
    kyros_lock(&internal->lock, {
        task = internal->async_task_queue;
        internal->async_task_queue = NULL;
    });
    if (task) {
        while (task) {
            auto next_task = task->next;
            task->task(task->ctx);
            // we dont wanna to cause dead locks so we need to lock/unlock to free
            kyros_lock(&internal->lock, {
                kyros_loop_free_task(loop, task);
            });
            task = next_task;
        }
    }
    auto ref_count = atomic_fetch_sub(&internal->aref_count, 1);
    m_assert(ref_count, "kyros_loop atomic double free detected");
    if (ref_count == 1) {
        // check for async loop cleanup
        if (internal->ref_count == 0) {
            kyros_loop_deinit(loop);
        }
    }
}

static void kyros_async_wakeup_callback(uv_async_t* p)
{
    uv_unref((uv_handle_t*)p);
    kyros_loop* loop = p->data;
    if (loop) {
        kyros_loop_drain_async_tasks(loop);
    }
}
static void kyros_async_callback(uv_async_t* p)
{
    uv_unref((uv_handle_t*)p);
    kyros_loop* loop = p->data;
    if (loop) {
        kyros_loop_drain_tasks(loop);
    }
}
static void kyros_before_callback(uv_prepare_t* p)
{
    // before IO
    kyros_loop* loop = p->data;
    if (loop) {
    }
}

static void kyros_after_callback(uv_check_t* p)
{
    // after IO
    kyros_loop* loop = p->data;
    if (loop) {
    }
}

kyros_loop* kyros_loop_default()
{
    if (!default_loop) {
        default_loop = kyros_loop_create(uv_default_loop());
    }
    return default_loop;
}

kyros_loop* kyros_loop_create(void* existing_loop)
{
    uv_loop_t* loop = existing_loop;
    if (!loop) {
        loop = kyros_alloc(sizeof(uv_loop_t));
        uv_loop_init(loop);
    }
    loop->data = kyros_alloc(sizeof(kyros_loop_internal));
    auto internal = (kyros_loop_internal*)loop->data;
    internal->ref_count = 1;
    atomic_init(&internal->aref_count, 1);
    internal->lock = (atomic_flag)ATOMIC_FLAG_INIT;
    internal->async_task_queue = NULL;
    internal->task_queue = NULL;
    uv_async_init(loop, &internal->task_queue_signal, kyros_async_callback);
    uv_async_init(loop, &internal->async_signal, kyros_async_wakeup_callback);
    uv_unref((uv_handle_t*)&internal->async_signal);
    uv_unref((uv_handle_t*)&internal->task_queue_signal);

    internal->task_queue_signal.data = loop;
    internal->async_signal.data = loop;
    internal->async_task_hive = (kyros_tasks_async_hive) { .set = kyros_bitset_full_n(ASYNC_TASK_HIVE_SIZE) };

    uv_prepare_init(loop, &internal->uv_prepare);
    uv_prepare_start(&internal->uv_prepare, kyros_before_callback);
    uv_unref((uv_handle_t*)&internal->uv_prepare);
    internal->uv_prepare.data = loop;

    uv_check_init(loop, &internal->uv_check);
    uv_unref((uv_handle_t*)&internal->uv_check);
    uv_check_start(&internal->uv_check, kyros_after_callback);
    internal->uv_check.data = loop;

    return (kyros_loop*)loop;
}

uint32_t kyros_loop_run_once(kyros_loop* loop)
{
    return uv_run((uv_loop_t*)loop, UV_RUN_NOWAIT);
}

uint32_t kyros_loop_run_forever(kyros_loop* loop)
{
    return uv_run((uv_loop_t*)loop, UV_RUN_DEFAULT);
}

uint64_t kyros_loop_ref(kyros_loop* loop)
{
    auto internal = kyros_get_internal_loop(loop);
    internal->ref_count++;
    return internal->ref_count;
}

void kyros_loop_atomic_ref(kyros_loop* loop)
{
    auto internal = kyros_get_internal_loop(loop);
    atomic_fetch_add(&internal->aref_count, 1);
}

static void kyros_loop_deinit(kyros_loop* loop)
{
    auto internal = kyros_get_internal_loop(loop);
    // drain all tasks
    kyros_loop_drain_tasks(loop);
    // drain all async tasks
    kyros_loop_drain_tasks(loop);

    // cancel all new pending tasks
    kyros_task* task;
    while ((task = internal->task_queue)) {
        kyros_free_task(task);
    }
    internal->task_queue = NULL;
    while ((task = internal->async_task_queue)) {
        kyros_loop_free_task(loop, task);
    }
    internal->async_task_queue = NULL;

    // stop check and prepare
    uv_check_stop(&internal->uv_check);
    uv_prepare_stop(&internal->uv_prepare);
    internal->uv_check.data = NULL;
    internal->uv_prepare.data = NULL;
    internal->task_queue_signal.data = NULL;
    internal->async_signal.data = NULL;
    uv_close((uv_handle_t*)&internal->uv_check, NULL);
    uv_close((uv_handle_t*)&internal->uv_prepare, NULL);
    uv_close((uv_handle_t*)&internal->task_queue_signal, NULL);
    uv_close((uv_handle_t*)&internal->async_signal, NULL);
    // free loop
    kyros_free(internal);
    // invalidate loop data
    ((uv_loop_t*)loop)->data = NULL;
    // close loop
    uv_loop_close((uv_loop_t*)loop);

    if (default_loop == loop) {
        default_loop = NULL;
    } else {
        kyros_free((uv_loop_t*)loop);
    }
}

bool kyros_loop_atomic_unref(kyros_loop* loop)
{
    auto internal = kyros_get_internal_loop(loop);
    auto ref_count = atomic_fetch_sub(&internal->aref_count, 1);
    m_assert(ref_count, "kyros_loop atomic double free detected");
    if (ref_count == 1) {
        // we need to check in the main thread the normal ref_count
        // yeah is a little expensive unref outside the main thread avoid this
        kyros_lock(&internal->lock, {
            if (!internal->async_task_queue) {
                // need to ref again just to wakeup but only necessary if we dont have an async task already in place
                kyros_loop_atomic_ref(loop);
                uv_ref((uv_handle_t*)&internal->async_signal);
                uv_async_send(&internal->async_signal);
            }
        });
        return true;
    }
    return false;
}

uint64_t kyros_loop_unref(kyros_loop* loop)
{
    auto internal = kyros_get_internal_loop(loop);
    m_assert(internal->ref_count, "kyros_loop double free detected");
    if (--internal->ref_count == 0) {
        if (atomic_load(&internal->aref_count) == 0) {
            kyros_loop_deinit(loop);
        }
        return 0;
    }
    return internal->ref_count;
}

void kyros_loop_stop(kyros_loop* loop)
{
    uv_stop((uv_loop_t*)loop);
}

void kyros_loop_atomic_defer(kyros_loop* loop, void (*task)(void* ctx), void* ctx)
{
    auto internal = kyros_get_internal_loop(loop);

    kyros_lock(&internal->lock, {
        auto new_task = kyros_loop_new_task(loop);
        new_task->ctx = ctx;
        new_task->task = task;
        auto current_queue = internal->async_task_queue;
        new_task->next = current_queue;
        // ref the loop if we dont have any task enqueued yet
        internal->async_task_queue = new_task;
        if (!current_queue) {
            kyros_loop_atomic_ref(loop);
            uv_ref((uv_handle_t*)&internal->async_signal);
            uv_async_send(&internal->async_signal);
        }
    });
}

void kyros_loop_defer(kyros_loop* loop, void (*task)(void* ctx), void* ctx)
{
    auto internal = kyros_get_internal_loop(loop);
    auto new_task = kyros_new_task();
    new_task->ctx = ctx;
    new_task->task = task;
    auto current_queue = internal->task_queue;
    new_task->next = current_queue;
    // ref the loop if we dont have any task enqueued yet
    internal->task_queue = new_task;
    if (!current_queue) {
        // keep loop alive until we drain tasks
        kyros_loop_ref(loop);
        uv_ref((uv_handle_t*)&internal->task_queue_signal);
        uv_async_send(&internal->task_queue_signal);
    }
}

void kyros_internal_timer_callback(uv_timer_t* timer)
{
    auto internal = kyros_get_internal_timer((kyros_timer*)timer);
    internal->task(internal->ctx);
}

kyros_timer* kyros_loop_timer(kyros_loop* loop, void (*task)(void* ctx), void* ctx, uint64_t timeout, uint64_t repeat, bool keep_alive)
{
    auto internal = kyros_get_internal_loop(loop);
    auto timer = (uv_timer_t*)kyros_alloc(sizeof(uv_timer_t));
    timer->data = loop;
    auto ktimer = (kyros_timer*)timer;
    auto itimer = kyros_get_internal_timer(ktimer);
    itimer->ref_count = 1;
    itimer->task = task;
    itimer->ctx = ctx;
    uv_timer_init((uv_loop_t*)loop, timer);
    uv_timer_start(timer, kyros_internal_timer_callback, timeout, repeat);
    if (!keep_alive) {
        uv_unref((uv_handle_t*)timer);
    }
    return ktimer;
}

void kyros_timer_set_callback(kyros_timer* timer, void (*task)(void* ctx), void* ctx)
{
    auto internal = kyros_get_internal_timer((kyros_timer*)timer);
    internal->task = task;
    internal->ctx = ctx;
}

void kyros_timer_set_times(kyros_timer* timer, uint64_t timeout, uint64_t repeat)
{
    uv_timer_stop((uv_timer_t*)timer);
    uv_timer_start((uv_timer_t*)timer, kyros_internal_timer_callback, timeout, repeat);
}

void kyros_timer_stop(kyros_timer* timer)
{
    uv_timer_stop((uv_timer_t*)timer);
}

uint64_t kyros_timer_ref(kyros_timer* timer)
{
    auto internal = kyros_get_internal_timer((kyros_timer*)timer);
    internal->ref_count++;
    return internal->ref_count;
}

uint64_t kyros_timer_unref(kyros_timer* timer)
{
    auto internal = kyros_get_internal_timer((kyros_timer*)timer);
    m_assert(internal->ref_count, "kyros_timer double free detected");
    if (--internal->ref_count == 0) {
        kyros_free(timer);
        return 0;
    }
    return internal->ref_count;
}

void kyros_timer_keepalive_loop(kyros_timer* timer, bool keep_alive)
{
    if (keep_alive) {
        uv_ref((uv_handle_t*)timer);
    } else {
        uv_unref((uv_handle_t*)timer);
    }
}