/*****************************************************************************
 * threadpool.c: thread pooling
 *****************************************************************************
*/

#include "bavs_globe.h"

typedef struct
{
    void *(*func)(void *);
    void *arg;
    void *ret;
} cavs_threadpool_job_t;

struct cavs_threadpool_t
{
    int            exit;
    int            threads;
    cavs_pthread_t *thread_handle;
    void           (*init_func)(void *);
    void           *init_arg;

    /* requires a synchronized list structure and associated methods,
       so use what is already implemented for frames */
    cavs_synch_frame_list_t uninit; /* list of jobs that are awaiting use */
    cavs_synch_frame_list_t run;    /* list of jobs that are queued for processing by the pool */
    cavs_synch_frame_list_t done;   /* list of jobs that have finished processing */
};

static void cavs_threadpool_thread( cavs_threadpool_t *pool )
{
    if( pool->init_func )
        pool->init_func( pool->init_arg );

    while( !pool->exit )
    {
        cavs_threadpool_job_t *job = NULL;
        cavs_pthread_mutex_lock( &pool->run.mutex );
        while( !pool->exit && !pool->run.i_size )
            cavs_pthread_cond_wait( &pool->run.cv_fill, &pool->run.mutex );
        if( pool->run.i_size )
        {
            job = (void*)cavs_frame_get( pool->run.list );
            pool->run.i_size--;
        }
        cavs_pthread_mutex_unlock( &pool->run.mutex );
        if( !job )
            continue;
        job->ret = job->func( job->arg ); /* execute the function */
        cavs_synch_frame_list_push( &pool->done, (void*)job );
    }
}

int cavs_threadpool_init( cavs_threadpool_t **p_pool, int threads,
						  void (*init_func)(void *), void *init_arg)
{
    int i;
    cavs_threadpool_t *pool;
    if( threads <= 0 )
        return -1;

    CHECKED_MALLOCZERO( pool, sizeof(cavs_threadpool_t) );
    *p_pool = pool;

    pool->init_func = init_func;
    pool->init_arg  = init_arg;
    pool->threads   = XAVS_MIN( threads, XAVS_THREAD_MAX );

    CHECKED_MALLOC( pool->thread_handle, pool->threads * sizeof(cavs_pthread_t) );

    if( cavs_synch_frame_list_init( &pool->uninit, pool->threads ) ||
        cavs_synch_frame_list_init( &pool->run, pool->threads ) ||
        cavs_synch_frame_list_init( &pool->done, pool->threads ) )
        goto fail;

    for( i = 0; i < pool->threads; i++ )
    {
       cavs_threadpool_job_t *job;
       CHECKED_MALLOC( job, sizeof(cavs_threadpool_job_t) );
       cavs_synch_frame_list_push( &pool->uninit, (void*)job );
    }
    for( i = 0; i < pool->threads; i++ )
        if( cavs_pthread_create( pool->thread_handle+i, NULL, (void*)cavs_threadpool_thread, pool ) )
            goto fail;

    return 0;
fail:
    return -1;
}

void cavs_threadpool_run( cavs_threadpool_t *pool, void *(*func)(void *), void *arg )
{
	cavs_threadpool_job_t *job = (void*)cavs_synch_frame_list_pop( &pool->uninit );
	job->func = func;
	job->arg  = arg;
	cavs_synch_frame_list_push( &pool->run, (void*)job );
}

void *cavs_threadpool_wait( cavs_threadpool_t *pool, void *arg )
{
    cavs_threadpool_job_t *job = NULL;
    int i;
    void *ret;

    cavs_pthread_mutex_lock( &pool->done.mutex );
    while( !job )
    {
        for( i = 0; i < pool->done.i_size; i++ )
        {
            cavs_threadpool_job_t *t = (void*)pool->done.list[i];
            if( t->arg == arg )
            {
                job = (void*)cavs_frame_get( pool->done.list+i );
                pool->done.i_size--;
            }
        }
        if( !job )
            cavs_pthread_cond_wait( &pool->done.cv_fill, &pool->done.mutex );
    }
    cavs_pthread_mutex_unlock( &pool->done.mutex );

    ret = job->ret;
    cavs_synch_frame_list_push( &pool->uninit, (void*)job );
    return ret;
}

static void cavs_threadpool_list_delete( cavs_synch_frame_list_t *slist )
{
    int i;
    for( i = 0; slist->list[i]; i++ )
    {
        cavs_free( slist->list[i] );
        slist->list[i] = NULL;
    }
    cavs_synch_frame_list_delete( slist );
}

void cavs_threadpool_delete( cavs_threadpool_t *pool )
{
    int i;
	if (!pool)
		return;
    cavs_pthread_mutex_lock( &pool->run.mutex );
    pool->exit = 1;
    cavs_pthread_cond_broadcast( &pool->run.cv_fill );
    cavs_pthread_mutex_unlock( &pool->run.mutex );
    for( i = 0; i < pool->threads; i++ )
        cavs_pthread_join( pool->thread_handle[i], NULL );

    cavs_threadpool_list_delete( &pool->uninit );
    cavs_threadpool_list_delete( &pool->run );
    cavs_threadpool_list_delete( &pool->done );
    cavs_free( pool->thread_handle );
    cavs_free( pool );
}
