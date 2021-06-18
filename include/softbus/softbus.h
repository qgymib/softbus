#ifndef __SOFTBUS_H__
#define __SOFTBUS_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <setjmp.h>

#include "ev.h"

#define BUS_INVALID_MESSAGE_ID  ((uint32_t)(-1))

#if defined(_WIN32)
#   include<windows.h>
    typedef union { int f; CRITICAL_SECTION r; }    bus_os_mutex_t;
#else
#   include <pthread.h>
#   include <semaphore.h>
    typedef union { int f; pthread_t r; }           bus_os_thread_t;
    typedef union { int f; pthread_mutex_t r; }     bus_os_mutex_t;
    typedef union { int f; sem_t r; }               bus_os_sem_t;
#endif

struct bus_msg;
typedef struct bus_msg bus_msg_t;

struct bus_dat;
typedef struct bus_dat bus_dat_t;

struct bus_service;
typedef struct bus_service bus_service_t;

struct bus_filber;
typedef struct bus_filber bus_filber_t;

/**
 * @brief Destroy data
 * @param[in] data  Data to destroy
 */
typedef void (*bus_dat_drop_fn)(void* data);

/**
 * @brief Message handle function
 * @param[in] msg   Message
 */
typedef void (*bus_msg_handle_fn)(bus_msg_t* msg);

/**
 * @brief Data handle function
 * @param[in] chid  Channel ID
 * @param[in] data  Data
 */
typedef void (*bus_dat_handle_fn)(uint32_t chid, bus_dat_t* data);

typedef enum bus_msg_type
{
    /**
     * @brief Request message.
     * 
     * If a service want to send a request message, it MUST already register a
     * corresponding response handler, otherwise #abort() will be triggered.
     *
     * The `uuid` MUST be unique during non-replay request. If a newly send
     * request have the same uuid as existing non-replay request, #abort() will
     * be triggered.
     *
     * If the target service id not exist, or error occured during transmission,
     * a auto-generate response will be received, and an error code can be read
     * from #bus_msg_t::errcode.
     */
    BUS_MSG_TYPE_REQ,

    /**
     * @brief Response message.
     *
     * A response can be send only after receive a request message, otherwise
     * #abort() will be trigger.
     */
    BUS_MSG_TYPE_RSP,
}bus_msg_type_t;

typedef enum bus_msg_attr
{
    BUS_MSG_ATTR_NO_REPLY_EXPECTED = 1,     /**< A reply is not expected */
    BUS_MSG_ATTR_URGENCY_REQUEST   = 2,     /**< A urgency request has higher priority */
}bus_msg_attr_t;

typedef enum bus_errno
{
    BUS_ERR_NONE           = 0,             /**< No error */
    BUS_ERR_NO_SID         = -1,            /**< Service ID not found */
    BUS_ERR_NO_MID         = -2,            /**< Message ID not found */
    BUS_ERR_NO_PERM        = -3,            /**< No permission */
}bus_errno_t;

typedef struct bus_msg_handle
{
    uint32_t                msgid;          /**< Message ID */
    bus_msg_type_t          type;           /**< Message type */
    bus_msg_handle_fn       handle;         /**< Message handler */
}bus_msg_handle_t;

typedef struct bus_dat_handle
{
    uint32_t                chan;           /**< Channel ID */
    bus_dat_handle_fn       handle;         /**< Data handler */
}bus_dat_handle_t;

typedef struct bus_service_entry
{
    const bus_msg_handle_t* msg_handles;        /**< Message handler list */
    size_t                  msg_handle_size;    /**< Message handler list size */

    const bus_dat_handle_t* dat_handles;        /**< Data handle lise */
    size_t                  dat_handle_size;    /**< Data handle list size */

    /**
     * @brief Initialize callback
     */
    void (*on_init)(void);

    /**
     * @brief Exit callback
     */
    void (*on_exit)(void);
}bus_service_entry_t;

typedef struct bus_filber_entry
{
    /**
     * @brief Initialize callback
     */
    void (*on_init)(void);

    /**
     * @brief Exit callback
     */
    void (*on_exit)(void);

    /**
     * @brief Execution point
     */
    void (*route)(void);
}bus_filber_entry_t;

struct bus_service
{
    uint32_t                    sid;            /**< Service ID */
    const bus_service_entry_t*  entry;          /**< Service entrypoint */
    size_t                      limit;          /**< Message queue limit */

    struct
    {
        ev_list_node_t          sche_s;         /**< #bus_group_t::sche_s */
        ev_map_node_t           global;         /**< Map node for global table */
    }node;

    struct
    {
        bus_os_mutex_t          mutex;          /**< Message queue guard */
        ev_list_t               queue;          /**< Message queue */
    }msg[3];

