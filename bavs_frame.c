/*****************************************************************************
 * frame.c: thread pooling
 *****************************************************************************
*/

#include <stdio.h>
#include <string.h>
#include "bavs_globe.h"

cavs_decoder * cavs_frame_get (cavs_decoder ** list)
{
	cavs_decoder *frame = list[0];
	int i;
	for (i = 0; list[i]; i++)
		list[i] = list[i + 1];
	return frame;
}


int cavs_synch_frame_list_init( cavs_synch_frame_list_t *slist, int max_size )
{
	if( max_size < 0 )
		return -1;
	slist->i_max_size = max_size;
	slist->i_size = 0;
	CHECKED_MALLOCZERO( slist->list, (max_size+1) * sizeof(cavs_decoder *) );
	if( cavs_pthread_mutex_init( &slist->mutex, NULL ) ||
		cavs_pthread_cond_init( &slist->cv_fill, NULL ) ||
		cavs_pthread_cond_init( &slist->cv_empty, NULL ) )
		return -1;
	return 0;
fail:
	return -1;
}

void cavs_synch_frame_list_delete( cavs_synch_frame_list_t *slist )
{
	int i;
	if (!slist)
		return;

	cavs_pthread_mutex_destroy( &slist->mutex );
	cavs_pthread_cond_destroy( &slist->cv_fill );
	cavs_pthread_cond_destroy( &slist->cv_empty );

	for (i = 0; i < slist->i_max_size; i++) 
	{
		if (slist->list[i])
			cavs_decoder_destroy(slist->list[i]);
	}
	cavs_free(slist->list);
}

void cavs_synch_frame_list_push( cavs_synch_frame_list_t *slist, cavs_decoder *frame )
{
	cavs_pthread_mutex_lock( &slist->mutex );
	while( slist->i_size == slist->i_max_size )
		cavs_pthread_cond_wait( &slist->cv_empty, &slist->mutex );
	slist->list[ slist->i_size++ ] = frame;
	cavs_pthread_mutex_unlock( &slist->mutex );
	cavs_pthread_cond_broadcast( &slist->cv_fill );
}

cavs_decoder *cavs_synch_frame_list_pop( cavs_synch_frame_list_t *slist )
{
	cavs_decoder *frame;
	cavs_pthread_mutex_lock( &slist->mutex );
	while( !slist->i_size )
		cavs_pthread_cond_wait( &slist->cv_fill, &slist->mutex );
	frame = slist->list[ --slist->i_size ];
	slist->list[ slist->i_size ] = NULL;
	cavs_pthread_cond_broadcast( &slist->cv_empty );
	cavs_pthread_mutex_unlock( &slist->mutex );
	return frame;
}