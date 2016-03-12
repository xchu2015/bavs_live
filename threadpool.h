/*****************************************************************************
 * threadpool.h: thread pooling
 *****************************************************************************
*/

#ifndef BAVS_THREADPOOL_H
#define BAVS_THREADPOOL_H

typedef struct cavs_threadpool_t cavs_threadpool_t;

#if HAVE_THREAD
int   cavs_threadpool_init( cavs_threadpool_t **p_pool, int threads,
                            void (*init_func)(void *), void *init_arg );
void  cavs_threadpool_run( cavs_threadpool_t *pool, void *(*func)(void *), void *arg );
void *cavs_threadpool_wait( cavs_threadpool_t *pool, void *arg );
void  cavs_threadpool_delete( cavs_threadpool_t *pool );
#else
#define cavs_threadpool_init(p,t,f,a) -1
#define cavs_threadpool_run(p,f,a)
#define cavs_threadpool_wait(p,a)     NULL
#define cavs_threadpool_delete(p)
#endif

#endif