    struct
    {
        uint32_t                seqid;          /**< Sequence counter */
    }cnt;
};
#define BUS_SERVICE_INIT(sid, msgqsize)   \
    {\
        sid,                                /* .sid */\
        NULL,                               /* .entry */\
        msgqsize,                           /* .limit */\
        {\
            EV_LIST_NODE_INIT,              /* .node.sche_s */\
            EV_MAP_NODE_INIT,               /* .node.global */\
        },\
        {\
            { 0, }, EV_LIST_INIT,           /* .msg[0]: response queue */\
            { 0, }, EV_LIST_INIT,           /* .msg[1]: urgency request queue */\
            { 0, }, EV_LIST_INIT,           /* .msg[2]: normal request queue */\
        },\
        {\
            0,                              /* .cnt.seqid */\
        },\
    }

struct bus_filber
{
    uint32_t                    fid;        /**< Filber ID */
    const bus_filber_entry_t*   entry;
    jmp_buf                     jmpbuf;     /**< Registers */

    struct
    {
        ev_list_node_t          sche_f;     /**< #bus_group_t::sche_f */
        ev_map_node_t           global;     /**< Map node for global table */
    }node;

    struct
    {
        void*                   addr;       /**< Stack base address */
        size_t                  size;       /**< Stack size */
    }stack;
};
#define BUS_FILBER_INIT(fid, stack) \
    {\
        fid,                                /* .fid */\
        NULL,                               /* .entry */\
        BUS_JMPBUF_INIT,                    /* .jmpbuf */\
        {\
            EV_LIST_NODE_INIT,              /* .node.sche_f */\
            EV_MAP_NODE_INIT,               /* .node.global */\
        },\
        {\
            stack,                          /* .stack.addr */\
            sizeof(stack),                  /* .stack.size */\
        },\
    }

typedef struct bus_group
{
    ev_loop_t               loop;           /**< Event loop */
    bus_os_thread_t         thread;         /**< Thread */
    bus_os_sem_t            sem[2];         /**< [0]: wait thread; [1]: start thread */
    jmp_buf                 jmpbuf;         /**< Registers */

    struct
    {
        unsigned            looping : 1;    /**< looping */
    }mask;

    struct
    {
        bus_service_t*      table;          /**< Service list */
        size_t              size;           /**< Service list size */
    }service;

    struct
    {
        bus_filber_t*       table;          /**< Filber list */
        size_t              size;           /**< Filber list size */
    }filber;

    struct
    {
        bus_os_mutex_t      mutex;          /**< Schedule mutex */
        bus_service_t*      current;        /**< Current running serivce */
        ev_list_t           idle_queue;     /**< IDLE queue */
        ev_list_t           busy_queue;     /**< Busy queue */
    }sche_s;

    struct
    {
        bus_os_mutex_t      mutex;          /**< Schedule mutex */
        bus_filber_t*       current;        /**< Current running filber */
        ev_list_t           idle_queue;     /**< IDLE queue */
        ev_list_t           busy_queue;     /**< Busy queue */
    }sche_f;

    struct
    {
        long                priority;       /**< Thread priority */
        size_t              stacksize;      /**< Stack size. Zero means default size fot the executable */
        unsigned long       tid;            /**< Thread ID */
        void*               baseaddr;       /**< Stack base address */
    }info;
}bus_group_t;
#define BUS_GROUP_INIT(priority, stacksize, services, filbers)  \
    {\
        EV_LOOP_INIT,                               /* .loop */\
        0,                                          /* .thread */\
        { { 0 }, { 0 } },                           /* .sem */\
        BUS_JMPBUF_INIT,                            /* .jmpbuf */\
        {\
            0,                                      /* .mask.looping */\
        },\
        {\
            services,                               /* .service.table */\
            sizeof(services) / sizeof(services[0]), /* .service.size */\
        },\
        {\
            filbers,                                /* .filber.table */\
            sizeof(filbers) / sizeof(filbers[0]),   /* .filber.size */\
        },\
        {\
            { 0, },                                 /* .sche_s.mutex */\
            NULL,                                   /* .sche_s.current */\
            EV_LIST_INIT,                           /* .sche_s.idle_queue */\
            EV_LIST_INIT,                           /* .sche_s.busy_queue */\
        },\
        {\
            { 0, },                                 /* .sche_f.mutex */\
            NULL,                                   /* .sche_f.current */\
            EV_LIST_INIT,                           /* .sche_f.idle_queue */\
            EV_LIST_INIT,                           /* .sche_f.busy_queue */\
        },\
        {\
            priority,                               /* .info.priority */\
            stacksize,                              /* .info.stacksize */\
            0,                                      /* .info.tid */\
            NULL                                    /* .info.baseaddr */\
        },\
    }

