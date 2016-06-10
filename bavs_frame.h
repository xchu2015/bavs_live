/*****************************************************************************
 * frame.h: bavs decoder library
 *****************************************************************************
*/
 
#ifndef CAVS_FRAME_H
#define CAVS_FRAME_H

#include "bavs_cavlc.h"
#include "bavs_osdep.h"

typedef struct
{
    uint8_t *p_ori;
    unsigned char buf[SVA_STREAM_BUF_SIZE];
    unsigned short/*int*/ uClearBits;                //不含填充位的位缓冲，32位，初始值是0xFFFFFFFF
    unsigned int uPre3Bytes;                //含填充位的位缓冲，32位，初始值是0x00000000
    int iBytePosition;		                   //当前字节位置
    int iBufBytesNum;		                   //最近一次读入缓冲区的字节数
    int iClearBitsNum;		                   //不含填充位的位的个数
    int iStuffBitsNum;		                   //已剔除的填充位的个数，遇到开始码时置0
    int iBitsCount;			                   //码流总位数
    int demulate_enable;

    int64_t min_length;
}bitstream_backup;

typedef struct
{
    uint32_t i_startcode;
    uint8_t *p_start; /* exclude startcode */
    int i_len;
    
}cavs_slice_t;


typedef struct
{
    uint8_t *p_start;	//当前帧的输入流起始地址
    uint8_t *p_cur;
    //uint8_t *p_end;
   
    int i_len;			//帧包总字节数
    int frame_num;       //当前帧的解码序号
    int slice_num;		 //当前帧的slice数
    int max_frame_size;

    uint32_t i_startcode_type; /* frametype */

    bitstream_backup bits;

    cavs_slice_t slice[CAVS_MAX_SLICE_SIZE];

    int i_mb_end[2]; /*0:top 1:bot    mb row end of top or bot field */
    int b_bottom; /* 0 : top 1: bot */
} frame_pack;

/* frame */
uint32_t cavs_frame_pack( InputStream *p, frame_pack *frame );

/* decode packed frame */
int decode_one_frame(void *p_decoder,frame_pack *, cavs_param * );

/* top field */
uint32_t cavs_topfield_pack( InputStream *p, frame_pack *field );
int decode_top_field(void *p_decoder,frame_pack *, cavs_param * );

/* bot field */
uint32_t cavs_botfield_pack( InputStream *p, frame_pack *field );
int decode_bot_field(void *p_decoder,frame_pack *, cavs_param * );

/* multi-threads */
/* synchronized frame list */
typedef struct
{
	cavs_decoder **list;
	int i_max_size;
	int i_size;
	cavs_pthread_mutex_t mutex;
	cavs_pthread_cond_t cv_fill;  /* event signaling that the list became fuller */
	cavs_pthread_cond_t cv_empty; /* event signaling that the list became emptier */
} cavs_synch_frame_list_t;

cavs_decoder * cavs_frame_get (cavs_decoder ** list);

int cavs_synch_frame_list_init( cavs_synch_frame_list_t *slist, int size );
void cavs_synch_frame_list_delete( cavs_synch_frame_list_t *slist );
void cavs_synch_frame_list_push( cavs_synch_frame_list_t *slist, cavs_decoder *frame );
cavs_decoder *cavs_synch_frame_list_pop( cavs_synch_frame_list_t *slist );
void cavs_frame_cond_broadcast( xavs_image *frame, int i_lines_completed, int b_bot_field );
void cavs_frame_cond_wait( xavs_image *frame, int i_lines_completed, int b_bot_field );


#define CHECKED_MALLOC( var, size )\
	do {\
	var = cavs_malloc( size );\
	if( !var )\
	goto fail;\
	} while( 0 )

#define CHECKED_MALLOCZERO( var, size )\
	do {\
	CHECKED_MALLOC( var, size );\
	memset( var, 0, size );\
	} while( 0 )

#endif