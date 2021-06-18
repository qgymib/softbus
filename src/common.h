#ifndef __SOFTBUS_COMMON_INTERNAL_H__
#define __SOFTBUS_COMMON_INTERNAL_H__
#ifdef __cplusplus
extern "C" {
#endif

#if !defined(container_of)
#   if defined(__GNUC__) || defined(__clang__)
#	    define container_of(ptr, type, member) \
		    ({ \
			    const typeof(((type *)0)->member)*__mptr = (ptr); \
			    (type *)((char *)__mptr - offsetof(type, member)); \
		    })
#   else
#	    define container_of(ptr, type, member) \
		((type *) ((char *) (ptr) - offsetof(type, member)))
#   endif
#endif

#if defined(_WIN32)

#else

	typedef union { int f; pthread_key_t r; } bus_os_tls;
#	define BUS_OS_TLS_INIT(p)	pthread_key_create(&((p)->r), NULL)
#	define BUS_OS_TLS_EXIT(p)	pthread_key_delete((p)->r)
#	define BUS_OS_TLS_SET(p, v)	pthread_setspecific((p)->r, v)
#	define BUS_OS_TLS_GET(p)	pthread_getspecific((p)->r)

#	define BUS_OS_MUTEX_INIT(p)   pthread_mutex_init(&((p)->r), NULL)
#	define BUS_OS_MUTEX_EXIT(p)   pthread_mutex_destroy(&((p)->r))
#	define BUS_OS_MUTEX_ENTER(p)  pthread_mutex_lock(&((p)->r))
#	define BUS_OS_MUTEX_LEAVE(p)  pthread_mutex_unlock(&((p)->r))

#	define BUS_OS_SEM_INIT(p)		sem_init(&((p)->r), 0, 0)
#	define BUS_OS_SEM_EXIT(p)		sem_destroy(&((p)->r))
#	define BUS_OS_SEM_POST(p)		sem_post(&((p)->r))
#	define BUS_OS_SEM_PEND(p)		sem_wait(&((p)->r))

#endif

#define ARRAY_SIZE(arr)	(sizeof(arr) / sizeof(arr[0]))

#ifdef __cplusplus
}
#endif
#endif