struct bus_msg
{
    int32_t                 errc;       /**< Error code */
    uint32_t                refcnt;     /**< Reference counter */

    uint32_t                fsid;       /**< From service */
    uint32_t                tsid;       /**< To service */

    uint32_t                msgid;      /**< Message ID */
    uint32_t                seqid;      /**< Sequence ID */

    uint32_t                attrs;      /**< Attributes */
    uint32_t                uuid;       /**< User-defined unique identifier */

    void*                   data;       /**< Message data */
    bus_dat_drop_fn         drop;       /**< Drop function */
};

struct bus_dat
{
    uint32_t                fsid;       /**< From service */
    uint32_t                tsid;       /**< To service */
    uint32_t                chid;       /**< Channel ID */
    uint32_t                refcnt;     /**< Reference counter */

    void*                   data;       /**< Data */
    bus_dat_drop_fn         drop;       /**< Drop function */
};

typedef struct bus_channel
{
    uint32_t                chid;       /**< Channel ID */

    /**
     * @brief Channel initialize callback
     * @param[in] sid       Service ID
     */
    void (*on_init)(uint32_t sid);

    /**
     * @brief Channel exit callback
     * @param[in] sid       Service ID
     */
    void (*on_exit)(uint32_t sid);

    /**
     * @brief Callback when data want to push into channel
     * @param[in] sid       Service ID
     * @param[in] dat       Channel Data
     */
    void (*on_push)(uint32_t sid, bus_dat_t* dat);

    /**
     * @brief Callback when data want to pull out the channel
     * @param[in] sid       Service ID
     * @param[in] callback  Callback function. If no more data, the `dat` parameter must set to `NULL`.
     * @param[in] arg       Private argument that must pass to callback
     */
    void (*on_pull)(uint32_t sid, void(*callback)(bus_dat_t* dat, void* arg), void* arg);
}bus_channel_t;

typedef struct bus_static_check
{
    size_t                  sz_self;        /**< sizeof(bus_static_check_t) */
    size_t                  sz_bus_service; /**< sizeof(bus_service_t) */
    size_t                  sz_bus_filber;  /**< sizeof(bus_filber_t) */
    size_t                  sz_bus_group;   /**< sizeof(bus_group_t) */
}bus_static_check_t;
#define BUS_STATIC_CHECK_INIT  \
    {\
        sizeof(bus_static_check_t), /* .sz_self */\
        sizeof(bus_service_t),      /* .sz_bus_service */\
        sizeof(bus_filber_t),       /* .sz_bus_filber */\
        sizeof(bus_group_t),        /* .sz_bus_group */\
    }

typedef struct bus_config
{
    bus_group_t*    groups;         /**< Service group list */
    size_t          group_size;     /**< Service group list size */

    bus_channel_t*  channels;       /**< Channel list */
    size_t          channel_size;   /**< Channel list size */
}bus_config_t;

/**
 * @brief Initialize BUS system.
 * @param[in] config    Configuration
 * @param[in] check     Static check information. Use #BUS_STATIC_CHECK_INIT to initialize structure.
 * @return              #bus_errno_t
 */
int bus_init(const bus_config_t* config, const bus_static_check_t* check);

int bus_load(void);
int bus_exit(void);

int bus_register_service(uint32_t sid, const bus_service_entry_t* entry);
int bus_register_filber(uint32_t fid, const bus_filber_entry_t* entry);

/**
 * @brief Send message
 *
 * @param[in] fsid    The service id of sender
 * @param[in] tsid    The service id of receiver
 * @param[in] type    Message type
 * @param[in] msgid   Message ID
 * @param[in] uuid    User-defined unique identifier
 * @param[in] data    Message data
 * @param[in] drop    How to destroy message
 * @param[in] attrs   BitOR attributes
 */
void bus_send_msg(uint32_t fsid, uint32_t tsid, bus_msg_type_t type,
    int32_t msgid, uint32_t uuid, void* data, bus_dat_drop_fn drop, unsigned attrs);

void bus_send_dat(uint32_t fsid, uint32_t tsid, uint32_t chid, void* data,
    bus_dat_drop_fn drop);

void bus_filber_yield(void);
void bus_filber_resume(uint32_t fid);

int bus_recv_msg(uint32_t sid, bus_msg_type_t type, void (*callback)(bus_msg_t* msg, void* arg), void* arg);
int bus_pick_msg(uint32_t sid, bus_msg_type_t type, void (*callback)(bus_msg_t* msg, void* arg), void* arg);

void bus_msg_add_ref(bus_msg_t* msg);
void bus_msg_dec_ref(bus_msg_t* msg);

void bus_dat_add_ref(bus_dat_t* dat);
void bus_dat_dec_ref(bus_dat_t* dat);

#ifdef __cplusplus
}
#endif
#endif
