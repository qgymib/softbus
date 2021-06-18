#include "softbus.h"
#include "call_in_stack.h"
#include "common.h"

#if defined(__linux__)
#   include <sys/auxv.h>
#endif

typedef struct bus_ctx
{
    bus_config_t        config;
    ev_map_t            service_table;
    ev_map_t            filber_table;

    struct
    {
        unsigned        init : 1;
    }mask;

    bus_os_tls          tls_group;
} bus_ctx_t;

static int _bus_cmp_service(const ev_map_node_t* key1, const ev_map_node_t* key2, void* arg)
{
    (void)arg;
    bus_service_t* s1 = container_of(key1, bus_service_t, node.global);
    bus_service_t* s2 = container_of(key2, bus_service_t, node.global);
    if (s1->sid == s2->sid)
    {
        return 0;
    }
    return s1->sid < s2->sid ? -1 : 1;
}

static int _bus_cmp_filber(const ev_map_node_t* key1, const ev_map_node_t* key2, void* arg)
{
    (void)arg;
    bus_filber_t* f1 = container_of(key1, bus_filber_t, node.global);
    bus_filber_t* f2 = container_of(key2, bus_filber_t, node.global);
    if (f1->fid == f2->fid)
    {
        return 0;
    }
    return f1->fid < f2->fid ? -1 : 1;
}

static bus_ctx_t g_bus_ctx = {
    { NULL, 0, NULL, 0, },
    EV_MAP_INIT(_bus_cmp_service, NULL),
    EV_MAP_INIT(_bus_cmp_filber, NULL),
    { 0 },
    { 0 },
};
size_t bus__hwcap = 0;

static int _bus_static_check(const bus_static_check_t* user)
{
    static bus_static_check_t stand = BUS_STATIC_CHECK_INIT;

    if (user->sz_self != stand.sz_self)
    {
        return -1;
    }
    if (user->sz_bus_service != stand.sz_bus_service)
    {
        return -1;
    }
    if (user->sz_bus_filber != stand.sz_bus_filber)
    {
        return -1;
    }
    if (user->sz_bus_group != stand.sz_bus_group)
    {
        return -1;
    }

    return 0;
}

static void _bus_init_hwcap(void)
{
#if defined(__linux__)
    bus__hwcap = getauxval(AT_HWCAP);
#endif
}

static int _bus_setup_serivce(bus_service_t* service)
{
    size_t i;
    for (i = 0; i < ARRAY_SIZE(service->msg); i++)
    {
        BUS_OS_MUTEX_INIT(&service->msg[i].mutex);
    }

    if (ev_map_insert(&g_bus_ctx.service_table, &service->node.global) < 0)
    {
        goto err;
    }

    return 0;

err:
    for (i = 0; i < ARRAY_SIZE(service->msg); i++)
    {
        BUS_OS_MUTEX_EXIT(&service->msg[i].mutex);
    }

    return -1;
}

static void _bus_cleanup_service(bus_service_t* service)
{
    size_t i;
    for (i = 0; i < ARRAY_SIZE(service->msg); i++)
    {
        BUS_OS_MUTEX_EXIT(&service->msg[i].mutex);
    }
}

static int _bus_setup_service_in_group(bus_group_t* group)
{
    size_t i, idx;
    for (idx = 0; idx < group->service.size; idx++)
    {
        if (_bus_setup_serivce(&group->service.table[idx]) < 0)
        {
            goto err;
        }
    }
    return 0;

err:
    for (i = 0; i < idx; i++)
    {
        _bus_cleanup_service(&group->service.table[i]);
    }
    return -1;
}

static void _bus_cleanup_service_in_group(bus_group_t* group)
{
    size_t i;
    for (i = 0; i < group->service.size; i++)
    {
        _bus_cleanup_service(&group->service.table[i]);
    }
}

static int _bus_setup_group(bus_group_t* group)
{
    ev_loop_init(&group->loop);
    BUS_OS_SEM_INIT(&group->sem[0]);
    BUS_OS_SEM_INIT(&group->sem[1]);
    group->mask.looping = 1;
    BUS_OS_MUTEX_INIT(&group->sche_s.mutex);
    BUS_OS_MUTEX_INIT(&group->sche_f.mutex);

    if (_bus_setup_service_in_group(group) < 0)
    {
        goto err;
    }

    return 0;

err:
    ev_loop_exit(&group->loop);
    BUS_OS_SEM_EXIT(&group->sem[0]);
    BUS_OS_SEM_EXIT(&group->sem[1]);
    group->mask.looping = 0;
    BUS_OS_MUTEX_EXIT(&group->sche_s.mutex);
    BUS_OS_MUTEX_EXIT(&group->sche_f.mutex);
    return -1;
}

