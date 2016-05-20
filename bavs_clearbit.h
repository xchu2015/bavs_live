/*****************************************************************************
 * bavs_clearbit.h: 2d-VLC
 *****************************************************************************
*/


#ifndef CAVS_CLEARBIT_H
#define CAVS_CLEARBIT_H 

#include <stdio.h>

#define SVA_STREAM_BUF_SIZE 1024
#define MAX_CODED_FRAME_SIZE 2000000         //!< bytes for one frame

#define I_PICTURE_START_CODE    0xB3
#define PB_PICTURE_START_CODE   0xB6
#define SLICE_START_CODE_MIN    0x00
#define SLICE_START_CODE_MAX    0xAF
#define USER_DATA_START_CODE    0xB2
#define SEQUENCE_HEADER_CODE    0xB0
#define EXTENSION_START_CODE    0xB5
#define SEQUENCE_END_CODE       0xB1
#define VIDEO_EDIT_CODE         0xB7

#define MIN2(a,b) ( (a)<(b) ? (a) : (b) )

typedef struct {
    unsigned char *f; /* save stream address */
    unsigned char *f_end;
    unsigned char buf[SVA_STREAM_BUF_SIZE];  //��������,size must be large than 3 bytes
	unsigned char m_buf[MAX_CODED_FRAME_SIZE];
    unsigned int uClearBits;                //�������λ��λ���壬32λ����ʼֵ��0xFFFFFFFF
    unsigned int uPre3Bytes;                //�����λ��λ���壬32λ����ʼֵ��0x00000000
    int iBytePosition;		                   //��ǰ�ֽ�λ��
    int iBufBytesNum;		                   //���һ�ζ��뻺�������ֽ���
    int iClearBitsNum;		                   //�������λ��λ�ĸ���
    int iStuffBitsNum;		                   //���޳������λ�ĸ�����������ʼ��ʱ��0
    int iBitsCount;			                   //������λ��

    int demulate_enable;
} InputStream;

int cavs_init_bitstream( InputStream *p , unsigned char *rawstream, unsigned int i_len );
unsigned int cavs_get_one_nal ( InputStream* p, unsigned char *buf, int *length, int buf_len);

#endif