static void _bus_cleanup_group(bus_group_t* group)
{
    _bus_cleanup_service_in_group(group);
    ev_loop_exit(&group->loop);
    BUS_OS_SEM_EXIT(&group->sem[0]);
    BUS_OS_SEM_EXIT(&group->sem[1]);
    BUS_OS_MUTEX_EXIT(&group->sche_s.mutex);
    BUS_OS_MUTEX_EXIT(&group->sche_f.mutex);
}

static int _bus_setup_groups(void)
{
    size_t i, idx;
    for (idx = 0; idx < g_bus_ctx.config.group_size; idx++)
    {
        if (_bus_setup_group(&g_bus_ctx.config.groups[idx]) < 0)
        {
            goto err;
        }
    }

    return 0;

err:
    for (i = 0; i < idx; i++)
    {
        _bus_cleanup_group(&g_bus_ctx.config.groups[i]);
    }
    return -1;
}

static void _bus_cleanup_groups(void)
{
    size_t i;
    for (i = 0; i < g_bus_ctx.config.group_size; i++)
    {
        _bus_cleanup_group(&g_bus_ctx.config.groups[i]);
    }
}

static void _bus_init_service_in_group(bus_group_t* group)
{
    size_t i;
    for (i = 0; i < group->service.size; i++)
    {
        bus_service_t* s = &group->service.table[i];

        group->sche_s.current = s;
        s->entry->on_init();
        group->sche_s.current = NULL;
    }
}

static void _bus_init_filber_in_group(bus_group_t* group)
{
    size_t i;
    for (i = 0; i < group->filber.size; i++)
    {
        bus_filber_t* f = &group->filber.table[i];

        group->sche_f.current = f;
        f->entry->on_init();
        group->sche_f.current = NULL;
    }
}

static void _bus_exit_filber_in_group(bus_group_t* group)
{
    size_t i;
    for (i = 0; i < group->filber.size; i++)
    {
        bus_filber_t* f = &group->filber.table[i];

        group->sche_f.current = f;
        f->entry->on_exit();
        group->sche_f.current = NULL;
    }
}

static void _bus_exit_service_in_group(bus_group_t* group)
{
    size_t i;
    for (i = 0; i < group->service.size; i++)
    {
        bus_service_t* s = &group->service.table[i];

        group->sche_s.current = s;
        s->entry->on_exit();
        group->sche_s.current = NULL;
    }
}

static void* _bus_thread(void* arg)
{
    bus_group_t* group = arg;

    BUS_OS_TLS_SET(&g_bus_ctx.tls_group, group);
    BUS_OS_SEM_POST(&group->sem[0]);
    BUS_OS_SEM_PEND(&group->sem[1]);

    _bus_init_service_in_group(group);
    _bus_init_filber_in_group(group);

    setjmp(group->jmpbuf);
    while (group->mask.looping)
    {
        ev_loop_run(&group->loop, ev_loop_mode_default);
    }

    _bus_exit_filber_in_group(group);
    _bus_exit_service_in_group(group);

    return NULL;
}

static int _bus_create_threads(size_t* idx)
{
    for (*idx = 0; *idx < g_bus_ctx.config.group_size; *idx += 1)
    {
        if (pthread_create(&g_bus_ctx.config.groups[*idx].thread.r, NULL,
            _bus_thread, &g_bus_ctx.config.groups[*idx]) < 0)
        {
            return -1;
        }
    }

    return 0;
}

static void _bus_destroy_threads(const size_t idx)
{
    size_t i;
    for (i = 0; i < idx; i++)
    {
        g_bus_ctx.config.groups[i].mask.looping = 0;
        BUS_OS_SEM_POST(&g_bus_ctx.config.groups[i].sem[1]);
        pthread_join(g_bus_ctx.config.groups[i].thread.r, NULL);
    }
}

static void _bus_wait_all_thread_ready(void)
{
    size_t i;
    for (i = 0; i < g_bus_ctx.config.group_size; i++)
    {
        BUS_OS_SEM_PEND(&g_bus_ctx.config.groups[i].sem[0]);
    }
}

int bus_init(const bus_config_t* config, const bus_static_check_t* check)
{
    int ret;
    if ((ret = _bus_static_check(check)) != 0)
    {
        return ret;
    }

    _bus_init_hwcap();

    BUS_OS_TLS_INIT(&g_bus_ctx.tls_group);
    g_bus_ctx.config = *config;
    g_bus_ctx.mask.init = 1;

    if ((ret = _bus_setup_groups()) < 0)
    {
        goto err_setup_groups;
    }

    size_t idx = 0;
    if ((ret = _bus_create_threads(&idx)) < 0)
    {
        goto err_create_thread;
    }

    _bus_wait_all_thread_ready();

    return 0;

err_create_thread:
    _bus_destroy_threads(idx);
err_setup_groups:
    _bus_cleanup_groups();
    return ret;
}

int bus_load(void)
{
    size_t i;
    for (i = 0; i < g_bus_ctx.config.group_size; i++)
    {
        BUS_OS_SEM_POST(&g_bus_ctx.config.groups[i].sem[1]);
    }

    return 0;
}
