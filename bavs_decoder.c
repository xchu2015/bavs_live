/*****************************************************************************
 * decoder.c: bavs decoder library
 *****************************************************************************
*/
 
#include <stdlib.h>
#include <stddef.h>

#include "bavs_globe.h"

#if HAVE_MMX
#include "common/x86/cavsdsp.h"
#endif

#define MIN_QP          0
#define MAX_QP          63

//#pragma warning(disable:4996)

uint8_t crop_table[256 + 2 * MAX_NEG_CROP];

static int cavs_decoder_reset( void *p_decoder );

#if THREADS_OPT
static int get_mb_i_aec_opt( cavs_decoder *p );
static int get_mb_i_rec_opt( cavs_decoder *p );
static int get_mb_p_aec_opt( cavs_decoder *p );
static int get_mb_p_rec_opt( cavs_decoder *p );
static int get_mb_b_aec_opt( cavs_decoder *p );
static int get_mb_b_rec_opt( cavs_decoder *p );
#endif

#if HAVE_MMX
/*asm*/

/* idct */
extern void cavs_add8x8_idct8_sse2( uint8_t *p_dst, int16_t block[64],int stride );

/* intra predict */
extern void cavs_predict_8x8_dc_128_mmxext( uint8_t *p_dest, uint8_t edge[33], int i_stride, uint8_t *p_top, uint8_t *p_left );
extern void cavs_predict_8x8_v_mmxext( uint8_t *p_dest, uint8_t edge[33], int i_stride, uint8_t *p_top, uint8_t *p_left );
extern void cavs_predict_8x8_h_mmxext( uint8_t *p_dest, uint8_t edge[33], int i_stride, uint8_t *p_top, uint8_t *p_left );
extern void cavs_predict_8x8_dc_top_mmxext( uint8_t *p_dest, uint8_t edge[33], int i_stride, uint8_t *p_top, uint8_t *p_left );
extern void cavs_predict_8x8_dc_left_mmxext( uint8_t *p_dest, uint8_t edge[33], int i_stride, uint8_t *p_top, uint8_t *p_left );
extern void cavs_predict_8x8_dc_mmxext( uint8_t *p_dest, uint8_t edge[33], int i_stride, uint8_t *p_top, uint8_t *p_left );
extern void cavs_predict_8x8_ddl_mmxext( uint8_t *p_dest, uint8_t edge[33], int i_stride, uint8_t *p_top, uint8_t *p_left );
extern void cavs_predict_8x8_ddr_mmxext( uint8_t *p_dest, uint8_t edge[33], int i_stride, uint8_t *p_top, uint8_t *p_left );

extern void cavs_predict_8x8c_h_mmxext( uint8_t *p_dest, int neighbor, int i_stride, uint8_t *p_top, uint8_t *p_left );
extern void cavs_predict_8x8c_v_mmxext( uint8_t *p_dest, int neighbor, int i_stride, uint8_t *p_top, uint8_t *p_left );
extern void cavs_predict_8x8c_dc_128_mmxext( uint8_t *p_dest, int neighbor, int i_stride, uint8_t *p_top, uint8_t *p_left );
extern void cavs_predict_8x8c_dc_top_sse4( uint8_t *p_dest, int neighbor, int i_stride, uint8_t *p_top, uint8_t *p_left );
extern void cavs_predict_8x8c_dc_left_core_sse4( uint8_t *p_dest, uint8_t left[8], int i_stride, uint8_t *p_top, uint8_t *p_left );
extern void cavs_predict_8x8c_dc_core_sse4( uint8_t *p_dest, int neighbor, uint8_t left[8], int i_stride, uint8_t *p_top, uint8_t *p_left );
extern void cavs_predict_8x8c_p_core_mmxext( uint8_t *p_dest, int i00, int b, int c, int i_stride );
    
#define SRC1(x) left[x]

#define PL1(y) \
	l[y] = (SRC1(y-1) + (SRC1(y)<<1) + SRC1(y+1) + 2) >> 2;

#define PREDICT_8x8_LOAD_LEFT \
	l[0] = (((i_neighbor&MB_TOPLEFT) ? SRC1(-1) : SRC1(0)) \
	+ (SRC1(0)<<1) + SRC1(1) + 2) >> 2; \
	PL1(1) PL1(2) PL1(3) PL1(4) PL1(5) PL1(6) \
	l[7] = (SRC1(6) + (SRC1(7)<<1) +((i_neighbor&MB_DOWNLEFT) ?\
	SRC1(8) : SRC1(7))+2) >> 2;

void cavs_predict_8x8c_dc_sse4( uint8_t *p_dest, int i_neighbor, 
                                                              int i_stride, uint8_t *p_top, uint8_t *p_left )
{
	DECLARE_ALIGNED_8(uint8_t l[8]);
       uint8_t *left = p_left + 1; 
        
	PREDICT_8x8_LOAD_LEFT	

	cavs_predict_8x8c_dc_core_sse4(p_dest, i_neighbor, l, i_stride, p_top, p_left );
}

void cavs_predict_8x8c_dc_left_sse4( uint8_t *p_dest, int i_neighbor, 
                                                                    int i_stride, uint8_t *p_top, uint8_t *p_left )
{
	DECLARE_ALIGNED_8(uint8_t l[8]);
       uint8_t *left = p_left + 1;    

	PREDICT_8x8_LOAD_LEFT
    
	cavs_predict_8x8c_dc_left_core_sse4(p_dest, l, i_stride, p_top, p_left);
}


#define PREDICT_P_SUM(j,i)\
	H += i * ( top[j+i]  - top[j-i] );\
	V += i * ( left[j+i] - left[j-i] );

void cavs_predict_8x8c_p_mmxext( uint8_t *p_dest, int i_neighbor, int i_stride,
                                                                  uint8_t *p_top, uint8_t *p_left )
{
    int a, b, c;
    int H = 0;
    int V = 0;
    int i00;
    uint8_t *left = p_left + 1;
    uint8_t *top = p_top + 1;
    
    PREDICT_P_SUM(3,1)
    PREDICT_P_SUM(3,2)
    PREDICT_P_SUM(3,3)
    PREDICT_P_SUM(3,4)
    a = 16 * ( left[7] + top[7] );
    b = ( ((H<<4)+H) + 16 ) >> 5;
    c = ( ((V<<4)+V) + 16 ) >> 5;
    i00 = a -3*b -3*c + 16;
    cavs_predict_8x8c_p_core_mmxext( p_dest, i00, b, c, i_stride );
}

/* MC */
extern void cavs_put_h264_chroma_mc8_rnd_mmxext(uint8_t *dst, uint8_t *src, int stride, int h, int mx, int my);
extern void cavs_put_h264_chroma_mc4_mmxext(uint8_t *dst, uint8_t *src, int stride, int h, int mx, int my);
extern void cavs_put_h264_chroma_mc2_mmxext(uint8_t *dst, uint8_t *src, int stride, int h, int mx, int my);

extern void cavs_put_h264_chroma_mc8_rnd_ssse3(uint8_t *dst, uint8_t *src, int stride, int h, int mx, int my);
extern void cavs_put_h264_chroma_mc4_ssse3(uint8_t *dst, uint8_t *src, int stride, int h, int mx, int my);

extern void cavs_avg_h264_chroma_mc8_rnd_mmxext(uint8_t *dst, uint8_t *src, int stride, int h, int mx, int my);
extern void cavs_avg_h264_chroma_mc4_mmxext(uint8_t *dst, uint8_t *src, int stride, int h, int mx, int my);
extern void cavs_avg_h264_chroma_mc2_mmxext(uint8_t *dst, uint8_t *src, int stride, int h, int mx, int my);

extern void cavs_avg_h264_chroma_mc8_rnd_ssse3(uint8_t *dst, uint8_t *src, int stride, int h, int mx, int my);
extern void cavs_avg_h264_chroma_mc4_ssse3(uint8_t *dst, uint8_t *src, int stride, int h, int mx, int my);

/* deblocking */
extern void cavs_deblock_v_luma_intra_sse2( pixel *pix, int stride, int alpha, int beta );
extern void cavs_deblock_h_luma_intra_sse2( pixel *pix, int stride, int alpha, int beta );
extern void cavs_deblock_v_chroma_intra_mmxext( pixel *pix, int stride, int alpha, int beta );
extern void cavs_deblock_h_chroma_intra_mmxext( pixel *pix, int stride, int alpha, int beta );

extern void cavs_deblock_v_luma_sse2( pixel *pix, int stride, int alpha, int beta, int8_t *tc0 );
extern void cavs_deblock_h_luma_sse2( pixel *pix, int stride, int alpha, int beta, int8_t *tc0 );
extern void cavs_deblock_v_chroma_sse2( pixel *pix, int stride, int alpha, int beta, int8_t *tc0 );
extern void cavs_deblock_h_chroma_sse2( pixel *pix, int stride, int alpha, int beta, int8_t *tc0 );

/* weighting prediction */
/* add avg asm */
#define DECL_SUF( func, args )\
    void func##_mmx2 args;\
    void func##_sse2 args;\
    void func##_ssse3 args;\
    void func##_avx2 args;

DECL_SUF( cavs_pixel_avg_16x16, ( uint8_t *dst, int i_dst, uint8_t *src, int i_src))
DECL_SUF( cavs_pixel_avg_8x8,   ( uint8_t *dst, int i_dst, uint8_t *src, int i_src))
DECL_SUF( cavs_pixel_avg_4x4,   ( uint8_t *dst, int i_dst, uint8_t *src, int i_src))

#endif

#if B_MB_WEIGHTING

static inline void pixel_avg_wxh( uint8_t *dst,  int i_dst,  uint8_t *src, int i_src, int width, int height )
{
    int x, y;
    
    for( y = 0; y < height; y++ )
    {
        for( x = 0; x < width; x++ )
            dst[x] = ( dst[x] + src[x] + 1 ) >> 1;
        src += i_src;
        dst += i_dst;
    }
}

void pixel_avg_16x16_c( uint8_t *dst,  int i_dst,  uint8_t *src, int i_src )
{
    pixel_avg_wxh( dst,  i_dst,  src, i_src, 16, 16 );
}

void pixel_avg_8x8_c( uint8_t *dst,  int i_dst,  uint8_t *src, int i_src )
{
    pixel_avg_wxh( dst,  i_dst,  src, i_src, 8, 8 );
}

void pixel_avg_4x4_c( uint8_t *dst,  int i_dst,  uint8_t *src, int i_src )
{
    pixel_avg_wxh( dst,  i_dst,  src, i_src, 4, 4 );
}
#endif

void init_crop_table()
{
    int i;
    
    for( i = 0; i < 256; i++ )
    {
        crop_table[i + MAX_NEG_CROP] = i;
    }

    memset(crop_table, 0, MAX_NEG_CROP*sizeof(uint8_t));
    memset(crop_table + MAX_NEG_CROP + 256, 255, MAX_NEG_CROP*sizeof(uint8_t));
}

void (*xavs_idct4_add)(uint8_t *dst, DCTELEM *block, int stride);
void (*xavs_idct8_add)(uint8_t *dst, DCTELEM *block, int stride);

void (*intra_pred_lum[8])(uint8_t *p_dest, uint8_t edge[33], int i_stride, uint8_t *p_top, uint8_t *p_left);
void (*intra_pred_lum_asm[8])(uint8_t *p_dest, uint8_t edge[33], int i_stride, uint8_t *p_top, uint8_t *p_left);

void (*intra_pred_lum_4x4[20])(uint8_t * src, uint8_t edge[17], int i_stride);
void (*intra_pred_chroma[7])(uint8_t *p_dest, int neighbor, int i_stride, uint8_t *p_top,uint8_t *p_left);
void (*intra_pred_chroma_asm[7])(uint8_t *p_dest, int neighbor, int i_stride, uint8_t *p_top,uint8_t *p_left);
#if !HAVE_MMX
void (*cavs_filter_lv)(cavs_decoder *p, uint8_t *d, int stride, int i_qp, int bs1, int bs2) ;
void (*cavs_filter_lh)(cavs_decoder *p, uint8_t *d, int stride, int i_qp, int bs1, int bs2) ;
void (*cavs_filter_cv)(cavs_decoder *p, uint8_t *d, int stride, int i_qp, int bs1, int bs2) ;
void (*cavs_filter_ch)(cavs_decoder *p, uint8_t *d, int stride, int i_qp, int bs1, int bs2) ;
#endif

#if 1
static const uint8_t alpha_tab[64] = 
{
	0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  2,  2,  2,  3,  3,
		4,  4,  5,  5,  6,  7,  8,  9, 10, 11, 12, 13, 15, 16, 18, 20,
		22, 24, 26, 28, 30, 33, 33, 35, 35, 36, 37, 37, 39, 39, 42, 44,
		46, 48, 50, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64
};

static const uint8_t beta_tab[64] = 
{
	0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,
		2,  2,  3,  3,  3,  3,  4,  4,  4,  4,  5,  5,  5,  5,  6,  6,
		6,  7,  7,  7,  8,  8,  8,  9,  9, 10, 10, 11, 11, 12, 13, 14,
		15, 16, 17, 18, 19, 20, 21, 22, 23, 23, 24, 24, 25, 25, 26, 27
};
static const uint8_t tc_tab[64] = 
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2,
		2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4,
		5, 5, 5, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 9, 9, 9
};
#endif

static void cavs_filter_lv_c(cavs_decoder *p, uint8_t *d, int stride, int  i_qp,
                          int bs1, int bs2) 
{
   int i;
   int alpha, beta, tc;
   int index_a, index_b;

   index_a = xavs_clip(i_qp + p->ph.i_alpha_c_offset,0,63);
   index_b = xavs_clip(i_qp + p->ph.i_beta_offset, 0,63);
   alpha = alpha_tab[index_a];
   beta  =  beta_tab[index_b];
   tc    =    tc_tab[index_a];
    
    if(bs1==2)
        for(i=0;i<16;i++)
             loop_filter_l2(d + i*stride,1,alpha,beta);
    else 
    {
        if(bs1)
            for(i=0;i<8;i++)
               loop_filter_l1(d + i*stride,1,alpha,beta,tc);
        if (bs2)
           for(i=8;i<16;i++)
               loop_filter_l1(d + i*stride,1,alpha,beta,tc);
    }
}

static void cavs_filter_lh_c(cavs_decoder *p, uint8_t *d, int stride, int i_qp, 
                                                int bs1, int bs2) 
{
   int i;
   int alpha, beta, tc;
   int index_a, index_b;

   index_a = xavs_clip(i_qp + p->ph.i_alpha_c_offset,0,63);
   index_b = xavs_clip(i_qp + p->ph.i_beta_offset, 0,63);
   alpha = alpha_tab[index_a];
   beta  =  beta_tab[index_b];
   tc    =    tc_tab[index_a];
   
   if(bs1==2)
        for(i=0;i<16;i++)
             loop_filter_l2(d + i,stride,alpha,beta);
    else {
         if(bs1)
            for(i=0;i<8;i++)
                 loop_filter_l1(d + i,stride,alpha,beta,tc);
         if (bs2)
            for(i=8;i<16;i++)
                 loop_filter_l1(d + i,stride,alpha,beta,tc);
     }
}
 
static void cavs_filter_cv_c(cavs_decoder *p, uint8_t *d, int stride, int i_qp, 
                                               int bs1, int bs2) 
{
    int i;
    int alpha, beta, tc;
    int index_a, index_b;

    index_a = xavs_clip(i_qp + p->ph.i_alpha_c_offset,0,63);
    index_b = xavs_clip(i_qp + p->ph.i_beta_offset, 0,63);
    alpha = alpha_tab[index_a];
    beta  =  beta_tab[index_b];
    tc    =    tc_tab[index_a];

     if(bs1==2)
        for(i=0;i<8;i++)
             loop_filter_c2(d + i*stride,1,alpha,beta);
     else 
     {
         if(bs1)
            for(i=0;i<4;i++)
                 loop_filter_c1(d + i*stride,1,alpha,beta,tc);
         if (bs2)
             for(i=4;i<8;i++)
                 loop_filter_c1(d + i*stride,1,alpha,beta,tc);
    }
}

static void cavs_filter_ch_c(cavs_decoder *p, uint8_t *d, int stride, int i_qp, 
                                                int bs1, int bs2) 
{
    int i;
    int alpha, beta, tc;
    int index_a, index_b;

    index_a = xavs_clip(i_qp + p->ph.i_alpha_c_offset,0,63);
    index_b = xavs_clip(i_qp + p->ph.i_beta_offset, 0,63);
    alpha = alpha_tab[index_a];
    beta  =  beta_tab[index_b];
    tc    =    tc_tab[index_a];
    
    if(bs1==2)
        for(i=0;i<8;i++)
             loop_filter_c2(d + i,stride,alpha,beta);
    else 
	{
         if(bs1)
            for(i=0;i<4;i++)
                loop_filter_c1(d + i,stride,alpha,beta,tc);
       if (bs2)
            for(i=4;i<8;i++)
                 loop_filter_c1(d + i,stride,alpha,beta,tc);
     }
}
#if HAVE_MMX
static void cavs_filter_lv_sse2(cavs_decoder *p, uint8_t *d, int stride, int i_qp,
                          int bs1, int bs2) 
{
    int i;
    int alpha, beta, tc;
    int index_a, index_b;

    index_a = xavs_clip(i_qp + p->ph.i_alpha_c_offset,0,63);
    index_b = xavs_clip(i_qp + p->ph.i_beta_offset, 0,63);
    alpha = alpha_tab[index_a];
    beta  =  beta_tab[index_b];
    tc    =    tc_tab[index_a];
   
   if( bs1==2 )
   {  
        cavs_deblock_h_luma_intra_sse2(d, stride, alpha ,beta);
   }
   else 
   {	
       int8_t tc_m[2];
       int bs[2];
       bs[0] = bs1;
       bs[1] = bs2;
       for (i = 0; i < 2; i++)
           tc_m[i] = bs[i] ? tc_tab[index_a] : -1;

       cavs_deblock_h_luma_sse2(d, stride, alpha, beta, tc_m );
    }
}

static void cavs_filter_lh_sse2(cavs_decoder *p, uint8_t *d, int stride, int i_qp,
                                                    int bs1, int bs2) 
{
    int i;
    int alpha, beta, tc;
    int index_a, index_b;

    index_a = xavs_clip(i_qp + p->ph.i_alpha_c_offset,0,63);
    index_b = xavs_clip(i_qp + p->ph.i_beta_offset, 0,63);
    alpha = alpha_tab[index_a];
    beta  =  beta_tab[index_b];
    tc    =    tc_tab[index_a];
    
   if(bs1==2)
   {
        cavs_deblock_v_luma_intra_sse2(d, stride, alpha , beta);
   }   
   else 
   {
       int8_t tc_m[2];
       int bs[2];
       bs[0] = bs1;
       bs[1] = bs2;
       for (i = 0; i < 2; i++)
           tc_m[i] = bs[i] ? tc_tab[index_a] : -1;

       cavs_deblock_v_luma_sse2(d, stride, alpha, beta, tc_m );
   }
}

static void cavs_filter_cv_mmxext(cavs_decoder *p, uint8_t *d, int stride, int i_qp, int bs1, int bs2) 
{
    int i;
    int alpha, beta, tc;
    int index_a, index_b;

    index_a = xavs_clip(i_qp + p->ph.i_alpha_c_offset,0,63);
    index_b = xavs_clip(i_qp + p->ph.i_beta_offset, 0,63);
    alpha = alpha_tab[index_a];
    beta  =  beta_tab[index_b];
    tc    =    tc_tab[index_a];
    
     if(bs1==2)
     {
        cavs_deblock_h_chroma_intra_mmxext(d, stride, alpha, beta);
     }
     else 
     {
        int8_t tc_m[2];
        int bs[2];
        bs[0] = bs1;
        bs[1] = bs2;
        for (i = 0; i < 2; i++)
           tc_m[i] = bs[i] ? tc_tab[index_a] : -1;

        cavs_deblock_h_chroma_sse2(d, stride, alpha, beta, tc_m );
     }
}

static void cavs_filter_ch_mmxext(cavs_decoder *p, uint8_t *d, int stride, int i_qp, 
                                                        int bs1, int bs2) 
{
    int i;
    int alpha, beta, tc;
    int index_a, index_b;

    index_a = xavs_clip(i_qp + p->ph.i_alpha_c_offset,0,63);
    index_b = xavs_clip(i_qp + p->ph.i_beta_offset, 0,63);
    alpha = alpha_tab[index_a];
    beta  =  beta_tab[index_b];
    tc    =    tc_tab[index_a];
    
    if(bs1==2)
    {
        cavs_deblock_v_chroma_intra_mmxext(d, stride, alpha, beta);
    }
    else 
    {
        int8_t tc_m[2];
        int bs[2];
        bs[0] = bs1;
        bs[1] = bs2;
        for (i = 0; i < 2; i++)
           tc_m[i] = bs[i] ? tc_tab[index_a] : -1;

        cavs_deblock_v_chroma_sse2(d, stride, alpha, beta, tc_m );
     }
}
#endif
//static inline int get_cbp(int  i_cbp_code,int i_inter)
//{
//	static const uint8_t cbp_tab[64][2] = 
//	{
//		{63, 0},{15,15},{31,63},{47,31},{ 0,16},{14,32},{13,47},{11,13},
//		{ 7,14},{ 5,11},{10,12},{ 8, 5},{12,10},{61, 7},{ 4,48},{55, 3},
//		{ 1, 2},{ 2, 8},{59, 4},{ 3, 1},{62,61},{ 9,55},{ 6,59},{29,62},
//		{45,29},{51,27},{23,23},{39,19},{27,30},{46,28},{53, 9},{30, 6},
//		{43,60},{37,21},{60,44},{16,26},{21,51},{28,35},{19,18},{35,20},
//		{42,24},{26,53},{44,17},{32,37},{58,39},{24,45},{20,58},{17,43},
//		{18,42},{48,46},{22,36},{33,33},{25,34},{49,40},{40,52},{36,49},
//		{34,50},{50,56},{52,25},{54,22},{41,54},{56,57},{38,41},{57,38}
//	};
//	return cbp_tab[i_cbp_code][i_inter];
//}

static inline int get_cbp_4x4(int i_cbp_code)
{
	static const uint8_t cbp_tab_4x4[16] = 
	{
		15,0,13,12,11,7,14,3,10,5,4,1,2,8,9,6
	};
	return cbp_tab_4x4[i_cbp_code];
}

#if 1 /* add weighting quant */

#define UNDETAILED 0
#define DETAILED   1

static const unsigned char WeightQuantModel[4][64]={
//   l a b c d h
//	 0 1 2 3 4 5
	{ 	
     // Mode 0
		0,0,0,4,4,4,5,5,
		0,0,3,3,3,3,5,5,
		0,3,2,2,1,1,5,5,
		4,3,2,2,1,5,5,5,
		4,3,1,1,5,5,5,5,
		4,3,1,5,5,5,5,5,
		5,5,5,5,5,5,5,5,
		5,5,5,5,5,5,5,5},
	{  
	  // Mode 1
		0,0,0,4,4,4,5,5,
		0,0,4,4,4,4,5,5,
		0,3,2,2,2,1,5,5,
		3,3,2,2,1,5,5,5,
		3,3,2,1,5,5,5,5,
		3,3,1,5,5,5,5,5,
		5,5,5,5,5,5,5,5,
		5,5,5,5,5,5,5,5},
	{   
       // Mode 2
		0,0,0,4,4,3,5,5,
		0,0,4,4,3,2,5,5,
		0,4,4,3,2,1,5,5,
		4,4,3,2,1,5,5,5,
		4,3,2,1,5,5,5,5,
		3,2,1,5,5,5,5,5,
		5,5,5,5,5,5,5,5,
		5,5,5,5,5,5,5,5},
	{  
		// Mode 3
		0,0,0,3,2,1,5,5,
		0,0,4,3,2,1,5,5,
		0,4,4,3,2,1,5,5,
		3,3,3,3,2,5,5,5,
		2,2,2,2,5,5,5,5,
		1,1,1,5,5,5,5,5,
		5,5,5,5,5,5,5,5,
		5,5,5,5,5,5,5,5}
};

short cavs_wq_param_default[2][6]=
{
	{135,143,143,160,160,213},   
	{128, 98,106,116,116,128}     
};

void cavs_init_frame_quant_param( cavs_decoder *p )
{
    int i, j, k;

    if( p->ph.weighting_quant_flag )
       p->b_weight_quant_enable = 1;
    else
       p->b_weight_quant_enable = 0;	

     if( p->b_weight_quant_enable )
     {     
        p->UseDefaultScalingMatrixFlag[0] = p->UseDefaultScalingMatrixFlag[1] = 0;  // use weighed scaling matrix
     }
     else
     {
        p->UseDefaultScalingMatrixFlag[0] = p->UseDefaultScalingMatrixFlag[1] = 1;  // use default scaling matrix  
     }

    /* Patch the Weighting Parameters */
    if( !p->ph.mb_adapt_wq_disable )  //MBAWQ
    {
    	// Use default weighted parameters,    
    	for( i = 0; i < 2; i++ )
    		for( j = 0; j < 6; j++ )
    			p->wq_param[i][j] = cavs_wq_param_default[i][j];
    		
    	if(p->ph.weighting_quant_param_index != 0 )
    	{
    		if(p->ph.weighting_quant_param_index == 1 )
    		{
    			for( i = 0; i < 6; i++)
    				p->wq_param[UNDETAILED][i] = p->ph.weighting_quant_param_undetail[i];
    		}
    		else if( p->ph.weighting_quant_param_index == 2 )
    		{
    			for( i = 0; i < 6; i++)
    				p->wq_param[DETAILED][i] = p->ph.weighting_quant_param_detail[i];
    		}
    		else if( p->ph.weighting_quant_param_index == 3 )
    		{
    			for( i = 0; i < 6; i++ )
        			p->wq_param[UNDETAILED][i] = p->ph.weighting_quant_param_undetail[i];
    			for(i=0;i<6;i++)
        			p->wq_param[DETAILED][i] = p->ph.weighting_quant_param_detail[i];
    		}
    	}
    }
    else   // PAWQ
    {
    	for(i=0;i<2;i++)
    		for(j=0;j<6;j++)
    			p->wq_param[i][j]=128;
    		
    	if(p->ph.weighting_quant_param_index==0)
    	{
    		for(i=0;i<6;i++)
    			p->wq_param[DETAILED][i]=cavs_wq_param_default[DETAILED][i];
    	}
    	else if(p->ph.weighting_quant_param_index==1)
    	{
    		for(i=0;i<6;i++)
    			p->wq_param[UNDETAILED][i] = p->ph.weighting_quant_param_undetail[i];
    	}
    	if(p->ph.weighting_quant_param_index==2)
    	{
    		for(i=0;i<6;i++)
    			p->wq_param[DETAILED][i] = p->ph.weighting_quant_param_detail[i];
    	}
    		
    }

    for (i=0;i<64;i++)  
        p->cur_wq_matrix[i]=1<<7;

    /* Reconstruct the Weighting matrix */
    if( !p->b_weight_quant_enable )
    {  
        for(k=0;k<2;k++)        
    	  for(j=0;j<8;j++)
            for (i=0;i<8;i++)
              p->wq_matrix[k][j*8+i]=1<<7;
    }
    else
    {			
    for(k=0;k<2;k++) 
      for(j=0;j<8;j++)
        for(i=0;i<8;i++)
          p->wq_matrix[k][j*8+i]=(p->wq_param[k][WeightQuantModel[p->ph.weighting_quant_model/*CurrentSceneModel*/][j*8+i]]); 
    } 
}

void cavs_frame_update_wq_matrix( cavs_decoder *p )
{
    int i;

    if( p->ph.weighting_quant_param_index == 0 )
    {
        for( i = 0; i < 64; i++ ) 
            p->cur_wq_matrix[i] = p->wq_matrix[DETAILED][i];     // Detailed weighted matrix
    }
    else if( p->ph.weighting_quant_param_index == 1 )
    {
        for( i = 0; i < 64; i++ ) 
            p->cur_wq_matrix[i] = p->wq_matrix[UNDETAILED][i];     // unDetailed weighted matrix
    }
    
    if( p->ph.weighting_quant_param_index == 2 )
    {
        for( i = 0; i < 64; i++ ) 
            p->cur_wq_matrix[i] = p->wq_matrix[DETAILED][i];     // Detailed weighted matrix
    }

    // CalculateQuant8Param();
}


#endif




static int cavs_free_resource(cavs_decoder *p)
{
    int i, i_edge;
    
    XAVS_SAFE_FREE(p->p_top_qp);
    XAVS_SAFE_FREE(p->p_top_mv[0]);
    XAVS_SAFE_FREE(p->p_top_mv[1]);
    XAVS_SAFE_FREE(p->p_top_intra_pred_mode_y);
    XAVS_SAFE_FREE(p->p_mb_type_top);
    XAVS_SAFE_FREE(p->p_chroma_pred_mode_top);
    XAVS_SAFE_FREE(p->p_cbp_top);
    XAVS_SAFE_FREE(p->p_ref_top);
    XAVS_SAFE_FREE(p->p_top_border_y);
    XAVS_SAFE_FREE(p->p_top_border_cb);
    XAVS_SAFE_FREE(p->p_top_border_cr);
    XAVS_SAFE_FREE(p->p_col_mv);
    XAVS_SAFE_FREE(p->p_col_type_base);
    XAVS_SAFE_FREE(p->p_block);

#if THREADS_OPT
    if( p->param.b_accelerate )
    {
        XAVS_SAFE_FREE(p->run_buf_tab);
        XAVS_SAFE_FREE(p->level_buf_tab);
        XAVS_SAFE_FREE(p->num_buf_tab);

        XAVS_SAFE_FREE(p->i_mb_type_tab);
        XAVS_SAFE_FREE(p->i_qp_tab);
        XAVS_SAFE_FREE(p->i_cbp_tab);
        XAVS_SAFE_FREE(p->mv_tab);
        XAVS_SAFE_FREE(p->i_intra_pred_mode_y_tab);
        XAVS_SAFE_FREE(p->i_pred_mode_chroma_tab);
        XAVS_SAFE_FREE(p->p_mvd_tab);
        XAVS_SAFE_FREE(p->p_ref_tab);

#if B_MB_WEIGHTING
        XAVS_SAFE_FREE(p->weighting_prediction_tab);
#endif      
    }    
#endif
    
    i_edge = (p->i_mb_width*XAVS_MB_SIZE + XAVS_EDGE*2)*2*17*2*2;
    if(p->p_edge)
    {
       p->p_edge -= i_edge/2;
       XAVS_SAFE_FREE(p->p_edge);	

    }

    if( p->param.i_thread_num <= 1 && p->param.b_accelerate == 0 )
    {
    	for( i = 0; i < 3; i++ )
    	{
    		if(p->image[i].p_data[0])
    		{
    			p->image[i].p_data[0]-=p->image[i].i_stride[0]*XAVS_EDGE+XAVS_EDGE;
    			XAVS_SAFE_FREE(p->image[i].p_data[0]);
    		}
    	
    		if(p->image[i].p_data[1])
    		{
    			p->image[i].p_data[1]-=p->image[i].i_stride[1]*XAVS_EDGE/2+XAVS_EDGE/2;
    			XAVS_SAFE_FREE(p->image[i].p_data[1]);
    		}
    	
    		if(p->image[i].p_data[2])
    		{
    			p->image[i].p_data[2]-=p->image[i].i_stride[2]*XAVS_EDGE/2+XAVS_EDGE/2;
    			XAVS_SAFE_FREE(p->image[i].p_data[2]);	
    		}
    	
    		memset(&p->image[i], 0, sizeof(xavs_image));
    	}
    }

    return 0;
}

static int cavs_alloc_resource( cavs_decoder *p )
{
    uint32_t i;
    uint32_t i_mb_width, i_mb_height, i_edge;
    int b_interlaced = p->param.b_interlaced;
    
    i_mb_width = (p->vsh.i_horizontal_size+15)>>4;
    if ( p->vsh.b_progressive_sequence )
        i_mb_height = (p->vsh.i_vertical_size+15)>>4;
    else
        i_mb_height = ((p->vsh.i_vertical_size+31) & 0xffffffe0)>>4;

    if( i_mb_width == p->i_mb_width && i_mb_height == p->i_mb_height && p->p_block ) /* when p_block is NULL, can't return directly */
    {
        //cavs_decoder_reset(p);//shut down this line to support open gop!
		p->last_delayed_pframe = p->p_save[1]; /* for output delay frame */
        
		return 0;
    }
	else
	{
		if(p->i_mb_width != 0 && p->i_mb_height != 0) // resolution change is not allowed.
		{	
			//memcpy(&p->vsh, &p->old, sizeof(xavs_video_sequence_header)); //set seq info to the origin info.
			memcpy(&p->vsh, &p->old, offsetof(xavs_video_sequence_header, b_flag_aec) - offsetof(xavs_video_sequence_header, i_profile_id)); /*NOTE : don't change aec flag and enable */
			if(p->p_block)
				return -1;
		}
		else
		{
			memcpy(&p->old, &p->vsh, sizeof(xavs_video_sequence_header));
		}
		
		/* need this */
		i_mb_width = (p->vsh.i_horizontal_size+15)>>4;
		if ( p->vsh.b_progressive_sequence )
			i_mb_height = (p->vsh.i_vertical_size+15)>>4;
		else
			i_mb_height = ((p->vsh.i_vertical_size+31) & 0xffffffe0)>>4;
	}

//#if !TEST_START_CODE // FIXIT	
    cavs_free_resource(p);
//#endif

    p->i_mb_width = i_mb_width;
    p->i_mb_height = i_mb_height;
    p->i_mb_num = i_mb_width*i_mb_height;
    p->i_mb_num_half = p->i_mb_num>>1;
    p->p_top_qp = (uint8_t *)cavs_malloc( p->i_mb_width);
    p->b_low_delay = p->vsh.b_low_delay;
    
    p->p_top_mv[0] = (xavs_vector *)cavs_malloc((p->i_mb_width*2+1)*sizeof(xavs_vector));
    p->p_top_mv[1] = (xavs_vector *)cavs_malloc((p->i_mb_width*2+1)*sizeof(xavs_vector));
    p->p_top_intra_pred_mode_y = (int *)cavs_malloc( p->i_mb_width*4*sizeof(*p->p_top_intra_pred_mode_y));
    p->p_mb_type_top = (int8_t *)cavs_malloc(p->i_mb_width*sizeof(int8_t));
    p->p_chroma_pred_mode_top = (int8_t *)cavs_malloc(p->i_mb_width*sizeof(int8_t));
    p->p_cbp_top = (int8_t *)cavs_malloc(p->i_mb_width*sizeof(int8_t));
    p->p_ref_top = (int8_t (*)[2][2])cavs_malloc(p->i_mb_width*2*2*sizeof(int8_t));
    p->p_top_border_y = (uint8_t *)cavs_malloc((p->i_mb_width+1)*XAVS_MB_SIZE);
    p->p_top_border_cb = (uint8_t *)cavs_malloc((p->i_mb_width)*10);
    p->p_top_border_cr = (uint8_t *)cavs_malloc((p->i_mb_width)*10);

    p->p_col_mv = (xavs_vector *)cavs_malloc( p->i_mb_width*(p->i_mb_height)*4*sizeof(xavs_vector));
    p->p_col_type_base = (uint8_t *)cavs_malloc(p->i_mb_width*(p->i_mb_height));
    p->p_block = (DCTELEM *)cavs_malloc(64*sizeof(DCTELEM));

#if THREADS_OPT
    if( p->param.b_accelerate )
    {
        p->level_buf_tab = (int (*)[6][64])cavs_malloc(p->i_mb_num*6*64*sizeof(int));
        p->run_buf_tab = (int (*)[6][64])cavs_malloc(p->i_mb_num*6*64*sizeof(int));
        p->num_buf_tab = (int (*)[6])cavs_malloc(p->i_mb_num*6*sizeof(int));

        p->i_mb_type_tab = (int *)cavs_malloc(p->i_mb_num*sizeof(int));
        p->i_qp_tab = (int *)cavs_malloc(p->i_mb_num*sizeof(int));
        p->i_cbp_tab = (int *)cavs_malloc(p->i_mb_num*sizeof(int));
        p->mv_tab = (xavs_vector (*)[24])cavs_malloc( p->i_mb_num*24*sizeof(xavs_vector));
	
        p->i_intra_pred_mode_y_tab = (int (*)[25])cavs_malloc(p->i_mb_num*25*sizeof(int));
        p->i_pred_mode_chroma_tab = (int *)cavs_malloc(p->i_mb_num*sizeof(int));
        p->p_mvd_tab = (int16_t (*)[2][6][2])cavs_malloc(p->i_mb_num*2*6*2*sizeof(int16_t));
        p->p_ref_tab = (int8_t (*)[2][9])cavs_malloc(p->i_mb_num*2*9*sizeof(int8_t));
#if B_MB_WEIGHTING
        p->weighting_prediction_tab = (int *)cavs_malloc(p->i_mb_num*sizeof(int));
#endif      
    }
#endif

    i_edge = ( p->i_mb_width*XAVS_MB_SIZE + XAVS_EDGE*2 )*2*17*2*2;
    p->p_edge = (uint8_t *)cavs_malloc(i_edge);
    if( p->p_edge )
    {
        memset(p->p_edge, 0, i_edge);
    }
    p->p_edge += i_edge/2;

    if(   !p->p_top_qp                 ||!p->p_top_mv[0]          ||!p->p_top_mv[1]
    	||!p->p_top_intra_pred_mode_y  ||!p->p_top_border_y       ||!p->p_top_border_cb
    	||!p->p_top_border_cr          ||!p->p_col_mv             ||!p->p_col_type_base
    	||!p->p_block                  ||!p->p_edge				  ||!p->p_mb_type_top
    	||!p->p_chroma_pred_mode_top   ||!p->p_cbp_top			  ||!p->p_ref_top)
    {
    	return -1;
    }

    memset(p->p_block, 0, 64*sizeof(DCTELEM));

#if THREADS_OPT /* used for aec to isolate from rec */
    if( p->param.b_accelerate )
    {
        for( i = 0; i < 3; i++ )
        {
            p->image_aec[i].i_colorspace = CAVS_CS_YUV420;
            p->image_aec[i].i_width = p->i_mb_width*XAVS_MB_SIZE;
            p->image_aec[i].i_height = p->i_mb_height*XAVS_MB_SIZE;

            if( !b_interlaced )
            {
                p->image_aec[i].i_stride[0] = p->image_aec[i].i_width + XAVS_EDGE*2;
                p->image_aec[i].i_stride[1] = p->image_aec[i].i_width/2 + XAVS_EDGE;
                p->image_aec[i].i_stride[2] = p->image_aec[i].i_width/2 + XAVS_EDGE;

                p->image_aec[i].p_data[0] = (uint8_t *)cavs_malloc(p->image_aec[i].i_stride[0]*(p->image_aec[i].i_height+XAVS_EDGE*2));
                p->image_aec[i].p_data[0] += p->image_aec[i].i_stride[0]*XAVS_EDGE + XAVS_EDGE;
                
                p->image_aec[i].p_data[1] = (uint8_t *)cavs_malloc(p->image_aec[i].i_stride[1]*(p->image_aec[i].i_height/2+XAVS_EDGE));
                p->image_aec[i].p_data[1] += p->image_aec[i].i_stride[1]*XAVS_EDGE/2 + XAVS_EDGE/2;

                p->image_aec[i].p_data[2] = (uint8_t *)cavs_malloc(p->image_aec[i].i_stride[2]*(p->image_aec[i].i_height/2+XAVS_EDGE));
                p->image_aec[i].p_data[2] += p->image_aec[i].i_stride[2]*XAVS_EDGE/2 + XAVS_EDGE/2;    
            }
            else
            {
                p->image_aec[i].i_stride[0] = (p->image_aec[i].i_width + XAVS_EDGE*2)<<b_interlaced;
                p->image_aec[i].i_stride[1] = (p->image_aec[i].i_width/2 + XAVS_EDGE)<<b_interlaced;
                p->image_aec[i].i_stride[2] = (p->image_aec[i].i_width/2 + XAVS_EDGE)<<b_interlaced;

                /*Y*/
                p->image_aec[i].p_data[0] = (uint8_t *)cavs_malloc( (p->image_aec[i].i_stride[0]>>b_interlaced)*(p->image_aec[i].i_height+((XAVS_EDGE*2)<<b_interlaced)));
                p->image_aec[i].p_data[0] += p->image_aec[i].i_stride[0]*XAVS_EDGE + XAVS_EDGE;

                /*U*/
                p->image_aec[i].p_data[1] = (uint8_t *)cavs_malloc( (p->image_aec[i].i_stride[1]>>b_interlaced)*(p->image_aec[i].i_height/2+(XAVS_EDGE<<b_interlaced)));            
                p->image_aec[i].p_data[1] += (p->image_aec[i].i_stride[1]*XAVS_EDGE/2) + XAVS_EDGE/2;

                /*V*/
                p->image_aec[i].p_data[2] = (uint8_t *)cavs_malloc( (p->image_aec[i].i_stride[2]>>b_interlaced)*(p->image_aec[i].i_height/2+(XAVS_EDGE<<b_interlaced)));            
                p->image_aec[i].p_data[2] += (p->image_aec[i].i_stride[2]*XAVS_EDGE/2) + XAVS_EDGE/2;
            }
             
            if( !p->image_aec[i].p_data[0] || !p->image_aec[i].p_data[1] || !p->image_aec[i].p_data[2] )
            {
            	return -1;
            }
        }

        p->p_save_aec[0] = &p->image_aec[0];
    }
    
#endif

    for( i = 0; i < 3; i++ )
    {
        p->image[i].i_colorspace = CAVS_CS_YUV420;
        p->image[i].i_width = p->i_mb_width*XAVS_MB_SIZE;
        p->image[i].i_height = p->i_mb_height*XAVS_MB_SIZE;

        if( !b_interlaced )
        {
            p->image[i].i_stride[0] = p->image[i].i_width + XAVS_EDGE*2;
            p->image[i].i_stride[1] = p->image[i].i_width/2 + XAVS_EDGE;
            p->image[i].i_stride[2] = p->image[i].i_width/2 + XAVS_EDGE;

            p->image[i].p_data[0] = (uint8_t *)cavs_malloc(p->image[i].i_stride[0]*(p->image[i].i_height+XAVS_EDGE*2));
            p->image[i].p_data[0] += p->image[i].i_stride[0]*XAVS_EDGE + XAVS_EDGE;

            p->image[i].p_data[1] = (uint8_t *)cavs_malloc(p->image[i].i_stride[1]*(p->image[i].i_height/2+XAVS_EDGE));
            p->image[i].p_data[1] += p->image[i].i_stride[1]*XAVS_EDGE/2 + XAVS_EDGE/2;

            p->image[i].p_data[2] = (uint8_t *)cavs_malloc(p->image[i].i_stride[2]*(p->image[i].i_height/2+XAVS_EDGE));
            p->image[i].p_data[2] += p->image[i].i_stride[2]*XAVS_EDGE/2 + XAVS_EDGE/2;    
        }
        else
        {
            p->image[i].i_stride[0] = (p->image[i].i_width + XAVS_EDGE*2)<<b_interlaced;
            p->image[i].i_stride[1] = (p->image[i].i_width/2 + XAVS_EDGE)<<b_interlaced;
            p->image[i].i_stride[2] = (p->image[i].i_width/2 + XAVS_EDGE)<<b_interlaced;

            /*Y*/
            p->image[i].p_data[0] = (uint8_t *)cavs_malloc( (p->image[i].i_stride[0]>>b_interlaced)*(p->image[i].i_height+((XAVS_EDGE*2)<<b_interlaced)));
            p->image[i].p_data[0] += p->image[i].i_stride[0]*XAVS_EDGE + XAVS_EDGE;

            /*U*/
            p->image[i].p_data[1] = (uint8_t *)cavs_malloc( (p->image[i].i_stride[1]>>b_interlaced)*(p->image[i].i_height/2+(XAVS_EDGE<<b_interlaced)));            
            p->image[i].p_data[1] += (p->image[i].i_stride[1]*XAVS_EDGE/2) + XAVS_EDGE/2;

            /*V*/
            p->image[i].p_data[2] = (uint8_t *)cavs_malloc( (p->image[i].i_stride[2]>>b_interlaced)*(p->image[i].i_height/2+(XAVS_EDGE<<b_interlaced)));            
            p->image[i].p_data[2] += (p->image[i].i_stride[2]*XAVS_EDGE/2) + XAVS_EDGE/2;
        }
         
        if( !p->image[i].p_data[0] || !p->image[i].p_data[1] || !p->image[i].p_data[2] )
        {
        	return -1;
        }
    }

    p->p_save[0] = &p->image[0];


    /*     
       0:    D3  B2  B3  C2
       4:    A1  X0  X1   -
       8:    A3  X2  X3   -
    */
    p->mv[ 7] = MV_NOT_AVAIL; 
    p->mv[19] = MV_NOT_AVAIL;

#if THREADS_OPT
    if( p->param.b_accelerate )
    {
        for( i = 0;i < p->i_mb_num; i++)
        {
            p->mv_tab[i][ 7] = MV_NOT_AVAIL; 
            p->mv_tab[i][19] = MV_NOT_AVAIL;
        }
    }

#endif

#if B_MB_WEIGHTING
    /*creat a temp buffer for weighting prediction */
	if( !b_interlaced )
    {
        p->mb_line_buffer[0] = (uint8_t *)cavs_malloc(p->image[0].i_stride[0]*(p->image[0].i_height+XAVS_EDGE*2));
        p->mb_line_buffer[0] += p->image[0].i_stride[0]*XAVS_EDGE + XAVS_EDGE;

        p->mb_line_buffer[1] = (uint8_t *)cavs_malloc(p->image[0].i_stride[1]*(p->image[0].i_height/2+XAVS_EDGE));
        p->mb_line_buffer[1] += p->image[0].i_stride[1]*XAVS_EDGE/2 + XAVS_EDGE/2;

        p->mb_line_buffer[2] = (uint8_t *)cavs_malloc(p->image[0].i_stride[2]*(p->image[0].i_height/2+XAVS_EDGE));
        p->mb_line_buffer[2] += p->image[0].i_stride[2]*XAVS_EDGE/2 + XAVS_EDGE/2;    
    }
    else
    {
        /*Y*/
        p->mb_line_buffer[0] = (uint8_t *)cavs_malloc( (p->image[0].i_stride[0]>>b_interlaced)*(p->image[0].i_height+((XAVS_EDGE*2)<<b_interlaced)));
        p->mb_line_buffer[0] += p->image[0].i_stride[0]*XAVS_EDGE + XAVS_EDGE;

        /*U*/
        p->mb_line_buffer[1] = (uint8_t *)cavs_malloc( (p->image[0].i_stride[1]>>b_interlaced)*(p->image[0].i_height/2+(XAVS_EDGE<<b_interlaced)));            
        p->mb_line_buffer[1] += (p->image[0].i_stride[1]*XAVS_EDGE/2) + XAVS_EDGE/2;

        /*V*/
        p->mb_line_buffer[2] = (uint8_t *)cavs_malloc( (p->image[0].i_stride[2]>>b_interlaced)*(p->image[0].i_height/2+(XAVS_EDGE<<b_interlaced)));            
        p->mb_line_buffer[2] += (p->image[0].i_stride[2]*XAVS_EDGE/2) + XAVS_EDGE/2;
    }
#endif

    /* use to reset for different seq header */
    cavs_decoder_reset(p);

    return 0;
}


static void copy_mvs(xavs_vector *mv, enum xavs_block_size size) 
{
    switch(size)
    {
    case BLK_16X16:
        mv[MV_STRIDE  ] = mv[0];
        mv[MV_STRIDE+1] = mv[0];
    case BLK_16X8:
        mv[1] = mv[0];
        break;
    case BLK_8X16:
        mv[MV_STRIDE] = mv[0];
        break;
    default:
    	break;
    }
}

static void cavs_image_frame_to_field(xavs_image *p_dst, xavs_image *p_src, uint32_t b_bottom, uint32_t b_next_field)
{
    memcpy(p_dst, p_src, sizeof(xavs_image));   
    if( b_bottom )
    {
        p_dst->p_data[0] += (p_dst->i_stride[0]>>1);
        p_dst->p_data[1] += (p_dst->i_stride[1]>>1);
        p_dst->p_data[2] += (p_dst->i_stride[2]>>1);      
    }

    p_dst->i_distance_index = p_src->i_distance_index + b_next_field;
}

#if THREADS_OPT
static void init_ref_list_for_aec(cavs_decoder *p,uint32_t b_next_field)
{
    int i;
    uint32_t b_top_field_first = p->ph.b_top_field_first;
    uint32_t b_bottom = (b_top_field_first && b_next_field) || ( !b_top_field_first && !b_next_field);
	
    for( i = 0; i < 4; i++ )
    {
        p->i_ref_distance[0] = 0;
        memset(&p->ref_aec[i], 0, sizeof(xavs_image));
    }
    
    p->p_save_aec[0]->b_picture_structure = p->ph.b_picture_structure;
    if(p->p_save_aec[1])
    {
        p->p_save_aec[0]->i_ref_distance[0] = p->p_save_aec[1]->i_distance_index;
    }
    if(p->p_save_aec[2])
    {
        p->p_save_aec[0]->i_ref_distance[1] = p->p_save_aec[2]->i_distance_index;
    }

    if(p->ph.b_picture_structure != 0) /* frame */
    {
        /* NOTE : use p->p_save[0] to init p->cur, especially, point to image memory */
        memcpy(&p->cur_aec, p->p_save_aec[0], sizeof(xavs_image)); 

        if( p->p_save_aec[1] )
        {
            memcpy(&p->ref_aec[0], p->p_save_aec[1], sizeof(xavs_image));
        }

        if( p->p_save_aec[2] )
        {
            memcpy(&p->ref_aec[1], p->p_save_aec[2], sizeof(xavs_image));	
        }

        if( p->ref_aec[0].p_data[0] )
        {	
        	p->i_ref_distance[0] = p->ph.i_picture_coding_type !=XAVS_B_PICTURE
        		?(p->cur_aec.i_distance_index - p->ref_aec[0].i_distance_index + 512) % 512
        		:(p->ref_aec[0].i_distance_index - p->cur_aec.i_distance_index + 512) % 512; 
        }
        
        if( p->ref_aec[1].p_data[0] )
        	p->i_ref_distance[1] = (p->cur_aec.i_distance_index - p->ref_aec[1].i_distance_index + 512) % 512;
    }
    else /* field */
    {
        /* init p_cur */
        cavs_image_frame_to_field(&p->cur_aec, p->p_save_aec[0], b_bottom, b_next_field);
        if( p->cur_aec.i_code_type == XAVS_I_PICTURE && b_next_field )
        {
            cavs_image_frame_to_field(&p->ref_aec[0], p->p_save_aec[0], !b_bottom, 0);	
        }
        else if( p->cur_aec.i_code_type == XAVS_P_PICTURE && p->p_save_aec[1] )
        {
            /* for P frame */
            if(b_next_field)
            {
                cavs_image_frame_to_field( &p->ref_aec[0], p->p_save_aec[0], !b_bottom, 0);
                cavs_image_frame_to_field( &p->ref_aec[1], p->p_save_aec[1], p->p_save_aec[1]->b_top_field_first, 1);	
                cavs_image_frame_to_field(&p->ref_aec[2], p->p_save_aec[1],  !p->p_save_aec[1]->b_top_field_first, 0);	

                if(p->p_save_aec[2])
                {
                    cavs_image_frame_to_field( &p->ref_aec[3], p->p_save_aec[2],p->p_save_aec[2]->b_top_field_first, 1);	
                }
            }
            else
            {
                cavs_image_frame_to_field( &p->ref_aec[0], p->p_save_aec[1], p->p_save_aec[1]->b_top_field_first, 1);	
                cavs_image_frame_to_field( &p->ref_aec[1], p->p_save_aec[1], !p->p_save_aec[1]->b_top_field_first, 0);	
                if(p->p_save_aec[2])
                {
                    cavs_image_frame_to_field( &p->ref_aec[2], p->p_save_aec[2],p->p_save_aec[2]->b_top_field_first, 1);	
                    cavs_image_frame_to_field( &p->ref_aec[3], p->p_save_aec[2], !p->p_save_aec[2]->b_top_field_first, 0);	
                }
            }
        }
        else if(p->cur_aec.i_code_type == XAVS_B_PICTURE && p->p_save_aec[2] && p->p_save_aec[1])
        {
        	cavs_image_frame_to_field( &p->ref_aec[0], p->p_save_aec[1], !p->p_save_aec[1]->b_top_field_first, 0);	
        	cavs_image_frame_to_field( &p->ref_aec[1], p->p_save_aec[1], p->p_save_aec[1]->b_top_field_first, 1);	
        	
        	cavs_image_frame_to_field( &p->ref_aec[2], p->p_save_aec[2], p->p_save_aec[2]->b_top_field_first, 1);	
        	cavs_image_frame_to_field( &p->ref_aec[3], p->p_save_aec[2], !p->p_save_aec[2]->b_top_field_first, 0);	
        }	
        for( i = 0; i < 2; i++ )
        {
            if(p->ref_aec[i].p_data[0])
            {
                p->i_ref_distance[i] = p->ph.i_picture_coding_type != XAVS_B_PICTURE
                	?(p->cur_aec.i_distance_index - p->ref_aec[i].i_distance_index + 512) % 512
                	:(p->ref_aec[i].i_distance_index - p->cur_aec.i_distance_index + 512) % 512; 
            }
        }
        for( i = 0; i < 2; i++)
        {
            if(p->ref_aec[2+i].p_data[0])
            {
                p->i_ref_distance[2+i] = (p->cur_aec.i_distance_index - p->ref_aec[2+i].i_distance_index + 512) % 512;
            }
        }
    }
    
    for( i = 0; i < 4; i++)
    {
        p->i_scale_den[i] = p->i_ref_distance[i] ? 512/p->i_ref_distance[i] : 0;
        if(p->ph.i_picture_coding_type != XAVS_B_PICTURE)
             p->i_direct_den[i] = p->i_ref_distance[i] ? 16384/p->i_ref_distance[i] : 0;
    }
    
    if( p->ph.i_picture_coding_type == XAVS_B_PICTURE) 
    {
        p->i_sym_factor = p->i_ref_distance[0]*p->i_scale_den[1];
    }
}
#endif

static void init_ref_list(cavs_decoder *p,uint32_t b_next_field)
{
    int i;
    uint32_t b_top_field_first = p->ph.b_top_field_first;
    uint32_t b_bottom = (b_top_field_first && b_next_field) || ( !b_top_field_first && !b_next_field);

    for( i = 0; i < 4; i++ )
    {
        p->i_ref_distance[0] = 0;
        memset(&p->ref[i], 0, sizeof(xavs_image));
    }
    
    p->p_save[0]->b_picture_structure = p->ph.b_picture_structure;
    if(p->p_save[1])
    {
        p->p_save[0]->i_ref_distance[0] = p->p_save[1]->i_distance_index;
    }
    if(p->p_save[2])
    {
        p->p_save[0]->i_ref_distance[1] = p->p_save[2]->i_distance_index;
    }

    if(p->ph.b_picture_structure != 0) /* frame */
    {
        /* NOTE : use p->p_save[0] to init p->cur, especially, point to image memory */
        memcpy(&p->cur, p->p_save[0], sizeof(xavs_image)); 

        if( p->p_save[1] )
        {
            memcpy(&p->ref[0], p->p_save[1], sizeof(xavs_image));
        }

        if( p->p_save[2] )
        {
            memcpy(&p->ref[1], p->p_save[2], sizeof(xavs_image));	
        }

        if( p->ref[0].p_data[0] )
        {	
        	p->i_ref_distance[0] = p->ph.i_picture_coding_type !=XAVS_B_PICTURE
        		?(p->cur.i_distance_index - p->ref[0].i_distance_index + 512) % 512
        		:(p->ref[0].i_distance_index - p->cur.i_distance_index + 512) % 512; 
        }
        
        if( p->ref[1].p_data[0] )
        	p->i_ref_distance[1] = (p->cur.i_distance_index - p->ref[1].i_distance_index + 512) % 512;
    }
    else /* field */
    {
        /* init p_cur */
        cavs_image_frame_to_field(&p->cur, p->p_save[0], b_bottom, b_next_field);
        if( p->cur.i_code_type == XAVS_I_PICTURE && b_next_field )
        {
            cavs_image_frame_to_field(&p->ref[0], p->p_save[0], !b_bottom, 0);	
        }
        else if( p->cur.i_code_type == XAVS_P_PICTURE && p->p_save[1] )
        {
            /* for P frame */
            if(b_next_field)
            {
                cavs_image_frame_to_field( &p->ref[0], p->p_save[0], !b_bottom, 0);	
                cavs_image_frame_to_field( &p->ref[1], p->p_save[1], p->p_save[1]->b_top_field_first, 1);	
                cavs_image_frame_to_field(&p->ref[2], p->p_save[1],  !p->p_save[1]->b_top_field_first, 0);
                if(p->p_save[2])
                {
                    cavs_image_frame_to_field( &p->ref[3], p->p_save[2],p->p_save[2]->b_top_field_first, 1);	
                }
            }
            else
            {
                cavs_image_frame_to_field( &p->ref[0], p->p_save[1], p->p_save[1]->b_top_field_first, 1);	
                cavs_image_frame_to_field( &p->ref[1], p->p_save[1], !p->p_save[1]->b_top_field_first, 0);	
                if(p->p_save[2])
                {
                    cavs_image_frame_to_field( &p->ref[2], p->p_save[2],p->p_save[2]->b_top_field_first, 1);	
                    cavs_image_frame_to_field( &p->ref[3], p->p_save[2], !p->p_save[2]->b_top_field_first, 0);	
                }
            }
        }
        else if(p->cur.i_code_type == XAVS_B_PICTURE && p->p_save[2] && p->p_save[1])
        {
        	cavs_image_frame_to_field( &p->ref[0], p->p_save[1], !p->p_save[1]->b_top_field_first, 0);	
        	cavs_image_frame_to_field( &p->ref[1], p->p_save[1], p->p_save[1]->b_top_field_first, 1);	

        	cavs_image_frame_to_field( &p->ref[2], p->p_save[2], p->p_save[2]->b_top_field_first, 1);	
        	cavs_image_frame_to_field( &p->ref[3], p->p_save[2], !p->p_save[2]->b_top_field_first, 0);	
        }	
        for( i = 0; i < 2; i++ )
        {
            if(p->ref[i].p_data[0])
            {
                p->i_ref_distance[i] = p->ph.i_picture_coding_type != XAVS_B_PICTURE
                	?(p->cur.i_distance_index - p->ref[i].i_distance_index + 512) % 512
                	:(p->ref[i].i_distance_index - p->cur.i_distance_index + 512) % 512; 
            }
        }
        for( i = 0; i < 2; i++)
        {
            if(p->ref[2+i].p_data[0])
            {
                p->i_ref_distance[2+i] = (p->cur.i_distance_index - p->ref[2+i].i_distance_index + 512) % 512;
            }
        }
    }
    
    for( i = 0; i < 4; i++)
    {
        p->i_scale_den[i] = p->i_ref_distance[i] ? 512/p->i_ref_distance[i] : 0;
        if(p->ph.i_picture_coding_type != XAVS_B_PICTURE)
             p->i_direct_den[i] = p->i_ref_distance[i] ? 16384/p->i_ref_distance[i] : 0;
    }
    
    if( p->ph.i_picture_coding_type == XAVS_B_PICTURE) 
    {
        p->i_sym_factor = p->i_ref_distance[0]*p->i_scale_den[1];
    }
    p->i_luma_offset[0] = 0;
    p->i_luma_offset[1] = 8;
    p->i_luma_offset[2] = 8*p->cur.i_stride[0];
    p->i_luma_offset[3] = p->i_luma_offset[2]+8;	
}

static int cavs_init_picture(cavs_decoder *p)
{
    int i;
    //int b_interlaced = p->param.b_interlaced;
    int b_bottom = p->b_bottom;
    
    for( i = 0; i <= 20; i += 4 )
        p->mv[i] = MV_NOT_AVAIL;
    p->mv[MV_FWD_X0]=p->mv[MV_BWD_X0] = MV_REF_DIR;
    copy_mvs(&p->mv[MV_BWD_X0], BLK_16X16);
    copy_mvs(&p->mv[MV_FWD_X0], BLK_16X16);

    p->i_intra_pred_mode_y[5] = p->i_intra_pred_mode_y[10] = 
    p->i_intra_pred_mode_y[15] = p->i_intra_pred_mode_y[20] = NOT_AVAIL;

    p->i_mb_x = p->i_mb_y = 0;
    p->i_mb_flags = 0;
    p->i_mb_index = 0;

    p->p_save[0]->i_distance_index = p->ph.i_picture_distance*2;
    p->p_save[0]->b_top_field_first = p->ph.b_top_field_first;
    p->p_save[0]->i_code_type = p->ph.i_picture_coding_type;

    init_ref_list(p, b_bottom);

    p->b_fixed_qp = p->ph.b_fixed_picture_qp;
    p->i_qp = p->ph.i_picture_qp;

    if (p->ph.asi_enable)
    {
        p->i_subpel_precision = 3;
        p->i_uint_length = 8;
    }
    else
    {
        p->i_subpel_precision = 2;
        p->i_uint_length = 4;
    }

    if (p->ph.b_aec_enable)
    {
        p->bs_read[SYNTAX_SKIP_RUN] = cavs_cabac_get_skip;
        p->bs_read[SYNTAX_MBTYPE_P] = cavs_cabac_get_mb_type_p;
        p->bs_read[SYNTAX_MBTYPE_B] = cavs_cabac_get_mb_type_b;
        p->bs_read[SYNTAX_INTRA_LUMA_PRED_MODE] = cavs_cabac_get_intra_luma_pred_mode;
        p->bs_read[SYNTAX_INTRA_CHROMA_PRED_MODE] = cavs_cabac_get_intra_chroma_pred_mode;
        p->bs_read[SYNTAX_INTRA_CBP] = cavs_cabac_get_cbp;
        p->bs_read[SYNTAX_INTER_CBP] = cavs_cabac_get_cbp;
        p->bs_read[SYNTAX_DQP] = cavs_cabac_get_dqp;
        p->bs_read[SYNTAX_MB_PART_TYPE] = cavs_cabac_get_mb_part_type;
        p->bs_read_mvd = cavs_cabac_get_mvd;
        p->bs_read_ref_p = cavs_cabac_get_ref_p;
        p->bs_read_ref_b = cavs_cabac_get_ref_b;
        p->bs_read_coeffs = cavs_cabac_get_coeffs;
    }
    else
    {
        p->bs_read[SYNTAX_SKIP_RUN] = cavs_cavlc_get_ue;
        p->bs_read[SYNTAX_MBTYPE_P] = cavs_cavlc_get_mb_type_p;
        p->bs_read[SYNTAX_MBTYPE_B] = cavs_cavlc_get_mb_type_b;
        p->bs_read[SYNTAX_INTRA_LUMA_PRED_MODE] = cavs_cavlc_get_intra_luma_pred_mode;
        p->bs_read[SYNTAX_INTRA_CHROMA_PRED_MODE] = cavs_cavlc_get_ue;
        p->bs_read[SYNTAX_INTRA_CBP] = cavs_cavlc_get_intra_cbp;
        p->bs_read[SYNTAX_INTER_CBP] = cavs_cavlc_get_inter_cbp;
        p->bs_read[SYNTAX_DQP] = cavs_cavlc_get_se;
        p->bs_read[SYNTAX_MB_PART_TYPE] = cavs_cavlc_get_mb_part_type;
        p->bs_read_mvd = cavs_cavlc_get_mvd;
        p->bs_read_ref_p = cavs_cavlc_get_ref_p;
        p->bs_read_ref_b = cavs_cavlc_get_ref_b;
        p->bs_read_coeffs = cavs_cavlc_get_coeffs;
    }
    
    return 0;
}

#if THREADS_OPT

static int cavs_init_picture_context( cavs_decoder *p )
{
    int i;
    int b_interlaced = p->param.b_interlaced;
    int b_bottom = p->b_bottom;

#if THREADS_OPT
    uint32_t k;

    for( k = (b_interlaced && b_bottom) ? p->i_mb_num_half : 0 ; k < p->i_mb_num; k++ ) /*when field, only init bottom.top has occupied by data yet*/
    {
        for( i = 0; i <= 20; i += 4 )
            p->mv_tab[k][i] = MV_NOT_AVAIL;
        p->mv_tab[k][MV_FWD_X0]=p->mv_tab[k][MV_BWD_X0] = MV_REF_DIR;
        copy_mvs(&p->mv_tab[k][MV_BWD_X0], BLK_16X16);
        copy_mvs(&p->mv_tab[k][MV_FWD_X0], BLK_16X16);

        p->i_intra_pred_mode_y_tab[k][5] = p->i_intra_pred_mode_y_tab[k][10] = 
        p->i_intra_pred_mode_y_tab[k][15] = p->i_intra_pred_mode_y_tab[k][20] = NOT_AVAIL;
    }
    
    for( i = 0; i <= 20; i += 4 )
        p->mv[i] = MV_NOT_AVAIL;
    p->mv[MV_FWD_X0]=p->mv[MV_BWD_X0] = MV_REF_DIR;
    copy_mvs(&p->mv[MV_BWD_X0], BLK_16X16);
    copy_mvs(&p->mv[MV_FWD_X0], BLK_16X16);

    p->i_intra_pred_mode_y[5] = p->i_intra_pred_mode_y[10] = 
    p->i_intra_pred_mode_y[15] = p->i_intra_pred_mode_y[20] = NOT_AVAIL;
    
#else    
    for( i = 0; i <= 20; i += 4 )
        p->mv[i] = MV_NOT_AVAIL;
    p->mv[MV_FWD_X0]=p->mv[MV_BWD_X0] = MV_REF_DIR;
    copy_mvs(&p->mv[MV_BWD_X0], BLK_16X16);
    copy_mvs(&p->mv[MV_FWD_X0], BLK_16X16);

    p->i_intra_pred_mode_y[5] = p->i_intra_pred_mode_y[10] = 
    p->i_intra_pred_mode_y[15] = p->i_intra_pred_mode_y[20] = NOT_AVAIL;
#endif

    p->i_mb_x = p->i_mb_y = 0;
    p->i_mb_flags = 0;
    p->i_mb_index = 0;

 #if THREADS_OPT
	if( p->p_save_aec[0] == NULL )
		return CAVS_ERROR;
    p->p_save_aec[0]->i_distance_index = p->ph.i_picture_distance*2;
    p->p_save_aec[0]->b_top_field_first = p->ph.b_top_field_first;
    p->p_save_aec[0]->i_code_type = p->ph.i_picture_coding_type;
 #endif
    
    p->b_fixed_qp = p->ph.b_fixed_picture_qp;
    p->i_qp = p->ph.i_picture_qp;

    if (p->ph.asi_enable)
    {
        p->i_subpel_precision = 3;
        p->i_uint_length = 8;
    }
    else
    {
        p->i_subpel_precision = 2;
        p->i_uint_length = 4;
    }

    if (p->ph.b_aec_enable)
    {
        p->bs_read[SYNTAX_SKIP_RUN] = cavs_cabac_get_skip;
        p->bs_read[SYNTAX_MBTYPE_P] = cavs_cabac_get_mb_type_p;
        p->bs_read[SYNTAX_MBTYPE_B] = cavs_cabac_get_mb_type_b;
        p->bs_read[SYNTAX_INTRA_LUMA_PRED_MODE] = cavs_cabac_get_intra_luma_pred_mode;
        p->bs_read[SYNTAX_INTRA_CHROMA_PRED_MODE] = cavs_cabac_get_intra_chroma_pred_mode;
        p->bs_read[SYNTAX_INTRA_CBP] = cavs_cabac_get_cbp;
        p->bs_read[SYNTAX_INTER_CBP] = cavs_cabac_get_cbp;
        p->bs_read[SYNTAX_DQP] = cavs_cabac_get_dqp;
        p->bs_read[SYNTAX_MB_PART_TYPE] = cavs_cabac_get_mb_part_type;
        p->bs_read_mvd = cavs_cabac_get_mvd;
        p->bs_read_ref_p = cavs_cabac_get_ref_p;
        p->bs_read_ref_b = cavs_cabac_get_ref_b;
        p->bs_read_coeffs = cavs_cabac_get_coeffs;
    }
    else
    {
        p->bs_read[SYNTAX_SKIP_RUN] = cavs_cavlc_get_ue;
        p->bs_read[SYNTAX_MBTYPE_P] = cavs_cavlc_get_mb_type_p;
        p->bs_read[SYNTAX_MBTYPE_B] = cavs_cavlc_get_mb_type_b;
        p->bs_read[SYNTAX_INTRA_LUMA_PRED_MODE] = cavs_cavlc_get_intra_luma_pred_mode;
        p->bs_read[SYNTAX_INTRA_CHROMA_PRED_MODE] = cavs_cavlc_get_ue;
        p->bs_read[SYNTAX_INTRA_CBP] = cavs_cavlc_get_intra_cbp;
        p->bs_read[SYNTAX_INTER_CBP] = cavs_cavlc_get_inter_cbp;
        p->bs_read[SYNTAX_DQP] = cavs_cavlc_get_se;
        p->bs_read[SYNTAX_MB_PART_TYPE] = cavs_cavlc_get_mb_part_type;
        p->bs_read_mvd = cavs_cavlc_get_mvd;
        p->bs_read_ref_p = cavs_cavlc_get_ref_p;
        p->bs_read_ref_b = cavs_cavlc_get_ref_b;
        p->bs_read_coeffs = cavs_cavlc_get_coeffs;
    }

    return 0;
}

static int cavs_init_picture_context_frame( cavs_decoder *p )
{
    //int b_interlaced = p->param.b_interlaced;
    //int b_bottom = p->b_bottom;

    p->p_save[0]->i_distance_index = p->ph.i_picture_distance*2;
    p->p_save[0]->b_top_field_first = p->ph.b_top_field_first;
    p->p_save[0]->i_code_type = p->ph.i_picture_coding_type;
    p->i_mb_x = p->i_mb_y = 0;
    p->i_mb_flags = 0;
    p->i_mb_index = 0;
    
    return 0;
}

static int cavs_init_picture_context_fld( cavs_decoder *p )
{
    //int b_interlaced = p->param.b_interlaced;
    //int b_bottom = p->b_bottom;

    p->p_save[0]->i_distance_index = p->ph.i_picture_distance*2;
    p->p_save[0]->b_top_field_first = p->ph.b_top_field_first;
    p->p_save[0]->i_code_type = p->ph.i_picture_coding_type;
    p->i_mb_x = p->i_mb_y = 0;
    p->i_mb_flags = 0;
    p->i_mb_index = 0;
    
    return 0;
}

static int cavs_init_picture_ref_list( cavs_decoder *p )
{
    //int b_interlaced = p->param.b_interlaced;
    int b_bottom = p->b_bottom;
    
    init_ref_list(p, b_bottom);

    return 0;
}

static int cavs_init_picture_ref_list_for_aec( cavs_decoder *p )
{
    //int b_interlaced = p->param.b_interlaced;
    int b_bottom = p->b_bottom;

    init_ref_list_for_aec(p, b_bottom);

    return 0;
}
#endif

static int cavs_init_slice(cavs_decoder *p)
{
    int i_y;
    
    p->i_mb_offset = ( !p->ph.b_picture_structure && p->i_mb_index >= p->i_mb_num_half ) ? (p->i_mb_height>>1) : 0;
    i_y = p->i_mb_y - p->i_mb_offset;
    if( p->i_mb_offset )
    {
        init_ref_list(p, 1); /* init ref list of bottom-field */
    }
    
    memset( p->p_top_intra_pred_mode_y, NOT_AVAIL, p->i_mb_width*4*sizeof(*p->p_top_intra_pred_mode_y) );
    p->p_y = p->cur.p_data[0] + i_y*XAVS_MB_SIZE*p->cur.i_stride[0];
    p->p_cb = p->cur.p_data[1] + i_y*(XAVS_MB_SIZE>>1)*p->cur.i_stride[1];
    p->p_cr = p->cur.p_data[2] + i_y*(XAVS_MB_SIZE>>1)*p->cur.i_stride[2];

#if B_MB_WEIGHTING
    p->p_back_y = p->mb_line_buffer[0] + i_y*XAVS_MB_SIZE*p->cur.i_stride[0];
    p->p_back_cb = p->mb_line_buffer[1] + i_y*(XAVS_MB_SIZE>>1)*p->cur.i_stride[1];
    p->p_back_cr = p->mb_line_buffer[2] + i_y*(XAVS_MB_SIZE>>1)*p->cur.i_stride[2];
#endif

    p->i_mb_flags = 0;
    p->i_mb_x = 0;
    p->b_first_line = 1;
    p->b_error_flag = 0;
#if B_MB_WEIGHTING
	p->weighting_prediction = 0;
#endif

    if (p->ph.b_aec_enable)
    {
        cavs_bitstream_align(&p->s);
        cavs_cabac_context_init(p);
        cavs_cabac_start_decoding(&p->cabac, &p->s);

        /*syntax context init*/
        memset(p->p_mb_type_top, NOT_AVAIL, p->i_mb_width*sizeof(int8_t));
        memset(p->p_chroma_pred_mode_top, 0, p->i_mb_width*sizeof(int8_t));
        memset(p->p_cbp_top, -1, p->i_mb_width*sizeof(int8_t));
        memset(p->p_ref_top, NOT_AVAIL, p->i_mb_width*2*2*sizeof(int8_t));
        p->i_last_dqp = 0;
        p->i_mb_type_left = -1;
    }
    
    return 0;
}

#if THREADS_OPT

static int cavs_init_slice_for_aec(cavs_decoder *p)
{
    int i_y;
    
    p->i_mb_offset = ( !p->ph.b_picture_structure && p->i_mb_index >= p->i_mb_num_half ) ? (p->i_mb_height>>1) : 0;
    i_y = p->i_mb_y - p->i_mb_offset;
    //if( p->i_mb_offset )
    //{
    //    init_ref_list_for_aec(p, 1); /* init ref list of bottom-field */
    //}
    
    memset( p->p_top_intra_pred_mode_y, NOT_AVAIL, p->i_mb_width*4*sizeof(*p->p_top_intra_pred_mode_y) );
    p->p_y = p->cur.p_data[0] + i_y*XAVS_MB_SIZE*p->cur.i_stride[0];
    p->p_cb = p->cur.p_data[1] + i_y*(XAVS_MB_SIZE>>1)*p->cur.i_stride[1];
    p->p_cr = p->cur.p_data[2] + i_y*(XAVS_MB_SIZE>>1)*p->cur.i_stride[2];

#if B_MB_WEIGHTING
    p->p_back_y = p->mb_line_buffer[0] + i_y*XAVS_MB_SIZE*p->cur.i_stride[0];
    p->p_back_cb = p->mb_line_buffer[1] + i_y*(XAVS_MB_SIZE>>1)*p->cur.i_stride[1];
    p->p_back_cr = p->mb_line_buffer[2] + i_y*(XAVS_MB_SIZE>>1)*p->cur.i_stride[2];
#endif

    p->i_mb_flags = 0;
    p->i_mb_x = 0;
    p->b_first_line = 1;
    p->b_error_flag = 0;
#if B_MB_WEIGHTING	
	p->weighting_prediction = 0;
#endif

    if (p->ph.b_aec_enable)
    {
        cavs_bitstream_align(&p->s);
        cavs_cabac_context_init(p);
        cavs_cabac_start_decoding(&p->cabac, &p->s);

        /*syntax context init*/
        memset(p->p_mb_type_top, NOT_AVAIL, p->i_mb_width*sizeof(int8_t));
        memset(p->p_chroma_pred_mode_top, 0, p->i_mb_width*sizeof(int8_t));
        memset(p->p_cbp_top, -1, p->i_mb_width*sizeof(int8_t));
        memset(p->p_ref_top, NOT_AVAIL, p->i_mb_width*2*2*sizeof(int8_t));
        p->i_last_dqp = 0;
        p->i_mb_type_left = -1;
    }
    
    return 0;
}

static int cavs_init_slice_for_rec(cavs_decoder *p)
{
    int i_y;

    p->i_mb_offset = ( !p->ph.b_picture_structure && p->i_mb_index >= p->i_mb_num_half ) ? (p->i_mb_height>>1) : 0;
    i_y = p->i_mb_y - p->i_mb_offset;
    //if( p->i_mb_offset )
    //{
        //init_ref_list(p, 1); /* init ref list of bottom-field */
    //}
    
    memset( p->p_top_intra_pred_mode_y, NOT_AVAIL, p->i_mb_width*4*sizeof(*p->p_top_intra_pred_mode_y) );
    p->p_y = p->cur.p_data[0] + i_y*XAVS_MB_SIZE*p->cur.i_stride[0];
    p->p_cb = p->cur.p_data[1] + i_y*(XAVS_MB_SIZE>>1)*p->cur.i_stride[1];
    p->p_cr = p->cur.p_data[2] + i_y*(XAVS_MB_SIZE>>1)*p->cur.i_stride[2];

#if B_MB_WEIGHTING
    p->p_back_y = p->mb_line_buffer[0] + i_y*XAVS_MB_SIZE*p->cur.i_stride[0];
    p->p_back_cb = p->mb_line_buffer[1] + i_y*(XAVS_MB_SIZE>>1)*p->cur.i_stride[1];
    p->p_back_cr = p->mb_line_buffer[2] + i_y*(XAVS_MB_SIZE>>1)*p->cur.i_stride[2];
#endif

    p->i_mb_flags = 0;
    p->i_mb_x = 0;
    p->b_first_line = 1;
    p->b_error_flag = 0;

    return 0;
}
#endif

/**
 * in-loop deblocking filter for a single macroblock
 *
 * boundary strength (bs) mapping:
 *
 * --4---5--
 * 0   2   |
 * | 6 | 7 |
 * 1   3   |
 * ---------
 *
 */
static inline int get_bs(xavs_vector *mvP, xavs_vector *mvQ, int b, int i_unit_length) 
{
    if((mvP->ref == REF_INTRA) || (mvQ->ref == REF_INTRA))
        return 2;
    if( mvP->ref != mvQ->ref || (abs(mvP->x - mvQ->x) >= i_unit_length) ||  (abs(mvP->y - mvQ->y) >= i_unit_length) )
        return 1;
    if(b)
	{
        mvP += MV_BWD_OFFS;
        mvQ += MV_BWD_OFFS;
        if( mvP->ref != mvQ->ref || (abs(mvP->x - mvQ->x) >= i_unit_length) ||  (abs(mvP->y - mvQ->y) >= i_unit_length) )
            return 1;
    }/*else{
        if(mvP->ref != mvQ->ref)
            return 1;
    }*/
    return 0;
}

#if 0
static void get_block(cavs_decoder *p,uint8_t *dst,int stride,int block)
{
    int i;
    uint8_t data[8][8];

    for( i = 0; i < 8; i++)
    {
        data[i][0] = dst[i + 0*stride];
        data[i][1] = dst[i + 1*stride];
        data[i][2] = dst[i + 2*stride];
        data[i][3] = dst[i + 3*stride];
        data[i][4] = dst[i + 4*stride];
        data[i][5] = dst[i + 5*stride];
        data[i][6] = dst[i + 6*stride];
        data[i][7] = dst[i + 7*stride];
    }
}
#endif

static void filter_mb(cavs_decoder *p, int i_mb_type) 
{
    uint8_t bs[8];
    int qp_avg;
    int qp_avg_c;
    int i,j;
	int qp_avg_cb, qp_avg_cr, imb_type_P8 = (i_mb_type > P_8X8);
	uint32_t stride0 = p->cur.i_stride[0], stride1 = p->cur.i_stride[1], stride2 = p->cur.i_stride[2];

    /* save c[0] or r[0] */
    p->i_topleft_border_y = p->p_top_border_y[p->i_mb_x*XAVS_MB_SIZE+XAVS_MB_SIZE-1];//0~15
    p->i_topleft_border_cb = p->p_top_border_cb[p->i_mb_x*10+8];//0~9
    p->i_topleft_border_cr = p->p_top_border_cr[p->i_mb_x*10+8];
    memcpy(&p->p_top_border_y[p->i_mb_x*XAVS_MB_SIZE], p->p_y + 15* stride0, 16);
    
    /* point 0 decided by topleft_border */
    memcpy(&p->p_top_border_cb[p->i_mb_x*10+1], p->p_cb +  7*  stride1, 8);
    memcpy(&p->p_top_border_cr[p->i_mb_x*10+1], p->p_cr +  7*  stride2, 8);
    for(i=0,j=0;i<8;i++,j+=2) 
    {
		//j = i << 1;
    	p->i_left_border_y[/*i*2*/j+1] = *(p->p_y + 15 + (/*i*2+0*/j)*stride0);
    	p->i_left_border_y[/*i*2*/j+2] = *(p->p_y + 15 + (/*i*2*/j+1)*stride0);
    	p->i_left_border_cb[i+1] = *(p->p_cb + 7 + i*stride1);
    	p->i_left_border_cr[i+1] = *(p->p_cr + 7 + i*stride2);
    }
    memset(&p->i_left_border_y[17], p->i_left_border_y[16], 9);

    if( p->param.b_accelerate )
    {
        if(!p->ph.b_loop_filter_disable) 
        {
            /* determine bs */
            if(i_mb_type == I_8X8)
            {
                *((uint64_t *)bs) = 0x0202020202020202;
            }
            else
            {
                /** mv motion vector cache
                0:    D3  B2  B3  C2
                4:    A1  X0  X1   -
                8:    A3  X2  X3   -
                imb_type_P8即表示B帧
                bs[8]一共8个元素分别表示最多以4个8*8的块分割的4个垂直边界和4个水平边界，每个边界是8个象素
                对于16*8或者8*16应该考虑其是否具有中间分割的两个水平和垂直边界
                0，1，4，5应该是宏块边界上的2个水平和垂直边界
                而2，3是宏块内垂直边界而，6，7是宏块内水平边界
                */
                *((uint64_t *)bs) = 0;
                if(partition_flags[i_mb_type] & SPLITV)
            	{
                    bs[2] = get_bs(&p->mv[MV_FWD_X0], &p->mv[MV_FWD_X1], imb_type_P8, p->i_uint_length);
                    bs[3] = get_bs(&p->mv[MV_FWD_X2], &p->mv[MV_FWD_X3], imb_type_P8, p->i_uint_length);
                }
                if(partition_flags[i_mb_type] & SPLITH)
            	{
                    bs[6] = get_bs(&p->mv[MV_FWD_X0], &p->mv[MV_FWD_X2], imb_type_P8, p->i_uint_length);
                    bs[7] = get_bs(&p->mv[MV_FWD_X1], &p->mv[MV_FWD_X3], imb_type_P8, p->i_uint_length);
                }
                bs[0] = get_bs(&p->mv[MV_FWD_A1], &p->mv[MV_FWD_X0], imb_type_P8, p->i_uint_length);
                bs[1] = get_bs(&p->mv[MV_FWD_A3], &p->mv[MV_FWD_X2], imb_type_P8, p->i_uint_length);
                bs[4] = get_bs(&p->mv[MV_FWD_B2], &p->mv[MV_FWD_X0], imb_type_P8, p->i_uint_length);
                bs[5] = get_bs(&p->mv[MV_FWD_B3], &p->mv[MV_FWD_X1], imb_type_P8, p->i_uint_length);
            }
            if( *((uint64_t *)bs) ) 
            {
            	if(p->i_mb_flags & A_AVAIL) 
            	{
            	   /* MB verticl edge */
                    qp_avg = (p->i_qp_tab[p->i_mb_index] + p->i_left_qp + 1) >> 1;

                    /* FIXIT : weighting quant qp for chroma
                        NOTE : not modify p->i_qp directly
                    */
                    if( p->b_weight_quant_enable && !p->ph.chroma_quant_param_disable )
                    { 
                        qp_avg_cb = (chroma_qp[clip3_int(p->i_qp_tab[p->i_mb_index] + p->ph.chroma_quant_param_delta_u, 0, 63 )]
                            +  chroma_qp[clip3_int(p->i_left_qp + p->ph.chroma_quant_param_delta_u, 0, 63)] + 1) >>1;
                        qp_avg_cr = (chroma_qp[clip3_int( p->i_qp_tab[p->i_mb_index] + p->ph.chroma_quant_param_delta_v, 0, 63 )]
                            +  chroma_qp[clip3_int( p->i_left_qp + p->ph.chroma_quant_param_delta_v, 0, 63)] + 1) >> 1; 

#if !HAVE_MMX
                        cavs_filter_lv(p, p->p_y, stride0, qp_avg, bs[0], bs[1]);
                        cavs_filter_cv(p, p->p_cb, stride1, qp_avg_cb, bs[0], bs[1]);
                        cavs_filter_cv(p, p->p_cr, stride2, qp_avg_cr, bs[0], bs[1]);
#else
                        p->filter_lv(p, p->p_y, stride0, qp_avg, bs[0], bs[1]);
                        p->filter_cv(p, p->p_cb, stride1, qp_avg_cb, bs[0], bs[1]);
                        p->filter_cv(p, p->p_cr, stride2, qp_avg_cr, bs[0], bs[1]);
#endif
                        
                    }
                    else
                    {
                        qp_avg_c = (chroma_qp[p->i_qp_tab[p->i_mb_index]] + chroma_qp[p->i_left_qp] + 1) >> 1;
                    
#if !HAVE_MMX
                        cavs_filter_lv(p, p->p_y, stride0, qp_avg, bs[0], bs[1]);
                        cavs_filter_cv(p, p->p_cb, stride1, qp_avg_c, bs[0], bs[1]);
                        cavs_filter_cv(p, p->p_cr, stride2, qp_avg_c, bs[0], bs[1]);
#else
                        p->filter_lv(p, p->p_y, stride0, qp_avg, bs[0], bs[1]);
                        p->filter_cv(p, p->p_cb, stride1, qp_avg_c, bs[0], bs[1]);
                        p->filter_cv(p, p->p_cr, stride2, qp_avg_c, bs[0], bs[1]);
#endif
                    }
                }
                qp_avg = p->i_qp_tab[p->i_mb_index];

#if !HAVE_MMX
                cavs_filter_lv(p, p->p_y + 8, stride0, qp_avg, bs[2], bs[3]);
                cavs_filter_lh(p, p->p_y + 8*stride0, stride0, qp_avg, bs[6], bs[7]);
#else
                p->filter_lv(p, p->p_y + 8, stride0, qp_avg, bs[2], bs[3]);
                p->filter_lh(p, p->p_y + 8*stride0, stride0, qp_avg, bs[6], bs[7]);
#endif

                if(p->i_mb_flags & B_AVAIL) 
                {
            	    /* MB horizontal edge */
                    qp_avg = (p->i_qp_tab[p->i_mb_index] + p->p_top_qp[p->i_mb_x] + 1) >> 1;
                    if( p->b_weight_quant_enable && !p->ph.chroma_quant_param_disable )
                    {
                        qp_avg_cb = (chroma_qp[clip3_int( p->i_qp_tab[p->i_mb_index] + p->ph.chroma_quant_param_delta_u,0, 63 )]
                            +  chroma_qp[clip3_int( p->p_top_qp[p->i_mb_x] + p->ph.chroma_quant_param_delta_u, 0, 63 )] + 1) >>1;
                        qp_avg_cr = (chroma_qp[clip3_int(p->i_qp_tab[p->i_mb_index] + p->ph.chroma_quant_param_delta_v ,0, 63 )]
                            +  chroma_qp[clip3_int( p->p_top_qp[p->i_mb_x] + p->ph.chroma_quant_param_delta_v, 0, 63 )] + 1) >> 1;

#if !HAVE_MMX
                    cavs_filter_lh(p, p->p_y, stride0, qp_avg, bs[4], bs[5]);
                    cavs_filter_ch(p, p->p_cb, stride1, qp_avg_cb, bs[4], bs[5]);
                    cavs_filter_ch(p, p->p_cr, stride2, qp_avg_cr, bs[4], bs[5]);
#else
                    p->filter_lh(p, p->p_y, stride0, qp_avg, bs[4], bs[5]);
                    p->filter_ch(p, p->p_cb, stride1, qp_avg_cb, bs[4], bs[5]);
                    p->filter_ch(p, p->p_cr, stride2, qp_avg_cr, bs[4], bs[5]);
#endif
                    }
                    else
                    {
                        qp_avg_c = (chroma_qp[p->i_qp_tab[p->i_mb_index]] + chroma_qp[p->p_top_qp[p->i_mb_x]] + 1) >> 1;

#if !HAVE_MMX
                    cavs_filter_lh(p, p->p_y, stride0, qp_avg, bs[4], bs[5]);
                    cavs_filter_ch(p, p->p_cb, stride1, qp_avg_c, bs[4], bs[5]);
                    cavs_filter_ch(p, p->p_cr, stride2, qp_avg_c, bs[4], bs[5]);
#else
                    p->filter_lh(p, p->p_y, stride0, qp_avg, bs[4], bs[5]);
                    p->filter_ch(p, p->p_cb, stride1, qp_avg_c, bs[4], bs[5]);
                    p->filter_ch(p, p->p_cr, stride2, qp_avg_c, bs[4], bs[5]);
#endif
                    }
                    

                }
            }
        }

        p->i_left_qp = p->i_qp_tab[p->i_mb_index];
        p->p_top_qp[p->i_mb_x] = p->i_qp_tab[p->i_mb_index];
    }
    else
    {
        if(!p->ph.b_loop_filter_disable) 
        {
            /* determine bs */
            if(i_mb_type == I_8X8)
            {
                *((uint64_t *)bs) = 0x0202020202020202;
            }
            else
            {
                /** mv motion vector cache
                0:    D3  B2  B3  C2
                4:    A1  X0  X1   -
                8:    A3  X2  X3   -
                imb_type_P8即表示B帧
                bs[8]一共8个元素分别表示最多以4个8*8的块分割的4个垂直边界和4个水平边界，每个边界是8个象素
                对于16*8或者8*16应该考虑其是否具有中间分割的两个水平和垂直边界
                0，1，4，5应该是宏块边界上的2个水平和垂直边界
                而2，3是宏块内垂直边界而，6，7是宏块内水平边界
                */
                *((uint64_t *)bs) = 0;
                if(partition_flags[i_mb_type] & SPLITV)
            	{
                    bs[2] = get_bs(&p->mv[MV_FWD_X0], &p->mv[MV_FWD_X1], imb_type_P8, p->i_uint_length);
                    bs[3] = get_bs(&p->mv[MV_FWD_X2], &p->mv[MV_FWD_X3], imb_type_P8, p->i_uint_length);
                }
                if(partition_flags[i_mb_type] & SPLITH)
            	{
                    bs[6] = get_bs(&p->mv[MV_FWD_X0], &p->mv[MV_FWD_X2], imb_type_P8, p->i_uint_length);
                    bs[7] = get_bs(&p->mv[MV_FWD_X1], &p->mv[MV_FWD_X3], imb_type_P8, p->i_uint_length);
                }
                bs[0] = get_bs(&p->mv[MV_FWD_A1], &p->mv[MV_FWD_X0], imb_type_P8, p->i_uint_length);
                bs[1] = get_bs(&p->mv[MV_FWD_A3], &p->mv[MV_FWD_X2], imb_type_P8, p->i_uint_length);
                bs[4] = get_bs(&p->mv[MV_FWD_B2], &p->mv[MV_FWD_X0], imb_type_P8, p->i_uint_length);
                bs[5] = get_bs(&p->mv[MV_FWD_B3], &p->mv[MV_FWD_X1], imb_type_P8, p->i_uint_length);
            }
            if( *((uint64_t *)bs) ) 
            {
            	if(p->i_mb_flags & A_AVAIL) 
            	{
            	   /* MB verticl edge */
                    qp_avg = (p->i_qp + p->i_left_qp + 1) >> 1;

                    /* FIXIT : weighting quant qp for chroma
                        NOTE : not modify p->i_qp directly
                    */
                    if( p->b_weight_quant_enable && !p->ph.chroma_quant_param_disable )
                    { 
                        qp_avg_cb = (chroma_qp[clip3_int(p->i_qp + p->ph.chroma_quant_param_delta_u, 0, 63 )]
                            +  chroma_qp[clip3_int(p->i_left_qp + p->ph.chroma_quant_param_delta_u, 0, 63)] + 1) >>1;
                        qp_avg_cr = (chroma_qp[clip3_int( p->i_qp + p->ph.chroma_quant_param_delta_v, 0, 63 )]
                            +  chroma_qp[clip3_int( p->i_left_qp + p->ph.chroma_quant_param_delta_v, 0, 63)] + 1) >> 1; 

#if !HAVE_MMX
                        cavs_filter_lv(p, p->p_y, stride0, qp_avg, bs[0], bs[1]);
                        cavs_filter_cv(p, p->p_cb, stride1, qp_avg_cb, bs[0], bs[1]);
                        cavs_filter_cv(p, p->p_cr, stride2, qp_avg_cr, bs[0], bs[1]);
#else
                        p->filter_lv(p, p->p_y, stride0, qp_avg, bs[0], bs[1]);
                        p->filter_cv(p, p->p_cb, stride1, qp_avg_cb, bs[0], bs[1]);
                        p->filter_cv(p, p->p_cr, stride2, qp_avg_cr, bs[0], bs[1]);
#endif
                        
                    }
                    else
                    {
                        qp_avg_c = (chroma_qp[p->i_qp] + chroma_qp[p->i_left_qp] + 1) >> 1;
                    
#if !HAVE_MMX
                        cavs_filter_lv(p, p->p_y, stride0, qp_avg, bs[0], bs[1]);
                        cavs_filter_cv(p, p->p_cb, stride1, qp_avg_c, bs[0], bs[1]);
                        cavs_filter_cv(p, p->p_cr, stride2, qp_avg_c, bs[0], bs[1]);
#else
                        p->filter_lv(p, p->p_y, stride0, qp_avg, bs[0], bs[1]);
                        p->filter_cv(p, p->p_cb, stride1, qp_avg_c, bs[0], bs[1]);
                        p->filter_cv(p, p->p_cr, stride2, qp_avg_c, bs[0], bs[1]);
#endif
                    }
                }
                qp_avg = p->i_qp;

#if !HAVE_MMX
                cavs_filter_lv(p, p->p_y + 8, stride0, qp_avg, bs[2], bs[3]);
                cavs_filter_lh(p, p->p_y + 8*stride0, stride0, qp_avg, bs[6], bs[7]);
#else
                p->filter_lv(p, p->p_y + 8, stride0, qp_avg, bs[2], bs[3]);
                p->filter_lh(p, p->p_y + 8*stride0, stride0, qp_avg, bs[6], bs[7]);
#endif

                if(p->i_mb_flags & B_AVAIL) 
                {
            	    /* MB horizontal edge */
                    qp_avg = (p->i_qp + p->p_top_qp[p->i_mb_x] + 1) >> 1;
                    if( p->b_weight_quant_enable && !p->ph.chroma_quant_param_disable )
                    {
                        qp_avg_cb = (chroma_qp[clip3_int( p->i_qp + p->ph.chroma_quant_param_delta_u,0, 63 )]
                            +  chroma_qp[clip3_int( p->p_top_qp[p->i_mb_x] + p->ph.chroma_quant_param_delta_u, 0, 63 )] + 1) >>1;
                        qp_avg_cr = (chroma_qp[clip3_int(p->i_qp + p->ph.chroma_quant_param_delta_v ,0, 63 )]
                            +  chroma_qp[clip3_int( p->p_top_qp[p->i_mb_x] + p->ph.chroma_quant_param_delta_v, 0, 63 )] + 1) >> 1;

#if !HAVE_MMX
                    cavs_filter_lh(p, p->p_y, stride0, qp_avg, bs[4], bs[5]);
                    cavs_filter_ch(p, p->p_cb, stride1, qp_avg_cb, bs[4], bs[5]);
                    cavs_filter_ch(p, p->p_cr, stride2, qp_avg_cr, bs[4], bs[5]);
#else
                    p->filter_lh(p, p->p_y, stride0, qp_avg, bs[4], bs[5]);
                    p->filter_ch(p, p->p_cb, stride1, qp_avg_cb, bs[4], bs[5]);
                    p->filter_ch(p, p->p_cr, stride2, qp_avg_cr, bs[4], bs[5]);
#endif
                    }
                    else
                    {
                        qp_avg_c = (chroma_qp[p->i_qp] + chroma_qp[p->p_top_qp[p->i_mb_x]] + 1) >> 1;

#if !HAVE_MMX
                    cavs_filter_lh(p, p->p_y, stride0, qp_avg, bs[4], bs[5]);
                    cavs_filter_ch(p, p->p_cb, stride1, qp_avg_c, bs[4], bs[5]);
                    cavs_filter_ch(p, p->p_cr, stride2, qp_avg_c, bs[4], bs[5]);
#else
                    p->filter_lh(p, p->p_y, stride0, qp_avg, bs[4], bs[5]);
                    p->filter_ch(p, p->p_cb, stride1, qp_avg_c, bs[4], bs[5]);
                    p->filter_ch(p, p->p_cr, stride2, qp_avg_c, bs[4], bs[5]);
#endif
                    }
                    

                }
            }
        }

        p->i_left_qp = p->i_qp;
        p->p_top_qp[p->i_mb_x] = p->i_qp;
        
    }
        
}


#define XAVS_CHROMA_MC(OPNAME, OP,L)\
static UNUSED void  xavs_chroma_mc2_## OPNAME ##_c(uint8_t *dst, uint8_t *src, int stride, int h, int x, int y){\
    const int A=(L-x)*(L-y);\
    const int B=(  x)*(L-y);\
    const int C=(L-x)*(  y);\
    const int D=(  x)*(  y);\
    int i;\
    \
\
    for(i=0; i<h; i++)\
    {\
        OP(dst[0], (A*src[0] + B*src[1] + C*src[stride+0] + D*src[stride+1]));\
        OP(dst[1], (A*src[1] + B*src[2] + C*src[stride+1] + D*src[stride+2]));\
        dst+= stride;\
        src+= stride;\
    }\
}\
\
static UNUSED void xavs_chroma_mc4_## OPNAME ##_c(uint8_t *dst, uint8_t *src, int stride, int h, int x, int y){\
    const int A=(L-x)*(L-y);\
    const int B=(  x)*(L-y);\
    const int C=(L-x)*(  y);\
    const int D=(  x)*(  y);\
    int i;\
    \
\
    for(i=0; i<h; i++)\
    {\
        OP(dst[0], (A*src[0] + B*src[1] + C*src[stride+0] + D*src[stride+1]));\
        OP(dst[1], (A*src[1] + B*src[2] + C*src[stride+1] + D*src[stride+2]));\
        OP(dst[2], (A*src[2] + B*src[3] + C*src[stride+2] + D*src[stride+3]));\
        OP(dst[3], (A*src[3] + B*src[4] + C*src[stride+3] + D*src[stride+4]));\
        dst+= stride;\
        src+= stride;\
    }\
}\
\
static UNUSED void xavs_chroma_mc8_## OPNAME ##_c(uint8_t *dst, uint8_t *src, int stride, int h, int x, int y){\
    const int A=(L-x)*(L-y);\
    const int B=(  x)*(L-y);\
    const int C=(L-x)*(  y);\
    const int D=(  x)*(  y);\
    int i;\
    \
\
    for(i=0; i<h; i++)\
    {\
        OP(dst[0], (A*src[0] + B*src[1] + C*src[stride+0] + D*src[stride+1]));\
        OP(dst[1], (A*src[1] + B*src[2] + C*src[stride+1] + D*src[stride+2]));\
        OP(dst[2], (A*src[2] + B*src[3] + C*src[stride+2] + D*src[stride+3]));\
        OP(dst[3], (A*src[3] + B*src[4] + C*src[stride+3] + D*src[stride+4]));\
        OP(dst[4], (A*src[4] + B*src[5] + C*src[stride+4] + D*src[stride+5]));\
        OP(dst[5], (A*src[5] + B*src[6] + C*src[stride+5] + D*src[stride+6]));\
        OP(dst[6], (A*src[6] + B*src[7] + C*src[stride+6] + D*src[stride+7]));\
        OP(dst[7], (A*src[7] + B*src[8] + C*src[stride+7] + D*src[stride+8]));\
        dst+= stride;\
        src+= stride;\
    }\
}

#define op_avg(a, b) a = (((a)+(((b) + 32)>>6)+1)>>1)
#define op_put(a, b) a = (((b) + 32)>>6)
#define op_avg16(a, b) a = (((a)+(((b) + 128)>>8)+1)>>1)
#define op_put16(a, b) a = (((b) + 128)>>8)

XAVS_CHROMA_MC(put       , op_put,8)
XAVS_CHROMA_MC(avg       , op_avg,8)
XAVS_CHROMA_MC(16put       , op_put16,16)
XAVS_CHROMA_MC(16avg       , op_avg16,16)
#undef op_avg
#undef op_put
#undef op_avg16
#undef op_put16

#define	BYTE_VEC32(c)	((c)*0x01010101UL)
static inline uint32_t rnd_avg32(uint32_t a, uint32_t b)
{
    return (a | b) - (((a ^ b) & ~BYTE_VEC32(0x01)) >> 1);
}

#define LD32(a) (*((uint32_t*)(a)))
#define PUT_PIX(OPNAME,OP) \
static UNUSED void OPNAME ## _pixels8_c(uint8_t *block, const uint8_t *pixels, int line_size, int h){\
    int i;\
    for(i=0; i<h; i++){\
        OP(*((uint32_t*)(block  )), LD32(pixels  ));\
        OP(*((uint32_t*)(block+4)), LD32(pixels+4));\
        pixels+=line_size;\
        block +=line_size;\
    }\
}\
static UNUSED void OPNAME ## _pixels16_c(uint8_t *block, const uint8_t *pixels, int line_size, int h){\
    OPNAME ## _pixels8_c(block  , pixels  , line_size, h);\
    OPNAME ## _pixels8_c(block+8, pixels+8, line_size, h);\
}

#define op_avg(a, b) a = rnd_avg32(a, b)
#define op_put(a, b) a = b
PUT_PIX(put,op_put);
PUT_PIX(avg,op_avg);

#undef op_avg
#undef op_put
#undef LD32

#if !HAVE_MMX // FIXIT
cavs_luma_mc_func xavs_luma_put[4][64];
cavs_luma_mc_func xavs_luma_avg[4][64];
cavs_chroma_mc_func xavs_chroma_put[6];
cavs_chroma_mc_func xavs_chroma_avg[6];
#endif

static void xavs_emulated_edge_mc(uint8_t *buf, uint8_t *src, int linesize, int block_w, int block_h, 
	int src_x, int src_y, int w, int h)
{
    int x, y, kx, ky;
    int start_y, start_x, end_y, end_x;
    
    if(src_y>= h){
        src+= (h-1-src_y)*linesize;
        src_y=h-1;
    }else if(src_y<=-block_h){
        src+= (1-block_h-src_y)*linesize;
        src_y=1-block_h;
    }
    if(src_x>= w){
        src+= (w-1-src_x);
        src_x=w-1;
    }else if(src_x<=-block_w){
        src+= (1-block_w-src_x);
        src_x=1-block_w;
    }

    start_y= XAVS_MAX(0, -src_y);
    start_x= XAVS_MAX(0, -src_x);
    end_y= XAVS_MIN(block_h, h-src_y);
    end_x= XAVS_MIN(block_w, w-src_x);


#if 0
    // copy existing part
    for(y=start_y; y<end_y; y++){
        for(x=start_x; x<end_x; x++){
            buf[x + y*linesize]= src[x + y*linesize];
        }
    }

    //top
    for(y=0; y<start_y; y++){
        for(x=start_x; x<end_x; x++){
            buf[x + y*linesize]= buf[x + start_y*linesize];
        }
    }

    //bottom
    for(y=end_y; y<block_h; y++){
        for(x=start_x; x<end_x; x++){
            buf[x + y*linesize]= buf[x + (end_y-1)*linesize];
        }
    }
#else
    // copy existing part
	kx = end_x - start_x; ky = start_x + start_y*linesize;
    for(y=start_y; y<end_y; y++){
        //for(x=start_x; x<end_x; x++){
        //    buf[x + y*linesize]= src[x + y*linesize];
        //}
        memcpy(  &buf[start_x + y*linesize], &src[start_x + y*linesize], kx/*end_x - start_x*/ );
    }
    
    //top
    for(y=0; y<start_y; y++){
        //for(x=start_x; x<end_x; x++){
        //    buf[x + y*linesize]= buf[x + start_y*linesize];
        //}
        memcpy(  &buf[start_x + y*linesize], &buf[ky/*start_x + start_y*linesize*/], kx/*end_x - start_x*/ );
    }

    //bottom
	ky = start_x + (end_y - 1)*linesize;
    for(y=end_y; y<block_h; y++){
        //for(x=start_x; x<end_x; x++){
        //    buf[x + y*linesize]= buf[x + (end_y-1)*linesize];
        //}
        memcpy( &buf[start_x + y*linesize], &buf[ky/*start_x + (end_y-1)*linesize*/], kx/*end_x - start_x */);
    }

#endif

    for(y=0; y<block_h; y++){
       //left
        for(x=0; x<start_x; x++){
            buf[x + y*linesize]= buf[start_x + y*linesize];
        }

       //right
		kx = end_x - 1 + y*linesize;
        for(x=end_x; x<block_w; x++){
            buf[x + y*linesize]= buf[kx/*end_x - 1 + y*linesize*/];
        }
    }
}


static inline void mc_dir_part(cavs_decoder *p,xavs_image *ref,
                        int chroma_height,int pic_ht/*list*/,uint8_t *dest_y,
                        uint8_t *dest_cb,uint8_t *dest_cr,int src_x_offset,
                        int src_y_offset,cavs_luma_mc_func *luma,
                        cavs_chroma_mc_func chroma,xavs_vector *mv)
{
#if 0
    const int mx = ( p->ph.asi_enable==1 ) ? (mv->x + src_x_offset*16):(mv->x + src_x_offset*8);
    const int my = ( p->ph.asi_enable==1 ) ? (mv->y + src_y_offset*16):(mv->y + src_y_offset*8);
    const int luma_xy = ( p->ph.asi_enable==1 ) ?( (mx&7) + ((my&7)<<3) ):( (mx&3) + ((my&3)<<2) );
    uint8_t * src_y,* src_cb,* src_cr;
    int extra_width = 0; //(s->flags&CODEC_FLAG_EMU_EDGE) ? 0 : 16;
    int extra_height = extra_width;
    int emu = 0;
    const int full_mx = ( p->ph.asi_enable==1 ) ? mx>>3: mx>>2;
    const int full_my = ( p->ph.asi_enable==1 ) ? my>>3: my>>2;
    const int pic_width  = 16*p->i_mb_width;
    const int pic_height = 16*( p->ph.b_picture_structure==0?(p->i_mb_height>>1):p->i_mb_height);
#else
    const int mx = (mv->x + (src_x_offset<<3));
    const int my =(mv->y + (src_y_offset<<3));
    const int luma_xy = ( (mx&3) + ((my&3)<<2) );
    uint8_t * src_y,* src_cb,* src_cr;
    int extra_width = 0; //(s->flags&CODEC_FLAG_EMU_EDGE) ? 0 : 16;
    int extra_height = extra_width;
    int emu = 0;
    const int full_mx = mx>>2;
    const int full_my = my>>2;
    const int pic_width  = p->i_mb_width<<4;
	const int pic_height = pic_ht; //(p->ph.b_picture_structure == 0 ? (p->i_mb_height >> 1) : p->i_mb_height) << 4;
#endif

    //if( p->ph.asi_enable == 1 )
    //{
    //    src_y = ref->p_data[0] + (mx>>3) + (my>>3)*(int)ref->i_stride[0];

    //    src_cb= ref->p_data[1] + (mx>>4) + (my>>4)*(int)ref->i_stride[1];   
    //    src_cr= ref->p_data[2] + (mx>>4) + (my>>4)*(int)ref->i_stride[2];   
    //}
    //else
    {
        src_y = ref->p_data[0] + (full_mx/*mx>>2*/) + (full_my/*my>>2*/)*(int)ref->i_stride[0]; // NOTE : unsigned sign must trans to sign
        src_cb= ref->p_data[1] + (mx>>3) + (my>>3)*(int)ref->i_stride[1];
        src_cr= ref->p_data[2] + (mx>>3) + (my>>3)*(int)ref->i_stride[2];
    }

    if(!ref->p_data[0])
        return;

    if(mx&15) extra_width -= 3;
    if(my&15) extra_height -= 3;


    if(   full_mx < 0-extra_width
          || full_my < 0-extra_height
          || full_mx + 16 > pic_width + extra_width
          || full_my + 16 > pic_height + extra_height)
    {
        xavs_emulated_edge_mc(p->p_edge, src_y - 2 - 2*(int)ref->i_stride[0], ref->i_stride[0],
                            16+9, 16+9, full_mx-2, full_my-2, pic_width, pic_height);
        src_y = p->p_edge + 2 + 2*(int)ref->i_stride[0];
        emu = 1;
    }

    luma[luma_xy](dest_y, src_y, ref->i_stride[0]);

    if(emu)
    {
        //if( p->ph.asi_enable==1 )
        //{
        //    xavs_emulated_edge_mc(p->p_edge, src_cb, ref->i_stride[1],
        //                9, 9, (mx>>4), (my>>4), pic_width>>1, pic_height>>1);
        //}
        //else
        {
            xavs_emulated_edge_mc(p->p_edge, src_cb, ref->i_stride[1],
                        9, 9, (mx>>3), (my>>3), pic_width>>1, pic_height>>1);
        }

        src_cb= p->p_edge;
    }

   // if( p->ph.asi_enable == 1 )
   //     chroma(dest_cb, src_cb, ref->i_stride[1], chroma_height, mx&15, my&15); 
   // else
        chroma(dest_cb, src_cb, ref->i_stride[1], chroma_height, mx&7, my&7);

    if(emu)
    {
       // if( p->ph.asi_enable == 1 )
       // {
       //     xavs_emulated_edge_mc(p->p_edge, src_cr, ref->i_stride[2],
       //                     17, 17, (mx>>4), (my>>4), pic_width>>1, pic_height>>1);
       // }
       // else
        {
            xavs_emulated_edge_mc(p->p_edge, src_cr, ref->i_stride[2],
                            9, 9, (mx>>3), (my>>3), pic_width>>1, pic_height>>1);
        }
        src_cr= p->p_edge;
    }

   // if( p->ph.asi_enable == 1 )
   //     chroma(dest_cr, src_cr, ref->i_stride[2], chroma_height, mx&15, my&15);
   // else
        chroma(dest_cr, src_cr, ref->i_stride[2], chroma_height, mx&7, my&7);
}

static inline void mc_part_std(cavs_decoder *p,int chroma_height,int pic_ht,
                       uint8_t *dest_y,uint8_t *dest_cb,uint8_t *dest_cr,
					   int x_offset, int y_offset,xavs_vector *mv,
					   cavs_luma_mc_func *luma_put,cavs_chroma_mc_func chroma_put,
					   cavs_luma_mc_func *luma_avg,cavs_chroma_mc_func chroma_avg)
{
    cavs_luma_mc_func *lum=  luma_put;
    cavs_chroma_mc_func chroma= chroma_put;

#if B_MB_WEIGHTING
    int i_dir = 0;
	uint8_t *dst_back_y = p->p_back_y, *dst_back_cb = p->p_back_cb, *dst_back_cr = p->p_back_cr;
    int x_offset_back = x_offset;
    int y_offset_back = y_offset;
    int fw_luma_scale, fw_chroma_scale;
    int fw_luma_shift, fw_chroma_shift;
    int bw_luma_scale, bw_chroma_scale;
    int bw_luma_shift, bw_chroma_shift;
    int ii, jj;
	int i_first_field = p->i_mb_index < p->i_mb_num_half;// ? 1 : 0; /* 0 is bottom */
	//int pic_ht = (p->ph.b_picture_structure == 0 ? (p->i_mb_height >> 1) : p->i_mb_height) << 4;

#endif

    dest_y  += (x_offset<<1) + ((y_offset*p->cur.i_stride[0])<<1);
    dest_cb +=   x_offset +   y_offset*p->cur.i_stride[1];
    dest_cr +=   x_offset +   y_offset*p->cur.i_stride[2];
    x_offset += (p->i_mb_x)<<3;
    y_offset += 8*(p->i_mb_y-p->i_mb_offset);

#if B_MB_WEIGHTING
    /* get temp buffer for dest_y, dest_cb, dest_cr */
    /*dst_back_y = p->p_back_y;
    dst_back_cb = p->p_back_cb;
    dst_back_cr = p->p_back_cr;*/

    dst_back_y += (2*x_offset_back) + (2*y_offset_back*p->cur.i_stride[0]); /* set buffer address */
    dst_back_cb += x_offset_back +  y_offset_back*p->cur.i_stride[1];
    dst_back_cr += x_offset_back +  y_offset_back*p->cur.i_stride[2];

    x_offset_back += 8*p->i_mb_x;
    y_offset_back += 8*(p->i_mb_y-p->i_mb_offset);
#endif

#if B_MB_WEIGHTING
	  /* forward */
    if(mv->ref >= 0)
    {
        xavs_image *ref= &p->ref[mv->ref];

        mc_dir_part(p, ref, chroma_height, pic_ht/*0*/, dest_y, dest_cb, dest_cr, x_offset, y_offset,
                    lum, chroma, mv);
        i_dir = 1; /* forward flag */

        /* weighting prediction */
        if( ((p->sh.b_slice_weighting_flag == 1) 
            && (p->sh.b_mb_weighting_flag == 1) && (p->weighting_prediction == 1))
            || ((p->sh.b_slice_weighting_flag == 1) &&  (p->sh.b_mb_weighting_flag == 0)))
            {
                if((p->ph.i_picture_coding_type ==  1) || (i_first_field == 0 && p->ph.i_picture_coding_type ==  0)) /* P-slice */
                {
                    /* scale shift */
                    fw_luma_scale = p->sh.i_luma_scale[mv->ref];
                    fw_luma_shift = p->sh.i_luma_shift[mv->ref];

                    /* luma */
                    for( ii = 0; ii < chroma_height<<1; ii++ )
                    {
                        for( jj = 0; jj < chroma_height<<1; jj++ )
                        {
                            *(dest_y + ii*p->cur.i_stride[0]+ jj) = 
                                xavs_clip_uint8(((  (*(dest_y + ii*p->cur.i_stride[0]+ jj)) *fw_luma_scale+16)>>5) + fw_luma_shift);
                        }
                    }

                    /* scale shift */
                    fw_chroma_scale = p->sh.i_chroma_scale[mv->ref];
                    fw_chroma_shift = p->sh.i_chroma_shift[mv->ref];

                    /* cb */
			        for( ii = 0; ii < chroma_height; ii++ )
                    {
                        for( jj = 0; jj < chroma_height; jj++ )
                        {
                            *(dest_cb + ii*p->cur.i_stride[1]+ jj) = 
                                xavs_clip_uint8(((  (*(dest_cb + ii*p->cur.i_stride[1]+ jj)) *fw_chroma_scale+16)>>5) + fw_chroma_shift);
                        }
                    }

                    /* cr */
					for( ii = 0; ii < chroma_height; ii++ )
                    {
                        for( jj = 0; jj < chroma_height; jj++ )
                        {
                            *(dest_cr + ii*p->cur.i_stride[2]+ jj) = 
                                xavs_clip_uint8(((  (*(dest_cr + ii*p->cur.i_stride[2]+ jj)) *fw_chroma_scale+16)>>5) + fw_chroma_shift);
                        }
                    }
                      
                }
                else if(p->ph.i_picture_coding_type ==  2)/* B-frame */
                {
					int refframe;
					int i_ref_offset = p->ph.b_picture_structure == 0 ? 2 : 1;

					refframe = mv->ref - i_ref_offset;

					refframe= (!p->param.b_interlaced )?(refframe):(2*refframe);

					/* scale shift */
                    fw_luma_scale = p->sh.i_luma_scale[refframe];
                    fw_luma_shift = p->sh.i_luma_shift[refframe];

					/* luma */
					for( ii = 0; ii < chroma_height<<1; ii++ )
                    {
                        for( jj = 0; jj < chroma_height<<1; jj++ )
                        {
                            *(dest_y + ii*p->cur.i_stride[0]+ jj) = 
                                xavs_clip_uint8(((  (*(dest_y + ii*p->cur.i_stride[0]+ jj)) *fw_luma_scale+16)>>5) + fw_luma_shift);
                        }
                    }

					/* cb */
					fw_chroma_scale = p->sh.i_chroma_scale[refframe];
					fw_chroma_shift = p->sh.i_chroma_shift[refframe];
				
					for( ii = 0; ii < chroma_height; ii++ )
                    {
                        for( jj = 0; jj < chroma_height; jj++ )
                        {
                            *(dest_cb + ii*p->cur.i_stride[1]+ jj) = 
                                xavs_clip_uint8(((  (*(dest_cb + ii*p->cur.i_stride[1]+ jj)) *fw_chroma_scale+16)>>5) + fw_chroma_shift);
                        }
                    }

					/* cr */
					for( ii = 0; ii < chroma_height; ii++ )
                    {
                        for( jj = 0; jj < chroma_height; jj++ )
                        {
                            *(dest_cr + ii*p->cur.i_stride[2]+ jj) = 
                                xavs_clip_uint8(((  (*(dest_cr + ii*p->cur.i_stride[2]+ jj)) *fw_chroma_scale+16)>>5) + fw_chroma_shift);
                        }
                    }

                }
            
            }
        
    }

    /* backward */
    if((mv+MV_BWD_OFFS)->ref >= 0)
    {
        xavs_image *ref= &p->ref[(mv+MV_BWD_OFFS)->ref];
        
        if( i_dir == 0 )/* has no forward */
        {
			mc_dir_part(p, ref, chroma_height, pic_ht/*1*/,
                                dest_y, dest_cb, dest_cr, x_offset, y_offset,
                                lum, chroma, mv+MV_BWD_OFFS);

             /* weighting prediction */
			if( ((p->sh.b_slice_weighting_flag == 1) 
				&& (p->sh.b_mb_weighting_flag == 1) && (p->weighting_prediction == 1))
				|| ((p->sh.b_slice_weighting_flag == 1) &&  (p->sh.b_mb_weighting_flag == 0)))
			{
				int refframe;

				refframe = (mv+MV_BWD_OFFS)->ref;

				refframe= (!p->param.b_interlaced )?(refframe+1):(2*refframe+1);

				/* scale shift */
                fw_luma_scale = p->sh.i_luma_scale[refframe];
                fw_luma_shift = p->sh.i_luma_shift[refframe];

				/* luma */
				for( ii = 0; ii < chroma_height<<1; ii++ )
                {
                    for( jj = 0; jj < chroma_height<<1; jj++ )
                    {
                        *(dest_y + ii*p->cur.i_stride[0]+ jj) = 
                            xavs_clip_uint8(((  (*(dest_y + ii*p->cur.i_stride[0]+ jj)) *fw_luma_scale+16)>>5) + fw_luma_shift);
                    }
                }

				/* cb */
				fw_chroma_scale = p->sh.i_chroma_scale[refframe];
				fw_chroma_shift = p->sh.i_chroma_shift[refframe];
				
				for( ii = 0; ii < chroma_height; ii++ )
                {
                    for( jj = 0; jj < chroma_height; jj++ )
                    {
                        *(dest_cb + ii*p->cur.i_stride[1]+ jj) = 
                            xavs_clip_uint8(((  (*(dest_cb + ii*p->cur.i_stride[1]+ jj)) *fw_chroma_scale+16)>>5) + fw_chroma_shift);
                    }
                }

				/* cr */
				for( ii = 0; ii < chroma_height; ii++ )
                {
                    for( jj = 0; jj < chroma_height; jj++ )
                    {
                        *(dest_cr + ii*p->cur.i_stride[2]+ jj) = 
                            xavs_clip_uint8(((  (*(dest_cr + ii*p->cur.i_stride[2]+ jj)) *fw_chroma_scale+16)>>5) + fw_chroma_shift);
                    }
                }
			}
             
        }
        else/* already has forward */
        {
			mc_dir_part(p, ref, chroma_height, pic_ht/*1*/,
                            dst_back_y, dst_back_cb, dst_back_cr, x_offset_back, y_offset_back,
                            lum, chroma, mv+MV_BWD_OFFS);

             /* weighting prediction */
			 if( ((p->sh.b_slice_weighting_flag == 1) 
				&& (p->sh.b_mb_weighting_flag == 1) && (p->weighting_prediction == 1))
				|| ((p->sh.b_slice_weighting_flag == 1) &&  (p->sh.b_mb_weighting_flag == 0)))
             {
				int refframe;

				refframe = (mv+MV_BWD_OFFS)->ref;

				refframe= (!p->param.b_interlaced )?(refframe+1):(2*refframe+1);

				/* scale shift */
                fw_luma_scale = p->sh.i_luma_scale[refframe];
                fw_luma_shift = p->sh.i_luma_shift[refframe];

				/* luma */
				for( ii = 0; ii < chroma_height<<1; ii++ )
                {
                    for( jj = 0; jj < chroma_height<<1; jj++ )
                    {
                        *(dst_back_y + ii*p->cur.i_stride[0]+ jj) = 
                            xavs_clip_uint8(((  (*(dst_back_y + ii*p->cur.i_stride[0]+ jj)) *fw_luma_scale+16)>>5) + fw_luma_shift);
                    }
                }

				/* cb */
				fw_chroma_scale = p->sh.i_chroma_scale[refframe];
				fw_chroma_shift = p->sh.i_chroma_shift[refframe];
				
				for( ii = 0; ii < chroma_height; ii++ )
                {
                    for( jj = 0; jj < chroma_height; jj++ )
                    {
                        *(dst_back_cb + ii*p->cur.i_stride[1]+ jj) = 
                            xavs_clip_uint8(((  (*(dst_back_cb + ii*p->cur.i_stride[1]+ jj)) *fw_chroma_scale+16)>>5) + fw_chroma_shift);
                    }
                }

				/* cr */
				for( ii = 0; ii < chroma_height; ii++ )
                {
                    for( jj = 0; jj < chroma_height; jj++ )
                    {
                        *(dst_back_cr + ii*p->cur.i_stride[2]+ jj) = 
                            xavs_clip_uint8(((  (*(dst_back_cr + ii*p->cur.i_stride[2]+ jj)) *fw_chroma_scale+16)>>5) + fw_chroma_shift);
                    }
                }
			}

			/* avg */
			/* luma */
			p->cavs_avg_pixels_tab[!(chroma_height == 8)](dest_y, p->cur.i_stride[0], dst_back_y, p->cur.i_stride[0]);
			/* cb */
			p->cavs_avg_pixels_tab[(!(chroma_height == 8)) + 1](dest_cb, p->cur.i_stride[1], dst_back_cb, p->cur.i_stride[1]);
			/* cr */
			p->cavs_avg_pixels_tab[(!(chroma_height == 8)) + 1](dest_cr, p->cur.i_stride[2], dst_back_cr, p->cur.i_stride[2]);
        }
        
        //i_dir = i_dir|2;  /* backward flag */
    }

    /* avg */
    //if( i_dir == 3 )
    //{
    //    /* luma */
    //    p->cavs_avg_pixels_tab[!(chroma_height==8)]( dest_y, p->cur.i_stride[0], dst_back_y, p->cur.i_stride[0] );

    //    /* cb */
    //    p->cavs_avg_pixels_tab[(!(chroma_height==8)) + 1]( dest_cb, p->cur.i_stride[1], dst_back_cb, p->cur.i_stride[1] );
    //    
    //    /* cr */
    //    p->cavs_avg_pixels_tab[(!(chroma_height==8)) + 1]( dest_cr, p->cur.i_stride[2], dst_back_cr, p->cur.i_stride[2] );
    //}

#else
    if(mv->ref >= 0)
    {
        xavs_image *ref= &p->ref[mv->ref];

        mc_dir_part(p, ref,chroma_height,0,dest_y, dest_cb, dest_cr, x_offset, y_offset,
                    lum, chroma, mv);

        lum=  luma_avg;
        chroma= chroma_avg;
    }

    if((mv+MV_BWD_OFFS)->ref >= 0)
    {
        xavs_image *ref= &p->ref[(mv+MV_BWD_OFFS)->ref];
        
        mc_dir_part(p, ref,  chroma_height,  1,
                dest_y, dest_cb, dest_cr, x_offset, y_offset,
                 lum, chroma, mv+MV_BWD_OFFS);		
    }
#endif

}

static void inter_pred(cavs_decoder *p, int i_mb_type) 
{
    //int y_16, c_16;
	int ind = (p->i_mb_x << 2);
	int pic_ht = (p->ph.b_picture_structure == 0 ? (p->i_mb_height >> 1) : p->i_mb_height) << 4;

    //y_16 = c_16 = 0;
    //if(p->ph.asi_enable == 1)
    //{
    //    y_16=2;
    //    c_16=3;
    //}

    if(partition_flags[i_mb_type] == 0)
    { 		
    	/* 16x16 */
#if !HAVE_MMX		
    	mc_part_std(p, 8, p->p_y, p->p_cb, p->p_cr,
    		0, 0,&p->mv[MV_FWD_X0],
    		xavs_luma_put[0+y_16],xavs_chroma_put[0+c_16],
    		xavs_luma_avg[0+y_16],xavs_chroma_avg[0+c_16]);
#else
    	mc_part_std(p, 8, pic_ht, p->p_y, p->p_cb, p->p_cr,
    		0, 0,&p->mv[MV_FWD_X0],
    		p->put_cavs_qpel_pixels_tab[0/*+y_16*/],p->put_h264_chroma_pixels_tab[0/*+c_16*/],
    		p->avg_cavs_qpel_pixels_tab[0/*+y_16*/],p->avg_h264_chroma_pixels_tab[0/*+c_16*/]);
#endif
    }
    else
    {
#if !HAVE_MMX
    	mc_part_std(p, 4, p->p_y, p->p_cb, p->p_cr, 
    		0, 0,&p->mv[MV_FWD_X0],
    		xavs_luma_put[1+y_16],xavs_chroma_put[1+c_16],
    		xavs_luma_avg[1+y_16],xavs_chroma_avg[1+c_16]);
    	
    	mc_part_std(p, 4, p->p_y, p->p_cb, p->p_cr,
    		4, 0,&p->mv[MV_FWD_X1],
    		xavs_luma_put[1+y_16],xavs_chroma_put[1+c_16],
    		xavs_luma_avg[1+y_16],xavs_chroma_avg[1+c_16]);
    	
    	mc_part_std(p, 4, p->p_y, p->p_cb, p->p_cr,
    		0, 4,&p->mv[MV_FWD_X2],
    		xavs_luma_put[1+y_16],xavs_chroma_put[1+c_16],
    		xavs_luma_avg[1+y_16],xavs_chroma_avg[1+c_16]);
    	
    	mc_part_std(p, 4, p->p_y, p->p_cb, p->p_cr,
    		4, 4,&p->mv[MV_FWD_X3],
    		xavs_luma_put[1+y_16],xavs_chroma_put[1+c_16],
    		xavs_luma_avg[1+y_16],xavs_chroma_avg[1+c_16]);
#else
    	mc_part_std(p, 4, pic_ht, p->p_y, p->p_cb, p->p_cr, 
    		0, 0,&p->mv[MV_FWD_X0],
    		p->put_cavs_qpel_pixels_tab[1/*+y_16*/],p->put_h264_chroma_pixels_tab[1/*+c_16*/],
    		p->avg_cavs_qpel_pixels_tab[1/*+y_16*/],p->avg_h264_chroma_pixels_tab[1/*+c_16*/]);
    	
    	mc_part_std(p, 4, pic_ht, p->p_y, p->p_cb, p->p_cr,
    		4, 0,&p->mv[MV_FWD_X1],
    		p->put_cavs_qpel_pixels_tab[1/*+y_16*/],p->put_h264_chroma_pixels_tab[1/*+c_16*/],
    		p->avg_cavs_qpel_pixels_tab[1/*+y_16*/],p->avg_h264_chroma_pixels_tab[1/*+c_16*/]);
    	
    	mc_part_std(p, 4, pic_ht, p->p_y, p->p_cb, p->p_cr,
    		0, 4,&p->mv[MV_FWD_X2],
    		p->put_cavs_qpel_pixels_tab[1/*+y_16*/],p->put_h264_chroma_pixels_tab[1/*+c_16*/],
    		p->avg_cavs_qpel_pixels_tab[1/*+y_16*/],p->avg_h264_chroma_pixels_tab[1/*+c_16*/]);
    	
    	mc_part_std(p, 4, pic_ht, p->p_y, p->p_cb, p->p_cr,
    		4, 4,&p->mv[MV_FWD_X3],
    		p->put_cavs_qpel_pixels_tab[1/*+y_16*/],p->put_h264_chroma_pixels_tab[1/*+c_16*/],
    		p->avg_cavs_qpel_pixels_tab[1/*+y_16*/],p->avg_h264_chroma_pixels_tab[1/*+c_16*/]);
#endif
    }

    /* set intra prediction modes to default values */
    p->i_intra_pred_mode_y[5] =  p->i_intra_pred_mode_y[10] = 
    p->i_intra_pred_mode_y[15] =  p->i_intra_pred_mode_y[20] = NOT_AVAIL;

	p->p_top_intra_pred_mode_y[ind/*(p->i_mb_x<<2)+0*/] = p->p_top_intra_pred_mode_y[ind/*(p->i_mb_x << 2)*/+1] =
    p->p_top_intra_pred_mode_y[ind/*(p->i_mb_x<<2)*/+2] = p->p_top_intra_pred_mode_y[ind/*(p->i_mb_x<<2)*/+3] = NOT_AVAIL;    
}


static inline int next_mb( cavs_decoder *p )
{
    int i, i_y;
    int i_offset = p->i_mb_x<<1;

    p->i_mb_flags |= A_AVAIL;
    p->p_y += 16;
    p->p_cb += 8;
    p->p_cr += 8;

    if( p->param.b_accelerate )
    {
        for( i = 0; i <= 20; i += 4 )
        {//原来B3,X1,X3的运动矢量成为右边新的宏块的预测运动矢量左边的候选子即D2,A1,A3
            p->mv[i] = p->mv[i+2];
        }

        //保存X2,X3作为下面宏块的预测运动矢量的候选子即B2,B3
        p->mv[MV_FWD_D3] = p->p_top_mv[0][i_offset+1];
        p->mv[MV_BWD_D3] = p->p_top_mv[1][i_offset+1];//修改前先保存到D3;
        p->p_top_mv[0][i_offset+0] = p->mv[MV_FWD_X2];
        p->p_top_mv[0][i_offset+1] = p->mv[MV_FWD_X3];
        p->p_top_mv[1][i_offset+0] = p->mv[MV_BWD_X2];
        p->p_top_mv[1][i_offset+1] = p->mv[MV_BWD_X3];

        /* for AEC */
        if (p->ph.b_aec_enable)
        {
            p->p_mb_type_top[p->i_mb_x] = p->i_mb_type_tab[p->i_mb_index];
            //p->i_chroma_pred_mode_left = p->p_chroma_pred_mode_top[p->i_mb_x] = p->i_pred_mode_chroma;
#if 0
            if( p->ph.b_skip_mode_flag  || p->ph.i_picture_coding_type == 0 )
            {
                p->i_chroma_pred_mode_left = p->p_chroma_pred_mode_top[p->i_mb_x] = p->i_pred_mode_chroma_tab[p->i_mb_index];
                p->i_cbp_left = p->p_cbp_top[p->i_mb_x] = p->i_cbp;//p->i_cbp_tab[p->i_mb_index];
            }
            else
            {
                 if( p->i_mb_type_tab[p->i_mb_index] != P_SKIP && p->i_mb_type_tab[p->i_mb_index] != B_SKIP )
                {
                    p->i_chroma_pred_mode_left = p->p_chroma_pred_mode_top[p->i_mb_x] = p->i_pred_mode_chroma_tab[p->i_mb_index];
                    p->i_cbp_left = p->p_cbp_top[p->i_mb_x] = p->i_cbp;//p->i_cbp_tab[p->i_mb_index];
                } 
                else
                {   
                    p->i_chroma_pred_mode_left = p->p_chroma_pred_mode_top[p->i_mb_x] = 0;
                    p->i_cbp_left = p->p_cbp_top[p->i_mb_x] = 0; /* skip has not cbp */
                    p->i_last_dqp = 0;
                }
            }
#else
			if( p->ph.i_picture_coding_type == 0 && p->b_bottom == 0 )
            {
                p->i_chroma_pred_mode_left = p->p_chroma_pred_mode_top[p->i_mb_x] = p->i_pred_mode_chroma_tab[p->i_mb_index];
                p->i_cbp_left = p->p_cbp_top[p->i_mb_x] = p->i_cbp;
            }
            else
            {
                 if( p->i_mb_type_tab[p->i_mb_index] != P_SKIP && p->i_mb_type_tab[p->i_mb_index] != B_SKIP )
                {
                    p->i_chroma_pred_mode_left = p->p_chroma_pred_mode_top[p->i_mb_x] = p->i_pred_mode_chroma_tab[p->i_mb_index];
                    p->i_cbp_left = p->p_cbp_top[p->i_mb_x] = p->i_cbp;//p->i_cbp_tab[p->i_mb_index];
                } 
                else
                {   
                    p->i_chroma_pred_mode_left = p->p_chroma_pred_mode_top[p->i_mb_x] = 0;
                    p->i_cbp_left = p->p_cbp_top[p->i_mb_x] = 0; /* skip has not cbp */
                    p->i_last_dqp = 0;
                }
            }
#endif

            CP16(p->p_ref_top[p->i_mb_x][0], &p->p_ref[0][7]);
            CP16(p->p_ref_top[p->i_mb_x][1], &p->p_ref[1][7]);
            
           // CP16(p->p_ref_top[p->i_mb_x][0], &p->p_ref_tab[p->i_mb_index][0][7]);
           // CP16(p->p_ref_top[p->i_mb_x][1], &p->p_ref_tab[p->i_mb_index][1][7]);
        }

        /*Move to next MB*/
        p->i_mb_x++;
        if(p->i_mb_x == p->i_mb_width) 
        { 
            //下一行宏块
            p->i_mb_flags = B_AVAIL|C_AVAIL;//如果i_mb_width=1;则C_AVAIL不可得到
            /* 清除左边块预测模式A1,A3*/
            p->i_intra_pred_mode_y[5] = p->i_intra_pred_mode_y[10] = 
            p->i_intra_pred_mode_y[15] = p->i_intra_pred_mode_y[20] = NOT_AVAIL;
            
            p->i_intra_pred_mode_y_tab[p->i_mb_index][5] = p->i_intra_pred_mode_y_tab[p->i_mb_index][10] = 
            p->i_intra_pred_mode_y_tab[p->i_mb_index][15] = p->i_intra_pred_mode_y_tab[p->i_mb_index][20] = NOT_AVAIL;
              
            /* 清除左边块运动矢量C2,A1,A3 */
            for( i = 0; i <= 20; i += 4 )
            {
                p->mv[i] = MV_NOT_AVAIL;
            }
            p->i_mb_x = 0;
            p->i_mb_y++;
            i_y = p->i_mb_y - p->i_mb_offset;
            p->p_y = p->cur.p_data[0] + i_y*XAVS_MB_SIZE*p->cur.i_stride[0];
            p->p_cb = p->cur.p_data[1] + i_y*XAVS_MB_SIZE/2*p->cur.i_stride[1];
            p->p_cr = p->cur.p_data[2] + i_y*XAVS_MB_SIZE/2*p->cur.i_stride[2];

#if B_MB_WEIGHTING
            p->p_back_y = p->mb_line_buffer[0] + i_y*XAVS_MB_SIZE*p->cur.i_stride[0];
            p->p_back_cb = p->mb_line_buffer[1] + i_y*(XAVS_MB_SIZE>>1)*p->cur.i_stride[1];
            p->p_back_cr = p->mb_line_buffer[2] + i_y*(XAVS_MB_SIZE>>1)*p->cur.i_stride[2];
#endif

            p->b_first_line = 0;

            /* for AEC */
            p->i_mb_type_left = -1;
        }
        else
        {
            /* for AEC */
            p->i_mb_type_left = p->i_mb_type_tab[p->i_mb_index];
        }

        if( p->i_mb_x == p->i_mb_width-1 )
        {
            p->i_mb_flags &= ~C_AVAIL;
        }

        if( p->i_mb_x && p->i_mb_y && p->b_first_line==0 ) 
        {
            p->i_mb_flags |= D_AVAIL;
        }
    }
    else
    {
        for( i = 0; i <= 20; i += 4 )
        {//原来B3,X1,X3的运动矢量成为右边新的宏块的预测运动矢量左边的候选子即D2,A1,A3
            p->mv[i] = p->mv[i+2];
        }
        //保存X2,X3作为下面宏块的预测运动矢量的候选子即B2,B3
        p->mv[MV_FWD_D3] = p->p_top_mv[0][i_offset+1];
        p->mv[MV_BWD_D3] = p->p_top_mv[1][i_offset+1];//修改前先保存到D3;
        p->p_top_mv[0][i_offset+0] = p->mv[MV_FWD_X2];
        p->p_top_mv[0][i_offset+1] = p->mv[MV_FWD_X3];
        p->p_top_mv[1][i_offset+0] = p->mv[MV_BWD_X2];
        p->p_top_mv[1][i_offset+1] = p->mv[MV_BWD_X3];

        /* for AEC */
        if (p->ph.b_aec_enable)
        {
            p->p_mb_type_top[p->i_mb_x] = p->i_mb_type;
#if 0
            if( p->ph.b_skip_mode_flag  || p->ph.i_picture_coding_type == 0 )
            {
                p->i_chroma_pred_mode_left = p->p_chroma_pred_mode_top[p->i_mb_x] = p->i_pred_mode_chroma;
                p->i_cbp_left = p->p_cbp_top[p->i_mb_x] = p->i_cbp;
            }
            else
            {
                 if( p->i_mb_type != P_SKIP && p->i_mb_type != B_SKIP )
                {
                    p->i_chroma_pred_mode_left = p->p_chroma_pred_mode_top[p->i_mb_x] = p->i_pred_mode_chroma;
                    p->i_cbp_left = p->p_cbp_top[p->i_mb_x] = p->i_cbp;
                } 
                else
                {   
                    p->i_chroma_pred_mode_left = p->p_chroma_pred_mode_top[p->i_mb_x] = 0;
                    p->i_cbp_left = p->p_cbp_top[p->i_mb_x] = 0; /* skip has not cbp */
                    p->i_last_dqp = 0;
                }
            }
#else
			if(p->ph.i_picture_coding_type == 0 && p->b_bottom == 0)/* frame or field-top is I-slice */
			{
				p->i_chroma_pred_mode_left = p->p_chroma_pred_mode_top[p->i_mb_x] = p->i_pred_mode_chroma;
                p->i_cbp_left = p->p_cbp_top[p->i_mb_x] = p->i_cbp;
			}
			else
			{
				if( p->i_mb_type != P_SKIP && p->i_mb_type != B_SKIP )
				{
					p->i_chroma_pred_mode_left = p->p_chroma_pred_mode_top[p->i_mb_x] = p->i_pred_mode_chroma;
					p->i_cbp_left = p->p_cbp_top[p->i_mb_x] = p->i_cbp;
				} 
				else
				{   
					p->i_chroma_pred_mode_left = p->p_chroma_pred_mode_top[p->i_mb_x] = 0;
					p->i_cbp_left = p->p_cbp_top[p->i_mb_x] = 0; /* skip has not cbp */
					p->i_last_dqp = 0;
				}
			}
#endif
            
            CP16(p->p_ref_top[p->i_mb_x][0], &p->p_ref[0][7]);
            CP16(p->p_ref_top[p->i_mb_x][1], &p->p_ref[1][7]);
        }

        /*Move to next MB*/
        p->i_mb_x++;
        if(p->i_mb_x == p->i_mb_width) 
        { 
            //下一行宏块
            p->i_mb_flags = B_AVAIL|C_AVAIL;//如果i_mb_width=1;则C_AVAIL不可得到
            /* 清除左边块预测模式A1,A3*/
            p->i_intra_pred_mode_y[5] = p->i_intra_pred_mode_y[10] = 
            p->i_intra_pred_mode_y[15] = p->i_intra_pred_mode_y[20] = NOT_AVAIL;
            
            /* 清除左边块运动矢量C2,A1,A3 */
            for( i = 0; i <= 20; i += 4 )
            {
                p->mv[i] = MV_NOT_AVAIL;
            }
            p->i_mb_x = 0;
            p->i_mb_y++;
            i_y = p->i_mb_y - p->i_mb_offset;
            p->p_y = p->cur.p_data[0] + i_y*XAVS_MB_SIZE*p->cur.i_stride[0];
            p->p_cb = p->cur.p_data[1] + i_y*XAVS_MB_SIZE/2*p->cur.i_stride[1];
            p->p_cr = p->cur.p_data[2] + i_y*XAVS_MB_SIZE/2*p->cur.i_stride[2];

#if B_MB_WEIGHTING
            p->p_back_y = p->mb_line_buffer[0] + i_y*XAVS_MB_SIZE*p->cur.i_stride[0];
            p->p_back_cb = p->mb_line_buffer[1] + i_y*(XAVS_MB_SIZE>>1)*p->cur.i_stride[1];
            p->p_back_cr = p->mb_line_buffer[2] + i_y*(XAVS_MB_SIZE>>1)*p->cur.i_stride[2];
#endif

            p->b_first_line = 0;

            /* for AEC */
            p->i_mb_type_left = -1;
        }
        else
        {
            /* for AEC */
            p->i_mb_type_left = p->i_mb_type;
        }

        if( p->i_mb_x == p->i_mb_width-1 )
        {
            p->i_mb_flags &= ~C_AVAIL;
        }

        if( p->i_mb_x && p->i_mb_y && p->b_first_line==0 ) 
        {
            p->i_mb_flags |= D_AVAIL;
        }
    }

    return 0;
}

static inline void init_mb(cavs_decoder *p) 
{
    //int i = 0;
    int i_offset = p->i_mb_x<<1;
    int i_offset_b4 = p->i_mb_x<<2;

    if( p->param.b_accelerate )
    {
        if(!(p->i_mb_flags & A_AVAIL))
        {
            p->mv[MV_FWD_A1] = MV_NOT_AVAIL;
            p->mv[MV_FWD_A3] = MV_NOT_AVAIL;
            p->mv[MV_BWD_A1] = MV_NOT_AVAIL;
            p->mv[MV_BWD_A3] = MV_NOT_AVAIL;

            p->i_intra_pred_mode_y[5] = p->i_intra_pred_mode_y[10] = 
            p->i_intra_pred_mode_y[15] = p->i_intra_pred_mode_y[20] = NOT_AVAIL;

            p->i_intra_pred_mode_y_tab[p->i_mb_index][5] = p->i_intra_pred_mode_y_tab[p->i_mb_index][10] = 
            p->i_intra_pred_mode_y_tab[p->i_mb_index][15] = p->i_intra_pred_mode_y_tab[p->i_mb_index][20] = NOT_AVAIL;
        }

        if(p->i_mb_flags & B_AVAIL)
        {
            p->mv[MV_FWD_B2] = p->p_top_mv[0][i_offset];
            p->mv[MV_BWD_B2] = p->p_top_mv[1][i_offset];
            p->mv[MV_FWD_B3] = p->p_top_mv[0][i_offset+1];
            p->mv[MV_BWD_B3] = p->p_top_mv[1][i_offset+1];

            p->i_intra_pred_mode_y[1] = p->p_top_intra_pred_mode_y[i_offset_b4];
            p->i_intra_pred_mode_y[2] = p->p_top_intra_pred_mode_y[i_offset_b4+1];
            p->i_intra_pred_mode_y[3] = p->p_top_intra_pred_mode_y[i_offset_b4+2];
            p->i_intra_pred_mode_y[4] = p->p_top_intra_pred_mode_y[i_offset_b4+3];

            p->i_intra_pred_mode_y_tab[p->i_mb_index][1] = p->p_top_intra_pred_mode_y[i_offset_b4];
            p->i_intra_pred_mode_y_tab[p->i_mb_index][2] = p->p_top_intra_pred_mode_y[i_offset_b4+1];
            p->i_intra_pred_mode_y_tab[p->i_mb_index][3] = p->p_top_intra_pred_mode_y[i_offset_b4+2];
            p->i_intra_pred_mode_y_tab[p->i_mb_index][4] = p->p_top_intra_pred_mode_y[i_offset_b4+3];
        }
        else
        {
            p->mv[MV_FWD_B2] = MV_NOT_AVAIL;
            p->mv[MV_FWD_B3] = MV_NOT_AVAIL;
            p->mv[MV_BWD_B2] = MV_NOT_AVAIL;
            p->mv[MV_BWD_B3] = MV_NOT_AVAIL;
            
            p->i_intra_pred_mode_y[1] = p->i_intra_pred_mode_y[2] = 
            p->i_intra_pred_mode_y[3] = p->i_intra_pred_mode_y[4] = NOT_AVAIL;
 
            p->i_intra_pred_mode_y_tab[p->i_mb_index][1] = p->i_intra_pred_mode_y_tab[p->i_mb_index][2] = 
            p->i_intra_pred_mode_y_tab[p->i_mb_index][3] = p->i_intra_pred_mode_y_tab[p->i_mb_index][4] = NOT_AVAIL;
        }

        if(p->i_mb_flags & C_AVAIL)
        {
            p->mv[MV_FWD_C2] = p->p_top_mv[0][i_offset+2];
            p->mv[MV_BWD_C2] = p->p_top_mv[1][i_offset+2];
        }
        else
        {
            p->mv[MV_FWD_C2] = MV_NOT_AVAIL;
            p->mv[MV_BWD_C2] = MV_NOT_AVAIL;
        }

        if(!(p->i_mb_flags & D_AVAIL))
        {
            p->mv[MV_FWD_D3] = MV_NOT_AVAIL;
            p->mv[MV_BWD_D3] = MV_NOT_AVAIL;
        }

        p->p_col_type = &p->p_col_type_base[p->i_mb_y*p->i_mb_width + p->i_mb_x];

        p->vbs_mb_intra_pred_flag = 0;
        p->vbs_mb_part_intra_pred_flag[0] = p->vbs_mb_part_intra_pred_flag[1] =
        p->vbs_mb_part_intra_pred_flag[2] = p->vbs_mb_part_intra_pred_flag[3] =
        p->vbs_mb_part_transform_4x4_flag[0] = p->vbs_mb_part_transform_4x4_flag[1] =
        p->vbs_mb_part_transform_4x4_flag[2] = p->vbs_mb_part_transform_4x4_flag[3] = 0;

        if (p->ph.b_aec_enable)
        {
            if (!p->i_mb_x)
            {
                p->i_chroma_pred_mode_left = 0;
                p->i_cbp_left = -1;

                M32(p->p_mvd[0][0]) 
                    = M32(p->p_mvd[0][3]) 
                    = M32(p->p_mvd[1][0]) 
                    = M32(p->p_mvd[1][3]) = 0;

                p->p_ref[0][3] 
                    = p->p_ref[0][6] 
                    = p->p_ref[1][3] 
                    = p->p_ref[1][6] = -1;
                     
            }
            else
            {
                M32(p->p_mvd[0][0]) = M32(p->p_mvd[0][2]);
                M32(p->p_mvd[0][3]) = M32(p->p_mvd[0][5]);
                M32(p->p_mvd[1][0]) = M32(p->p_mvd[1][2]);
                M32(p->p_mvd[1][3]) = M32(p->p_mvd[1][5]);

                p->p_ref[0][3] = p->p_ref[0][5];
                p->p_ref[0][6] = p->p_ref[0][8];
                p->p_ref[1][3] = p->p_ref[1][5];
                p->p_ref[1][6] = p->p_ref[1][8];
            }

            CP16(&p->p_ref[0][1], p->p_ref_top[p->i_mb_x][0]);
            CP16(&p->p_ref[1][1], p->p_ref_top[p->i_mb_x][1]);

            M64(p->p_mvd[0][1]) 
                = M64(p->p_mvd[1][1]) 
                = M64(p->p_mvd[0][4]) 
                = M64(p->p_mvd[1][4]) = 0;

            M16(&p->p_ref[0][4]) 
                = M16(&p->p_ref[0][7]) 
                = M16(&p->p_ref[1][4]) 
                = M16(&p->p_ref[1][7]) = -1;
            
            p->i_pred_mode_chroma_tab[p->i_mb_index] = 0;
        }
        
    }
    else
    {
        if(!(p->i_mb_flags & A_AVAIL))
        {
            p->mv[MV_FWD_A1] = MV_NOT_AVAIL;
            p->mv[MV_FWD_A3] = MV_NOT_AVAIL;
            p->mv[MV_BWD_A1] = MV_NOT_AVAIL;
            p->mv[MV_BWD_A3] = MV_NOT_AVAIL;

            p->i_intra_pred_mode_y[5] = p->i_intra_pred_mode_y[10] = 
            p->i_intra_pred_mode_y[15] = p->i_intra_pred_mode_y[20] = NOT_AVAIL;
        }

        if(p->i_mb_flags & B_AVAIL)
        {
            p->mv[MV_FWD_B2] = p->p_top_mv[0][i_offset];
            p->mv[MV_BWD_B2] = p->p_top_mv[1][i_offset];
            p->mv[MV_FWD_B3] = p->p_top_mv[0][i_offset+1];
            p->mv[MV_BWD_B3] = p->p_top_mv[1][i_offset+1];

            p->i_intra_pred_mode_y[1] = p->p_top_intra_pred_mode_y[i_offset_b4];
            p->i_intra_pred_mode_y[2] = p->p_top_intra_pred_mode_y[i_offset_b4+1];
            p->i_intra_pred_mode_y[3] = p->p_top_intra_pred_mode_y[i_offset_b4+2];
            p->i_intra_pred_mode_y[4] = p->p_top_intra_pred_mode_y[i_offset_b4+3];
        }
        else
        {
            p->mv[MV_FWD_B2] = MV_NOT_AVAIL;
            p->mv[MV_FWD_B3] = MV_NOT_AVAIL;
            p->mv[MV_BWD_B2] = MV_NOT_AVAIL;
            p->mv[MV_BWD_B3] = MV_NOT_AVAIL;
            p->i_intra_pred_mode_y[1] = p->i_intra_pred_mode_y[2] = 
            p->i_intra_pred_mode_y[3] = p->i_intra_pred_mode_y[4] = NOT_AVAIL;
        }

        if(p->i_mb_flags & C_AVAIL)
        {
            p->mv[MV_FWD_C2] = p->p_top_mv[0][i_offset+2];
            p->mv[MV_BWD_C2] = p->p_top_mv[1][i_offset+2];
        }
        else
        {
            p->mv[MV_FWD_C2] = MV_NOT_AVAIL;
            p->mv[MV_BWD_C2] = MV_NOT_AVAIL;
        }

        if(!(p->i_mb_flags & D_AVAIL))
        {
            p->mv[MV_FWD_D3] = MV_NOT_AVAIL;
            p->mv[MV_BWD_D3] = MV_NOT_AVAIL;	
        }

        p->p_col_type = &p->p_col_type_base[p->i_mb_y*p->i_mb_width + p->i_mb_x];

        p->vbs_mb_intra_pred_flag = 0;
        p->vbs_mb_part_intra_pred_flag[0] = p->vbs_mb_part_intra_pred_flag[1] =
        p->vbs_mb_part_intra_pred_flag[2] = p->vbs_mb_part_intra_pred_flag[3] =
        p->vbs_mb_part_transform_4x4_flag[0] = p->vbs_mb_part_transform_4x4_flag[1] =
        p->vbs_mb_part_transform_4x4_flag[2] = p->vbs_mb_part_transform_4x4_flag[3] = 0;

        if (p->ph.b_aec_enable)
        {
            if (!p->i_mb_x)
            {
                p->i_chroma_pred_mode_left = 0;
                p->i_cbp_left = -1;
                M32(p->p_mvd[0][0]) = M32(p->p_mvd[0][3]) = M32(p->p_mvd[1][0]) = M32(p->p_mvd[1][3]) = 0;
                p->p_ref[0][3] = p->p_ref[0][6] = p->p_ref[1][3] = p->p_ref[1][6] = -1;
            }
            else
            {
                M32(p->p_mvd[0][0]) = M32(p->p_mvd[0][2]);
                M32(p->p_mvd[0][3]) = M32(p->p_mvd[0][5]);
                M32(p->p_mvd[1][0]) = M32(p->p_mvd[1][2]);
                M32(p->p_mvd[1][3]) = M32(p->p_mvd[1][5]);
                p->p_ref[0][3] = p->p_ref[0][5];
                p->p_ref[0][6] = p->p_ref[0][8];
                p->p_ref[1][3] = p->p_ref[1][5];
                p->p_ref[1][6] = p->p_ref[1][8];
            }
            CP16(&p->p_ref[0][1], p->p_ref_top[p->i_mb_x][0]);
            CP16(&p->p_ref[1][1], p->p_ref_top[p->i_mb_x][1]);
            M64(p->p_mvd[0][1]) = M64(p->p_mvd[1][1]) = M64(p->p_mvd[0][4]) = M64(p->p_mvd[1][4]) = 0;
            M16(&p->p_ref[0][4]) = M16(&p->p_ref[0][7]) = M16(&p->p_ref[1][4]) = M16(&p->p_ref[1][7]) = -1;
            p->i_pred_mode_chroma = 0;
        }
    }
    
}

char AVS_2DVLC_INTRA_dec_4x4[7][40][2] = {{{-1,-1}}};	
char AVS_2DVLC_INTER_dec_4x4[7][40][2] = {{{-1,-1}}};
const int SCAN_4x4[16][2] = 
{
	{0,0},{1,0},{0,1},{0,2},
	{1,1},{2,0},{3,0},{2,1},
	{1,2},{0,3},{1,3},{2,2},
	{3,1},{3,2},{2,3},{3,3}
};  

const int SCAN[2][64][2] = // [scan_pos][x/y] ATTENTION: the ScanPositions are (pix,lin)!
{
	{
		{0,0},{0,1},{0,2},{1,0},{0,3},{0,4},{1,1},{1,2},
		{0,5},{0,6},{1,3},{2,0},{2,1},{0,7},{1,4},{2,2},
		{3,0},{1,5},{1,6},{2,3},{3,1},{3,2},{4,0},{1,7},
		{2,4},{4,1},{2,5},{3,3},{4,2},{2,6},{3,4},{4,3},
		{5,0},{5,1},{2,7},{3,5},{4,4},{5,2},{6,0},{5,3},
		{3,6},{4,5},{6,1},{6,2},{5,4},{3,7},{4,6},{6,3},
		{5,5},{4,7},{6,4},{5,6},{6,5},{5,7},{6,6},{7,0},
		{6,7},{7,1},{7,2},{7,3},{7,4},{7,5},{7,6},{7,7}
	},
	{
		{ 0, 0}, { 1, 0}, { 0, 1}, { 0, 2}, { 1, 1}, { 2, 0}, { 3, 0}, { 2, 1},
		{ 1, 2}, { 0, 3}, { 0, 4}, { 1, 3}, { 2, 2}, { 3, 1}, { 4, 0}, { 5, 0},
		{ 4, 1}, { 3, 2}, { 2, 3}, { 1, 4}, { 0, 5}, { 0, 6}, { 1, 5}, { 2, 4},
		{ 3, 3}, { 4, 2}, { 5, 1}, { 6, 0}, { 7, 0}, { 6, 1}, { 5, 2}, { 4, 3},
		{ 3, 4}, { 2, 5}, { 1, 6}, { 0, 7}, { 1, 7}, { 2, 6}, { 3, 5}, { 4, 4},
		{ 5, 3}, { 6, 2}, { 7, 1}, { 7, 2}, { 6, 3}, { 5, 4}, { 4, 5}, { 3, 6},
		{ 2, 7}, { 3, 7}, { 4, 6}, { 5, 5}, { 6, 4}, { 7, 3}, { 7, 4}, { 6, 5},
		{ 5, 6}, { 4, 7}, { 5, 7}, { 6, 6}, { 7, 5}, { 7, 6}, { 6, 7}, { 7, 7}
	}
};

static UNUSED void cavs_inv_transform_B8 (int curr_blk[8][8])
{
    int xx, yy;
    int tmp[8];
    int t;
    int b[8];
    int clip1, clip2; 

    clip1 = 0-(1<<(8+7));
    clip2 = (1<<(8+7))-1;

    for( yy = 0; yy < 8; yy++ )
    {
         /* Horizontal inverse transform */
         /* Reorder */
         tmp[0] = curr_blk[yy][0];
         tmp[1] = curr_blk[yy][4];
         tmp[2] = curr_blk[yy][2];
         tmp[3] = curr_blk[yy][6];
         tmp[4] = curr_blk[yy][1];
         tmp[5] = curr_blk[yy][3];
         tmp[6] = curr_blk[yy][5];
         tmp[7] = curr_blk[yy][7];
         
         /* Downleft Butterfly */
         b[0] = ((tmp[4] - tmp[7])<<1) + tmp[4];
         b[1] = ((tmp[5] + tmp[6])<<1) + tmp[5];
         b[2] = ((tmp[5] - tmp[6])<<1) - tmp[6];
         b[3] = ((tmp[4] + tmp[7])<<1) + tmp[7];

         b[4] = ((b[0] + b[1] + b[3])<<1) + b[1];
         b[5] = ((b[0] - b[1] + b[2])<<1) + b[0];
         b[6] = ((-b[1] - b[2] + b[3])<<1) + b[3];
         b[7] = ((b[0] - b[2] - b[3])<<1) - b[2];
         
         /* Upleft Butterfly */
         t=((tmp[2]*10)+(tmp[3]<<2));
         tmp[3]=((tmp[2]<<2)-(tmp[3]*10));
         tmp[2]=t;
         
         t = (tmp[0]+tmp[1])<<3;
         tmp[1] = (tmp[0]-tmp[1])<<3;
         tmp[0] = t;
         
         b[0]=tmp[0]+tmp[2];
         b[1]=tmp[1]+tmp[3];
         b[2]=tmp[1]-tmp[3];
         b[3]=tmp[0]-tmp[2];	 
         
         /* Last Butterfly */
         curr_blk[yy][0] = (int)((Clip3(clip1,clip2,((b[0]+b[4])+ 4)))>>3);
         curr_blk[yy][1] = (int)((Clip3(clip1,clip2,((b[1]+b[5])+ 4)))>>3);
         curr_blk[yy][2] = (int)((Clip3(clip1,clip2,((b[2]+b[6])+ 4)))>>3);
         curr_blk[yy][3] = (int)((Clip3(clip1,clip2,((b[3]+b[7])+ 4)))>>3);
         curr_blk[yy][7] = (int)((Clip3(clip1,clip2,((b[0]-b[4])+ 4)))>>3);
         curr_blk[yy][6] = (int)((Clip3(clip1,clip2,((b[1]-b[5])+ 4)))>>3);
         curr_blk[yy][5] = (int)((Clip3(clip1,clip2,((b[2]-b[6])+ 4)))>>3);
         curr_blk[yy][4] = (int)((Clip3(clip1,clip2,((b[3]-b[7])+ 4)))>>3);   
    }
    
    // Vertical inverse transform
    for(xx=0; xx<8; xx++)
    {
         /* Reorder */
         tmp[0] = curr_blk[0][xx];
         tmp[1] = curr_blk[4][xx];
         tmp[2] = curr_blk[2][xx];
         tmp[3] = curr_blk[6][xx];
         tmp[4] = curr_blk[1][xx];
         tmp[5] = curr_blk[3][xx];
         tmp[6] = curr_blk[5][xx];
         tmp[7] = curr_blk[7][xx];
         
        /* Downleft Butterfly */
        b[0] = ((tmp[4] - tmp[7])<<1) + tmp[4];
        b[1] = ((tmp[5] + tmp[6])<<1) + tmp[5];
        b[2] = ((tmp[5] - tmp[6])<<1) - tmp[6];
        b[3] = ((tmp[4] + tmp[7])<<1) + tmp[7];

        b[4] = ((b[0] + b[1] + b[3])<<1) + b[1];
        b[5] = ((b[0] - b[1] + b[2])<<1) + b[0];
        b[6] = ((-b[1] - b[2] + b[3])<<1) + b[3];
        b[7] = ((b[0] - b[2] - b[3])<<1) - b[2];

        /* Upleft Butterfly */
        t = ((tmp[2]*10)+(tmp[3]<<2));
        tmp[3] = ((tmp[2]<<2)-(tmp[3]*10));
        tmp[2] = t;

        t = (tmp[0]+tmp[1])<<3;
        tmp[1] = (tmp[0]-tmp[1])<<3;
        tmp[0] = t;

        b[0] = tmp[0] + tmp[2];
        b[1] = tmp[1] + tmp[3];
        b[2] = tmp[1] - tmp[3];
        b[3] = tmp[0] - tmp[2];

        curr_blk[0][xx]=(Clip3(clip1,clip2,(b[0]+b[4])+64))>>7;
        curr_blk[1][xx]=(Clip3(clip1,clip2,(b[1]+b[5])+64))>>7;
        curr_blk[2][xx]=(Clip3(clip1,clip2,(b[2]+b[6])+64))>>7;
        curr_blk[3][xx]=(Clip3(clip1,clip2,(b[3]+b[7])+64))>>7;
        curr_blk[7][xx]=(Clip3(clip1,clip2,(b[0]-b[4])+64))>>7;
        curr_blk[6][xx]=(Clip3(clip1,clip2,(b[1]-b[5])+64))>>7;
        curr_blk[5][xx]=(Clip3(clip1,clip2,(b[2]-b[6])+64))>>7;
        curr_blk[4][xx]=(Clip3(clip1,clip2,(b[3]-b[7])+64))>>7;
    }
}

static UNUSED void xavs_add_c(uint8_t *dst, int idct8x8[8][8], int stride )
{
    int i;

    uint8_t *cm = crop_table + MAX_NEG_CROP;

    for( i = 0; i < 8; i++ ) 
    {
        dst[i + 0*stride] = cm[ dst[i + 0*stride] + idct8x8[0][i]];
        dst[i + 1*stride] = cm[ dst[i + 1*stride] + idct8x8[1][i]];
        dst[i + 2*stride] = cm[ dst[i + 2*stride] + idct8x8[2][i]];
        dst[i + 3*stride] = cm[ dst[i + 3*stride] + idct8x8[3][i]];
        dst[i + 4*stride] = cm[ dst[i + 4*stride] + idct8x8[4][i]];
        dst[i + 5*stride] = cm[ dst[i + 5*stride] + idct8x8[5][i]];
        dst[i + 6*stride] = cm[ dst[i + 6*stride] + idct8x8[6][i]];
        dst[i + 7*stride] = cm[ dst[i + 7*stride] + idct8x8[7][i]];
    }
}

static int get_residual_block(cavs_decoder *p,const xavs_vlc *p_vlc_table, 
	int i_escape_order,int i_qp, uint8_t *p_dest, int i_stride, int b_chroma)
{
    int i,pos = -1;
    int *level_buf = p->level_buf;
    int *run_buf = p->run_buf;
    int dqm = dequant_mul[i_qp];
    int dqs = dequant_shift[i_qp];
    int dqa = 1 << (dqs - 1);
    const uint8_t *zigzag = p->ph.b_picture_structure==0 ? zigzag_field : zigzag_progressive;
    DCTELEM *block = p->p_block;

    DECLARE_ALIGNED_16(short dct8x8[8][8]);
    int j, k;

    //memset(dct8x8, 0, 64*sizeof(int16_t));

    i = p->bs_read_coeffs(p, p_vlc_table, i_escape_order, b_chroma);
    if( i == -1 || p->b_error_flag )
    {
        printf("[error]MB coeffs is wrong\n");
        return -1;
    }
  
    if( !p->b_weight_quant_enable )
    {
    	while(--i >= 0)
    	{
    		pos += run_buf[i];
    		if(pos > 63) 
    		{
    			printf("[error]MB run pos is too long\n");
    			p->b_error_flag = 1;
            
    			return -1;
    		}

    		block[zigzag[pos]] = (level_buf[i]*dqm + dqa) >> dqs;
    	}
    }
    else
    {
    	int m, n;

    	while(--i >= 0)
    	{
    		pos += run_buf[i];
    		if(pos > 63) 
    		{
    			printf("[error]MB run pos is too long\n");
    			p->b_error_flag = 1;
            
    			return -1;
    		}

    		m = SCAN[p->ph.b_picture_structure][pos][0];
    		n = SCAN[p->ph.b_picture_structure][pos][1];

             /* NOTE : bug will conflict with idct asm when Win32-Release, don't why now */
    		block[zigzag[pos]] = (((((int)(level_buf[i]*p->cur_wq_matrix[n*8+m])>>3)*dqm)>>4) + dqa) >> dqs;
    	}
    }

#if WIN32//!HAVE_MMX

#if 1 // bug:    
	xavs_idct8_add(p_dest, block, i_stride);
#else
    for( j = 0 ; j < 8; j++ )
        for( k = 0; k < 8; k++ )
        {
            idct8x8[j][k] = (int)block[j*8+k];
        }
    memset(block, 0, 64*sizeof(DCTELEM));
        
    /* idct 8x8 */    
    cavs_inv_transform_B8( idct8x8 );

#if 0 // FIXIT
    if( /*p->i_mb_index == 0*/0)
    {
        int i;

        printf("block coeffs after idct:\n");

        for( j = 0; j < 8; j++ )
        {
            for( k = 0; k < 8; k++ )
            {
                printf("%3d,",idct8x8[j][k] );
            }
            printf("\n");
        }
    }
#endif

    /* reconstruct = residual + predidct */
    xavs_add_c( p_dest, idct8x8, i_stride );

#endif

#else /* bug : asm is wrong when worst situation,  */	

#if 0
	for( j = 0 ; j < 8; j++ )
    	for( k = 0; k < 8; k++ )
    	{
    		dct8x8[k][j] = block[j*8+k];
    	}
    memset(block,0,64*sizeof(DCTELEM));
    cavs_add8x8_idct8_sse2( p_dest, dct8x8,i_stride );
#else
	
	cavs_add8x8_idct8_sse2( p_dest, block,i_stride );
	memset(block,0,64*sizeof(DCTELEM));
#endif

#endif

    return 0;
}

static int get_residual_block4x4(cavs_decoder *p, int i_qp, uint8_t *p_dest4x4, int i_stride, int intra)
{
    int i,ipos;
    int level, run, mask, symbol2D;
    int level_buf[64];
    int run_buf[64];
    int dqm, dqs, dqa;

    //const uint8_t *zigzag = zigzag_progressive_4x4;
    static const int incLevel_intra[7]={0,1,2,3,4,6,3000};
    static const int incRun_intra[7]={-1,3,17,17,17,17,17};
    static const int incLevel_inter[7]={0,1,1,2,3,5,3000};
    static const int incRun_inter[7]={-1,3,6,17,17,17,17};
    static const int level1_inter[16]={1,1,1,1,2,2,2,3,3,3,3,3,3,3,3,3};
    static const int level1_intra[16]={1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2};
    DCTELEM *block = p->p_block;
    int CurrentVLCTable=0;
    int abslevel;

    int EOB_Pos_4x4[7] = {-1,0,0,0,0,0,0};
    const char (*table2D_4x4)[16];
    const char (*table2D_inter_4x4)[14];
#define assert(_Expression)     ((void)0)

    //int maxAbsLevel=0;
    //int maxrun=-1;
    int PreviousRunCnt = 0;
    int vlc_numcoef = 0;
    int ii, jj;
    int abs_lev_diff;

    dqm = dequant_mul[i_qp - p->ph.vbs_qp_shift];
    dqs = dequant_shift[i_qp - p->ph.vbs_qp_shift]/* - 1*/;
    dqa = 1 << (dqs - 1);

    if(AVS_2DVLC_INTRA_dec_4x4[0][0][1]<0)            
    {
    	memset(AVS_2DVLC_INTRA_dec_4x4,-1,sizeof(AVS_2DVLC_INTRA_dec_4x4));
    	for(i=0;i<7;i++)
    	{
    		table2D_4x4=intra_2dvlc_4x4[i];
    		for(run=0;run<16;run++)
    			for(level=0;level<16;level++)
    			{
    				ipos=table2D_4x4[run][level];
    				assert(ipos<40);
    				if(ipos>=0)
    				{
    					if(i==0)
    					{
    						AVS_2DVLC_INTRA_dec_4x4[i][ipos][0]=level+1;
    						AVS_2DVLC_INTRA_dec_4x4[i][ipos][1]=run;

    						AVS_2DVLC_INTRA_dec_4x4[i][ipos+1][0]=-(level+1);
    						AVS_2DVLC_INTRA_dec_4x4[i][ipos+1][1]=run;
    					}
    					else
    					{
    						AVS_2DVLC_INTRA_dec_4x4[i][ipos][0]=level;
    						AVS_2DVLC_INTRA_dec_4x4[i][ipos][1]=run;              
    						if(level)
    						{
    							AVS_2DVLC_INTRA_dec_4x4[i][ipos+1][0]=-(level);
    							AVS_2DVLC_INTRA_dec_4x4[i][ipos+1][1]=run;
    						}
    					}
    				}
    			}
    	}
    	assert(AVS_2DVLC_INTRA_dec_4x4[0][0][1]>=0);        //otherwise, tables are bad.
    }
    if(AVS_2DVLC_INTER_dec_4x4[0][0][1]<0)                                                          // Don't need to set this every time. rewrite later.
    {
    	memset(AVS_2DVLC_INTER_dec_4x4,-1,sizeof(AVS_2DVLC_INTER_dec_4x4));
    	for(i=0;i<7;i++)
    	{
    		table2D_inter_4x4=inter_2dvlc_4x4[i];
    		for(run=0;run<16;run++)
    			for(level=0;level<14;level++)
    			{
    				ipos=table2D_inter_4x4[run][level];
    				assert(ipos<40);
    				if(ipos>=0)
    				{
    					if(i==0)
    					{
    						AVS_2DVLC_INTER_dec_4x4[i][ipos][0]=level+1;
    						AVS_2DVLC_INTER_dec_4x4[i][ipos][1]=run;

    						AVS_2DVLC_INTER_dec_4x4[i][ipos+1][0]=-(level+1);
    						AVS_2DVLC_INTER_dec_4x4[i][ipos+1][1]=run;
    					}
    					else
    					{
    						AVS_2DVLC_INTER_dec_4x4[i][ipos][0]=level;
    						AVS_2DVLC_INTER_dec_4x4[i][ipos][1]=run;

    						if(level)
    						{
    							AVS_2DVLC_INTER_dec_4x4[i][ipos+1][0]=-(level);
    							AVS_2DVLC_INTER_dec_4x4[i][ipos+1][1]=run;
    						}
    					}
    				}
    			}
    	}
    	assert(AVS_2DVLC_INTER_dec_4x4[0][0][1]>=0);        //otherwise, tables are bad.
    }

    if (intra)
    {
    	CurrentVLCTable = 0;
    	PreviousRunCnt = 0;
    	for(i=0;i<17;i++)
    	{
    		symbol2D = cavs_bitstream_get_ue_k(&p->s,VLC_GC_Order_INTRA[CurrentVLCTable][0]);
    		if(symbol2D==EOB_Pos_4x4[CurrentVLCTable])
    		{
    			vlc_numcoef = i;  // 0~i last is "0"? rm上的
    			break;
    		}
    		if(symbol2D >= ESCAPE_CODE_PART2)
    		{
    			abs_lev_diff = 2+(symbol2D-ESCAPE_CODE_PART2)/2;
    			mask = -(symbol2D & 1);//0或-1
    			symbol2D = cavs_bitstream_get_ue_k(&p->s,0);
    			run = symbol2D;
    			level = abs_lev_diff + LEVRUN_INTRA[CurrentVLCTable][run];
    			level = (level^mask) - mask;
    		}
    		else if (symbol2D>=ESCAPE_CODE_PART1)
    		{
    			run =((symbol2D-ESCAPE_CODE_PART1)&31)/2;
    			level =1+LEVRUN_INTRA[CurrentVLCTable][run];
    			mask = -(symbol2D & 1);//0或-1
    			level = (level^mask) - mask;
    		} 
    		else
    		{
    			level = AVS_2DVLC_INTRA_dec_4x4[CurrentVLCTable][symbol2D][0];
    			run   = AVS_2DVLC_INTRA_dec_4x4[CurrentVLCTable][symbol2D][1];
    		}
    		level_buf[i] = level;
    		run_buf[i] = run;
    		PreviousRunCnt += run+1;
    		
    		abslevel = abs(level);
    		if(abslevel>=incLevel_intra[CurrentVLCTable])
    		{
    			if(abslevel == 1)
    				CurrentVLCTable = (run>incRun_intra[CurrentVLCTable])?level1_intra[run]:CurrentVLCTable;
    			else if(abslevel < 4)
    				CurrentVLCTable = (abslevel+1);
    			else if(abslevel < 6)
    				CurrentVLCTable = 5;
    			else
    				CurrentVLCTable = 6;
    		}			

    		if (PreviousRunCnt==16)
    		{
    			vlc_numcoef=i+1;
    			break;
    		}
    	
    	}
    	ipos = -1;
    	for (i = 0; i<vlc_numcoef; i++)
    	{
    		ipos += run_buf[vlc_numcoef-1-i]+1;
    		ii = SCAN_4x4[ipos][0];
    		jj = SCAN_4x4[ipos][1];
    		
    		//Juncheng: It's for weighted quantization
    		block[jj*8 + ii] = (level_buf[vlc_numcoef-1-i]*dqm + dqa) >> dqs;
    	}
    }
    else	//inter
    {
    	CurrentVLCTable = 0;
    	PreviousRunCnt = 0;

    	for(i=0;i<17;i++)
    	{
    		symbol2D = cavs_bitstream_get_ue_k(&p->s,VLC_GC_Order_INTER[CurrentVLCTable][0]);
    		if(symbol2D==EOB_Pos_4x4[CurrentVLCTable])
    		{
    			vlc_numcoef = i;  // 0~i last is "0"? rm上的
    			break;
    		}
    		if(symbol2D >= ESCAPE_CODE_PART2)
    		{
    			abs_lev_diff = 2+(symbol2D-ESCAPE_CODE_PART2)/2;
    			mask = -(symbol2D & 1);//0或-1
    			symbol2D = cavs_bitstream_get_ue_k(&p->s,0);
    			run = symbol2D;
    			level = abs_lev_diff + LEVRUN_INTER[CurrentVLCTable][run];
    			level = (level^mask) - mask;
    		}
    		else if (symbol2D>=ESCAPE_CODE_PART1)
    		{
    			run =((symbol2D-ESCAPE_CODE_PART1)&31)/2;
    			level =1 + LEVRUN_INTER[CurrentVLCTable][run];
    			mask = -(symbol2D & 1);//0或-1
    			level = (level^mask) - mask;
    		} 
    		else
    		{
    			level = AVS_2DVLC_INTER_dec_4x4[CurrentVLCTable][symbol2D][0];
    			run   = AVS_2DVLC_INTER_dec_4x4[CurrentVLCTable][symbol2D][1];
    		}
    		level_buf[i] = level;
    		run_buf[i] = run;
    		PreviousRunCnt += run+1;

    		abslevel = abs(level);
    		if(abslevel>=incLevel_inter[CurrentVLCTable])
    		{
    			if(abslevel == 1)
    				CurrentVLCTable = (run>incRun_inter[CurrentVLCTable])?level1_inter[run]:CurrentVLCTable;
    			else if(abslevel == 2)
    				CurrentVLCTable = 4;
    			else if(abslevel <= 4)
    				CurrentVLCTable = 5;
    			else
    				CurrentVLCTable = 6;
    		}

    		if (PreviousRunCnt==16)
    		{
    			vlc_numcoef=i+1;
    			break;
    		}

    	}
    	ipos = -1;
    	for (i = 0; i<vlc_numcoef; i++)
    	{
    		ipos += run_buf[vlc_numcoef-1-i]+1;
    		ii = SCAN_4x4[ipos][0];
    		jj = SCAN_4x4[ipos][1];
    		block[jj*8 + ii] = (level_buf[vlc_numcoef-1-i]*dqm + dqa) >> dqs;
    	}

    }

    add4x4_idct(p_dest4x4, block, i_stride);
    memset(block, 0, 64*sizeof(DCTELEM));
    
    return 0;
}

static inline void scale_mv(cavs_decoder *p, int *d_x, int *d_y, xavs_vector *src, int distp) 
{

    int den;

    /*if(abs(src->ref) > 3)
    {
    	p->b_error_flag = 1;
    	return;
    }*/

    den = p->i_scale_den[src->ref];

    *d_x = (src->x*distp*den + 256 + (src->x>>31)) >> 9;
    *d_y = (src->y*distp*den + 256 + (src->y>>31)) >> 9;
}

static inline int mid_pred(int a, int b, int c)
{
    if(a>b){
        if(c>b){
            if(c>a) b=a;
            else    b=c;
        }
    }else{
        if(b>c){
            if(c>a) b=c;
            else    b=a;
        }
    }
    return b;

}

static inline void mv_pred_median(cavs_decoder *p, xavs_vector *mvP, xavs_vector *mvA, xavs_vector *mvB, xavs_vector *mvC)
{
    int ax, ay, bx, by, cx, cy;
    int len_ab, len_bc, len_ca, len_mid;
	if( abs(mvA->ref)>3 || abs(mvB->ref)>3 || abs(mvC->ref)>3 ){
		p->b_error_flag = 1;
		return;
	}

    /* scale candidates according to their temporal span */
    scale_mv(p, &ax, &ay, mvA, mvP->dist);
    //if(p->b_error_flag)
    	//return;
    scale_mv(p, &bx, &by, mvB, mvP->dist);
    //if(p->b_error_flag)
    	//return;
    scale_mv(p, &cx, &cy, mvC, mvP->dist);
    //if(p->b_error_flag)
    	//return;
    
    /* find the geometrical median of the three candidates */
    len_ab = abs(ax - bx) + abs(ay - by);
    len_bc = abs(bx - cx) + abs(by - cy);
    len_ca = abs(cx - ax) + abs(cy - ay);
    len_mid = mid_pred(len_ab, len_bc, len_ca);
    
    if(len_mid == len_ab) 
    {
        mvP->x = cx;
        mvP->y = cy;
    }
    else if(len_mid == len_bc) 
    {
        mvP->x = ax;
        mvP->y = ay;
    }
    else 
    {
        mvP->x = bx;
        mvP->y = by;
    }
}


static inline int get_residual_inter(cavs_decoder *p,int i_mb_type) 
{      
    int block;
    int i_stride = p->cur.i_stride[0];
    uint8_t *p_d;
    int i_qp_cb, i_qp_cr;
	int i_ret = 0;

    for(block = 0; block < 4; block++)
    {
        if(p->i_cbp & (1<<block))
        {
            p_d = p->p_y + p->i_luma_offset[block];
            if (p->vbs_mb_part_transform_4x4_flag[block])
            {
                int i;
                
                for (i = 0; i < 4; i++)
                    if (p->i_cbp_part[block] & (1<<i))
                        get_residual_block4x4(p, p->i_qp, p_d + (i>>1)*4*i_stride + (i&1)*4, i_stride, 0);
            }
            else
            {
                i_ret = get_residual_block(p, inter_2dvlc, 0, p->i_qp, p_d, i_stride, 0);
				if(i_ret == -1)
					return -1;
            }
        }
    }

    /* FIXIT : weighting quant qp for chroma
        NOTE : not modify p->i_qp directly
    */
    if( p->b_weight_quant_enable && !p->ph.chroma_quant_param_disable )
    { 
        i_qp_cb = clip3_int( p->i_qp + p->ph.chroma_quant_param_delta_u, 0, 63  );
        i_qp_cr = clip3_int( p->i_qp + p->ph.chroma_quant_param_delta_v, 0, 63 );
    }

    if(p->i_cbp & (1<<4))
    {
        i_ret = get_residual_block(p, chroma_2dvlc, 0, 
                              chroma_qp[p->b_weight_quant_enable && !p->ph.chroma_quant_param_disable ? i_qp_cb : p->i_qp],
                              p->p_cb, p->cur.i_stride[1], 1);
		if(i_ret == -1)
			return -1;
    }
    if(p->i_cbp & (1<<5))
    {
        i_ret = get_residual_block(p, chroma_2dvlc, 0, 
                              chroma_qp[ p->b_weight_quant_enable && !p->ph.chroma_quant_param_disable ? i_qp_cr : p->i_qp],
                              p->p_cr, p->cur.i_stride[2], 1);
		if(i_ret == -1)
			return -1;
    }

    return 0;
}


static inline void adapt_pred_mode(int i_num, int *p_mode) 
{
    static const uint32_t adapt_tale[4][/*8*/20] =
    {
    	{ 0,-1, 6,-1,-1, 7, 6, 7,  8, 9, I_PRED_4x4_DC_TOP, 11, 12, 13, 14, 15, 16, 17, 18, 19},
    	{-1, 1, 5,-1,-1, 5, 7, 7,  8, 9, I_PRED_4x4_DC_LEFT, 11, 12, 13, 14, 15, 16, 17, I_PRED_4x4_DC_128, 19},
    	{ 5,-1, 2,-1, 6, 5, 6,-1,  8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19},
    	{ 4, 1,-1,-1, 4, 6, 6,-1,  8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19}
    };
    
     *p_mode = adapt_tale[i_num][*p_mode];
    if(*p_mode  < 0) 
    {
        *p_mode  = 0;
    }
}

static inline void load_intra_pred_chroma(cavs_decoder *p) 
{
    int i_offset=p->i_mb_x*10;
    
    p->i_left_border_cb[9] = p->i_left_border_cb[8];
    p->i_left_border_cr[9] = p->i_left_border_cr[8];
    if(p->i_mb_x==p->i_mb_width-1)
    {
        p->p_top_border_cb[i_offset+9] = p->p_top_border_cb[i_offset+8];
        p->p_top_border_cr[i_offset+9] = p->p_top_border_cr[i_offset+8];
    }
    else
    {
        p->p_top_border_cb[i_offset+9] = p->p_top_border_cb[i_offset+11];
        p->p_top_border_cr[i_offset+9] = p->p_top_border_cr[i_offset+11];
    }

    if(p->i_mb_x && p->i_mb_y&&p->b_first_line==0) 
    {
        p->p_top_border_cb[i_offset] = p->i_left_border_cb[0] = p->i_topleft_border_cb;
        p->p_top_border_cr[i_offset] = p->i_left_border_cr[0] = p->i_topleft_border_cr;
    } 
    else
    {
        p->i_left_border_cb[0] = p->i_left_border_cb[1];
        p->i_left_border_cr[0] = p->i_left_border_cr[1];
        p->p_top_border_cb[i_offset] = p->p_top_border_cb[i_offset+1];
        p->p_top_border_cr[i_offset] = p->p_top_border_cr[i_offset+1];
    }
}

static void load_intra_4x4_pred_luma(cavs_decoder *p ,uint8_t *src, uint8_t edge[17], int idx)
{
#define SRC(x,y) src[(x) + (y)*i_stride]
#define EP (edge+8)
	int i_stride = p->cur.i_stride[0];
	int i_neighbor = p->i_neighbour4[idx];
	int have_lt = i_neighbor & MB_TOPLEFT;
	int have_tr = i_neighbor & MB_TOPRIGHT;
	int have_dl = i_neighbor & MB_DOWNLEFT;

	if( i_neighbor & MB_LEFT )
	{
		if (offset_block4x4[idx][0] == 0)
		{
			EP[-1] = p->i_left_border_y[1 + offset_block4x4[idx][1]*4 + 0];
			EP[-2] = p->i_left_border_y[1 + offset_block4x4[idx][1]*4 + 1];
			EP[-3] = p->i_left_border_y[1 + offset_block4x4[idx][1]*4 + 2];
			EP[-4] = p->i_left_border_y[1 + offset_block4x4[idx][1]*4 + 3];
			if (have_dl)
			{
				EP[-5] = p->i_left_border_y[1 + offset_block4x4[idx][1]*4 + 4];
				EP[-6] = p->i_left_border_y[1 + offset_block4x4[idx][1]*4 + 5];
				EP[-7] = p->i_left_border_y[1 + offset_block4x4[idx][1]*4 + 6];
				EP[-8] = p->i_left_border_y[1 + offset_block4x4[idx][1]*4 + 7];
			}
			else
			{
				MPIXEL_X4(&EP[-8]) = PIXEL_SPLAT_X4(EP[-4]);
			}
		}
		else
		{
			EP[-1] = SRC(-1, 0);
			EP[-2] = SRC(-1, 1);
			EP[-3] = SRC(-1, 2);
			EP[-4] = SRC(-1, 3);
			if (have_dl)
			{
				EP[-5] = SRC(-1, 4);
				EP[-6] = SRC(-1, 5);
				EP[-7] = SRC(-1, 6);
				EP[-8] = SRC(-1, 7);
			}
			else
			{
				//EP[-5] = EP[-6] = EP[-7] = EP[-8] = EP[-4];
				MPIXEL_X4(&EP[-8]) = PIXEL_SPLAT_X4(EP[-4]);
			}
		}
		
		EP[0] = EP[-1];
	}
	if (i_neighbor & MB_TOP)
	{
		if (offset_block4x4[idx][1] == 0)
		{
			MPIXEL_X4(&EP[1]) =
				MPIXEL_X4(&p->p_top_border_y[p->i_mb_x*XAVS_MB_SIZE+offset_block4x4[idx][0]*4]);
			if (have_tr)
			{
				MPIXEL_X4(&EP[5]) =
					MPIXEL_X4(&p->p_top_border_y[p->i_mb_x*XAVS_MB_SIZE+offset_block4x4[idx][0]*4+4]);
			}
			else
			{
				MPIXEL_X4(&EP[5]) =
					PIXEL_SPLAT_X4(p->p_top_border_y[p->i_mb_x*XAVS_MB_SIZE+offset_block4x4[idx][0]*4+3]);
			}
		}
		else
		{
			MPIXEL_X4(&EP[1]) = SRC_X4(0, -1);
			if (have_tr)
			{
				MPIXEL_X4(&EP[5]) = SRC_X4(4, -1);
			}
			else
			{
				MPIXEL_X4(&EP[5]) = PIXEL_SPLAT_X4(SRC(3, -1));
			}
		}
		
		EP[0] = EP[1];
	}

	if (have_lt)
	{
		if (idx == 0)
		{
			EP[0] = p->i_topleft_border_y;
		}
		else if (offset_block4x4[idx][0] == 0)
		{
			EP[0] = p->i_left_border_y[/*1 + */offset_block4x4[idx][1]*4 /*- 1*/];
		}
		else if (offset_block4x4[idx][1] == 0)
		{
			EP[0] = p->p_top_border_y[p->i_mb_x*XAVS_MB_SIZE + offset_block4x4[idx][0]*4 - 1];
		}
		else
		{
			EP[0] = SRC(-1,-1);
		}
	}
#undef SRC
#undef EP
}

static inline void load_intra_pred_luma(cavs_decoder *p, uint8_t *p_top,uint8_t **pp_left, int block) 
{
    int i;
    
    switch( block ) 
    {
        case 0:
            *pp_left = p->i_left_border_y;
            p->i_left_border_y[0] = p->i_left_border_y[1];
            //memset( &p->i_left_border_y[17], p->i_left_border_y[16], 9); 
            memcpy( &p_top[1], &p->p_top_border_y[p->i_mb_x*XAVS_MB_SIZE], 16);
            p_top[17] = p_top[16];
            p_top[0] = p_top[1];
            if((p->i_mb_flags & A_AVAIL) && (p->i_mb_flags & B_AVAIL))
                p->i_left_border_y[0] = p_top[0] = p->i_topleft_border_y;
            break;
            
        case 1:
            *pp_left = p->i_internal_border_y;
            for( i = 0; i < 8; i++)
            {
                p->i_internal_border_y[i+1] = *(p->p_y + 7 + i*p->cur.i_stride[0]);
            }
            
            //由于下边块（索引为3）未解码所以9以后的象素直接用象素8的值
            //当解码1，3块的时候由于左边块c[i]只有1~8是可用的，其他的还未解码，所以不可用
            //所以不可能采用Intra_8x8_Down_Left,Intra_8x8_Down_Right模式，所以不可能采用17位的象素
            memset( &p->i_internal_border_y[9], p->i_internal_border_y[8], 9);
            p->i_internal_border_y[0] = p->i_internal_border_y[1];
            memcpy( &p_top[1], &p->p_top_border_y[p->i_mb_x*XAVS_MB_SIZE+8], 8 );
            if( p->i_mb_flags & C_AVAIL )
                memcpy( &p_top[9], &p->p_top_border_y[(p->i_mb_x + 1)*XAVS_MB_SIZE], 8 );
            else
                memset( &p_top[9], p_top[8], 9 );
            p_top[17] =p_top[16];
            p_top[0] = p_top[1];
            if(p->i_mb_flags & B_AVAIL)
                p->i_internal_border_y[0] = p_top[0] = p->p_top_border_y[p->i_mb_x*XAVS_MB_SIZE+7];
            break;
            
        case 2:
            *pp_left = &p->i_left_border_y[8];
            memcpy( &p_top[1], p->p_y + 7*p->cur.i_stride[0], 16);
            p_top[17] = p_top[16];
            p_top[0] = p_top[1];
            if(p->i_mb_flags & A_AVAIL)
                p_top[0] = p->i_left_border_y[8];
            break;
        
        case 3:
            *pp_left = &p->i_internal_border_y[8];
            for(i=0;i<8;i++)
            {
                p->i_internal_border_y[i+9] = *(p->p_y + 7 + (i+8)*p->cur.i_stride[0]);
            }
            memset( &p->i_internal_border_y[17], p->i_internal_border_y[16], 9 );
            memcpy( &p_top[0], p->p_y + 7 + 7*p->cur.i_stride[0], 9 );
            memset( &p_top[9], p_top[8], 9 );
            break;
    }
}
#if 0
static inline void mv_pred_symex(cavs_decoder *p, xavs_vector *src, enum xavs_block_size size) 
{
    xavs_vector *dst = src + MV_BWD_OFFS;

    /* backward mv is the scaled and negated forward mv */
    dst->x = -((src->x * p->i_sym_factor + 256) >> 9);
    dst->y = -((src->y * p->i_sym_factor + 256) >> 9);
    dst->ref = 0;
    dst->dist = p->i_ref_distance[0];
    copy_mvs(dst, size);
}
#endif

static inline void mv_pred_sym(cavs_decoder *p, xavs_vector *src, enum xavs_block_size size,int i_ref) 
{
    xavs_vector *dst = src + MV_BWD_OFFS;

    /* backward mv is the scaled and negated forward mv */
    if(p->ph.b_picture_structure==0)
    {
        int  iBlockDistanceBw, iBlockDistanceFw;
        
        dst->ref = 1-i_ref;

        if( (abs(i_ref) > 1) 
        || (abs(dst->ref) > 3) )
        {
            p->b_error_flag = 1;
            return;
        }

#if 0
		if( p->param.b_accelerate )
		{
			iBlockDistanceFw = p->cur_aec.i_distance_index-p->ref_aec[i_ref+2].i_distance_index;
			iBlockDistanceBw = (p->ref_aec[dst->ref].i_distance_index-p->cur_aec.i_distance_index)%512;
		}
		else
		{
			iBlockDistanceFw = p->cur.i_distance_index-p->ref[i_ref+2].i_distance_index;
			iBlockDistanceBw = (p->ref[dst->ref].i_distance_index-p->cur.i_distance_index)%512;
		}		
#else
		if( p->param.b_accelerate )
		{
			iBlockDistanceFw = (p->cur_aec.i_distance_index-p->ref_aec[i_ref+2].i_distance_index+512)%512;
			iBlockDistanceBw = (p->ref_aec[dst->ref].i_distance_index-p->cur_aec.i_distance_index+512)%512;
		}
		else
		{
			iBlockDistanceFw = (p->cur.i_distance_index-p->ref[i_ref+2].i_distance_index+512)%512;
			iBlockDistanceBw = (p->ref[dst->ref].i_distance_index-p->cur.i_distance_index+512)%512;
		}
#endif    

        if( iBlockDistanceFw == 0 )
        {
            p->b_error_flag = 1;
            return;
        }
        
        dst->x = -((src->x * iBlockDistanceBw*(512/iBlockDistanceFw) + 256) >> 9);
        dst->y = -((src->y * iBlockDistanceBw*(512/iBlockDistanceFw) + 256) >> 9);
    }
    else
    {
        dst->ref = i_ref;
        dst->x = -((src->x * p->i_sym_factor + 256) >> 9);
        dst->y = -((src->y * p->i_sym_factor + 256) >> 9);
    }
    
    copy_mvs(dst, size);
}

//down8x8(for mvd context of cabac only): 1 if the block index is 2 or 3, 0 if it's 0 or 1
static void mv_pred(cavs_decoder *p, uint8_t nP,uint8_t nC,
                    enum xavs_mv_pred mode, enum xavs_block_size size, int ref, int mvd_scan_idx) 
{
    xavs_vector *mvP = &p->mv[nP];
    xavs_vector *mvA = &p->mv[nP-1];
    xavs_vector *mvB = &p->mv[nP-4];
    xavs_vector *mvC = &p->mv[nC];
    const xavs_vector *mvP2 = NULL;
    const int i_list = nP >= MV_BWD_OFFS;
    //	int i_ref_offset=i_fwd?0:(p->ph.b_picture_structure==0)

    mvP->ref = ref;
    mvP->dist = p->i_ref_distance[mvP->ref];
    if(mvC->ref == NOT_AVAIL)
        mvC = &p->mv[nP-5]; // set to top-left (mvD)
    if((mode == MV_PRED_PSKIP) &&
       ((mvA->ref == NOT_AVAIL) || (mvB->ref == NOT_AVAIL) ||
           ((mvA->x | mvA->y /*| mvA->ref*/) == 0 && mvA->ref == PSKIP_REF)  ||
           ((mvB->x | mvB->y /*| mvB->ref*/) == 0 && mvB->ref == PSKIP_REF) )) 
    {
        mvP2 = &MV_NOT_AVAIL;
        /* if there is only one suitable candidate, take it */
    }
    else if((mvA->ref >= 0) && (mvB->ref < 0) && (mvC->ref < 0)) 
    {
        mvP2= mvA;
    }
    else if((mvA->ref < 0) && (mvB->ref >= 0) && (mvC->ref < 0)) 
    {
        mvP2= mvB;
    }
    else if((mvA->ref < 0) && (mvB->ref < 0) && (mvC->ref >= 0)) 
    {
        mvP2= mvC;
    } 
    else if(mode == MV_PRED_LEFT     && mvA->ref == ref)
    {
        mvP2= mvA;
    }
    else if(mode == MV_PRED_TOP      && mvB->ref == ref)
    {
        mvP2= mvB;
    }
    else if(mode == MV_PRED_TOPRIGHT && mvC->ref == ref)
    {
        mvP2= mvC;
    }
    
    if(mvP2)
    {
        mvP->x = mvP2->x;
        mvP->y = mvP2->y;
    }
    else
    {
        mv_pred_median(p, mvP, mvA, mvB, mvC);
        if(p->b_error_flag)
            return;
    }

    if(mode < MV_PRED_PSKIP)
    {
#if DEBUG_P_INTER|DEBUG_B_INTER
       int16_t tmp_x, tmp_y;

       tmp_x = mvP->x;
       tmp_y = mvP->y; 
        
       {
                int tmp;
                FILE *fp;

                fp = fopen("data_mvp.dat","ab");
                fprintf(fp,"frame %d MB %d \n", p->i_frame_decoded, p->i_mb_index );
                fprintf(fp,"mvp = (%d, %d)\n", mvP->x, mvP->y);
                fclose(fp);
        }
#endif       

        mvP->x += p->bs_read_mvd(p, i_list, mvd_scan_idx, 0);
        if( p->b_error_flag )
        	return;
        mvP->y += p->bs_read_mvd(p, i_list, mvd_scan_idx, 1);
        if( p->b_error_flag )
        	return;

#if DEBUG_P_INTER|DEBUG_B_INTER      
        {
                int tmp;
                FILE *fp;

                fp = fopen("data_mvd.dat","ab");
                fprintf(fp,"frame %d MB %d mvd = (%d, %d)\n", p->i_frame_decoded, p->i_mb_index, mvP->x - tmp_x, mvP->y - tmp_y);
                fclose(fp);
        }
#endif       
        
    }
    copy_mvs(mvP,size);
}

static void mv_pred_sub_direct(cavs_decoder *p,xavs_vector *mv, int i_Offset,uint8_t nP,uint8_t nC,
                    enum xavs_mv_pred mode, enum xavs_block_size size, int ref) 
{
    xavs_vector *mvP = &p->mv[nP];
    xavs_vector *mvA = &mv[MV_FWD_A1+i_Offset];
    xavs_vector *mvB = &mv[MV_FWD_B2+i_Offset];
    xavs_vector *mvC = &mv[MV_FWD_C2+i_Offset];
    const xavs_vector *mvP2 = NULL;

    mvP->ref = ref;
    mvP->dist = p->i_ref_distance[mvP->ref];
    if(mvC->ref == NOT_AVAIL)
        mvC = &mv[MV_FWD_D3+i_Offset]; // set to top-left (mvD)
    if((mode == MV_PRED_PSKIP) &&
       ((mvA->ref == NOT_AVAIL) || (mvB->ref == NOT_AVAIL) ||
           ((mvA->x | mvA->y | mvA->ref) == 0)  ||
           ((mvB->x | mvB->y | mvB->ref) == 0) )) 
    {
        mvP2 = &MV_NOT_AVAIL;
        /* if there is only one suitable candidate, take it */
    }
    else if((mvA->ref >= 0) && (mvB->ref < 0) && (mvC->ref < 0)) 
    {
        mvP2= mvA;
    }
    else if((mvA->ref < 0) && (mvB->ref >= 0) && (mvC->ref < 0)) 
    {
        mvP2= mvB;
    }
    else if((mvA->ref < 0) && (mvB->ref < 0) && (mvC->ref >= 0)) 
    {
        mvP2= mvC;
    } 
    else if(mode == MV_PRED_LEFT     && mvA->ref == ref)
    {
        mvP2= mvA;
    }
    else if(mode == MV_PRED_TOP      && mvB->ref == ref)
    {
        mvP2= mvB;
    }
    else if(mode == MV_PRED_TOPRIGHT && mvC->ref == ref)
    {
        mvP2= mvC;
    }
    if(mvP2)
    {
        mvP->x = mvP2->x;
        mvP->y = mvP2->y;
    }
    else
    {
        mv_pred_median(p, mvP, mvA, mvB, mvC);
		if(p->b_error_flag)
			return;
    }
}

static inline void get_b_frame_ref(cavs_decoder *p, int *p_ref)
{
    if(p->ph.b_picture_structure==0&&p->ph.b_picture_reference_flag==0 )
    {
        *p_ref = cavs_bitstream_get_bit1(&p->s);	//mb_reference_index
        //*p_ref = p->bs_read[SYNTAX_REF_B](p);
    }
    else
    {	
        *p_ref=0;
    }
}

static const uint8_t mv_scan[4] = 
{
	MV_FWD_X0,MV_FWD_X1,
	MV_FWD_X2,MV_FWD_X3
};

static const uint8_t mvd_scan[4] =
{
	1, 2, 4, 5
};

static const uint8_t ref_scan[4] =
{
	4, 5, 7, 8
};
#if 0
static inline void mv_pred_directex(cavs_decoder *p, xavs_vector *pmv_fw, xavs_vector *col_mv)
{
    xavs_vector *pmv_bw = pmv_fw + MV_BWD_OFFS;
    int den = p->i_direct_den[col_mv->ref];
    int m = col_mv->x >> 31;

    pmv_fw->dist = p->i_ref_distance[1];
    pmv_bw->dist = p->i_ref_distance[0];
    pmv_fw->ref = 1;
    pmv_bw->ref = 0;
    /* scale the co-located motion vector according to its temporal span */
    if(m)
    {
        pmv_fw->x = -((den-den*col_mv->x*pmv_fw->dist-1)>>14);
        pmv_bw->x = (den-den*col_mv->x*pmv_bw->dist-1)>>14;
    }
    else
    {
        pmv_fw->x = ((den+den*col_mv->x*pmv_fw->dist-1)>>14);
        pmv_bw->x = -((den+den*col_mv->x*pmv_bw->dist-1)>>14);
    }
    
    m = col_mv->y >> 31;
    if(m)
    {
        pmv_fw->y = -((den-den*col_mv->y*pmv_fw->dist-1)>>14);
        pmv_bw->y = (den-den*col_mv->y*pmv_bw->dist-1)>>14;
    }
    else
    {
        pmv_fw->y = ((den+den*col_mv->y*pmv_fw->dist-1)>>14);
        pmv_bw->y = -((den+den*col_mv->y*pmv_bw->dist-1)>>14);
    }
}
#endif

static inline void mv_pred_direct(cavs_decoder *p, xavs_vector *pmv_fw, xavs_vector *col_mv,
	int iDistanceIndexRef, int iDistanceIndexFw, int iDistanceIndexBw,
	int deltaRef, int deltaMvFw, int deltaMvBw) 
{
    int iDistanceIndexCur = p->param.b_accelerate?p->cur_aec.i_distance_index:p->cur.i_distance_index;
    int iBlockDistanceRef;
    xavs_vector *pmv_bw = pmv_fw + MV_BWD_OFFS;	

    int den;
    int m = col_mv->x >> 31;

    /*if(abs(col_mv->ref) > 3)
    {
        p->b_error_flag = 1;
        return;
    }*/
    
    //den = p->i_direct_den[col_mv->ref];
    
	iBlockDistanceRef = (iDistanceIndexBw - iDistanceIndexRef + 512) & 0x1ff; // % 512;
	pmv_fw->dist = (iDistanceIndexCur - iDistanceIndexFw + 512) & 0x1ff; // % 512;
	pmv_bw->dist = (iDistanceIndexBw - iDistanceIndexCur + 512) & 0x1ff; // % 512;
    den = (iBlockDistanceRef == 0) ? 0 : 16384/iBlockDistanceRef;

    if(m)
    {	
        pmv_fw->x = -((den-den*col_mv->x*pmv_fw->dist-1)>>14);
        pmv_bw->x = (den-den*col_mv->x*pmv_bw->dist-1)>>14;
    }
    else
    {
        pmv_fw->x = ((den+den*col_mv->x*pmv_fw->dist-1)>>14);
        pmv_bw->x = -((den+den*col_mv->x*pmv_bw->dist-1)>>14);
    }
    
    m = (col_mv->y + deltaRef) >> 31;
    if(m)
    {
        pmv_fw->y = -((den-den*(col_mv->y+deltaRef)*pmv_fw->dist-1)>>14) - deltaMvFw;
        pmv_bw->y = ((den-den*(col_mv->y+deltaRef)*pmv_bw->dist-1)>>14) - deltaMvBw;
    }
    else
    {
        pmv_fw->y = ((den+den*(col_mv->y+deltaRef)*pmv_fw->dist-1)>>14) - deltaMvFw;
        pmv_bw->y = -((den+den*(col_mv->y+deltaRef)*pmv_bw->dist-1)>>14) - deltaMvBw;
    }
}

static inline void get_col_info(cavs_decoder *p, uint8_t *p_col_type, xavs_vector **pp_mv, int block)
{
    int i_mb_offset = p->i_mb_index >= p->i_mb_num_half ? p->i_mb_num_half : 0;
    int b_interlaced = p->param.b_interlaced;

    if( !b_interlaced )
    {
    	*pp_mv = &p->p_col_mv[(p->i_mb_index /*- NEW_DIRECT * i_mb_offset*/)*4+block];
    	*p_col_type = p->p_col_type_base[(p->i_mb_index/* - NEW_DIRECT * i_mb_offset*/)];
    }
    else
    {
    	*pp_mv = &p->p_col_mv[(p->i_mb_index - NEW_DIRECT * i_mb_offset)*4+block];
    	*p_col_type = p->p_col_type_base[(p->i_mb_index - NEW_DIRECT * i_mb_offset)];
    }

    if( p->param.b_accelerate )
    {
        if( p->p_save_aec[1] == NULL )
        {
            p->b_error_flag = 1;
            return;
        }

        if(p->ph.b_picture_structure && p->p_save_aec[1]->b_picture_structure == 0)
        {
            i_mb_offset = p->i_mb_y/2*p->i_mb_width;
            *p_col_type = p->p_col_type_base[i_mb_offset + p->i_mb_x];
            *pp_mv = &p->p_col_mv[(i_mb_offset+p->i_mb_x)*4+(block%2)+2*(p->i_mb_y%2)];
        }
        
        if(p->ph.b_picture_structure == 0 && p->p_save_aec[1]->b_picture_structure)
        {
            i_mb_offset = p->i_mb_y*p->i_mb_width-i_mb_offset;
            i_mb_offset *= 2;
            if(block > 1)
                i_mb_offset += p->i_mb_width;
            *p_col_type = p->p_col_type_base[i_mb_offset + p->i_mb_x];
            *pp_mv = &p->p_col_mv[(i_mb_offset+p->i_mb_x)*4+(block%2)];
        }
    }
    else
    {
        if( p->p_save[1] == NULL )
        {
            p->b_error_flag = 1;
            return;
        }

        if(p->ph.b_picture_structure && p->p_save[1]->b_picture_structure == 0)
        {
            i_mb_offset = p->i_mb_y/2*p->i_mb_width;
            *p_col_type = p->p_col_type_base[i_mb_offset + p->i_mb_x];
            *pp_mv = &p->p_col_mv[(i_mb_offset+p->i_mb_x)*4+(block%2)+2*(p->i_mb_y%2)];
        }
        
        if(p->ph.b_picture_structure == 0 && p->p_save[1]->b_picture_structure)
        {
            i_mb_offset = p->i_mb_y*p->i_mb_width-i_mb_offset;
            i_mb_offset *= 2;
            if(block > 1)
                i_mb_offset += p->i_mb_width;
            *p_col_type = p->p_col_type_base[i_mb_offset + p->i_mb_x];
            *pp_mv = &p->p_col_mv[(i_mb_offset+p->i_mb_x)*4+(block%2)];
        }
    }
    
}

static void get_b_direct_skip_sub_mb(cavs_decoder *p,int block,xavs_vector *p_mv)
{
    xavs_vector temp_mv;
    uint32_t b_next_field = p->i_mb_index >= p->i_mb_num_half;
    int iDistanceIndexFw, iDistanceIndexBw, iDistanceIndexRef;
    int iDistanceIndexFw0, iDistanceIndexFw1, iDistanceIndexBw0, iDistanceIndexBw1;
    int deltaRef = 0, deltaMvBw = 0, deltaMvFw = 0;

    if( p->param.b_accelerate )
    {
        if( p->p_save_aec[1] == NULL 
        || p->p_save_aec[2] == NULL )
        {
            p->b_error_flag = 1;
            return;
        }

        temp_mv = p_mv[0];
        if(p->ph.b_picture_structure) /* frame */
        {
            /* direct prediction from co-located P MB, block-wise */
            if(p->p_save_aec[1]->b_picture_structure == 0) /* ref-field */
            {
                temp_mv.y *= 2;
                if( abs(temp_mv.ref)/2 > 1 )
                {
                    p->b_error_flag = 1;
                    return;
                }
                iDistanceIndexRef = p->p_save_aec[1]->i_ref_distance[temp_mv.ref/2];
            }
            else
            {
                if( abs(temp_mv.ref) > 1 )
                {
                    p->b_error_flag = 1;
                    return;
                }
                iDistanceIndexRef = p->p_save_aec[1]->i_ref_distance[temp_mv.ref];
            }
            iDistanceIndexFw = p->p_save_aec[2]->i_distance_index;
            iDistanceIndexBw = p->p_save_aec[1]->i_distance_index;
            p->mv[mv_scan[block]].ref = 1;
            p->mv[mv_scan[block]+MV_BWD_OFFS].ref = 0;

            mv_pred_direct(p,&p->mv[mv_scan[block]], &temp_mv,
                    iDistanceIndexRef, iDistanceIndexFw, iDistanceIndexBw, 0, 0, 0);
            //if( p->b_error_flag )
                //return;
        } 
        else /* field */ 
        {
            if(p->p_save_aec[1]->b_picture_structure == 1) /* ref-frame */
            {
                temp_mv.y /= 2;
                if( temp_mv.ref > 1 )
                {
                    p->b_error_flag = 1;
                    return;
                }
                iDistanceIndexRef = p->p_save_aec[1]->i_ref_distance[temp_mv.ref] + 1;	
            }
            else
            {
                int i_ref = temp_mv.ref;

                if( (abs(temp_mv.ref)>>1) > 1 )
                {
                    p->b_error_flag = 1;
                    return;
                }
                    
                if (NEW_DIRECT)
                {
                    iDistanceIndexRef = p->p_save_aec[1]->i_ref_distance[i_ref>>1] + !(i_ref & 1); /* NOTE : refer to reverse field */
                }
                else if(b_next_field && i_ref == 0)
                {
                    iDistanceIndexRef = p->p_save_aec[1]->i_distance_index;
                }
                else
                {
                    i_ref -= b_next_field;
                    iDistanceIndexRef = p->p_save_aec[1]->i_ref_distance[i_ref>>1] - (i_ref&1) + 1;
                }
            }

            /* set as field reference list */
            iDistanceIndexFw0 = p->p_save_aec[2]->i_distance_index+1;
            iDistanceIndexFw1 = p->p_save_aec[2]->i_distance_index;
            iDistanceIndexBw0 = p->p_save_aec[1]->i_distance_index;
            iDistanceIndexBw1 = p->p_save_aec[1]->i_distance_index+1;

            if(iDistanceIndexRef == iDistanceIndexFw0 || (NEW_DIRECT & b_next_field))
            {
                iDistanceIndexFw = iDistanceIndexFw0;
                p->mv[mv_scan[block]].ref = 2;
            }
            else
            {
                iDistanceIndexFw = iDistanceIndexFw1;
                p->mv[mv_scan[block]].ref = 3;
            }

            if(b_next_field==0 || NEW_DIRECT)
            {
                iDistanceIndexBw = iDistanceIndexBw0;
                p->mv[mv_scan[block]+MV_BWD_OFFS].ref = 0;
            }
            else
            {
                iDistanceIndexBw = iDistanceIndexBw1;
                p->mv[mv_scan[block]+MV_BWD_OFFS].ref = 1;
            }

            if (NEW_DIRECT)
            {
                deltaRef = ((temp_mv.ref & 1) ^ 1) << 1;
                deltaMvFw = (p->mv[mv_scan[block]].ref-2 == b_next_field) << 1;
                deltaMvBw = b_next_field ? -2 : 0;
            }

            mv_pred_direct(p,&p->mv[mv_scan[block]], &temp_mv,
                                    iDistanceIndexRef, iDistanceIndexFw, iDistanceIndexBw, deltaRef, deltaMvFw, deltaMvBw);
            //if( p->b_error_flag )
                //return;
        }
         
    }
    else
    {
		if( p->p_save[1] == NULL 
			|| p->p_save[2] == NULL )
		{
			p->b_error_flag = 1;
			return;
		}

        temp_mv = p_mv[0];
        if(p->ph.b_picture_structure) /* frame */
        {
            /* direct prediction from co-located P MB, block-wise */
            if(p->p_save[1]->b_picture_structure == 0) /* ref-field */
            {
                temp_mv.y *= 2;
                if( abs(temp_mv.ref) /2 > 1 )
                {
                    p->b_error_flag = 1;
                    return;
                }
                iDistanceIndexRef = p->p_save[1]->i_ref_distance[temp_mv.ref/2];
            }
            else
            {
                if( abs(temp_mv.ref) > 1 )
                {
                    p->b_error_flag = 1;
                    return;
                }
                iDistanceIndexRef = p->p_save[1]->i_ref_distance[temp_mv.ref];
            }
            iDistanceIndexFw = p->p_save[2]->i_distance_index;
            iDistanceIndexBw = p->p_save[1]->i_distance_index;
            p->mv[mv_scan[block]].ref = 1;
            p->mv[mv_scan[block]+MV_BWD_OFFS].ref = 0;

            mv_pred_direct(p,&p->mv[mv_scan[block]], &temp_mv,
                                    iDistanceIndexRef, iDistanceIndexFw, iDistanceIndexBw, 0, 0, 0);
            //if( p->b_error_flag )
                //return;
        }	
        else /* field */ 
        {
            if(p->p_save[1]->b_picture_structure == 1) /* ref-frame */
            {
                temp_mv.y /= 2;
                if( abs(temp_mv.ref) > 1 )
                {
                    p->b_error_flag = 1;
                    return;
                }
                iDistanceIndexRef = p->p_save[1]->i_ref_distance[temp_mv.ref] + 1;	
            }
            else
            {
                int i_ref = temp_mv.ref;

                if( (abs(temp_mv.ref)>>1) > 1 )
                {
                    p->b_error_flag = 1;
                    return;
                }
        
                if (NEW_DIRECT)
                {
                    iDistanceIndexRef = p->p_save[1]->i_ref_distance[i_ref>>1] + !(i_ref & 1); /* NOTE : refer to reverse field */
                }
                else if(b_next_field && i_ref == 0)
                {
                    iDistanceIndexRef = p->p_save[1]->i_distance_index;
                }
                else
                {
                    i_ref -= b_next_field;
                    iDistanceIndexRef = p->p_save[1]->i_ref_distance[i_ref>>1] - (i_ref&1) + 1;
                }
            }

            /* set as field reference list */
            iDistanceIndexFw0 = p->p_save[2]->i_distance_index+1;
            iDistanceIndexFw1 = p->p_save[2]->i_distance_index;
            iDistanceIndexBw0 = p->p_save[1]->i_distance_index;
            iDistanceIndexBw1 = p->p_save[1]->i_distance_index+1;

            if(iDistanceIndexRef == iDistanceIndexFw0 || (NEW_DIRECT & b_next_field))
            {
                iDistanceIndexFw = iDistanceIndexFw0;
                p->mv[mv_scan[block]].ref = 2;
            }
            else
            {
                iDistanceIndexFw = iDistanceIndexFw1;
                p->mv[mv_scan[block]].ref = 3;
            }

            if(b_next_field==0 || NEW_DIRECT)
            {
                iDistanceIndexBw = iDistanceIndexBw0;
                p->mv[mv_scan[block]+MV_BWD_OFFS].ref = 0;
            }
            else
            {
                iDistanceIndexBw = iDistanceIndexBw1;
                p->mv[mv_scan[block]+MV_BWD_OFFS].ref = 1;
            }

            if (NEW_DIRECT)
            {
                deltaRef = ((temp_mv.ref & 1) ^ 1) << 1;
                deltaMvFw = (p->mv[mv_scan[block]].ref-2 == b_next_field) << 1;
                deltaMvBw = b_next_field ? -2 : 0;
            }

            mv_pred_direct(p,&p->mv[mv_scan[block]], &temp_mv,
                                    iDistanceIndexRef, iDistanceIndexFw, iDistanceIndexBw, deltaRef, deltaMvFw, deltaMvBw);
            //if( p->b_error_flag )
                //return;
        }
    }
}

static void get_b_direct_skip_mb(cavs_decoder *p)
{
    xavs_vector mv[24];
    int block;
    //uint32_t b_next_field = p->i_mb_index >= p->i_mb_num_half;
    int i_ref_offset = p->ph.b_picture_structure == 0 ? 2 : 1;
    uint8_t i_col_type;
    xavs_vector *p_mv;

    mv[MV_FWD_A1] = p->mv[MV_FWD_A1];
    mv[MV_FWD_B2] = p->mv[MV_FWD_B2];
    mv[MV_FWD_C2] = p->mv[MV_FWD_C2];
    mv[MV_FWD_D3] = p->mv[MV_FWD_D3];
    mv[MV_BWD_A1] = p->mv[MV_BWD_A1];
    mv[MV_BWD_B2] = p->mv[MV_BWD_B2];
    mv[MV_BWD_C2] = p->mv[MV_BWD_C2];
    mv[MV_BWD_D3] = p->mv[MV_BWD_D3];
    for( block = 0; block < 4; block++ )
    {
        get_col_info(p, &i_col_type, &p_mv, block);
        if( !i_col_type ) /* col_type is I_8x8 */ 
        {
            mv_pred_sub_direct(p, mv, 0, mv_scan[block], mv_scan[block]-3,
                                                MV_PRED_BSKIP, BLK_8X8, i_ref_offset);
			if(p->b_error_flag)
				return;
            mv_pred_sub_direct(p, mv, MV_BWD_OFFS, mv_scan[block]+MV_BWD_OFFS,
                                        	mv_scan[block]-3+MV_BWD_OFFS,
                                        	MV_PRED_BSKIP, BLK_8X8, 0);
        }
        else
        {
            get_b_direct_skip_sub_mb(p,block,p_mv);
        }
		
		if(p->b_error_flag)
        {
            printf("[error]MB %d mv direct is wrong\n", p->i_mb_index );
            return;
        }
    }
}

static int get_mb_b(cavs_decoder *p)
{   
    int kk = 0; 
    xavs_vector mv[24];
    int block;
    enum xavs_mb_sub_type sub_type[4];
    int flags;
    int ref[4];    
    int i_ref_offset = p->ph.b_picture_structure == 0 ? 2 : 1;
    uint8_t i_col_type;
    xavs_vector *p_mv;
    int i_mb_type = p->i_mb_type;
    int16_t (*p_mvd)[6][2] = p->p_mvd;
    int8_t	(*p_ref)[9] = p->p_ref;
	int i_ret = 0;
    
    init_mb(p);

    p->mv[MV_FWD_X0] = MV_REF_DIR;
    copy_mvs(&p->mv[MV_FWD_X0], BLK_16X16);
    p->mv[MV_BWD_X0] = MV_REF_DIR;
    copy_mvs(&p->mv[MV_BWD_X0], BLK_16X16);

#define FWD		0
#define BWD		1
#define MVD_X0	1
#define MVD_X1	2
#define MVD_X2	4
#define MVD_X3	5
#define REF_X0  4
#define REF_X1  5
#define REF_X2  7
#define REF_X3  8

    /* The MVD of pos X[0-3] have been initialized as 0
        The REF of pos X[0-3] have been initialized as -1 */
    switch(i_mb_type) 
    {
        case B_SKIP:
        case B_DIRECT:
            get_b_direct_skip_mb(p);
            if(p->b_error_flag)
            {
                printf("[error]MB %d mv direct is wrong\n", p->i_mb_index );
                return -1;
            }
            break;
            
        case B_FWD_16X16:
            ref[0] = p->bs_read_ref_b(p, FWD, REF_X0);
            if( ref[0] > 1 || p->b_error_flag )
            {
                printf("[error]MB %d refidx is wrong\n", p->i_mb_index );
                p->b_error_flag = 1;
                
                return -1;
            }
            mv_pred(p, MV_FWD_X0, MV_FWD_C2, MV_PRED_MEDIAN, BLK_16X16, ref[0]+i_ref_offset, MVD_X0);
            if( p->b_error_flag )
            {
                printf("[error]MB %d mvd is wrong\n", p->i_mb_index );
                
                return -1;
            }
            
            M32(p_mvd[FWD][MVD_X1]) = M32(p_mvd[FWD][MVD_X3]) = M32(p_mvd[FWD][MVD_X0]);
            p_ref[FWD][REF_X1] = p_ref[FWD][REF_X2] = p_ref[FWD][REF_X3] = p_ref[FWD][REF_X0];
            break;
            
        case B_SYM_16X16:
            ref[0] = p->bs_read_ref_b(p, FWD, REF_X0);
            if( ref[0] > 1 || p->b_error_flag )
            {
                printf("[error]MB %d refidx is wrong\n", p->i_mb_index );
                p->b_error_flag = 1;
                
                return -1;
            }
            mv_pred(p, MV_FWD_X0, MV_FWD_C2, MV_PRED_MEDIAN, BLK_16X16, ref[0]+i_ref_offset, MVD_X0);
            if( p->b_error_flag )
            {
                printf("[error]MB %d mvd is wrong\n", p->i_mb_index );
                
                return -1;
            }
            mv_pred_sym(p, &p->mv[MV_FWD_X0], BLK_16X16,ref[0]);
            if(p->b_error_flag)
            {
                printf("[error]MB %d mv sym is wrong\n", p->i_mb_index );
                return -1;
            }
            
            M32(p_mvd[FWD][MVD_X1]) = M32(p_mvd[FWD][MVD_X3]) = M32(p_mvd[FWD][MVD_X0]);
            p_ref[FWD][REF_X1] = p_ref[FWD][REF_X2] = p_ref[FWD][REF_X3] = p_ref[FWD][REF_X0];
            break;
            
        case B_BWD_16X16:
            ref[0] = p->bs_read_ref_b(p, BWD, REF_X0);
            if( ref[0] > 1 || p->b_error_flag )
            {
                printf("[error]MB %d refidx is wrong\n", p->i_mb_index );
                p->b_error_flag = 1;
                
                return -1;
            }
            mv_pred(p, MV_BWD_X0, MV_BWD_C2, MV_PRED_MEDIAN, BLK_16X16, ref[0], MVD_X0);
            if( p->b_error_flag )
            {
                printf("[error]MB %d mvd is wrong\n", p->i_mb_index );
                
                return -1;
            }
            M32(p_mvd[BWD][MVD_X1]) = M32(p_mvd[BWD][MVD_X3]) = M32(p_mvd[BWD][MVD_X0]);
            p_ref[BWD][REF_X1] = p_ref[BWD][REF_X2] = p_ref[BWD][REF_X3] = p_ref[BWD][REF_X0];
            break;
            
        case B_8X8:
            mv[MV_FWD_A1] = p->mv[MV_FWD_A1];
            mv[MV_FWD_B2] = p->mv[MV_FWD_B2];
            mv[MV_FWD_C2] = p->mv[MV_FWD_C2];
            mv[MV_FWD_D3] = p->mv[MV_FWD_D3];
            mv[MV_BWD_A1] = p->mv[MV_BWD_A1];
            mv[MV_BWD_B2] = p->mv[MV_BWD_B2];
            mv[MV_BWD_C2] = p->mv[MV_BWD_C2];
            mv[MV_BWD_D3] = p->mv[MV_BWD_D3];

            for(block = 0; block < 4; block++)
            {    
                sub_type[block] = (enum xavs_mb_sub_type)p->bs_read[SYNTAX_MB_PART_TYPE](p);
                if(sub_type[block] < 0 || sub_type[block] > 3 || p->b_error_flag )
                {
                    printf("[error]MB %d sub_type is wrong\n", p->i_mb_index );
                    p->b_error_flag = 1;
                
                    return -1;
                }
            }
            
            for( block = 0; block < 4; block++)
            {
                if(sub_type[block] == B_SUB_DIRECT || sub_type[block] == B_SUB_BWD)
                    ref[block] = 0;
                else
                    ref[block] = p->bs_read_ref_b(p, FWD, ref_scan[block]);
                if( ref[block] > 1 || p->b_error_flag )
                {
                    printf("[error]MB %d refidx is wrong\n", p->i_mb_index );
                    p->b_error_flag = 1;
                
                    return -1;
                }
            }
            
            for(block = 0; block < 4; block++)
            {
                if(sub_type[block] == B_SUB_BWD)
                {
                    ref[block] = p->bs_read_ref_b(p, BWD, ref_scan[block]);
                }
                if( ref[block] > 1 || p->b_error_flag )
                {
                    printf("[error]MB %d refidx is wrong\n", p->i_mb_index );
                    p->b_error_flag = 1;
                
                    return -1;
                }
            }
            
            for( block = 0; block < 4; block++ ) 
            {
                switch(sub_type[block])
                {
                    case B_SUB_DIRECT:
                        get_col_info(p, &i_col_type, &p_mv, block);
                        if(!i_col_type)
                        {
                            mv_pred_sub_direct(p, mv, 0, mv_scan[block], mv_scan[block]-3,
                                    MV_PRED_BSKIP, BLK_8X8, i_ref_offset);
							if(p->b_error_flag)
								return -1;
                            mv_pred_sub_direct(p, mv, MV_BWD_OFFS, mv_scan[block]+MV_BWD_OFFS,
                                    mv_scan[block]-3+MV_BWD_OFFS,
                                    MV_PRED_BSKIP, BLK_8X8, 0);
                        } 
                        else
                        {
                            get_b_direct_skip_sub_mb(p, block, p_mv);
                        }
						
                        if(p->b_error_flag)
                        {
                            printf("[error]MB %d mv direct is wrong\n", p->i_mb_index );
                            return -1;
                        }
                        break;
                        
                    case B_SUB_FWD:
                        mv_pred(p, mv_scan[block], mv_scan[block]-3,
                                    MV_PRED_MEDIAN, BLK_8X8, ref[block]+i_ref_offset, mvd_scan[block]);
                        if( p->b_error_flag )
                        {
                            printf("[error]MB %d mvd is wrong\n", p->i_mb_index );
                            
                            return -1;
                        }
                        break;
                        
                    case B_SUB_SYM:
                        mv_pred(p, mv_scan[block], mv_scan[block]-3,
                                        MV_PRED_MEDIAN, BLK_8X8, ref[block]+i_ref_offset, mvd_scan[block]);
                        if( p->b_error_flag )
                        {
                            printf("[error]MB %d mvd is wrong\n", p->i_mb_index );

                            return -1;
                        }
                        mv_pred_sym(p, &p->mv[mv_scan[block]], BLK_8X8,ref[block]);
                        if(p->b_error_flag)
                        {
                            printf("[error]MB %d mv sym is wrong\n", p->i_mb_index );
                            return -1;
                        }
                             
                        break;
					default:
                    	break;
                }
            }
            for( block = 0; block < 4; block++ )
            {
                if(sub_type[block] == B_SUB_BWD)
                {
                    mv_pred(p, mv_scan[block]+MV_BWD_OFFS,
                            mv_scan[block]+MV_BWD_OFFS-3,
                            MV_PRED_MEDIAN, BLK_8X8, ref[block], mvd_scan[block]);
                    if( p->b_error_flag )
                    {
                        printf("[error]MB %d mvd is wrong\n", p->i_mb_index );

                        return -1;
                    }
                }
            }
            break;
        default:
            flags = partition_flags[i_mb_type];
            if( i_mb_type <= B_SYM_16X16 )
            {
                printf("[error]MB %d mb_type is wrong\n", p->i_mb_index );
                p->b_error_flag = 1;

                return -1;
            }
            if(i_mb_type & 1) /* 16x8 macroblock types */
            { 
                int i = 0, k = 0;
                
                if (flags & FWD0)
                	ref[i++] = p->bs_read_ref_b(p, FWD, REF_X0);
                if (flags & FWD1)
                	ref[i++] = p->bs_read_ref_b(p, FWD, REF_X2);
                if (flags & BWD0)
                	ref[i++] = p->bs_read_ref_b(p, BWD, REF_X0);
                if (flags & BWD1)
                	ref[i++] = p->bs_read_ref_b(p, BWD, REF_X2);

                k = i;
                for( i = 0 ; i < k; i++ )
                {
                    if( ref[i] > 1 || p->b_error_flag )
                    {
                        printf("[error]MB %d refidx is wrong\n", p->i_mb_index );
                        p->b_error_flag = 1;
                
                        return -1;
                    }    
                }
                
                if(flags & FWD0)
                {
                    mv_pred(p, MV_FWD_X0, MV_FWD_C2, MV_PRED_TOP,  BLK_16X8, ref[kk++]+i_ref_offset, MVD_X0);
                    if( p->b_error_flag )
                    {
                        printf("[error]MB %d mvd is wrong\n", p->i_mb_index );

                        return -1;
                    }
                    CP32(p_mvd[FWD][MVD_X1], p_mvd[FWD][MVD_X0]);
                    p_ref[FWD][REF_X1] = p_ref[FWD][REF_X0];
                }
                
                if(flags & SYM0)
                {    
                    mv_pred_sym(p, &p->mv[MV_FWD_X0], BLK_16X8,ref[kk-1]);
                    if(p->b_error_flag)
                    {
                        printf("[error]MB %d mv sym is wrong\n", p->i_mb_index );
                        return -1;
                    }
                }
                if(flags & FWD1)
                {
                    mv_pred(p, MV_FWD_X2, MV_FWD_A1, MV_PRED_LEFT, BLK_16X8, ref[kk++]+i_ref_offset, MVD_X2);
                    if( p->b_error_flag )
                    {
                        printf("[error]MB %d mvd is wrong\n", p->i_mb_index );

                        return -1;
                    }
                    CP32(p_mvd[FWD][MVD_X3], p_mvd[FWD][MVD_X2]);
                    p_ref[FWD][REF_X3] = p_ref[FWD][REF_X2];
                }
                if(flags & SYM1)
                {    
                    mv_pred_sym(p, &p->mv[MV_FWD_X2], BLK_16X8,ref[kk-1]);
                    if(p->b_error_flag)
                    {
                        printf("[error]MB %d mv sym is wrong\n", p->i_mb_index );
                        return -1;
                    }
                }
                if(flags & BWD0)
                {
                    mv_pred(p, MV_BWD_X0, MV_BWD_C2, MV_PRED_TOP,  BLK_16X8, ref[kk++], MVD_X0);
                    if( p->b_error_flag )
                    {
                        printf("[error]MB %d mvd is wrong\n", p->i_mb_index );

                        return -1;
                    }
                    CP32(p_mvd[BWD][MVD_X1], p_mvd[BWD][MVD_X0]);
                    p_ref[BWD][REF_X1] = p_ref[BWD][REF_X0];
                }
                if(flags & BWD1)
                {
                    mv_pred(p, MV_BWD_X2, MV_BWD_A1, MV_PRED_LEFT, BLK_16X8, ref[kk++], MVD_X2);
                    if( p->b_error_flag )
                    {
                        printf("[error]MB %d mvd is wrong\n", p->i_mb_index );

                        return -1;
                    }
                    CP32(p_mvd[BWD][MVD_X3], p_mvd[BWD][MVD_X2]);
                    p_ref[BWD][REF_X3] = p_ref[BWD][REF_X2];
                }
            } 
            else  /* 8x16 macroblock types */
            {
                int i = 0, k = 0;
                
                if (flags & FWD0)
                	ref[i++] = p->bs_read_ref_b(p, FWD, REF_X0);
                if (flags & FWD1)
                	ref[i++] = p->bs_read_ref_b(p, FWD, REF_X1);
                if (flags & BWD0)
                	ref[i++] = p->bs_read_ref_b(p, BWD, REF_X0);
                if (flags & BWD1)
                	ref[i++] = p->bs_read_ref_b(p, BWD, REF_X1);

                k = i;
                for( i = 0 ; i < k; i++ )
                {
                    if( ref[i] > 1 || p->b_error_flag )
                    {
                        printf("[error]MB %d refidx is wrong\n", p->i_mb_index );
                        p->b_error_flag = 1;
                
                        return -1;
                    }    
                }
                           
                if(flags & FWD0)
                {
                    mv_pred(p, MV_FWD_X0, MV_FWD_B3, MV_PRED_LEFT, BLK_8X16, ref[kk++]+i_ref_offset, MVD_X0);
                    if( p->b_error_flag )
                    {
                        printf("[error]MB %d mvd is wrong\n", p->i_mb_index );

                        return -1;
                    }
                	p_ref[FWD][REF_X2] = p_ref[FWD][REF_X0];
                }
                if(flags & SYM0)
                {    
                    mv_pred_sym(p, &p->mv[MV_FWD_X0], BLK_8X16,ref[kk-1]);
                    if(p->b_error_flag)
                    {
                        printf("[error]MB %d mv sym is wrong\n", p->i_mb_index );
                        return -1;
                    }
                }
                if(flags & FWD1)
                {
                    mv_pred(p, MV_FWD_X1, MV_FWD_C2, MV_PRED_TOPRIGHT,BLK_8X16, ref[kk++]+i_ref_offset, MVD_X1);
                    if( p->b_error_flag )
                    {
                        printf("[error]MB %d mvd is wrong\n", p->i_mb_index );

                        return -1;
                    }
                	CP32(p_mvd[FWD][MVD_X3], p_mvd[FWD][MVD_X1]);
                	p_ref[FWD][REF_X3] = p_ref[FWD][REF_X1];
                }
                if(flags & SYM1)
                {    
                    mv_pred_sym(p, &p->mv[MV_FWD_X1], BLK_8X16,ref[kk-1]);
                    if(p->b_error_flag)
                    {
                        printf("[error]MB %d mv sym is wrong\n", p->i_mb_index );
                        return -1;
                    }
                }
                if(flags & BWD0)
                {
                    mv_pred(p, MV_BWD_X0, MV_BWD_B3, MV_PRED_LEFT, BLK_8X16, ref[kk++], MVD_X0);
                    if( p->b_error_flag )
                    {
                        printf("[error]MB %d mvd is wrong\n", p->i_mb_index );

                        return -1;
                    }
                	 p_ref[BWD][REF_X2] = p_ref[BWD][REF_X0];
                }
                if(flags & BWD1)
                {
                	mv_pred(p, MV_BWD_X1, MV_BWD_C2, MV_PRED_TOPRIGHT,BLK_8X16, ref[kk++], MVD_X1);
                    if( p->b_error_flag )
                    {
                        printf("[error]MB %d mvd is wrong\n", p->i_mb_index );

                        return -1;
                    }                    
                	CP32(p_mvd[BWD][MVD_X3], p_mvd[BWD][MVD_X1]);
                	p_ref[BWD][REF_X3] = p_ref[BWD][REF_X1];
                }
            }
    }
    
#undef FWD
#undef BWD
#undef MVD_X3
#undef MVD_X2
#undef MVD_X1
#undef MVD_X0
#undef REF_X3
#undef REF_X2
#undef REF_X1
#undef REF_X0

#if B_MB_WEIGHTING
    p->weighting_prediction = 0;
#endif
    if (i_mb_type != B_SKIP)
    {
#if B_MB_WEIGHTING
        if ( p->sh.b_slice_weighting_flag && p->sh.b_mb_weighting_flag )
        {
           	if( p->ph.b_aec_enable )
			{
				p->weighting_prediction = cavs_cabac_get_mb_weighting_prediction( p );
			}
			else
				p->weighting_prediction = cavs_bitstream_get_bit1(&p->s);	//weighting_prediction
        }
#else 
        if (p->sh.b_mb_weighting_flag)
        {
            cavs_bitstream_get_bit1(&p->s);	//weighting_prediction
        }
#endif
    }

    inter_pred(p, i_mb_type);

    if(i_mb_type != B_SKIP)
    {
        /* get coded block pattern */
        p->i_cbp = p->bs_read[SYNTAX_INTER_CBP](p);
        if( p->i_cbp > 63 || p->b_error_flag ) 
        {
            printf("[error]MB cbp is wrong\n");
            p->b_error_flag = 1;

            return -1;
        }

        /* get quantizer */
        if(p->i_cbp && !p->b_fixed_qp)
        {    
#if 1
            int delta_qp =  p->bs_read[SYNTAX_DQP](p);
            
			p->i_qp = (p->i_qp-MIN_QP + delta_qp + (MAX_QP-MIN_QP+1))%(MAX_QP-MIN_QP+1)+MIN_QP;

            if( p->i_qp < 0 || p->i_qp > 63  || p->b_error_flag )
            {
                printf("[error]MB qp delta is wrong\n");
				p->b_error_flag = 1;
                return -1;  
            }
#else        
            p->i_qp = (p->i_qp + p->bs_read[SYNTAX_DQP](p)) & 63;
#endif            
        }
        else
            p->i_last_dqp = 0;

        i_ret = get_residual_inter(p,i_mb_type);    	
		if(i_ret == -1)
			return -1;
    }
    filter_mb(p, i_mb_type);
    
    return next_mb(p);
}

static inline void get_p_frame_ref(cavs_decoder *p, int *p_ref)
{
	if(p->ph.b_picture_reference_flag )
	{
		*p_ref=0;
	}
	else
	{
		if(p->ph.b_picture_structure==0&&p->ph.i_picture_coding_type==XAVS_P_PICTURE)
		{
			*p_ref=cavs_bitstream_get_bits(&p->s,2);
		}
		else
		{
			*p_ref=cavs_bitstream_get_bit1(&p->s);	//mb_reference_index
		}
	}
}

static int get_mb_p(cavs_decoder *p)
{
    int i;
    int i_offset;
    int ref[4];
    int i_mb_type = p->i_mb_type;
    int16_t (*p_mvd)[6][2] = p->p_mvd;
    int8_t (*p_ref)[9] = p->p_ref;
	int i_ret = 0;

    init_mb(p);

#define FWD		0
//#define BWD		1	//no need for backward mvd in P frame
#define MVD_X0  1
#define MVD_X1	2
#define MVD_X2  4
#define MVD_X3	5
#define REF_X0  4
#define REF_X1	5
#define REF_X2	7
#define REF_X3  8

    switch(i_mb_type)
    {
        case P_SKIP:
            mv_pred(p, MV_FWD_X0, MV_FWD_C2, MV_PRED_PSKIP, BLK_16X16, PSKIP_REF, MVD_X0);
            if( p->b_error_flag )
            {
                printf("[error]MB %d mvd is wrong\n", p->i_mb_index );

                return -1;
            }
            break;
        case P_16X16:
            ref[0] = p->bs_read_ref_p(p, REF_X0);
            if( ref[0] > 3 ||  p->b_error_flag )
            {
                printf("[error]MB %d refidx is wrong\n", p->i_mb_index );
                p->b_error_flag = 1;
                
                return -1;
            }
            mv_pred(p, MV_FWD_X0, MV_FWD_C2, MV_PRED_MEDIAN,   BLK_16X16, ref[0], MVD_X0);
            if( p->b_error_flag )
            {
                printf("[error]MB %d mvd is wrong\n", p->i_mb_index );
                
                return -1;
            }
            M32(p_mvd[FWD][MVD_X1]) = M32(p_mvd[FWD][MVD_X3]) = M32(p_mvd[FWD][MVD_X0]);
            p_ref[FWD][REF_X1] = p_ref[FWD][REF_X2] = p_ref[FWD][REF_X3] = p_ref[FWD][REF_X0];
            break;
        case P_16X8:
            ref[0] = p->bs_read_ref_p(p, REF_X0);
            if( ref[0] > 3 || p->b_error_flag  )
            {
                printf("[error]MB %d refidx is wrong\n", p->i_mb_index );
                p->b_error_flag = 1;
                
                return -1;
            }
            ref[2] = p->bs_read_ref_p(p, REF_X2);
            if( ref[2] > 3 || p->b_error_flag )
            {
                printf("[error]MB %d refidx is wrong\n", p->i_mb_index );
                p->b_error_flag = 1;
                
                return -1;
            }
            mv_pred(p, MV_FWD_X0, MV_FWD_C2, MV_PRED_TOP,      BLK_16X8, ref[0], MVD_X0);
            if( p->b_error_flag )
            {
                printf("[error]MB %d mvd is wrong\n", p->i_mb_index );
                
                return -1;
            }
            mv_pred(p, MV_FWD_X2, MV_FWD_A1, MV_PRED_LEFT,     BLK_16X8, ref[2], MVD_X2);
            if( p->b_error_flag )
            {
                printf("[error]MB %d mvd is wrong\n", p->i_mb_index );
                
                return -1;
            }
            CP32(p_mvd[FWD][MVD_X1], p_mvd[FWD][MVD_X0]);
            CP32(p_mvd[FWD][MVD_X3], p_mvd[FWD][MVD_X2]);
            p_ref[FWD][REF_X1] = p_ref[FWD][REF_X0];
            p_ref[FWD][REF_X3] = p_ref[FWD][REF_X2];
            break;
        case P_8X16:
            ref[0] = p->bs_read_ref_p(p, REF_X0);
            if( ref[0] > 3 || p->b_error_flag )
            {
                printf("[error]MB %d refidx is wrong\n", p->i_mb_index );
                p->b_error_flag = 1;
                
                return -1;
            }
            ref[1] = p->bs_read_ref_p(p, REF_X1);
            if( ref[1] > 3 || p->b_error_flag )
            {
                printf("[error]MB %d refidx is wrong\n", p->i_mb_index );
                p->b_error_flag = 1;
                
                return -1;
            }
            mv_pred(p, MV_FWD_X0, MV_FWD_B3, MV_PRED_LEFT,     BLK_8X16, ref[0], MVD_X0);
            if( p->b_error_flag )
            {
                printf("[error]MB %d mvd is wrong\n", p->i_mb_index );
                
                return -1;
            }
            mv_pred(p, MV_FWD_X1, MV_FWD_C2, MV_PRED_TOPRIGHT, BLK_8X16, ref[1], MVD_X1);
            if( p->b_error_flag )
            {
                printf("[error]MB %d mvd is wrong\n", p->i_mb_index );
                
                return -1;
            }
            CP32(p_mvd[FWD][MVD_X3], p_mvd[FWD][MVD_X1]);
            p_ref[FWD][REF_X2] = p_ref[FWD][REF_X0];
            p_ref[FWD][REF_X3] = p_ref[FWD][REF_X1];
            break;
        case P_8X8:
            ref[0] = p->bs_read_ref_p(p, REF_X0);
            if( ref[0] > 3 || p->b_error_flag )
            {
                printf("[error]MB %d refidx is wrong\n", p->i_mb_index );
                p->b_error_flag = 1;
                
                return -1;
            }
            ref[1] = p->bs_read_ref_p(p, REF_X1);
            if( ref[1] > 3 || p->b_error_flag )
            {
                printf("[error]MB %d refidx is wrong\n", p->i_mb_index );
                p->b_error_flag = 1;
                
                return -1;
            }
            ref[2] = p->bs_read_ref_p(p, REF_X2);
            if( ref[2] > 3  || p->b_error_flag )
            {
                printf("[error]MB %d refidx is wrong\n", p->i_mb_index );
                p->b_error_flag = 1;
                
                return -1;
            }
            ref[3] = p->bs_read_ref_p(p, REF_X3);
            if( ref[3] > 3 || p->b_error_flag )
            {
                printf("[error]MB %d refidx is wrong\n", p->i_mb_index );
                p->b_error_flag = 1;
                
                return -1;
            }
            mv_pred(p, MV_FWD_X0, MV_FWD_B3, MV_PRED_MEDIAN,   BLK_8X8, ref[0], MVD_X0);
            if( p->b_error_flag )
            {
                printf("[error]MB %d mvd is wrong\n", p->i_mb_index );
                
                return -1;
            }
            mv_pred(p, MV_FWD_X1, MV_FWD_C2, MV_PRED_MEDIAN,   BLK_8X8, ref[1], MVD_X1);
            if( p->b_error_flag )
            {
                printf("[error]MB %d mvd is wrong\n", p->i_mb_index );
                
                return -1;
            }
            mv_pred(p, MV_FWD_X2, MV_FWD_X1, MV_PRED_MEDIAN,   BLK_8X8, ref[2], MVD_X2);
            if( p->b_error_flag )
            {
                printf("[error]MB %d mvd is wrong\n", p->i_mb_index );
                
                return -1;
            }
            mv_pred(p, MV_FWD_X3, MV_FWD_X0, MV_PRED_MEDIAN,   BLK_8X8, ref[3], MVD_X3);
            if( p->b_error_flag )
            {
                printf("[error]MB %d mvd is wrong\n", p->i_mb_index );
                
                return -1;
            }
            break;
    }
    
#undef FWD
//#undef BWD
#undef MVD_X3
#undef MVD_X2
#undef MVD_X1
#undef MVD_X0
#undef REF_X3
#undef REF_X2
#undef REF_X1
#undef REF_X0

#if B_MB_WEIGHTING
	p->weighting_prediction = 0;
#endif
    if (i_mb_type != P_SKIP)
    {
#if B_MB_WEIGHTING
		if ( p->sh.b_slice_weighting_flag && p->sh.b_mb_weighting_flag )
		{
			if( p->ph.b_aec_enable )
			{
				p->weighting_prediction = cavs_cabac_get_mb_weighting_prediction( p );
			}
			else
				p->weighting_prediction = cavs_bitstream_get_bit1(&p->s);	//weighting_prediction
		}
#else 
        if (p->sh.b_mb_weighting_flag)
        {
            cavs_bitstream_get_bit1(&p->s);	//weighting_prediction
        }
#endif
    }

    inter_pred(p, i_mb_type);

    i_offset = (p->i_mb_y*p->i_mb_width + p->i_mb_x)*4;
    p->p_col_mv[i_offset+ 0] = p->mv[MV_FWD_X0];
    p->p_col_mv[i_offset + 1] = p->mv[MV_FWD_X1];
    p->p_col_mv[i_offset + 2] = p->mv[MV_FWD_X2];
    p->p_col_mv[i_offset + 3] = p->mv[MV_FWD_X3];

    if (i_mb_type != P_SKIP)
    {
        /* get coded block pattern */
        p->i_cbp = p->bs_read[SYNTAX_INTER_CBP](p);
        if( p->i_cbp > 63 || p->b_error_flag ) 
        {
            printf("[error]MB cbp is wrong\n");
            p->b_error_flag = 1;

            return -1;
        }
    
        if(p->ph.vbs_enable)
        {
            if(i_mb_type == P_8X8)
            {
                for(i=0;i<4;i++)
                {
                    if(p->i_cbp & (1<<i))
                        p->vbs_mb_part_transform_4x4_flag[i] = cavs_bitstream_get_bit1(&p->s);
                }
                for(i=0;i<4;i++)
                {
                    if(p->vbs_mb_part_transform_4x4_flag[i] && (p->i_cbp&(1<<i)))
                    {
                        p->i_cbp_4x4[i] = cavs_bitstream_get_ue_k(&p->s,1);
                        p->i_cbp_part[i] = get_cbp_4x4(p->i_cbp_4x4[i]);
                    }
                }
            }
        }

        /* get quantizer */
        if(p->i_cbp && !p->b_fixed_qp)
        {
#if 1
            int delta_qp =  p->bs_read[SYNTAX_DQP](p);
            
			p->i_qp = (p->i_qp-MIN_QP + delta_qp + (MAX_QP-MIN_QP+1))%(MAX_QP-MIN_QP+1)+MIN_QP;

            if( p->i_qp < 0 || p->i_qp > 63  || p->b_error_flag )
            {
                printf("[error]MB qp delta is wrong\n");
				p->b_error_flag = 1;
                return -1;  
            }
#else        
            p->i_qp = (p->i_qp + p->bs_read[SYNTAX_DQP](p)) & 63;
#endif
        }
        else
            p->i_last_dqp = 0;

        i_ret = get_residual_inter(p,i_mb_type);
        if(i_ret == -1)
            return -1;
    }

    filter_mb(p,i_mb_type);

    *p->p_col_type = i_mb_type;
    
    return next_mb(p);
}

static int cavs_mb_predict_intra8x8_mode (cavs_decoder *p, int i_pos)
{
    const int ma = p->i_intra_pred_mode_y[i_pos-1];
    const int mb = p->i_intra_pred_mode_y[i_pos-XAVS_ICACHE_STRIDE];
    const int m = XAVS_MIN (xavs_mb_pred_mode (ma),
    	xavs_mb_pred_mode (mb));
    
    if (m < 0)
        return INTRA_L_LP;
    else if (ma < NO_INTRA_8x8_MODE && mb < NO_INTRA_8x8_MODE)
        return  m;
    else if (ma < NO_INTRA_8x8_MODE)
        return xavs_mb_pred_mode(ma);
    else if (mb < NO_INTRA_8x8_MODE)
        return xavs_mb_pred_mode(mb);
    else
        return xavs_pred_mode_4x4to8x8[pred4x4[xavs_mb_pred_mode(ma)+1][xavs_mb_pred_mode(mb)+1]];
}

static int cavs_mb_predict_intra4x4_mode (cavs_decoder *p, int i_pos)
{
    const int ma = p->i_intra_pred_mode_y[i_pos-1];
    const int mb = p->i_intra_pred_mode_y[i_pos-XAVS_ICACHE_STRIDE];
    
    return pred4x4[xavs_mb_pred_mode(ma)+1][xavs_mb_pred_mode(mb)+1];
}


#define SRC_TOP(x) top[x]
#define SRC_LEFT(x) left[x]
#define F1(a,b)			(((a)+(b)+1)>>1)
#define F2(a,b,c)		(((a)+((b)<<1)+(c)+2)>>2)

#define PPL(y) \
    edge[16-y] = F2(SRC_LEFT(y-2), SRC_LEFT(y-1), SRC_LEFT(y));
#define PPT(x) \
    edge[16+x] = F2(SRC_TOP(x-2), SRC_TOP(x-1), SRC_TOP(x));
#define EP (edge+16)

static void cavs_predict_8x8_filter( cavs_decoder*p, uint8_t edge[33], uint8_t *p_top, uint8_t *p_left, int i_neighbor )
{   
    int have_lt = i_neighbor & MB_TOPLEFT;
    int have_tr =i_neighbor & MB_TOPRIGHT;
    int have_dl = i_neighbor & MB_DOWNLEFT;
    uint8_t *top = p_top+1;
    uint8_t *left = p_left+1;

    if (i_neighbor & MB_TOP)
    {
        EP[1] = F2((have_lt ? SRC_TOP(-1) : SRC_TOP(0)), SRC_TOP(0), SRC_TOP(1));
        PPT(2) PPT(3) PPT(4) PPT(5) PPT(6) PPT(7)
        	EP[8] = F2(SRC_TOP(6), SRC_TOP(7), (have_tr ? SRC_TOP(8):SRC_TOP(7)));
        if (have_tr)
        {
        	PPT(9) PPT(10) PPT(11) PPT(12) PPT(13) PPT(14) PPT(15)
        		EP[16] = F2(SRC_TOP(14), SRC_TOP(15), SRC_TOP(15));
        }
        else
        {
        	EP[9] = EP[10] = EP[11] = EP[12] = EP[13] = EP[14] = EP[15] = EP[16] = SRC_TOP(7);
        }
    }

    if( i_neighbor & MB_LEFT )
    {
        EP[-1] = F2((have_lt ? SRC_LEFT(-1) : SRC_LEFT(0)), SRC_LEFT(0), SRC_LEFT(1));
        PPL(2) PPL(3) PPL(4) PPL(5) PPL(6) PPL(7)
        	EP[-8] = F2(SRC_LEFT(6), SRC_LEFT(7), (have_dl ? SRC_LEFT(8):SRC_LEFT(7)));
        if (have_dl)
        {
        	PPL(9) PPL(10) PPL(11) PPL(12) PPL(13) PPL(14) PPL(15)
        		EP[-16] = F2(SRC_LEFT(14), SRC_LEFT(15), SRC_LEFT(15));
        }
        else
        {
        	EP[-9] = EP[-10] = EP[-11] = EP[-12] = EP[-13] = EP[-14] = EP[-15] = EP[-16] = SRC_LEFT(7);
        }
    }
    if (have_lt) 
    {
        EP[0] = F2(SRC_LEFT(0), SRC_LEFT(-1), SRC_TOP(0));
    }
}


void cavs_mb_neighbour_avail( cavs_decoder *p )
{
    uint32_t i_mb_x = p->i_mb_x;
    //int i_mb_y = p->i_mb_y;
    uint32_t i_mb_xy = p->i_mb_index;
    int i_top_xy = p->i_mb_index - p->i_mb_width;

    /* init */
    p->mb.i_neighbour = 0;
    p->mb.i_neighbour8[0] = 0;
    p->mb.i_neighbour8[1] = 0;
    p->mb.i_neighbour8[2] = 0;
    p->mb.i_neighbour8[3] = 0;

    /* set neighbour */
    if( i_mb_xy >= 0 /*h->sh.i_first_mb*/+ p->i_mb_width )
    {
        p->mb.i_neighbour |= MB_TOP;
    }

    if (i_mb_x > 0 && i_mb_xy > 0/*h->sh.i_first_mb*/)
    {
        p->mb.i_neighbour |= MB_LEFT;        
    }

    if (i_mb_x < p->i_mb_width - 1 && i_top_xy + 1 >= /*h->sh.i_first_mb*/0)
    {
        p->mb.i_neighbour |= MB_TOPRIGHT;
    }

    if (i_mb_x > 0 && i_top_xy - 1 >= /*h->sh.i_first_mb*/0)
    {
        p->mb.i_neighbour |= MB_TOPLEFT;
    }

    /* set neighbour8 */
    p->mb.i_neighbour8[0] = (p->mb.i_neighbour & (MB_TOP | MB_LEFT | MB_TOPLEFT))
      | ((p->mb.i_neighbour & MB_TOP) ? MB_TOPRIGHT : 0) | ((p->mb.i_neighbour & MB_LEFT) ? MB_DOWNLEFT : 0);
    p->mb.i_neighbour8[2] = MB_TOP | MB_TOPRIGHT | ((p->mb.i_neighbour & MB_LEFT) ? (MB_LEFT | MB_TOPLEFT) : 0); 
    p->mb.i_neighbour8[3] = MB_LEFT | MB_TOP | MB_TOPLEFT;
    p->mb.i_neighbour8[1] = MB_LEFT | (p->mb.i_neighbour & MB_TOPRIGHT) | ((p->mb.i_neighbour & MB_TOP) ? MB_TOP | MB_TOPLEFT : 0);
}


static int get_mb_i(cavs_decoder *p)
{
    uint8_t i_top[18];
    uint8_t *p_left = NULL;
    uint8_t *p_d;

    static const uint8_t scan5x5[4][4] = {
    	{6, 7, 11, 12},
    	{8, 9, 13, 14},
    	{16, 17, 21, 22},
    	{18, 19, 23, 24}
    };

    int i, j;
    int i_offset = p->i_mb_x<<2;
    //int i_mb_type = p->i_cbp_code;
    int i_rem_mode;
    int i_pred_mode;
    DECLARE_ALIGNED_16(uint8_t edge[33]);
    DECLARE_ALIGNED_16(uint8_t edge_8x8[33]);
    int i_qp_cb = 0, i_qp_cr = 0;
	int i_ret = 0;

    init_mb(p);

#if HAVE_MMX
    cavs_mb_neighbour_avail( p );
#endif

    if (p->ph.vbs_enable)
    {
        p->vbs_mb_intra_pred_flag=cavs_bitstream_get_bit1(&p->s);
        if(p->vbs_mb_intra_pred_flag)	//vbs_mb_intra_pred_flag == '1'
        {
            for( j = 0; j < 4; ++j )
            {
                p->vbs_mb_part_intra_pred_flag[j]=cavs_bitstream_get_bit1(&p->s);
                p->vbs_mb_part_transform_4x4_flag[j]=p->vbs_mb_part_intra_pred_flag[j];
            }
        }
    }
    	
    for( i = 0; i < 4; i++ ) 
    {
        if (p->vbs_mb_part_intra_pred_flag[i])
        {
            int m;

            for( m = 0; m < 4; m++ )
            {
                int i_pos = scan5x5[i][m];
                
                p->pred_mode_4x4_flag[m] = cavs_bitstream_get_bit1(&p->s);
                if(!p->pred_mode_4x4_flag[m])
                    p->intra_luma_pred_mode_4x4[m] = cavs_bitstream_get_bits(&p->s, 3);

                i_pred_mode = cavs_mb_predict_intra4x4_mode(p, i_pos);
                if(!p->pred_mode_4x4_flag[m])
                {
                    i_pred_mode = p->intra_luma_pred_mode_4x4[m] + (p->intra_luma_pred_mode_4x4[m] >= i_pred_mode);
                }
                p->i_intra_pred_mode_y[i_pos] = i_pred_mode + NO_INTRA_8x8_MODE;
            }
        }
        else
        {
            int i_pos = scan5x5[i][0];

            i_rem_mode = p->bs_read[SYNTAX_INTRA_LUMA_PRED_MODE](p);
            if( p->b_error_flag )
            {
                printf("[error]MB pred_mode_luma is wrong\n");
                return -1;    
            }

            i_pred_mode = cavs_mb_predict_intra8x8_mode(p, i_pos);
            if(!p->pred_mode_flag)
            {
                i_pred_mode = i_rem_mode + (i_rem_mode >= i_pred_mode);
            }
            if( i_pred_mode > 4) 
            {
                printf("[error]MB pred_mode_luma is wrong\n");
                p->b_error_flag = 1;
                
                return -1;
            }
            
            p->i_intra_pred_mode_y[scan5x5[i][0]] =
            p->i_intra_pred_mode_y[scan5x5[i][1]] =
            p->i_intra_pred_mode_y[scan5x5[i][2]] =
            p->i_intra_pred_mode_y[scan5x5[i][3]] = i_pred_mode;
        }
    }

    p->i_pred_mode_chroma = p->bs_read[SYNTAX_INTRA_CHROMA_PRED_MODE](p);
    if(p->i_pred_mode_chroma > 6 || p->b_error_flag ) 
    {
        printf("[error]MB pred_mode_chroma is wrong\n");
        p->b_error_flag = 1;
                
        return -1;
    }

    p->i_intra_pred_mode_y[5] =  p->i_intra_pred_mode_y[9];
    p->i_intra_pred_mode_y[10] =  p->i_intra_pred_mode_y[14];
    p->i_intra_pred_mode_y[15] =  p->i_intra_pred_mode_y[19];
    p->i_intra_pred_mode_y[20] =  p->i_intra_pred_mode_y[24];

    p->p_top_intra_pred_mode_y[i_offset+0] = p->i_intra_pred_mode_y[21];
    p->p_top_intra_pred_mode_y[i_offset+1] = p->i_intra_pred_mode_y[22];
    p->p_top_intra_pred_mode_y[i_offset+2] = p->i_intra_pred_mode_y[23];
    p->p_top_intra_pred_mode_y[i_offset+3] = p->i_intra_pred_mode_y[24];

    if(!(p->i_mb_flags & A_AVAIL))
    {
        adapt_pred_mode(LEFT_ADAPT_INDEX_L, &p->i_intra_pred_mode_y[6] );
        adapt_pred_mode(LEFT_ADAPT_INDEX_L, &p->i_intra_pred_mode_y[11] );
        adapt_pred_mode(LEFT_ADAPT_INDEX_L, &p->i_intra_pred_mode_y[16] );
        adapt_pred_mode(LEFT_ADAPT_INDEX_L, &p->i_intra_pred_mode_y[21] );
        adapt_pred_mode(LEFT_ADAPT_INDEX_C, &p->i_pred_mode_chroma );
    }
    if(!(p->i_mb_flags & B_AVAIL))
    {
        adapt_pred_mode(TOP_ADAPT_INDEX_L, &p->i_intra_pred_mode_y[6] );
        adapt_pred_mode(TOP_ADAPT_INDEX_L, &p->i_intra_pred_mode_y[7] );
        adapt_pred_mode(TOP_ADAPT_INDEX_L, &p->i_intra_pred_mode_y[8] );
        adapt_pred_mode(TOP_ADAPT_INDEX_L, &p->i_intra_pred_mode_y[9] );
        adapt_pred_mode(TOP_ADAPT_INDEX_C, &p->i_pred_mode_chroma );
    }

    p->i_cbp = p->bs_read[SYNTAX_INTRA_CBP](p);
    if( p->i_cbp > 63 ||  p->b_error_flag ) 
    {
        printf("[error]MB cbp is wrong\n");
        p->b_error_flag = 1;

        return -1;
    }

    if(p->ph.vbs_enable)
    {
        for( i = 0; i < 4; i++ )
        {
            if(p->vbs_mb_part_transform_4x4_flag[i]&&(p->i_cbp&(1<<i)))
            {
                p->i_cbp_4x4[i]=cavs_bitstream_get_ue_k(&p->s,1);
                p->i_cbp_part[i] = get_cbp_4x4(p->i_cbp_4x4[i]);
            }
        }
    }

    if(p->i_cbp && !p->b_fixed_qp)
    {	
#if 1
        int delta_qp =  p->bs_read[SYNTAX_DQP](p);

		p->i_qp = (p->i_qp-MIN_QP + delta_qp + (MAX_QP-MIN_QP+1))%(MAX_QP-MIN_QP+1)+MIN_QP;

        if( p->i_qp < 0 || p->i_qp > 63  || p->b_error_flag )
        {
            printf("[error]MB qp delta is wrong\n");
			p->b_error_flag = 1;
            return -1;  
        }
#else    
        p->i_qp = (p->i_qp + p->bs_read[SYNTAX_DQP](p)) & 63;
#endif
    }
    else
        p->i_last_dqp = 0; // for cabac only

    for( i = 0; i < 4; i++ )
    {
        int i_mode;

        p_d = p->p_y + p->i_luma_offset[i];

        load_intra_pred_luma(p, i_top, &p_left, i);

#if HAVE_MMX
        cavs_predict_8x8_filter(p, edge_8x8, i_top, p_left, p->mb.i_neighbour8[i]);
#endif
        if (p->vbs_mb_part_intra_pred_flag[i])
        {
            for (j = 0; j < 4; j++)
            {
                uint8_t *p_d_4x4 = p_d + 4*(j&1) + 4*(j>>1)*p->cur.i_stride[0];
                
                i_mode = p->i_intra_pred_mode_y[scan5x5[i][j]];
                load_intra_4x4_pred_luma(p, p_d_4x4, edge, i*4+j);
                intra_pred_lum_4x4[i_mode](p_d_4x4, edge, p->cur.i_stride[0]);
                if ((p->i_cbp & (1<<i)) && (p->i_cbp_part[i] & (1<<j)))
                {
                    get_residual_block4x4(p,p->i_qp,p_d_4x4,p->cur.i_stride[0], 1);
                }
            }
        }
        else
        {
            i_mode = p->i_intra_pred_mode_y[scan5x5[i][0]];
#if !HAVE_MMX
            intra_pred_lum[i_mode](p_d, edge_8x8, p->cur.i_stride[0], i_top, p_left);
#else
            p->cavs_intra_luma[i_mode](p_d, edge_8x8, p->cur.i_stride[0], i_top, p_left);
#endif

            if(p->i_cbp & (1<<i))
            {
                i_ret = get_residual_block(p,intra_2dvlc,1,p->i_qp,p_d,p->cur.i_stride[0], 0);
                if(i_ret == -1)
                    return -1;
            }
        }
    }

    load_intra_pred_chroma(p);

#if !HAVE_MMX
       intra_pred_chroma[p->i_pred_mode_chroma](p->p_cb, p->mb.i_neighbour, p->cur.i_stride[1], &p->p_top_border_cb[p->i_mb_x*10], p->i_left_border_cb);

       intra_pred_chroma[p->i_pred_mode_chroma](p->p_cr, p->mb.i_neighbour, p->cur.i_stride[2], &p->p_top_border_cr[p->i_mb_x*10], p->i_left_border_cr);
#else
       p->cavs_intra_chroma[p->i_pred_mode_chroma](p->p_cb, p->mb.i_neighbour, p->cur.i_stride[1], &p->p_top_border_cb[p->i_mb_x*10], p->i_left_border_cb);
       p->cavs_intra_chroma[p->i_pred_mode_chroma](p->p_cr, p->mb.i_neighbour, p->cur.i_stride[2], &p->p_top_border_cr[p->i_mb_x*10], p->i_left_border_cr);
#endif
    
    /* FIXIT : weighting quant qp for chroma
        NOTE : not modify p->i_qp directly
    */
    if( p->b_weight_quant_enable && !p->ph.chroma_quant_param_disable )
    { 
        i_qp_cb = clip3_int( p->i_qp + p->ph.chroma_quant_param_delta_u, 0, 63 );
        i_qp_cr = clip3_int( p->i_qp + p->ph.chroma_quant_param_delta_v, 0, 63 );
    }

    if(p->i_cbp & (1<<4))
    {
        i_ret = get_residual_block(p,chroma_2dvlc,0, 
             chroma_qp[ p->b_weight_quant_enable && !p->ph.chroma_quant_param_disable? i_qp_cb : p->i_qp],
    		p->p_cb,p->cur.i_stride[1], 1);
        if(i_ret == -1)
        	return -1;
    }
    if(p->i_cbp & (1<<5))
    {
        i_ret = get_residual_block(p,chroma_2dvlc,0, 
             chroma_qp[ p->b_weight_quant_enable && !p->ph.chroma_quant_param_disable? i_qp_cr : p->i_qp],
    		p->p_cr,p->cur.i_stride[2], 1);
        if(i_ret == -1)
            return -1;
    }

    filter_mb(p, I_8X8);
    p->mv[MV_FWD_X0] = MV_INTRA;
    copy_mvs(&p->mv[MV_FWD_X0], BLK_16X16);
    p->mv[MV_BWD_X0] = MV_INTRA;
    copy_mvs(&p->mv[MV_BWD_X0], BLK_16X16);

    if(p->ph.i_picture_coding_type != XAVS_B_PICTURE)
    	*p->p_col_type = I_8X8;

    return next_mb(p);
}

#if THREADS_OPT
/* decode slice only for aec */
static int cavs_get_slice_aec( cavs_decoder *p )
{
    int i, i_num = 0, i_result;
    uint32_t i_limit, i_skip;
    int i_first_field, i_skip_count = 0;
    int (*get_mb_type)(cavs_decoder *p);

    int (*get_mb_pb_aec_opt)(cavs_decoder *p);
    //int (*get_mb_pb_rec_opt)(cavs_decoder *p);
    int i_slice_mb_y;
    int i_slice_mb_index;

    
    /* picture_type, picture_structure, i_first_field */
    static int8_t i_num_of_reference_table[3][2][2]={
        {{1,0}/*picture_structure=0*/,{0,0}/*picture_structure=1*/},//i_picuture
        {{4,4}/*picture_structure=0*/,{2,2}/*picture_structure=1*/},//p_picuture
        {{4,4}/*picture_structure=0*/,{2,2}/*picture_structure=1*/}//b_picuture
    };
    
    int b_interlaced = p->param.b_interlaced;    

    if(p->vsh.i_vertical_size > 2800)
    {
        p->sh.i_slice_vertical_position_extension = cavs_bitstream_get_bits(&p->s,3);
    }
    else
    {
        p->sh.i_slice_vertical_position_extension = 0;
    }
    //if (p->vsh.i_profile_id == PROFILE_YIDONG)
    //{
    //    p->sh.i_slice_horizontal_position = cavs_bitstream_get_bits(&p->s,8);
    //    if(p->vsh.i_horizontal_size > 4080)
    //       p->sh.i_slice_vertical_position_extension = cavs_bitstream_get_bits(&p->s,2);
    //}
    if(p->ph.b_fixed_picture_qp == 0)
    {
        p->b_fixed_qp = p->sh.b_fixed_slice_qp = cavs_bitstream_get_bit1(&p->s);
        p->i_qp = p->sh.i_slice_qp = cavs_bitstream_get_bits(&p->s,6);
    }
    else
    {
        p->sh.b_fixed_slice_qp = 1;
        p->sh.i_slice_qp = p->i_qp;
    }
    p->i_mb_y += (p->sh.i_slice_vertical_position_extension<<7);
    p->i_mb_index = p->i_mb_y*p->i_mb_width;

    if(b_interlaced)
    {
        if( !p->b_bottom ) /* top field can't exceed half num */
        {
        	if(p->i_mb_y >= (p->i_mb_height>>1) )
        	{
                 p->b_error_flag = 1;
                 return -1;
        	}

        	if( p->i_mb_index >= p->i_mb_num_half )
        	{
                 p->b_error_flag = 1;
                 return -1;
        	}
        }
        else
        {
        	if(p->i_mb_y < (p->i_mb_height>>1) )
        	{
                 p->b_error_flag = 1;
                 return -1;
        	}

        	if( p->i_mb_index < p->i_mb_num_half )
        	{
                 p->b_error_flag = 1;
                 return -1;
        	}
        }
    }
    
    p->i_mb_y_start = p->i_mb_y;
    p->i_mb_index_start = p->i_mb_index;
    p->i_mb_y_start_fld[p->b_bottom] = p->i_mb_y;
    p->i_mb_index_start_fld[p->b_bottom] = p->i_mb_index;

    i_slice_mb_y = p->i_mb_y;
    i_slice_mb_index = p->i_mb_index;

    i_first_field = p->i_mb_index < p->i_mb_num_half ? 1 : 0;
    i_num = i_num_of_reference_table[p->ph.i_picture_coding_type][p->ph.b_picture_structure][i_first_field];
    p->b_have_pred = (p->ph.i_picture_coding_type != XAVS_I_PICTURE) || (p->ph.b_picture_structure == 0 && i_first_field == 0);
    if( p->b_have_pred )
    {
        p->sh.b_slice_weighting_flag = cavs_bitstream_get_bit1(&p->s);
#if B_MB_WEIGHTING
		/* copy weighting prediction info */
    	p->b_slice_weighting_flag[p->b_bottom] = p->sh.b_slice_weighting_flag;
#endif			
        if(p->sh.b_slice_weighting_flag)
        {
            for(i = 0; i < i_num; i++)
            {
                p->sh.i_luma_scale[i] = cavs_bitstream_get_bits(&p->s, 8);
                p->sh.i_luma_shift[i] = cavs_bitstream_get_int(&p->s, 8);
                cavs_bitstream_clear_bits(&p->s, 1);
                p->sh.i_chroma_scale[i] = cavs_bitstream_get_bits(&p->s, 8);
                p->sh.i_chroma_shift[i] = cavs_bitstream_get_int(&p->s, 8);
                cavs_bitstream_clear_bits(&p->s, 1);
            }
            p->sh.b_mb_weighting_flag = cavs_bitstream_get_bit1(&p->s);
#if B_MB_WEIGHTING
			/* copy weighting prediction info */
			for(i = 0; i < i_num; i++)
			{
				 p->i_luma_scale[p->b_bottom][i] = p->sh.i_luma_scale[i];
				 p->i_luma_shift[p->b_bottom][i] = p->sh.i_luma_shift[i];
				 p->i_chroma_scale[p->b_bottom][i] = p->sh.i_chroma_scale[i];
				 p->i_chroma_shift[p->b_bottom][i] = p->sh.i_chroma_shift[i];
			}
			p->b_mb_weighting_flag[p->b_bottom] = p->sh.b_mb_weighting_flag;
#endif				
        }
        else
        {
            p->sh.b_mb_weighting_flag = 0;
#if B_MB_WEIGHTING
    		/* copy weighting prediction info */
        	p->b_mb_weighting_flag[p->b_bottom] = 0;
#endif			
        }
    }
    else
    {
        p->sh.b_slice_weighting_flag = 0;
        p->sh.b_mb_weighting_flag = 0;
#if B_MB_WEIGHTING
		/* copy weighting prediction info */
		p->b_slice_weighting_flag[p->b_bottom] = 0;
		p->b_mb_weighting_flag[p->b_bottom] = 0;
#endif		
    }
    
    //if (p->vsh.i_profile_id == PROFILE_YIDONG && p->ph.i_picture_coding_type == XAVS_P_PICTURE)
    //{
    //    p->quant_coeff_pred_flag = cavs_bitstream_get_bit1(&p->s);
    //}
    p->sh.i_number_of_reference = i_num;
    
    if (p->ph.i_picture_coding_type < XAVS_B_PICTURE)   //p frame
    {
        i_skip = P_SKIP;
        i_limit = P_8X8;
        
        get_mb_pb_aec_opt = get_mb_p_aec_opt;

        get_mb_type = p->bs_read[SYNTAX_MBTYPE_P];
    }
    else    //b frame
    {
        i_skip = B_SKIP;
        i_limit = B_8X8;

        get_mb_pb_aec_opt = get_mb_b_aec_opt;

        get_mb_type = p->bs_read[SYNTAX_MBTYPE_B];
    }

    /* init slice */
    i_result = cavs_init_slice_for_aec(p);
	if(i_result == -1)
		goto end;

    /* MB loop */
    for(;;)
    {
        /* AEC loop */
        for( ; ; )
        {
             if(p->b_have_pred)
            {
                if(p->ph.b_skip_mode_flag)
                {
                    i_skip_count = p->bs_read[SYNTAX_SKIP_RUN](p);
                    if( i_skip_count == -1 ||
                        (p->i_mb_index + i_skip_count > p->i_slice_mb_end)
                        || i_skip_count < -1 )
                    {
                        i_result = -1;
                        printf("[error]skip_count %d is wrong\n", i_skip_count);
                        goto end;
                    }

#if DEBUG_P_INTER|DEBUG_B_INTER
        
       {
                int tmp;
                FILE *fp;

                fp = fopen("data_skipcount.dat","ab");
                fprintf(fp,"frame %d MB %d \n", p->i_frame_decoded, p->i_mb_index );
                fprintf(fp,"skipcount = %d\n", i_skip_count );
                fclose(fp);
        }
#endif 
                    
                    if (p->ph.b_aec_enable && i_skip_count)
                    {
                        cavs_biari_decode_stuffing_bit(&p->cabac);
                        p->i_last_dqp = 0;
                        p->i_cbp = 0;
                    }
                    while(i_skip_count--)
                    {
                        p->i_mb_type_tab[p->i_mb_index] = p->i_mb_type = i_skip;

                        i_result = get_mb_pb_aec_opt(p);

                        if( p->b_error_flag )
                        {
                            i_result = -1;
                        }
                        
                        if(i_result != 0)
                        {
                            goto end;
                        }
                        
                        p->i_mb_index++;
                    }

                    if(p->i_mb_index >= p->i_slice_mb_end) /* slice finished */
                    {
                        if( !b_interlaced )
                        {
                            if (p->i_mb_index >= p->i_mb_num)   /* frame finished */
                                p->b_complete = 0;
                        }
                        else
                        {
                            if( !p->b_bottom )
                            {
                                if (p->i_mb_index >= (p->i_mb_num>>1))  /* top-field finished */
                                    p->b_complete = 0;
                            }
                            else
                            {
                                if (p->i_mb_index >= p->i_mb_num)   /* bot-field finished */
                                    p->b_complete = 0;
                            }
                        }

                        i_result = 0;
                        goto end;
                    }
                }

                p->i_mb_type_tab[p->i_mb_index]  = p->i_mb_type = get_mb_type(p);
                if( p->i_mb_type < 0 || p->i_mb_type > 29)
                {
                    p->b_error_flag = 1;
                    i_result = -1;
                    printf("[error] wrong mb_type!\n");
                    
                    goto end;
                }
                        
                if( p->i_mb_type == I_8X8 )
                {
                    i_result = get_mb_i_aec_opt(p);
                }
                else
                {
                    i_result = get_mb_pb_aec_opt(p);
                }

                if( p->b_error_flag )
                {
                    i_result = -1;
                }

                if (p->ph.b_aec_enable)
                {
                    cavs_biari_decode_stuffing_bit(&p->cabac);
                }
                if(i_result!=0)
                {
                    goto end;
                }
            } 
            else
            {
                p->i_mb_type_tab[p->i_mb_index] = I_8X8; /* init for I_8x8 decision */
                i_result = get_mb_i_aec_opt(p);

                if( p->b_error_flag )
                {
                    i_result = -1;
                }

                if (p->ph.b_aec_enable)
                {
                    cavs_biari_decode_stuffing_bit(&p->cabac);
                }

                if( i_result != 0 )
                {
                    goto end;
                }
            }
            
            p->i_mb_index++;
	      
            if( p->i_mb_index >= p->i_slice_mb_end )  /* slice finished */
            {
                if( !b_interlaced )
                {
                    if (p->i_mb_index >= p->i_mb_num)   /* frame finished */
                        p->b_complete = 0;
                }
                else
                {
                    if( !p->b_bottom )
                    {
                        if (p->i_mb_index >= (p->i_mb_num>>1))  /* top-field finished */
                            p->b_complete = 0;
                    }
                    else
                    {
                        if (p->i_mb_index >= p->i_mb_num)   /* bot-field finished */
                            p->b_complete = 0;
                    }
                }

                i_result = 0;

                goto end; /* */
            }
        }
    }

end:
    if (p->ph.b_aec_enable)
    {
        p->s = p->cabac.bs;
    }

    return i_result;
}

/* only rec */
static int cavs_get_slice_rec(cavs_decoder *p)
{
    int i_num = 0, i_result;
    uint32_t i_limit, i_skip;
    int i_first_field;// i_skip_count = 0;

    int (*get_mb_type)(cavs_decoder *p);
    int (*get_mb_pb_rec_opt)(cavs_decoder *p);
    
    /* picture_type, picture_structure, i_first_field */
    static int8_t i_num_of_reference_table[3][2][2]={
        {{1,0}/*picture_structure=0*/,{0,0}/*picture_structure=1*/},//i_picuture
        {{4,4}/*picture_structure=0*/,{2,2}/*picture_structure=1*/},//p_picuture
        {{4,4}/*picture_structure=0*/,{2,2}/*picture_structure=1*/}//b_picuture
    };
    
    int b_interlaced = p->param.b_interlaced;
    
    if( !b_interlaced )
    {	
        p->i_mb_y = p->i_mb_y_start;
        p->i_mb_index = p->i_mb_index_start;
    }
    else
    {
        if( !p->b_bottom )
        {
        	if((uint32_t)p->i_mb_y_start_fld[p->b_bottom] > (p->i_mb_height>>1) 
        		|| (uint32_t)p->i_mb_index_start_fld[p->b_bottom] > p->i_mb_num_half
        		|| (uint32_t)p->i_slice_mb_end_fld[p->b_bottom] > p->i_mb_num_half )
        	{
        		p->b_error_flag = 1;
        		return -1;
        	}
        }
        else
        {
        	if((uint32_t)p->i_mb_y_start_fld[p->b_bottom] < (p->i_mb_height>>1) 
        		|| (uint32_t)p->i_mb_index_start_fld[p->b_bottom] < p->i_mb_num_half
        		|| (uint32_t)p->i_slice_mb_end_fld[p->b_bottom] < p->i_mb_num_half )
        	{
        		p->b_error_flag = 1;
        		return -1;
        	}
        }

        p->i_mb_y = p->i_mb_y_start_fld[p->b_bottom];
        p->i_mb_index = p->i_mb_index_start_fld[p->b_bottom];
        p->i_slice_mb_end = p->i_slice_mb_end_fld[p->b_bottom];       
    }

    i_first_field = p->i_mb_index < p->i_mb_num_half ? 1 : 0;
    i_num = i_num_of_reference_table[p->ph.i_picture_coding_type][p->ph.b_picture_structure][i_first_field];
    p->b_have_pred = (p->ph.i_picture_coding_type != XAVS_I_PICTURE) || (p->ph.b_picture_structure == 0 && i_first_field == 0);
    p->sh.i_number_of_reference = i_num;
    
    if (p->ph.i_picture_coding_type < XAVS_B_PICTURE)   //p frame
    {
        i_skip = P_SKIP;
        i_limit = P_8X8;
        
        get_mb_pb_rec_opt = get_mb_p_rec_opt;

        get_mb_type = p->bs_read[SYNTAX_MBTYPE_P];
    }
    else    //b frame
    {
        i_skip = B_SKIP;
        i_limit = B_8X8;

        get_mb_pb_rec_opt = get_mb_b_rec_opt;

        get_mb_type = p->bs_read[SYNTAX_MBTYPE_B];
    }

    /* MB loop */
    for(;;)
    {  
        /* REC loop */
        cavs_init_slice_for_rec( p );
        for( ; ; )
        {   
            if( p->b_have_pred )
            {                
                p->i_mb_type =  p->i_mb_type_tab[p->i_mb_index];
                if(p->i_mb_type == I_8X8)
                {
                    i_result = get_mb_i_rec_opt(p);
                }
                else
                {
                    i_result = get_mb_pb_rec_opt(p);
                }
                
                if( p->b_error_flag )
                {
                    i_result = -1;
                }

                if(i_result!=0)
                {
                    goto end;
                }
            }
            else
            {
                p->i_mb_type_tab[p->i_mb_index] = I_8X8;
                i_result = get_mb_i_rec_opt(p);
                if( p->b_error_flag )
                {
                      i_result = -1;
                }
                
                if( i_result != 0 )
                {
                    goto end;
                }
            }

            p->i_mb_index++;
            if( p->i_mb_index >= p->i_slice_mb_end )  /* slice finished */
            {
                if( !b_interlaced )
                {
                    if (p->i_mb_index >= p->i_mb_num)   /* frame finished */
                    p->b_complete = 1;
                }
                else
                {
                    if( !p->b_bottom )
                    {
                        if (p->i_mb_index >= (p->i_mb_num>>1))  /* top-field finished */
                            p->b_complete = 1;
                    }
                    else
                    {
                        if (p->i_mb_index >= p->i_mb_num)   /* bot-field finished */
                            p->b_complete = 1;
                    }
                }

                i_result = 0;

                goto end;
            }
        }/* REC loop end */
    }

end:

    return i_result;
}	

static int cavs_get_slice_rec_threads( cavs_decoder *p )
{
	int ret;

#if B_MB_WEIGHTING
	/* load weighting prediction info */
	{
		int i, j;

        p->sh.b_slice_weighting_flag = p->b_slice_weighting_flag[p->b_bottom];
    	p->sh.b_mb_weighting_flag = p->b_mb_weighting_flag[p->b_bottom];
		for( i = 0; i < 4; i++ )
		{
			p->sh.i_luma_scale[i] = p->i_luma_scale[p->b_bottom][i];
			p->sh.i_luma_shift[i] = p->i_luma_shift[p->b_bottom][i];
			p->sh.i_chroma_scale[i] = p->i_chroma_scale[p->b_bottom][i];
			p->sh.i_chroma_shift[i] = p->i_chroma_shift[p->b_bottom][i];
		}
	}
#endif

    cavs_init_picture_ref_list( p );
    ret  = cavs_get_slice_rec( p );
    
    return ret;
}


#endif

static int cavs_get_slice(cavs_decoder *p)
{
    int i, i_num = 0, i_result;
    uint32_t i_limit, i_skip;
    int i_first_field, i_skip_count = 0;
    int (*get_mb_pb)(cavs_decoder *p);
    int (*get_mb_type)(cavs_decoder *p);
    
    /* picture_type, picture_structure, i_first_field */
    static int8_t i_num_of_reference_table[3][2][2]={
        {{1,0}/*picture_structure=0*/,{0,0}/*picture_structure=1*/},//i_picuture
        {{4,4}/*picture_structure=0*/,{2,2}/*picture_structure=1*/},//p_picuture
        {{4,4}/*picture_structure=0*/,{2,2}/*picture_structure=1*/}//b_picuture
    };
    
    int b_interlaced = p->param.b_interlaced;    

    if(p->vsh.i_vertical_size > 2800)
    {
        p->sh.i_slice_vertical_position_extension = cavs_bitstream_get_bits(&p->s,3);
    }
    else
    {
        p->sh.i_slice_vertical_position_extension = 0;
    }
    //if (p->vsh.i_profile_id == PROFILE_YIDONG)
    //{
    //    p->sh.i_slice_horizontal_position = cavs_bitstream_get_bits(&p->s,8);
    //    if(p->vsh.i_horizontal_size > 4080)
    //        p->sh.i_slice_vertical_position_extension = cavs_bitstream_get_bits(&p->s,2);
    //}
    if(p->ph.b_fixed_picture_qp == 0)
    {
        p->b_fixed_qp = p->sh.b_fixed_slice_qp = cavs_bitstream_get_bit1(&p->s);
        p->i_qp = p->sh.i_slice_qp = cavs_bitstream_get_bits(&p->s,6);
    }
    else
    {
        p->sh.b_fixed_slice_qp = 1;
        p->sh.i_slice_qp = p->i_qp;
    }
    p->i_mb_y += (p->sh.i_slice_vertical_position_extension<<7);
    p->i_mb_index = p->i_mb_y*p->i_mb_width;

    if(b_interlaced)
    {
        if( !p->b_bottom ) /* top field can't exceed half num */
        {
            if( (p->i_mb_y >= (p->i_mb_height>>1))
            || ( p->i_mb_index >= p->i_mb_num_half ) )
            {
                p->b_error_flag = 1;
                return -1;
            }
        }
        else
        {
            if( (p->i_mb_y < (p->i_mb_height>>1) )
            || ( p->i_mb_index < p->i_mb_num_half ))
            {
                p->b_error_flag = 1;
                return -1;
            }
        }
    }
    
    i_first_field = p->i_mb_index < p->i_mb_num_half ? 1 : 0;
    i_num = i_num_of_reference_table[p->ph.i_picture_coding_type][p->ph.b_picture_structure][i_first_field];
    p->b_have_pred = (p->ph.i_picture_coding_type != XAVS_I_PICTURE) || (p->ph.b_picture_structure == 0 && i_first_field == 0);
    if( p->b_have_pred )
    {
        p->sh.b_slice_weighting_flag = cavs_bitstream_get_bit1(&p->s);
        if(p->sh.b_slice_weighting_flag)
        {
            for(i = 0; i < i_num; i++)
            {
                p->sh.i_luma_scale[i] = cavs_bitstream_get_bits(&p->s, 8);
                p->sh.i_luma_shift[i] = cavs_bitstream_get_int(&p->s, 8);
                cavs_bitstream_clear_bits(&p->s, 1);
                p->sh.i_chroma_scale[i] = cavs_bitstream_get_bits(&p->s, 8);
                p->sh.i_chroma_shift[i] = cavs_bitstream_get_int(&p->s, 8);
                cavs_bitstream_clear_bits(&p->s, 1);
            }
            p->sh.b_mb_weighting_flag = cavs_bitstream_get_bit1(&p->s);
        }
        else
        {
            p->sh.b_mb_weighting_flag = 0;
        }
    }
    else
    {
        p->sh.b_slice_weighting_flag = 0;
        p->sh.b_mb_weighting_flag = 0;
    }
    
    //if (p->vsh.i_profile_id == PROFILE_YIDONG && p->ph.i_picture_coding_type == XAVS_P_PICTURE)
    //{
    //    p->quant_coeff_pred_flag = cavs_bitstream_get_bit1(&p->s);
    //}
    p->sh.i_number_of_reference = i_num;
    
    if (p->ph.i_picture_coding_type < XAVS_B_PICTURE)   //p frame
    {
        i_skip = P_SKIP;
        i_limit = P_8X8;
        
        get_mb_pb = get_mb_p;

        get_mb_type = p->bs_read[SYNTAX_MBTYPE_P];
    }
    else    //b frame
    {
        i_skip = B_SKIP;
        i_limit = B_8X8;

        get_mb_pb = get_mb_b;

        get_mb_type = p->bs_read[SYNTAX_MBTYPE_B];
    }

    /* init slice */
    i_result = cavs_init_slice(p);
	if(i_result == -1)
		goto end;

    /* MB loop */
    for(;;)
    {
        if(p->b_have_pred)
        {
            if(p->ph.b_skip_mode_flag)
            {
                i_skip_count = p->bs_read[SYNTAX_SKIP_RUN](p);
                if( i_skip_count == -1 ||
                    (p->i_mb_index + i_skip_count > p->i_slice_mb_end)
                    || i_skip_count < -1 )
                {
                    i_result = -1;
                    printf("[error]skip_count %d is wrong\n", i_skip_count);
                    goto end;
                }
                                    
                if (p->ph.b_aec_enable && i_skip_count)
                {
                    cavs_biari_decode_stuffing_bit(&p->cabac);
                    p->i_last_dqp = 0;
                    p->i_cbp = 0;
                }
                while(i_skip_count--)
                {
                    p->i_mb_type = i_skip;
                    i_result = get_mb_pb(p);

                    if( p->b_error_flag )
                    {
                        i_result = -1;
                    }
                    
                    if(i_result != 0)
                    {
                        goto end;
                    }
                    
                    p->i_mb_index++;
                }
                
                if(p->i_mb_index >= p->i_slice_mb_end) //slice finished
                {
                    if( !b_interlaced )
                    {
                        if (p->i_mb_index >= p->i_mb_num)   /* frame finished */
                            p->b_complete = 1;
                    }
                    else
                    {
                        if( !p->b_bottom )
                        {
                            if (p->i_mb_index >= (p->i_mb_num>>1))  /* top-field finished */
                                p->b_complete = 1;
                        }
                        else
                        {
                            if (p->i_mb_index >= p->i_mb_num)   /* bot-field finished */
                                p->b_complete = 1;
                        }
                    }

                    i_result = 0;
                    goto end;
                }
            }

            p->i_mb_type = get_mb_type(p);
            if( p->i_mb_type < 0 || p->i_mb_type > 29)
            {
                p->b_error_flag = 1;
                i_result = -1;
                printf("[error] wrong mb_type!\n");
                goto end;
            }
            
            if(p->i_mb_type == I_8X8)
            {
                i_result = get_mb_i(p);
            }
            else
            {
                i_result = get_mb_pb(p);
            }
            
            if( p->b_error_flag )
            {
                i_result = -1;
            }

            if (p->ph.b_aec_enable)
            {
                cavs_biari_decode_stuffing_bit(&p->cabac);
            }
            if(i_result != 0)
            {
                goto end;
            }
        }
        else
        {
            i_result = get_mb_i(p);

            if( p->b_error_flag )
            {
                  i_result = -1;
            }
            
            if (p->ph.b_aec_enable)
            {
                cavs_biari_decode_stuffing_bit(&p->cabac);
            }
            
            if( i_result != 0 )
            {
                goto end;
            }
        }

        p->i_mb_index++;
        if( p->i_mb_index >= p->i_slice_mb_end )  /* slice finished */
        {
            if( !b_interlaced )
            {
                if (p->i_mb_index >= p->i_mb_num)   /* frame finished */
                    p->b_complete = 1;
            }
            else
            {
                if( !p->b_bottom )
                {
                    if (p->i_mb_index >= (p->i_mb_num>>1))  /* top-field finished */
                        p->b_complete = 1;
                }
                else
                {
                    if (p->i_mb_index >= p->i_mb_num)   /* bot-field finished */
                        p->b_complete = 1;
                }
            }

            i_result = 0;

            goto end;
        }
    }

end:
    if (p->ph.b_aec_enable)
    {
        p->s = p->cabac.bs;
    }

    return i_result;
}




void cavs_decoder_init( cavs_decoder *p )
{
    int i_first=0;
    if(i_first!=0)
    {
    	return ;
    }
    i_first++;
    init_crop_table();   
	
    p->i_frame_decoded = -1;

    /* cpu  detect */
    p->cpu = cavs_cpu_detect ();
    printf("[cavs info]using cpu capabilities %s%s%s%s%s%s%s%s%s\n",
            p->cpu & CAVS_CPU_MMX ? "MMX " : "",
    		p->cpu & CAVS_CPU_MMXEXT ? "MMX2 " : "",
    		p->cpu & CAVS_CPU_SSE ? "SSE " : "",
    		p->cpu & CAVS_CPU_SSE2 ? "SSE2 " : "",
    		p->cpu & CAVS_CPU_SSSE3 ? "SSSE3 " : "",
    		p->cpu & CAVS_CPU_SSE4 ? "SSE4 " : "",
    		p->cpu & CAVS_CPU_3DNOW ? "3DNow! " : "",
    		p->cpu & CAVS_CPU_ALTIVEC ? "Altivec " : "",
    		p->cpu & CAVS_CPU_AVX ? "AVX" : "");

    /* intra 4x4 */
    intra_pred_lum_4x4[I_PRED_4x4_V] = predict_4x4_v;
    intra_pred_lum_4x4[I_PRED_4x4_H] = predict_4x4_h;
    intra_pred_lum_4x4[I_PRED_4x4_DC] = predict_4x4_dc;
    intra_pred_lum_4x4[I_PRED_4x4_DDL]    = predict_4x4_ddl;
    intra_pred_lum_4x4[I_PRED_4x4_DDR]    = predict_4x4_ddr;
    intra_pred_lum_4x4[I_PRED_4x4_VR]     = predict_4x4_vr;
    intra_pred_lum_4x4[I_PRED_4x4_HD]     = predict_4x4_hd;
    intra_pred_lum_4x4[I_PRED_4x4_VL]     = predict_4x4_vl;
    intra_pred_lum_4x4[I_PRED_4x4_HU]     = predict_4x4_hu;
    intra_pred_lum_4x4[I_PRED_4x4_DC_LEFT]= predict_4x4_dc_left;
    intra_pred_lum_4x4[I_PRED_4x4_DC_TOP] = predict_4x4_dc_top;
    intra_pred_lum_4x4[I_PRED_4x4_DC_128] = predict_4x4_dc_128;

#if !HAVE_MMX // FIXIT
    //*predict_filter4x4 = xavs_predict_4x4_filter;
    /* intra_8x8  luma */
    intra_pred_lum[INTRA_L_VERT] = xavs_intra_pred_vertical;
    intra_pred_lum[INTRA_L_HORIZ] = xavs_intra_pred_horizontal;
    intra_pred_lum[INTRA_L_LP] = xavs_intra_pred_dc_lp;
    intra_pred_lum[INTRA_L_DOWN_LEFT] = xavs_intra_pred_down_left;
    intra_pred_lum[INTRA_L_DOWN_RIGHT] = xavs_intra_pred_down_right;
    intra_pred_lum[INTRA_L_LP_LEFT] = xavs_intra_pred_dc_lp_left;
    intra_pred_lum[NTRA_L_LP_TOP] = xavs_intra_pred_dc_lp_top;
    intra_pred_lum[INTRA_L_DC_128] = xavs_intra_pred_dc_128;
#else
    /*
    intra_pred_lum[INTRA_L_VERT] = xavs_intra_pred_vertical;
    intra_pred_lum[INTRA_L_HORIZ] = xavs_intra_pred_horizontal;
    intra_pred_lum[INTRA_L_LP] = xavs_intra_pred_dc_lp;
    intra_pred_lum[INTRA_L_DOWN_LEFT] = xavs_intra_pred_down_left;
    intra_pred_lum[INTRA_L_DOWN_RIGHT] = xavs_intra_pred_down_right;
    intra_pred_lum[INTRA_L_LP_LEFT] = xavs_intra_pred_dc_lp_left;
    intra_pred_lum[NTRA_L_LP_TOP] = xavs_intra_pred_dc_lp_top;
    intra_pred_lum[INTRA_L_DC_128] = xavs_intra_pred_dc_128;
    if( p->cpu & CAVS_CPU_MMXEXT )
    {	
    	intra_pred_lum_asm[INTRA_L_VERT] = cavs_predict_8x8_v_mmxext;
    	intra_pred_lum_asm[INTRA_L_HORIZ] = cavs_predict_8x8_h_mmxext;//xavs_intra_pred_horizontal;
    	intra_pred_lum_asm[INTRA_L_LP] = cavs_predict_8x8_dc_mmxext;//xavs_intra_pred_dc_lp;
    	intra_pred_lum_asm[INTRA_L_DOWN_LEFT] = cavs_predict_8x8_ddl_mmxext;//xavs_intra_pred_down_left;
    	intra_pred_lum_asm[INTRA_L_DOWN_RIGHT] = cavs_predict_8x8_ddr_mmxext;//xavs_intra_pred_down_right;
    	intra_pred_lum_asm[INTRA_L_LP_LEFT] = cavs_predict_8x8_dc_left_mmxext;//xavs_intra_pred_dc_lp_left;
    	intra_pred_lum_asm[NTRA_L_LP_TOP] = cavs_predict_8x8_dc_top_mmxext;//xavs_intra_pred_dc_lp_top;
    	intra_pred_lum_asm[INTRA_L_DC_128] = cavs_predict_8x8_dc_128_mmxext;
    }
    */
    p->cavs_intra_luma[INTRA_L_VERT] = xavs_intra_pred_vertical;
    p->cavs_intra_luma[INTRA_L_HORIZ] = xavs_intra_pred_horizontal;
    p->cavs_intra_luma[INTRA_L_LP] = xavs_intra_pred_dc_lp;
    p->cavs_intra_luma[INTRA_L_DOWN_LEFT] = xavs_intra_pred_down_left;
    p->cavs_intra_luma[INTRA_L_DOWN_RIGHT] = xavs_intra_pred_down_right;
    p->cavs_intra_luma[INTRA_L_LP_LEFT] = xavs_intra_pred_dc_lp_left;
    p->cavs_intra_luma[NTRA_L_LP_TOP] = xavs_intra_pred_dc_lp_top;
    p->cavs_intra_luma[INTRA_L_DC_128] = xavs_intra_pred_dc_128;
    //if( p->cpu & CAVS_CPU_MMXEXT )
    {	
    	p->cavs_intra_luma[INTRA_L_VERT] = cavs_predict_8x8_v_mmxext;
    	p->cavs_intra_luma[INTRA_L_HORIZ] = cavs_predict_8x8_h_mmxext;//xavs_intra_pred_horizontal;
    	p->cavs_intra_luma[INTRA_L_LP] = cavs_predict_8x8_dc_mmxext;//xavs_intra_pred_dc_lp;
    	p->cavs_intra_luma[INTRA_L_DOWN_LEFT] = cavs_predict_8x8_ddl_mmxext;//xavs_intra_pred_down_left;
    	p->cavs_intra_luma[INTRA_L_DOWN_RIGHT] = cavs_predict_8x8_ddr_mmxext;//xavs_intra_pred_down_right;
    	p->cavs_intra_luma[INTRA_L_LP_LEFT] = cavs_predict_8x8_dc_left_mmxext;//xavs_intra_pred_dc_lp_left;
    	p->cavs_intra_luma[NTRA_L_LP_TOP] = cavs_predict_8x8_dc_top_mmxext;//xavs_intra_pred_dc_lp_top;
    	p->cavs_intra_luma[INTRA_L_DC_128] = cavs_predict_8x8_dc_128_mmxext;
    }

#endif

#if !HAVE_MMX // FIXIT
    /* chroma */
    intra_pred_chroma[INTRA_C_DC] = xavs_intra_pred_chroma_dc_lp;
    intra_pred_chroma[INTRA_C_HORIZ] = xavs_intra_pred_chroma_horizontal;
    intra_pred_chroma[INTRA_C_VERT] = xavs_intra_pred_chroma_vertical;
    intra_pred_chroma[INTRA_C_PLANE] = xavs_intra_pred_chroma_plane;
    intra_pred_chroma[INTRA_C_DC_LEFT] = xavs_intra_pred_chroma_dc_lp_left;
    intra_pred_chroma[INTRA_C_DC_TOP] = xavs_intra_pred_chroma_dc_lp_top;
    intra_pred_chroma[INTRA_C_DC_128] = xavs_intra_pred_chroma_dc_128;
#else
    /*
    intra_pred_chroma[INTRA_C_DC] = xavs_intra_pred_chroma_dc_lp;
    intra_pred_chroma[INTRA_C_HORIZ] = xavs_intra_pred_chroma_horizontal;
    intra_pred_chroma[INTRA_C_VERT] = xavs_intra_pred_chroma_vertical;
    intra_pred_chroma[INTRA_C_PLANE] = xavs_intra_pred_chroma_plane;
    intra_pred_chroma[INTRA_C_DC_LEFT] = xavs_intra_pred_chroma_dc_lp_left;
    intra_pred_chroma[INTRA_C_DC_TOP] = xavs_intra_pred_chroma_dc_lp_top;
    intra_pred_chroma[INTRA_C_DC_128] = xavs_intra_pred_chroma_dc_128;
    if( p->cpu & CAVS_CPU_MMXEXT )
    {
    	//intra_pred_chroma_asm[INTRA_C_DC] = cavs_predict_8x8c_dc_sse4;//xavs_intra_pred_chroma_dc_lp;
    	intra_pred_chroma_asm[INTRA_C_HORIZ] = cavs_predict_8x8c_h_mmxext;//xavs_intra_pred_chroma_horizontal;
    	intra_pred_chroma_asm[INTRA_C_VERT] = cavs_predict_8x8c_v_mmxext;//xavs_intra_pred_chroma_vertical;
    	intra_pred_chroma_asm[INTRA_C_PLANE] = cavs_predict_8x8c_p_mmxext;//xavs_intra_pred_chroma_plane;
    	//intra_pred_chroma_asm[INTRA_C_DC_LEFT] = cavs_predict_8x8c_dc_left_sse4;//xavs_intra_pred_chroma_dc_lp_left;
    	//intra_pred_chroma_asm[INTRA_C_DC_TOP] = cavs_predict_8x8c_dc_top_sse4;//xavs_intra_pred_chroma_dc_lp_top;
    	intra_pred_chroma_asm[INTRA_C_DC_128] = cavs_predict_8x8c_dc_128_mmxext;//xavs_intra_pred_chroma_dc_128;
    }

    if( p->cpu & XAVS_CPU_SSE4)
    {
    	intra_pred_chroma_asm[INTRA_C_DC] = cavs_predict_8x8c_dc_sse4;//xavs_intra_pred_chroma_dc_lp;
    	intra_pred_chroma_asm[INTRA_C_DC_LEFT] = cavs_predict_8x8c_dc_left_sse4;//xavs_intra_pred_chroma_dc_lp_left;
    	intra_pred_chroma_asm[INTRA_C_DC_TOP] = cavs_predict_8x8c_dc_top_sse4;//xavs_intra_pred_chroma_dc_lp_top;
    }
    */
    p->cavs_intra_chroma[INTRA_C_DC] = xavs_intra_pred_chroma_dc_lp;
    p->cavs_intra_chroma[INTRA_C_HORIZ] = xavs_intra_pred_chroma_horizontal;
    p->cavs_intra_chroma[INTRA_C_VERT] = xavs_intra_pred_chroma_vertical;
    p->cavs_intra_chroma[INTRA_C_PLANE] = xavs_intra_pred_chroma_plane;
    p->cavs_intra_chroma[INTRA_C_DC_LEFT] = xavs_intra_pred_chroma_dc_lp_left;
    p->cavs_intra_chroma[INTRA_C_DC_TOP] = xavs_intra_pred_chroma_dc_lp_top;
    p->cavs_intra_chroma[INTRA_C_DC_128] = xavs_intra_pred_chroma_dc_128;
    //if( p->cpu & CAVS_CPU_MMXEXT )
    {
    	//p->cavs_intra_chroma[INTRA_C_DC] = cavs_predict_8x8c_dc_sse4;//xavs_intra_pred_chroma_dc_lp;
    	p->cavs_intra_chroma[INTRA_C_HORIZ] = cavs_predict_8x8c_h_mmxext;//xavs_intra_pred_chroma_horizontal;
    	p->cavs_intra_chroma[INTRA_C_VERT] = cavs_predict_8x8c_v_mmxext;//xavs_intra_pred_chroma_vertical;
    	p->cavs_intra_chroma[INTRA_C_PLANE] = cavs_predict_8x8c_p_mmxext;//xavs_intra_pred_chroma_plane;
    	//p->cavs_intra_chroma[INTRA_C_DC_LEFT] = cavs_predict_8x8c_dc_left_sse4;//xavs_intra_pred_chroma_dc_lp_left;
    	//p->cavs_intra_chroma[INTRA_C_DC_TOP] = cavs_predict_8x8c_dc_top_sse4;//xavs_intra_pred_chroma_dc_lp_top;
    	p->cavs_intra_chroma[INTRA_C_DC_128] = cavs_predict_8x8c_dc_128_mmxext;//xavs_intra_pred_chroma_dc_128;
    }

    if( p->cpu & CAVS_CPU_SSE4)
    {
    	p->cavs_intra_chroma[INTRA_C_DC] = cavs_predict_8x8c_dc_sse4;//xavs_intra_pred_chroma_dc_lp;
    	p->cavs_intra_chroma[INTRA_C_DC_LEFT] = cavs_predict_8x8c_dc_left_sse4;//xavs_intra_pred_chroma_dc_lp_left;
    	p->cavs_intra_chroma[INTRA_C_DC_TOP] = cavs_predict_8x8c_dc_top_sse4;//xavs_intra_pred_chroma_dc_lp_top;
    }
#endif

    /* deblock */
#if !HAVE_MMX
    cavs_filter_lv = cavs_filter_lv_c;
    cavs_filter_lh = cavs_filter_lh_c;
    cavs_filter_cv = cavs_filter_cv_c;
    cavs_filter_ch = cavs_filter_ch_c;
#else
    p->filter_lv = cavs_filter_lv_c;
    p->filter_lh = cavs_filter_lh_c;
    p->filter_cv = cavs_filter_cv_c;
    p->filter_ch = cavs_filter_ch_c;
    //if( p->cpu & CAVS_CPU_SSE2)
    {   
        p->filter_lv = cavs_filter_lv_sse2;
        p->filter_lh = cavs_filter_lh_sse2;
    }
    //if( p->cpu & CAVS_CPU_SSE2 )
    {
        p->filter_cv = cavs_filter_cv_mmxext;
        p->filter_ch = cavs_filter_ch_mmxext;
    }
#endif

    xavs_idct8_add = xavs_idct8_add_c;
    xavs_idct4_add = xavs_idct4_add_c;

#if !HAVE_MMX
    //色度八分之一像素插值
    xavs_chroma_put[0] = xavs_chroma_mc8_put_c;
    xavs_chroma_put[1] = xavs_chroma_mc4_put_c;
    xavs_chroma_put[2] = xavs_chroma_mc2_put_c;

    xavs_chroma_avg[0] = xavs_chroma_mc8_avg_c;
    xavs_chroma_avg[1] = xavs_chroma_mc4_avg_c;
    xavs_chroma_avg[2] = xavs_chroma_mc2_avg_c;
#else
    p->put_h264_chroma_pixels_tab[0] = xavs_chroma_mc8_put_c;
    p->put_h264_chroma_pixels_tab[1] = xavs_chroma_mc4_put_c;
    p->put_h264_chroma_pixels_tab[2] = xavs_chroma_mc2_put_c;

    p->avg_h264_chroma_pixels_tab[0] = xavs_chroma_mc8_avg_c;
    p->avg_h264_chroma_pixels_tab[1] = xavs_chroma_mc4_avg_c;
    p->avg_h264_chroma_pixels_tab[2] = xavs_chroma_mc2_avg_c;
    if( p->cpu & CAVS_CPU_SSSE3)
    {
    	p->put_h264_chroma_pixels_tab[0] = cavs_put_h264_chroma_mc8_rnd_ssse3;//mmxext;// xavs_chroma_mc8_put_c;
    	p->put_h264_chroma_pixels_tab[1] = cavs_put_h264_chroma_mc4_ssse3;//mmxext;// xavs_chroma_mc4_put_c;//
    	p->avg_h264_chroma_pixels_tab[0] = cavs_avg_h264_chroma_mc8_rnd_ssse3;//mmxext;//xavs_chroma_mc8_avg_c;
    	p->avg_h264_chroma_pixels_tab[1] = cavs_avg_h264_chroma_mc4_ssse3;//mmxext;//xavs_chroma_mc4_avg_c;
    }
    p->put_h264_chroma_pixels_tab[2] = xavs_chroma_mc2_put_c;
    p->avg_h264_chroma_pixels_tab[2] = xavs_chroma_mc2_avg_c;
#endif

#if !HAVE_MMX
    //16x16块 亮度四分之一像素插值/运动补偿xavs_luma_put[x=0..3][y=0..3] 对应16个四分之一像素位置
     xavs_luma_put[0][ 0] =  cavs_qpel16_put_mc00_c;
     xavs_luma_put[0][ 1] =  cavs_qpel16_put_mc10_c;
     xavs_luma_put[0][ 2] =  cavs_qpel16_put_mc20_c;
     xavs_luma_put[0][ 3] =  cavs_qpel16_put_mc30_c;
     xavs_luma_put[0][ 4] =  cavs_qpel16_put_mc01_c;
     xavs_luma_put[0][ 5] =  cavs_qpel16_put_mc11_c;
     xavs_luma_put[0][ 6] =  cavs_qpel16_put_mc21_c;
     xavs_luma_put[0][ 7] =  cavs_qpel16_put_mc31_c;
     xavs_luma_put[0][ 8] =  cavs_qpel16_put_mc02_c;
     xavs_luma_put[0][ 9] =  cavs_qpel16_put_mc12_c;
     xavs_luma_put[0][10] =  cavs_qpel16_put_mc22_c;
     xavs_luma_put[0][11] =  cavs_qpel16_put_mc32_c;
     xavs_luma_put[0][12] =  cavs_qpel16_put_mc03_c;
     xavs_luma_put[0][13] =  cavs_qpel16_put_mc13_c;
     xavs_luma_put[0][14] =  cavs_qpel16_put_mc23_c;
     xavs_luma_put[0][15] =  cavs_qpel16_put_mc33_c;

     xavs_luma_put[1][ 0] =  cavs_qpel8_put_mc00_c;
     xavs_luma_put[1][ 1] =  cavs_qpel8_put_mc10_c;
     xavs_luma_put[1][ 2] =  cavs_qpel8_put_mc20_c;
     xavs_luma_put[1][ 3] =  cavs_qpel8_put_mc30_c;
     xavs_luma_put[1][ 4] =  cavs_qpel8_put_mc01_c;
     xavs_luma_put[1][ 5] =  cavs_qpel8_put_mc11_c;
     xavs_luma_put[1][ 6] =  cavs_qpel8_put_mc21_c;
     xavs_luma_put[1][ 7] =  cavs_qpel8_put_mc31_c;
     xavs_luma_put[1][ 8] =  cavs_qpel8_put_mc02_c;
     xavs_luma_put[1][ 9] =  cavs_qpel8_put_mc12_c;
     xavs_luma_put[1][10] =  cavs_qpel8_put_mc22_c;
     xavs_luma_put[1][11] =  cavs_qpel8_put_mc32_c;
     xavs_luma_put[1][12] =  cavs_qpel8_put_mc03_c;
     xavs_luma_put[1][13] =  cavs_qpel8_put_mc13_c;
     xavs_luma_put[1][14] =  cavs_qpel8_put_mc23_c;
     xavs_luma_put[1][15] =  cavs_qpel8_put_mc33_c;

     xavs_luma_avg[0][ 0] =  cavs_qpel16_avg_mc00_c;
     xavs_luma_avg[0][ 1] =  cavs_qpel16_avg_mc10_c;
     xavs_luma_avg[0][ 2] =  cavs_qpel16_avg_mc20_c;
     xavs_luma_avg[0][ 3] =  cavs_qpel16_avg_mc30_c;
     xavs_luma_avg[0][ 4] =  cavs_qpel16_avg_mc01_c;
     xavs_luma_avg[0][ 5] =  cavs_qpel16_avg_mc11_c;
     xavs_luma_avg[0][ 6] =  cavs_qpel16_avg_mc21_c;
     xavs_luma_avg[0][ 7] =  cavs_qpel16_avg_mc31_c;
     xavs_luma_avg[0][ 8] =  cavs_qpel16_avg_mc02_c;
     xavs_luma_avg[0][ 9] =  cavs_qpel16_avg_mc12_c;
     xavs_luma_avg[0][10] =  cavs_qpel16_avg_mc22_c;
     xavs_luma_avg[0][11] =  cavs_qpel16_avg_mc32_c;
     xavs_luma_avg[0][12] =  cavs_qpel16_avg_mc03_c;
     xavs_luma_avg[0][13] =  cavs_qpel16_avg_mc13_c;
     xavs_luma_avg[0][14] =  cavs_qpel16_avg_mc23_c;
     xavs_luma_avg[0][15] =  cavs_qpel16_avg_mc33_c;

     xavs_luma_avg[1][ 0] =  cavs_qpel8_avg_mc00_c;
     xavs_luma_avg[1][ 1] =  cavs_qpel8_avg_mc10_c;
     xavs_luma_avg[1][ 2] =  cavs_qpel8_avg_mc20_c;
     xavs_luma_avg[1][ 3] =  cavs_qpel8_avg_mc30_c;
     xavs_luma_avg[1][ 4] =  cavs_qpel8_avg_mc01_c;
     xavs_luma_avg[1][ 5] =  cavs_qpel8_avg_mc11_c;
     xavs_luma_avg[1][ 6] =  cavs_qpel8_avg_mc21_c;
     xavs_luma_avg[1][ 7] =  cavs_qpel8_avg_mc31_c;
     xavs_luma_avg[1][ 8] =  cavs_qpel8_avg_mc02_c;
     xavs_luma_avg[1][ 9] =  cavs_qpel8_avg_mc12_c;
     xavs_luma_avg[1][10] =  cavs_qpel8_avg_mc22_c;
     xavs_luma_avg[1][11] =  cavs_qpel8_avg_mc32_c;
     xavs_luma_avg[1][12] =  cavs_qpel8_avg_mc03_c;
     xavs_luma_avg[1][13] =  cavs_qpel8_avg_mc13_c;
     xavs_luma_avg[1][14] =  cavs_qpel8_avg_mc23_c;
     xavs_luma_avg[1][15] =  cavs_qpel8_avg_mc33_c;
#else
     /*c*/
     p->put_cavs_qpel_pixels_tab[0][0] =  cavs_qpel16_put_mc00_c; 
     p->put_cavs_qpel_pixels_tab[0][ 1] = cavs_qpel16_put_mc10_c;
     p->put_cavs_qpel_pixels_tab[0][ 2] = cavs_qpel16_put_mc20_c;
     p->put_cavs_qpel_pixels_tab[0][ 3] = cavs_qpel16_put_mc30_c;
     p->put_cavs_qpel_pixels_tab[0][ 4] = cavs_qpel16_put_mc01_c; 
     p->put_cavs_qpel_pixels_tab[0][ 5] = cavs_qpel16_put_mc11_c;
     p->put_cavs_qpel_pixels_tab[0][ 6] = cavs_qpel16_put_mc21_c;
     p->put_cavs_qpel_pixels_tab[0][ 7] = cavs_qpel16_put_mc31_c;
     p->put_cavs_qpel_pixels_tab[0][ 8] = cavs_qpel16_put_mc02_c;
     p->put_cavs_qpel_pixels_tab[0][ 9] = cavs_qpel16_put_mc12_c;
     p->put_cavs_qpel_pixels_tab[0][10] = cavs_qpel16_put_mc22_c;
     p->put_cavs_qpel_pixels_tab[0][11] = cavs_qpel16_put_mc32_c;
     p->put_cavs_qpel_pixels_tab[0][12] = cavs_qpel16_put_mc03_c;
     p->put_cavs_qpel_pixels_tab[0][13] = cavs_qpel16_put_mc13_c;
     p->put_cavs_qpel_pixels_tab[0][14] = cavs_qpel16_put_mc23_c;
     p->put_cavs_qpel_pixels_tab[0][15] = cavs_qpel16_put_mc33_c;

     p->put_cavs_qpel_pixels_tab[1][ 0] = cavs_qpel8_put_mc00_c; 
     p->put_cavs_qpel_pixels_tab[1][ 1] = cavs_qpel8_put_mc10_c;
     p->put_cavs_qpel_pixels_tab[1][ 2] = cavs_qpel8_put_mc20_c; 
     p->put_cavs_qpel_pixels_tab[1][ 3] = cavs_qpel8_put_mc30_c;
     p->put_cavs_qpel_pixels_tab[1][ 4] = cavs_qpel8_put_mc01_c; 
     p->put_cavs_qpel_pixels_tab[1][ 5] = cavs_qpel8_put_mc11_c;
     p->put_cavs_qpel_pixels_tab[1][ 6] = cavs_qpel8_put_mc21_c;
     p->put_cavs_qpel_pixels_tab[1][ 7] = cavs_qpel8_put_mc31_c;
     p->put_cavs_qpel_pixels_tab[1][ 8] = cavs_qpel8_put_mc02_c; 
     p->put_cavs_qpel_pixels_tab[1][ 9] = cavs_qpel8_put_mc12_c;
     p->put_cavs_qpel_pixels_tab[1][10] = cavs_qpel8_put_mc22_c;
     p->put_cavs_qpel_pixels_tab[1][11] = cavs_qpel8_put_mc32_c;
     p->put_cavs_qpel_pixels_tab[1][12] = cavs_qpel8_put_mc03_c; 
     p->put_cavs_qpel_pixels_tab[1][13] = cavs_qpel8_put_mc13_c;
     p->put_cavs_qpel_pixels_tab[1][14] = cavs_qpel8_put_mc23_c;
     p->put_cavs_qpel_pixels_tab[1][15] = cavs_qpel8_put_mc33_c;

     p->avg_cavs_qpel_pixels_tab[0][ 0] = cavs_qpel16_avg_mc00_c; 
     p->avg_cavs_qpel_pixels_tab[0][ 1] = cavs_qpel16_avg_mc10_c;
     p->avg_cavs_qpel_pixels_tab[0][ 2] = cavs_qpel16_avg_mc20_c;
     p->avg_cavs_qpel_pixels_tab[0][ 3] = cavs_qpel16_avg_mc30_c;
     p->avg_cavs_qpel_pixels_tab[0][ 4] = cavs_qpel16_avg_mc01_c;
     p->avg_cavs_qpel_pixels_tab[0][ 5] = cavs_qpel16_avg_mc11_c;
     p->avg_cavs_qpel_pixels_tab[0][ 6] = cavs_qpel16_avg_mc21_c;
     p->avg_cavs_qpel_pixels_tab[0][ 7] = cavs_qpel16_avg_mc31_c;
     p->avg_cavs_qpel_pixels_tab[0][ 8] = cavs_qpel16_avg_mc02_c;
     p->avg_cavs_qpel_pixels_tab[0][ 9] = cavs_qpel16_avg_mc12_c;
     p->avg_cavs_qpel_pixels_tab[0][10] = cavs_qpel16_avg_mc22_c;
     p->avg_cavs_qpel_pixels_tab[0][11] = cavs_qpel16_avg_mc32_c;
     p->avg_cavs_qpel_pixels_tab[0][12] = cavs_qpel16_avg_mc03_c;
     p->avg_cavs_qpel_pixels_tab[0][13] = cavs_qpel16_avg_mc13_c;
     p->avg_cavs_qpel_pixels_tab[0][14] = cavs_qpel16_avg_mc23_c;
     p->avg_cavs_qpel_pixels_tab[0][15] = cavs_qpel16_avg_mc33_c;

     p->avg_cavs_qpel_pixels_tab[1][ 0] = cavs_qpel8_avg_mc00_c;
     p->avg_cavs_qpel_pixels_tab[1][ 1] = cavs_qpel8_avg_mc10_c;
     p->avg_cavs_qpel_pixels_tab[1][ 2] = cavs_qpel8_avg_mc20_c;
     p->avg_cavs_qpel_pixels_tab[1][ 3] = cavs_qpel8_avg_mc30_c;
     p->avg_cavs_qpel_pixels_tab[1][ 4] = cavs_qpel8_avg_mc01_c;
     p->avg_cavs_qpel_pixels_tab[1][ 5] = cavs_qpel8_avg_mc11_c;
     p->avg_cavs_qpel_pixels_tab[1][ 6] = cavs_qpel8_avg_mc21_c;
     p->avg_cavs_qpel_pixels_tab[1][ 7] = cavs_qpel8_avg_mc31_c;
     p->avg_cavs_qpel_pixels_tab[1][ 8] = cavs_qpel8_avg_mc02_c;
     p->avg_cavs_qpel_pixels_tab[1][ 9] = cavs_qpel8_avg_mc12_c;
     p->avg_cavs_qpel_pixels_tab[1][10] = cavs_qpel8_avg_mc22_c;
     p->avg_cavs_qpel_pixels_tab[1][11] = cavs_qpel8_avg_mc32_c;
     p->avg_cavs_qpel_pixels_tab[1][12] = cavs_qpel8_avg_mc03_c;
     p->avg_cavs_qpel_pixels_tab[1][13] = cavs_qpel8_avg_mc13_c;
     p->avg_cavs_qpel_pixels_tab[1][14] = cavs_qpel8_avg_mc23_c;
     p->avg_cavs_qpel_pixels_tab[1][15] = cavs_qpel8_avg_mc33_c;

     /* asm */
     /* put qpel 16 */
     //if( p->cpu & CAVS_CPU_MMXEXT )
     {
        p->put_cavs_qpel_pixels_tab[0][0] =  ff_put_bavs_qpel16_mc00_mmxext;
        p->put_cavs_qpel_pixels_tab[0][ 2] = ff_put_cavs_qpel16_mc20_mmxext;
        p->put_cavs_qpel_pixels_tab[0][ 4] =  ff_put_cavs_qpel16_mc01_mmxext;
        p->put_cavs_qpel_pixels_tab[0][ 8] =  ff_put_cavs_qpel16_mc02_mmxext;
        p->put_cavs_qpel_pixels_tab[0][12] =  ff_put_cavs_qpel16_mc03_mmxext;
     }

     //if( p->cpu & CAVS_CPU_SSE2 )
     { 
        p->put_cavs_qpel_pixels_tab[0][ 1] =  ff_put_cavs_qpel16_mc10_mmxext;
        p->put_cavs_qpel_pixels_tab[0][ 3] =  ff_put_cavs_qpel16_mc30_mmxext;
        p->put_cavs_qpel_pixels_tab[0][ 5] =  ff_put_cavs_qpel16_mc11_mmxext;
        p->put_cavs_qpel_pixels_tab[0][ 7] =  ff_put_cavs_qpel16_mc31_mmxext;
        p->put_cavs_qpel_pixels_tab[0][10] =  ff_put_cavs_qpel16_mc22_mmxext;
        p->put_cavs_qpel_pixels_tab[0][13] =  ff_put_cavs_qpel16_mc13_mmxext;
        p->put_cavs_qpel_pixels_tab[0][15] =  ff_put_cavs_qpel16_mc33_mmxext;
     }

     if( p->cpu & CAVS_CPU_SSE4 )
     {
        p->put_cavs_qpel_pixels_tab[0][ 6] =  ff_put_cavs_qpel16_mc21_mmxext;
        p->put_cavs_qpel_pixels_tab[0][ 9] =  ff_put_cavs_qpel16_mc12_mmxext;
        p->put_cavs_qpel_pixels_tab[0][11] =  ff_put_cavs_qpel16_mc32_mmxext;
        p->put_cavs_qpel_pixels_tab[0][14] =  ff_put_cavs_qpel16_mc23_mmxext;
     }
     
     /* put qpel 8 */
     //if( p->cpu & CAVS_CPU_MMXEXT )
     {
    	 p->put_cavs_qpel_pixels_tab[1][ 0] = ff_put_bavs_qpel8_mc00_mmxext;
    	 p->put_cavs_qpel_pixels_tab[1][ 2] =  ff_put_cavs_qpel8_mc20_mmxext;
    	 p->put_cavs_qpel_pixels_tab[1][ 4] =  ff_put_cavs_qpel8_mc01_mmxext;
    	 p->put_cavs_qpel_pixels_tab[1][ 8] =  ff_put_cavs_qpel8_mc02_mmxext;
    	 p->put_cavs_qpel_pixels_tab[1][12] =  ff_put_cavs_qpel8_mc03_mmxext;
     }

     //if( p->cpu & CAVS_CPU_SSE2 )
     {
    	p->put_cavs_qpel_pixels_tab[1][ 1] =  ff_put_cavs_qpel8_mc10_mmxext;
    	p->put_cavs_qpel_pixels_tab[1][ 3] =  ff_put_cavs_qpel8_mc30_mmxext;
    	p->put_cavs_qpel_pixels_tab[1][ 5] =  ff_put_cavs_qpel8_mc11_mmxext;
    	p->put_cavs_qpel_pixels_tab[1][ 7] =  ff_put_cavs_qpel8_mc31_mmxext;
    	p->put_cavs_qpel_pixels_tab[1][10] =  ff_put_cavs_qpel8_mc22_mmxext;
    	p->put_cavs_qpel_pixels_tab[1][13] =  ff_put_cavs_qpel8_mc13_mmxext;
    	p->put_cavs_qpel_pixels_tab[1][15] =  ff_put_cavs_qpel8_mc33_mmxext;
     }

     if( p->cpu & CAVS_CPU_SSE4 )
     {
    	p->put_cavs_qpel_pixels_tab[1][ 6] =  ff_put_cavs_qpel8_mc21_mmxext;
    	p->put_cavs_qpel_pixels_tab[1][ 9] =  ff_put_cavs_qpel8_mc12_mmxext;
    	p->put_cavs_qpel_pixels_tab[1][11] =  ff_put_cavs_qpel8_mc32_mmxext;
    	p->put_cavs_qpel_pixels_tab[1][14] =  ff_put_cavs_qpel8_mc23_mmxext;
     }

     /* avg qpel 16 */
     //if( p->cpu & CAVS_CPU_MMXEXT )
     {
    	p->avg_cavs_qpel_pixels_tab[0][ 0] =  ff_avg_bavs_qpel16_mc00_mmxext;
    	p->avg_cavs_qpel_pixels_tab[0][ 2] =  ff_avg_cavs_qpel16_mc20_mmxext;
    	p->avg_cavs_qpel_pixels_tab[0][ 4] =  ff_avg_cavs_qpel16_mc01_mmxext;
    	p->avg_cavs_qpel_pixels_tab[0][ 8] =  ff_avg_cavs_qpel16_mc02_mmxext;
    	p->avg_cavs_qpel_pixels_tab[0][12] =  ff_avg_cavs_qpel16_mc03_mmxext;
     }
     //if( p->cpu & CAVS_CPU_SSE2 )
     {
        p->avg_cavs_qpel_pixels_tab[0][ 1] =  ff_avg_cavs_qpel16_mc10_mmxext;    
        p->avg_cavs_qpel_pixels_tab[0][ 3] =  ff_avg_cavs_qpel16_mc30_mmxext;   
        p->avg_cavs_qpel_pixels_tab[0][ 5] =  ff_avg_cavs_qpel16_mc11_mmxext;
        p->avg_cavs_qpel_pixels_tab[0][ 7] =  ff_avg_cavs_qpel16_mc31_mmxext;
        p->avg_cavs_qpel_pixels_tab[0][10] =  ff_avg_cavs_qpel16_mc22_mmxext;
        p->avg_cavs_qpel_pixels_tab[0][13] =  ff_avg_cavs_qpel16_mc13_mmxext;
        p->avg_cavs_qpel_pixels_tab[0][15] =  ff_avg_cavs_qpel16_mc33_mmxext;
     }
     if( p->cpu & CAVS_CPU_SSE4 )
     {
        p->avg_cavs_qpel_pixels_tab[0][ 6] =  ff_avg_cavs_qpel16_mc21_mmxext;     
        p->avg_cavs_qpel_pixels_tab[0][ 9] =  ff_avg_cavs_qpel16_mc12_mmxext;
        p->avg_cavs_qpel_pixels_tab[0][11] =  ff_avg_cavs_qpel16_mc32_mmxext;  
        p->avg_cavs_qpel_pixels_tab[0][14] =  ff_avg_cavs_qpel16_mc23_mmxext;
     }

     /* avg qpel 8 */
     //if( p->cpu & CAVS_CPU_MMXEXT )
     {
        p->avg_cavs_qpel_pixels_tab[1][ 0] = ff_avg_bavs_qpel8_mc00_mmxext;
        p->avg_cavs_qpel_pixels_tab[1][ 2] =  ff_avg_cavs_qpel8_mc20_mmxext;
        p->avg_cavs_qpel_pixels_tab[1][ 4] =  ff_avg_cavs_qpel8_mc01_mmxext;
        p->avg_cavs_qpel_pixels_tab[1][ 8] =  ff_avg_cavs_qpel8_mc02_mmxext;
        p->avg_cavs_qpel_pixels_tab[1][12] =  ff_avg_cavs_qpel8_mc03_mmxext;
     }
     //if( p->cpu & CAVS_CPU_SSE2 )
     {
        p->avg_cavs_qpel_pixels_tab[1][ 1] =  ff_avg_cavs_qpel8_mc10_mmxext;
        p->avg_cavs_qpel_pixels_tab[1][ 3] =  ff_avg_cavs_qpel8_mc30_mmxext;
        p->avg_cavs_qpel_pixels_tab[1][ 5] =  ff_avg_cavs_qpel8_mc11_mmxext;
        p->avg_cavs_qpel_pixels_tab[1][ 7] = ff_avg_cavs_qpel8_mc31_mmxext ;
        p->avg_cavs_qpel_pixels_tab[1][10] =  ff_avg_cavs_qpel8_mc22_mmxext;
        p->avg_cavs_qpel_pixels_tab[1][13] =  ff_avg_cavs_qpel8_mc13_mmxext;
        p->avg_cavs_qpel_pixels_tab[1][15] =  ff_avg_cavs_qpel8_mc33_mmxext;
     }
     if( p->cpu & CAVS_CPU_SSE4 )
     {
        p->avg_cavs_qpel_pixels_tab[1][ 6] =  ff_avg_cavs_qpel8_mc21_mmxext;
        p->avg_cavs_qpel_pixels_tab[1][ 9] =  ff_avg_cavs_qpel8_mc12_mmxext;
        p->avg_cavs_qpel_pixels_tab[1][11] =  ff_avg_cavs_qpel8_mc32_mmxext;
        p->avg_cavs_qpel_pixels_tab[1][14] =  ff_avg_cavs_qpel8_mc23_mmxext;
     }

#endif

#if B_MB_WEIGHTING
    /* weighting prediction */
    p->cavs_avg_pixels_tab[0] = pixel_avg_16x16_c;
    p->cavs_avg_pixels_tab[1] = pixel_avg_8x8_c;
    p->cavs_avg_pixels_tab[2] = pixel_avg_4x4_c;

#if HAVE_MMX
	if( p->cpu & CAVS_CPU_MMX2 )
    {
	    p->cavs_avg_pixels_tab[0] = cavs_pixel_avg_16x16_mmx2;
	    p->cavs_avg_pixels_tab[1] = cavs_pixel_avg_8x8_mmx2;
	    p->cavs_avg_pixels_tab[2] = cavs_pixel_avg_4x4_mmx2;
	}

#if HIGH_BIT_DEPTH

	if( p->cpu & CAVS_CPU_SSE2 )
    {
	    p->cavs_avg_pixels_tab[0] = cavs_pixel_avg_16x16_sse2;
	    p->cavs_avg_pixels_tab[1] = cavs_pixel_avg_8x8_sse2;
	    p->cavs_avg_pixels_tab[2] = cavs_pixel_avg_4x4_sse2;
	}
#else //!HIGH_BIT_DEPTH
	if( p->cpu & CAVS_CPU_SSSE3 )
    {
	    p->cavs_avg_pixels_tab[0] = cavs_pixel_avg_16x16_ssse3;
	    p->cavs_avg_pixels_tab[1] = cavs_pixel_avg_8x8_ssse3;
	    p->cavs_avg_pixels_tab[2] = cavs_pixel_avg_4x4_ssse3;
	}

	//if( p->cpu & CAVS_CPU_AVX2 )
    //{
	//    p->cavs_avg_pixels_tab[0] = cavs_pixel_avg_16x16_avx2;
	//}
#endif /* HIGH_BIT_DEPTH */

#endif /* HAVE_MMX */

#endif /* B_MB_WEIGHTING */     
}

extern void *cavs_memcpy_aligned_sse( void *dst, const void *src, int n );
extern void *cavs_memcpy_aligned_mmx( void *dst, const void *src, int n );

static void cavs_out_image_yuv420(cavs_decoder *p, xavs_image *p_cur, uint8_t *p_yuv_in[3], int b_bottom )
{
    uint32_t j = 0, vs, hs, fs;
    unsigned char* p_bufimg;
    uint8_t *p_yuv;
    int b_interlaced = p->param.b_interlaced;
    int b_align = 0;

    //b_align = 0;//(p->vsh.i_horizontal_size%16) ==0; // BUG : not for SD
    
    if( !b_interlaced )
    {
        if( 1/*!b_align*/ )
        {
            /* Y */
            p_bufimg = p_cur->p_data[0];
            p_yuv = p_yuv_in[0];
            for (j = 0; j < p->vsh.i_vertical_size; j++)
            {
                memcpy(p_yuv, p_bufimg, p->vsh.i_horizontal_size);
                p_yuv += p->vsh.i_horizontal_size;
                p_bufimg += p_cur->i_stride[0];
            }

            /* U */
            p_yuv = p_yuv_in[1];
            p_bufimg = p_cur->p_data[1];
            for (j = 0; j < p->vsh.i_vertical_size/2; j++)
            {
                memcpy(p_yuv, p_bufimg, p->vsh.i_horizontal_size/2); 
                p_yuv += p->vsh.i_horizontal_size/2;
                p_bufimg += p_cur->i_stride[1];
            }

            /* V */
            p_yuv = p_yuv_in[2];
            p_bufimg = p_cur->p_data[2];
            for ( j = 0; j < p->vsh.i_vertical_size/2; j++ )
            {
                memcpy(p_yuv, p_bufimg, p->vsh.i_horizontal_size/2);      
                p_yuv += p->vsh.i_horizontal_size/2;
                p_bufimg += p_cur->i_stride[2];
            }
        }
        else
        {
            /* Y */
            p_bufimg = p_cur->p_data[0];
            p_yuv = p_yuv_in[0];
            for (j = 0; j < p->vsh.i_vertical_size; j++)
            {
                cavs_memcpy_aligned_sse(p_yuv, p_bufimg, p->vsh.i_horizontal_size);
                p_yuv += p->vsh.i_horizontal_size;
                p_bufimg += p_cur->i_stride[0];
            }   

            /* U */
            p_yuv = p_yuv_in[1];
            p_bufimg = p_cur->p_data[1];
            for (j = 0; j < p->vsh.i_vertical_size/2; j++)
            {
                cavs_memcpy_aligned_mmx(p_yuv, p_bufimg, p->vsh.i_horizontal_size/2); 
                p_yuv += p->vsh.i_horizontal_size/2;
                p_bufimg += p_cur->i_stride[1];
            }

            /* V */
            p_yuv = p_yuv_in[2];
            p_bufimg = p_cur->p_data[2];
            for ( j = 0; j < p->vsh.i_vertical_size/2; j++ )
            {
                cavs_memcpy_aligned_mmx(p_yuv, p_bufimg, p->vsh.i_horizontal_size/2);      
                p_yuv += p->vsh.i_horizontal_size/2;
                p_bufimg += p_cur->i_stride[2];
            }
        }
    }
    else
    {
        int i_offset_y = 0;
        int i_offset_uv = 0;
        int i_bufimg_y = 0;
        int i_bufimg_uv = 0;

        if( /*b_interlaced &&*/ /*p->b_bottom*/b_bottom )
        {
            i_offset_y = ((p_cur->i_stride[0])>>1) - 32;
            i_offset_uv = ((p_cur->i_stride[1])>>1) - 16;
            i_bufimg_y =  (p_cur->i_stride[0]>>1);
            i_bufimg_uv =  (p_cur->i_stride[1]>>1);
        }

        if( 1/*!b_align*/ )
        {
            /* Y */
			p_yuv = p_yuv_in[0] + i_offset_y; fs = p->vsh.i_horizontal_size << 1;
			p_bufimg = p_cur->p_data[0] + i_bufimg_y; vs = (p->vsh.i_vertical_size >> b_interlaced);
            for ( j = 0; j < vs/*(p->vsh.i_vertical_size>>b_interlaced)*/; j++ )
            {
                memcpy( p_yuv, p_bufimg, p->vsh.i_horizontal_size);
				p_yuv += fs;// (p->vsh.i_horizontal_size << 1/*b_interlaced*/);
                p_bufimg += p_cur->i_stride[0];
            }

            /* U */
			p_yuv = p_yuv_in[1] + i_offset_uv; hs = p->vsh.i_horizontal_size / 2; fs = ((p->vsh.i_horizontal_size >> 1/*/2*/) << 1/*b_interlaced*/);
			p_bufimg = p_cur->p_data[1] + i_bufimg_uv; vs = ((p->vsh.i_vertical_size / 2) >> b_interlaced);
            for ( j = 0; j < vs/*((p->vsh.i_vertical_size/2)>>b_interlaced)*/; j++ )
            {
                memcpy( p_yuv, p_bufimg, hs/*p->vsh.i_horizontal_size/2*/ );
				p_yuv += fs;// ((p->vsh.i_horizontal_size >> 1/*/2*/) << 1/*b_interlaced*/);
                p_bufimg += p_cur->i_stride[1];
            }

            /* V */
            p_yuv = p_yuv_in[2] + i_offset_uv;
			p_bufimg = p_cur->p_data[2] + i_bufimg_uv;
            for ( j = 0; j < vs/*((p->vsh.i_vertical_size/2)>>b_interlaced)*/; j++ )
            {  
                memcpy(p_yuv, p_bufimg, hs/*p->vsh.i_horizontal_size/2*/); 
				p_yuv += fs;// ((p->vsh.i_horizontal_size >> 1/*/2*/) << 1/*b_interlaced*/);
                p_bufimg += p_cur->i_stride[2];
            }
        }
        else
        {
            /* Y */
            p_yuv = p_yuv_in[0] + i_offset_y;
            p_bufimg = p_cur->p_data[0] + i_bufimg_y;
            for ( j = 0; j < (p->vsh.i_vertical_size>>b_interlaced); j++ )
            {
                cavs_memcpy_aligned_sse( p_yuv, p_bufimg, p->vsh.i_horizontal_size);
                p_yuv += (p->vsh.i_horizontal_size<<b_interlaced);
                p_bufimg += p_cur->i_stride[0];
            } 

            /* U */
            p_yuv = p_yuv_in[1] + i_offset_uv;
            p_bufimg = p_cur->p_data[1] + i_bufimg_uv;
            for ( j = 0; j < ((p->vsh.i_vertical_size/2)>>b_interlaced); j++ )
            {
                cavs_memcpy_aligned_mmx( p_yuv, p_bufimg, p->vsh.i_horizontal_size/2 );
                p_yuv += ((p->vsh.i_horizontal_size/2)<<b_interlaced);
                p_bufimg += p_cur->i_stride[1];
            }

            /* V */
            p_yuv = p_yuv_in[2] + i_offset_uv;
            p_bufimg = p_cur->p_data[2] + i_bufimg_uv;
            for ( j = 0; j < ((p->vsh.i_vertical_size/2)>>b_interlaced); j++ )
            {  
                cavs_memcpy_aligned_mmx(p_yuv, p_bufimg, p->vsh.i_horizontal_size/2); 
                p_yuv += ((p->vsh.i_horizontal_size/2)<<b_interlaced);
                p_bufimg += p_cur->i_stride[2];
            }
        }
    }
}

static int cavs_get_extension(cavs_bitstream *s, cavs_decoder *p)
{
	int extension_id = cavs_bitstream_get_bits(s, 4);
	if (extension_id == 2)	//sequence display extension
	{
		return cavs_get_sequence_display_extension(s, &p->sde);
	}
	else if (extension_id == 4) //copyright extension
	{
		return cavs_get_copyright_extension(s, &p->ce);
	}
	else if (extension_id == 11) //camera extension
	{
		return cavs_get_camera_parameters_extension(s, &p->cpe);
	}
	else
	{
		while (((*(uint32_t*)(s->p)) & 0x00ffffff) != 0x000001/*0x00010000*/ && s->p < s->p_end )
			cavs_bitstream_get_bits(s, 8);
	}
	return 0;
}

int cavs_macroblock_cache_init( cavs_decoder *p );

int cavs_decoder_realloc( void *p_decoder )
{
    cavs_decoder *p=(cavs_decoder *)p_decoder;
    
    if(cavs_alloc_resource(p)!=0)
    {
        return CAVS_ERROR;
    }
    cavs_decoder_reset(p);
    p->b_get_video_sequence_header=1;
    //if( !p->b_thread_flag )
    if( p->param.i_thread_model ) /* reset multi-thread */
    {
        int i;

        p->thread[0] = p;
        if( cavs_pthread_mutex_init( &p->thread[0]->mutex, NULL ) )
            goto fail;
        if( cavs_pthread_cond_init( &p->thread[0]->cv, NULL ) )
        	goto fail;
        for( i = 1; i < p->param.i_thread_num; i++ )
        	CHECKED_MALLOC( p->thread[i], sizeof(cavs_decoder) );

        for( i = 1; i < p->param.i_thread_num; i++ )
        {
        	if( i > 0 )
        		*p->thread[i] = *p;
        	if( cavs_pthread_mutex_init( &p->thread[i]->mutex, NULL ) )
        		goto fail;
        	if( cavs_pthread_cond_init( &p->thread[i]->cv, NULL ) )
        		goto fail;

        	if(cavs_macroblock_cache_init( p->thread[i]))
                   goto fail;
        }
        p->b_thread_flag = 1;
    }
    p->b_skip_first = 0;

#if THREADS_OPT
     if( /*!p->b_accelerate_flag &&*/ p->param.b_accelerate ) /* reset accelerate */
    {
        p->unused[0] = p;

        if( cavs_pthread_mutex_init( &p->unused[0]->mutex, NULL ) )
        	goto fail;
        if( cavs_pthread_cond_init( &p->unused[0]->cv, NULL ) )
        	goto fail;

        CHECKED_MALLOC( p->unused[1], sizeof(cavs_decoder) );

        *p->unused[1] = *p;

        if( cavs_pthread_mutex_init( &p->unused[1]->mutex, NULL ) )
        	goto fail;
        if( cavs_pthread_cond_init( &p->unused[1]->cv, NULL ) )
        	goto fail;

        if(cavs_macroblock_cache_init( p->unused[1] ))
             goto fail;              

        p->b_accelerate_flag = 1;

        p->unused_backup[0] = p->unused[0];
        p->unused_backup[1] = p->unused[1];
    }
#endif
             
    return 0;
fail:
    printf("[error]realloc failed!\n");
    
    return -1;
    
}

void cavs_bitstream_save( frame_pack *frame, InputStream*p )
{
    int64_t min_length;
    
    frame->bits.p_ori = p->f;
    frame->bits.iBitsCount = p->iBitsCount;
    frame->bits.iBufBytesNum = p->iBufBytesNum;
    frame->bits.iBytePosition = p->iBytePosition;
    frame->bits.iClearBitsNum = p->iClearBitsNum;
    frame->bits.iStuffBitsNum = p->iStuffBitsNum;
    frame->bits.uClearBits = p->uClearBits;
    frame->bits.uPre3Bytes = p->uPre3Bytes;
    frame->bits.demulate_enable = p->demulate_enable;

    min_length = p->f_end - p->f;
    min_length = MIN2(SVA_STREAM_BUF_SIZE, min_length );
    frame->bits.min_length = min_length;
    memcpy(frame->bits.buf, p->buf,  /*min_length*/SVA_STREAM_BUF_SIZE );
}

void cavs_bitstream_restore( InputStream*p, frame_pack *frame )
{
    p->f = frame->bits.p_ori;
    p->iBitsCount = frame->bits.iBitsCount;
    p->iBufBytesNum = frame->bits.iBufBytesNum;
    p->iBytePosition = frame->bits.iBytePosition;
    p->iClearBitsNum = frame->bits.iClearBitsNum;
    p->iStuffBitsNum = frame->bits.iStuffBitsNum;
    p->uClearBits = frame->bits.uClearBits;
    p->uPre3Bytes = frame->bits.uPre3Bytes;
    p->demulate_enable = frame->bits.demulate_enable;    
    memcpy( p->buf, frame->bits.buf, /*frame->bits.min_length*/SVA_STREAM_BUF_SIZE );
}

/*
Input:
 1>bits stream;
 2>frame node;
Output:
 1>frame node with slice packed;
Call:
 when get slice startcode
*/
uint32_t cavs_frame_pack( InputStream*p, frame_pack *frame )
{
    //uint32_t i_startcode;

    int i_len;
    /*unsigned char *m_buf;

    m_buf = (unsigned char *)cavs_malloc(MAX_CODED_FRAME_SIZE*sizeof(unsigned char));
    if( m_buf == NULL )
    {
        return CAVS_ERROR;
    }
    memset(m_buf, 0, MAX_CODED_FRAME_SIZE*sizeof(unsigned char));*/

    /* pic header */
	uint32_t i_startcode = frame->slice[0].i_startcode;
	
    while(i_startcode)
    {  
        cavs_bitstream_save( frame, p );
		i_startcode = cavs_get_one_nal(p, frame->p_cur/*p->m_buf*/, &i_len, MAX_CODED_FRAME_SIZE- frame->i_len);

        switch( i_startcode )
        {	
            case XAVS_VIDEO_SEQUENCE_START_CODE:
                cavs_bitstream_restore( p, frame );
                //cavs_free(m_buf);
                return i_startcode;
            case XAVS_VIDEO_SEQUENCE_END_CODE:
                cavs_bitstream_restore( p, frame );
                //cavs_free(m_buf);
                return i_startcode;
            case XAVS_VIDEO_EDIT_CODE:
                cavs_bitstream_restore( p, frame );
                //cavs_free(m_buf);
                return i_startcode;
            case XAVS_I_PICUTRE_START_CODE:
                cavs_bitstream_restore( p, frame );
                //cavs_free(m_buf);
                return i_startcode;
            case XAVS_PB_PICUTRE_START_CODE:
                cavs_bitstream_restore( p, frame );
                //cavs_free(m_buf);
                return i_startcode;

            case XAVS_USER_DATA_CODE:
                break;
            case XAVS_EXTENSION_START_CODE:
                break;
            case 0x000001fe: // FIXIT
                cavs_bitstream_restore( p, frame );
                //cavs_free(m_buf);
                return i_startcode;
                break;
			case -1:
				frame->p_cur = frame->p_start;
				frame->i_len = 0;
				frame->slice_num = 0;
				break;
            default:
                //memcpy(frame->p_cur, p->m_buf, i_len ); /* pack slice into one frame */

                /* set slice info */
                frame->slice[frame->slice_num].i_startcode = i_startcode;
                frame->slice[frame->slice_num].p_start = frame->p_cur + 4; /* for skip current startcode */
                frame->slice[frame->slice_num].i_len = i_len-4;
        
                /*update frame info */
                frame->p_cur = frame->p_cur + i_len;
                frame->i_len += i_len;
                frame->slice_num++;
                break;
    	 }
    }

    //cavs_free(m_buf);
    
    return 0;
}

#if TEST_POOL
static void cavs_decoder_thread_init( cavs_decoder *p )
{
#if HAVE_MMX
    /* Misalign mask has to be set separately for each thread. */
    if( p->param.cpu & CAVS_CPU_SSE_MISALIGN )
        cavs_cpu_mask_misalign_sse();
#endif
}

#endif

int cavs_decoder_process(void *p_decoder, uint8_t *p_in, int i_in_length)
{
    cavs_decoder *p=(cavs_decoder *)p_decoder;
    uint32_t   i_startcode;
    uint8_t   *p_buf;
    int    i_len,i_result=0;

    p_buf = p_in;
    i_len = i_in_length;	

    i_startcode = ((p_buf[0]<<24))|((p_buf[1]<<16))|(p_buf[2]<<8)|p_buf[3];
    p_buf += 4; /* skip startcode */
    i_len -= 4;
    if(i_startcode==0)
    {
    	return CAVS_ERROR;
    }

    if(i_startcode!=XAVS_VIDEO_SEQUENCE_START_CODE&&!p->b_get_video_sequence_header)
    {
    	return  CAVS_ERROR;
    } 

    switch(i_startcode)
    {
    case XAVS_VIDEO_SEQUENCE_START_CODE:
    	cavs_bitstream_init(&p->s,p_buf,i_len);
    	if(cavs_get_video_sequence_header(&p->s,&p->vsh)==0)
    	{
    		if(cavs_alloc_resource(p)!=0)
    		{
    			return CAVS_ERROR;
    		}
    		i_result = CAVS_SEQ_HEADER;
    		p->b_get_video_sequence_header=1;

#if TEST_POOL
		if(!p->b_threadpool_flag)
		{
			if( p->param.i_thread_num >= 1 &&
				cavs_threadpool_init( &p->threadpool, p->param.i_thread_num, (void*)cavs_decoder_thread_init, p ) )
					goto fail;
			p->b_threadpool_flag = 1;
		}

#endif

            
    		if( !p->b_thread_flag &&  p->param.i_thread_num != 0 )
    		{
    			int i;

    			p->thread[0] = p;
				if( cavs_pthread_mutex_init( &p->thread[0]->mutex, NULL ) )
    					goto fail;
    			if( cavs_pthread_cond_init( &p->thread[0]->cv, NULL ) )
    					goto fail;

    			for( i = 1; i < p->param.i_thread_num; i++ )
    				CHECKED_MALLOC( p->thread[i], sizeof(cavs_decoder) );

    			for( i = 1; i < p->param.i_thread_num; i++ )
    			{
    				if( i > 0 )
    					*p->thread[i] = *p;
    				if( cavs_pthread_mutex_init( &p->thread[i]->mutex, NULL ) )
    					goto fail;
    				if( cavs_pthread_cond_init( &p->thread[i]->cv, NULL ) )
    					goto fail;

                    if(cavs_macroblock_cache_init( p->thread[i]))
                    	goto fail;    			
    			}
    			p->b_thread_flag = 1;
    		}

#if THREADS_OPT
             if( !p->b_accelerate_flag && p->param.b_accelerate )
            {
                p->unused[0] = p;
				/*  // xun: already be allocated at above threadpool loop
                if( cavs_pthread_mutex_init( &p->unused[0]->mutex, NULL ) )
                	goto fail;
                if( cavs_pthread_cond_init( &p->unused[0]->cv, NULL ) )
                	goto fail;
				*/
                CHECKED_MALLOC( p->unused[1], sizeof(cavs_decoder) );

                *p->unused[1] = *p;

                if( cavs_pthread_mutex_init( &p->unused[1]->mutex, NULL ) )
                	goto fail;
                if( cavs_pthread_cond_init( &p->unused[1]->cv, NULL ) )
                	goto fail;

                if(cavs_macroblock_cache_init( p->unused[1] ))
                    goto fail;

                p->b_accelerate_flag = 1;

                p->unused_backup[0] = p->unused[0];
                p->unused_backup[1] = p->unused[1];
            }
#endif
             
    	}
       else
       {
           return CAVS_ERROR;          
       }
    	break;
    case XAVS_VIDEO_SEQUENCE_END_CODE:
      if( !p->param.b_accelerate )
        p->last_delayed_pframe = p->p_save[1];

    	i_result = CAVS_SEQ_END;
    	break;
    case XAVS_USER_DATA_CODE:
    	cavs_bitstream_init(&p->s,p_buf,i_len);
    	i_result = cavs_get_user_data(&p->s, &p->user_data);
		i_result = CAVS_USER_DATA;
    	break;
    case XAVS_EXTENSION_START_CODE:
    	cavs_bitstream_init(&p->s,p_buf,i_len);
    	i_result = cavs_get_extension(&p->s, p);
		i_result = CAVS_USER_DATA;
    	break;
    case XAVS_VIDEO_EDIT_CODE:
    	p->i_video_edit_code_flag=0;
		i_result = CAVS_USER_DATA;
    	break;
    default:
	    i_result = CAVS_ERROR;
	    break;

    }

    return i_result;

fail:
    return CAVS_ERROR;//-1;
}

int cavs_macroblock_cache_init( cavs_decoder *p )
{
    uint32_t i_mb_width, i_mb_height, i_edge;
    //int b_interlaced = p->param.b_interlaced;

    i_mb_width = (p->vsh.i_horizontal_size+15)>>4;
    if ( p->vsh.b_progressive_sequence )
        i_mb_height = (p->vsh.i_vertical_size+15)>>4;
    else
        i_mb_height = ((p->vsh.i_vertical_size+31) & 0xffffffe0)>>4;

    p->i_mb_width = i_mb_width;
    p->i_mb_height = i_mb_height;
    p->i_mb_num = i_mb_width*i_mb_height;
    p->i_mb_num_half = p->i_mb_num>>1;
    p->p_top_qp = (uint8_t *)cavs_malloc( p->i_mb_width);

    p->p_top_mv[0] = (xavs_vector *)cavs_malloc((p->i_mb_width*2+1)*sizeof(xavs_vector));
    p->p_top_mv[1] = (xavs_vector *)cavs_malloc((p->i_mb_width*2+1)*sizeof(xavs_vector));
    p->p_top_intra_pred_mode_y = (int *)cavs_malloc( p->i_mb_width*4*sizeof(*p->p_top_intra_pred_mode_y));
    p->p_mb_type_top = (int8_t *)cavs_malloc(p->i_mb_width*sizeof(int8_t));
    p->p_chroma_pred_mode_top = (int8_t *)cavs_malloc(p->i_mb_width*sizeof(int8_t));
    p->p_cbp_top = (int8_t *)cavs_malloc(p->i_mb_width*sizeof(int8_t));
    p->p_ref_top = (int8_t (*)[2][2])cavs_malloc(p->i_mb_width*2*2*sizeof(int8_t));
    p->p_top_border_y = (uint8_t *)cavs_malloc((p->i_mb_width+1)*XAVS_MB_SIZE);
    p->p_top_border_cb = (uint8_t *)cavs_malloc((p->i_mb_width)*10);
    p->p_top_border_cr = (uint8_t *)cavs_malloc((p->i_mb_width)*10);

    p->p_col_mv = (xavs_vector *)cavs_malloc( p->i_mb_width*(p->i_mb_height)*4*sizeof(xavs_vector));
    p->p_col_type_base = (uint8_t *)cavs_malloc(p->i_mb_width*(p->i_mb_height));
    p->p_block = (DCTELEM *)cavs_malloc(64*sizeof(DCTELEM));

#if THREADS_OPT
    if( p->param.b_accelerate )
    {
        p->level_buf_tab = (int (*)[6][64])cavs_malloc(p->i_mb_num*6*64*sizeof(int));
        p->run_buf_tab = (int (*)[6][64])cavs_malloc(p->i_mb_num*6*64*sizeof(int));
        p->num_buf_tab = (int (*)[6])cavs_malloc(p->i_mb_num*6*sizeof(int));

        p->i_mb_type_tab = (int *)cavs_malloc(p->i_mb_num*sizeof(int));
        p->i_qp_tab = (int *)cavs_malloc(p->i_mb_num*sizeof(int));
        p->i_cbp_tab = (int *)cavs_malloc(p->i_mb_num*sizeof(int));
        p->mv_tab = (xavs_vector (*)[24])cavs_malloc( p->i_mb_num*24*sizeof(xavs_vector));

        p->i_intra_pred_mode_y_tab = (int (*)[25])cavs_malloc(p->i_mb_num*25*sizeof(int));
        p->i_pred_mode_chroma_tab = (int *)cavs_malloc(p->i_mb_num*sizeof(int));
        p->p_mvd_tab = (int16_t (*)[2][6][2])cavs_malloc(p->i_mb_num*2*6*2*sizeof(int16_t));
        p->p_ref_tab = (int8_t (*)[2][9])cavs_malloc(p->i_mb_num*2*9*sizeof(int8_t));
#if B_MB_WEIGHTING
        p->weighting_prediction_tab = (int *)cavs_malloc(p->i_mb_num*sizeof(int));
#endif		
	}
    
#endif
    

    i_edge = (p->i_mb_width*XAVS_MB_SIZE + XAVS_EDGE*2)*2*17*2*2;
    p->p_edge = (uint8_t *)cavs_malloc(i_edge);
    if( p->p_edge )
    {
        memset(p->p_edge, 0, i_edge);
    }
    p->p_edge += i_edge/2;

    if(   !p->p_top_qp                 ||!p->p_top_mv[0]          ||!p->p_top_mv[1]
        ||!p->p_top_intra_pred_mode_y  ||!p->p_top_border_y       ||!p->p_top_border_cb
        ||!p->p_top_border_cr          ||!p->p_col_mv             ||!p->p_col_type_base
        ||!p->p_block                  ||!p->p_edge				  ||!p->p_mb_type_top
        ||!p->p_chroma_pred_mode_top   ||!p->p_cbp_top			  ||!p->p_ref_top)
    {
        return -1;
    }

    memset(p->p_block,0,64*sizeof(DCTELEM));

    return 0;
}

static void cavs_slice_thread_sync_context( cavs_decoder *dst, cavs_decoder *src )
{
    memcpy( &dst->copy_flag1, &src->copy_flag1, offsetof(cavs_decoder, copy_flag2) - offsetof(cavs_decoder, copy_flag1) );
    memcpy( &dst->vsh, &src->vsh, offsetof(cavs_decoder, copy_flag3) - offsetof(cavs_decoder, s) );
}

#if THREADS_OPT
int cavs_decode_one_frame_delay( void *p_decoder, cavs_param *param )
{
    cavs_decoder *p = (cavs_decoder *)p_decoder;
    cavs_decoder *p_m = p;
    cavs_decoder *p_rec;
    int b_interlaced = p->param.b_interlaced;

    int i_result = 0;
    
    /* REC */
    /*init REC handle */
    p->current[0] = p->current[1];
    p_rec = p->current[0];
    
    p->i_frame_decoded++;
    if( p_rec != NULL )
    {
        if( !b_interlaced )
        {  
            cavs_init_picture_ref_list( p_rec );
            cavs_get_slice_rec( p_rec );

            /* this module will effect ref-list creature and update */
            if(p_rec->b_complete && p_rec->ph.i_picture_coding_type != XAVS_B_PICTURE)
            {
                xavs_image *p_image = p_rec->p_save[0];
                if(p_rec->vsh.b_low_delay)
                {
                    i_result = CAVS_FRAME_OUT;
                    //printf("prepare one frame yuv: I or P frame\n");

                    /* set for ffmpeg interface */
                    param->output_type = p_rec->p_save[0]->i_code_type;

                    /* when b_low_delay is 1, output current frame when finish decoded current */
                    cavs_out_image_yuv420(p, p_rec->p_save[0], param->p_out_yuv, 0);
                }
                if(!p_rec->p_save[1])
                {
                    p_rec->p_save[0] = &p_rec->image[1];
                }
                else
                {
                    if( p_rec->vsh.b_low_delay == 0 /*&& p_rec->ph.i_picture_coding_type != XAVS_I_PICTURE*/ )
                    {
                        i_result = CAVS_FRAME_OUT;

                        /* set for ffmpeg interface */
                        param->output_type = p_rec->p_save[1]->i_code_type;
                          
                        //printf("prepare one frame yuv : I or P frame\n");

                        /* when b_low_delay is 0, current frame should delay one frame to output */
                        cavs_out_image_yuv420(p, p_rec->p_save[1], param->p_out_yuv, 0);
                    }

                    if(!p_rec->p_save[2])
                    {
                        p_rec->p_save[0] = &p_rec->image[2];	 
                    }
                    else
                    {
                        p_rec->p_save[0] = p_rec->p_save[2];
                    }
                }
                p_rec->p_save[2] = p_rec->p_save[1];
                p_rec->p_save[1] = p_image;
                memset(&p_rec->cur, 0, sizeof(xavs_image));
                p_rec->b_complete = 0;

                /* copy ref-list to anther threads */
                p->current[1]->p_save[0] = p_rec->p_save[0];
                p->current[1]->p_save[1] = p_rec->p_save[1];
                p->current[1]->p_save[2] = p_rec->p_save[2];
            }
            else if(p_rec->b_complete)
            {
                i_result = CAVS_FRAME_OUT;

                /* set for ffmpeg interface */
                param->output_type = p_rec->p_save[0]->i_code_type;
                  
                //printf("prepare one frame yuv : B frame\n");

                /* ouput B-frame no delay */
                cavs_out_image_yuv420(p, p_rec->p_save[0], param->p_out_yuv, 0);
                memset(&p_rec->cur,0,sizeof(xavs_image));
                p_rec->b_complete=0;
            }

            /* NOTE : need this to output last frame */
            p->last_delayed_pframe = p_rec->p_save[1];
            p->p_save[1] = p_rec->p_save[1];
        }
        else /* field */
        {
            int field_count = 2;

            while(field_count--)
            {
                p_rec->b_bottom = !field_count; /* first-top second-botttom */
                cavs_init_picture_context_fld( p_rec );
                cavs_get_slice_rec_threads( p_rec );

                if(p_rec->b_complete && p_rec->ph.i_picture_coding_type != XAVS_B_PICTURE)
                {
                    xavs_image *p_image = p_rec->p_save[0];
                    if(p_rec->vsh.b_low_delay)
                    {
                        i_result = CAVS_FRAME_OUT;
                        
                        /* set for ffmpeg interface */
                        param->output_type = p_rec->p_save[0]->i_code_type;
                          
                        cavs_out_image_yuv420(p_m, p_rec->p_save[0], param->p_out_yuv, p_rec->b_bottom);
                    }
                    if(!p_rec->p_save[1])
                    {
                        if( p_rec->b_bottom )
                            p_rec->p_save[0] = &p_rec->image[1];
                    }
                    else
                    {
                        if(p_rec->vsh.b_low_delay == 0 /*&& p_rec->ph.i_picture_coding_type != XAVS_I_PICTURE*/ )
                        {
                            i_result = CAVS_FRAME_OUT;

                            /* set for ffmpeg interface */
                            param->output_type = p_rec->p_save[1]->i_code_type;
                              
                            cavs_out_image_yuv420(p_m, p_rec->p_save[1], param->p_out_yuv, p_rec->b_bottom);
                        }

                        if( p_rec->b_bottom )
                        {
                            if(!p_rec->p_save[2])
                            {
                                p_rec->p_save[0] = &p_rec->image[2];	 
                            }
                            else
                            {
                                p_rec->p_save[0] = p_rec->p_save[2];
                            }    
                        }
                    }

                    if( p_rec->b_bottom )
                    {
                        p_rec->p_save[2] = p_rec->p_save[1];
                        p_rec->p_save[1] = p_image;
                        memset(&p_rec->cur, 0, sizeof(xavs_image));

                        /* copy ref-list to anther threads */
                        p_m->current[1]->p_save[0] = p_rec->p_save[0];
                        p_m->current[1]->p_save[1] = p_rec->p_save[1];
                        p_m->current[1]->p_save[2] = p_rec->p_save[2];
                    }
                    p_rec->b_complete = 0;
                    p_rec->b_bottom = 0;
                }
                else if( p_rec->b_complete )
                {
                    i_result = CAVS_FRAME_OUT;
                    
                    /* set for ffmpeg interface */
                    param->output_type = p_rec->p_save[0]->i_code_type;
                      
                    cavs_out_image_yuv420(p_m, p_rec->p_save[0], param->p_out_yuv, p_rec->b_bottom);
                    memset(&p_rec->cur,0,sizeof(xavs_image));
                    p_rec->b_complete=0;
                    p_rec->b_bottom = 0;
                }	                         
            }

            /* NOTE : need this to output last frame */
            p->last_delayed_pframe = p_rec->p_save[1];
            p->p_save[1] = p_rec->p_save[1];
        }
    }
  
    return i_result;

}
#else
int cavs_decode_one_frame_delay( void *p_decoder, cavs_param *param )
{

}
#endif

int decode_one_frame(void *p_decoder, frame_pack *frame, cavs_param *param )
{

#if 1//!THREADS_OPT
    cavs_decoder *p=(cavs_decoder *)p_decoder;
#else
    cavs_decoder *p = (cavs_decoder *)p_decoder;
    cavs_decoder *p_m = p;/* backup main threads */
    cavs_decoder *p_aec, *p_rec;
#endif

    uint32_t  cur_startcode;
    uint8_t   *p_buf;
    int    i_len, i_result=0;
    int i, sliceNum, threadNum;
    int slice_count = 1; /* 0 is for pic header */

    sliceNum = frame->slice_num - 1; /* exclude pic header */
    threadNum = 0;

    cur_startcode = frame->slice[0].i_startcode;
    p_buf = frame->slice[0].p_start;
    i_len = frame->slice[0].i_len;
    frame->slice_num--;

#if 0//THREADS_OPT
    /* FIXIT : should init handle here */
    p_aec = p_m->unused[0];
    p = p_aec; /* switch to aec threads */
#endif

    /* decode header of pic */
    switch( cur_startcode )
    {
        case XAVS_I_PICUTRE_START_CODE:
            cavs_bitstream_init(&p->s, p_buf, i_len);
            cavs_get_i_picture_header(&p->s, &p->ph, &p->vsh);
			
			if( p->vsh.i_profile_id == PROFILE_GUANGDIAN )
			{
				//p->ph.b_aec_enable = 1; /* AEC only for AVS+ */
				p->ph.b_aec_enable = p->vsh.b_aec_enable;
			}
			else if(p->vsh.i_profile_id == PROFILE_JIZHUN)
			{
				p->ph.b_aec_enable = 0; /* AVS */
			}
			else
			{
				printf("[error]wrong profile id\n");
				p->b_error_flag  = 1;

                return CAVS_ERROR;
			}
            
#if 0//THREADS_OPT
            cavs_init_picture_context(p);
            cavs_init_picture_ref_list_for_aec(p);
#else
			p->ph.b_picture_structure = 1; /* NOTE : guarantee frame not change */
            cavs_init_picture(p);
#endif

#if 1//!THREADS_OPT
            /* add for no-seq header before I-frame */
            p->last_delayed_pframe = p->p_save[1];
#endif
            break;

        case XAVS_PB_PICUTRE_START_CODE:
            cavs_bitstream_init(&p->s, p_buf, i_len);
            cavs_get_pb_picture_header(&p->s, &p->ph, &p->vsh);

			if( p->vsh.i_profile_id == PROFILE_GUANGDIAN )
			{	
				//p->ph.b_aec_enable = 1; /* AEC only for AVS+ */
				p->ph.b_aec_enable = p->vsh.b_aec_enable;
			}
			else if(p->vsh.i_profile_id == PROFILE_JIZHUN)
			{
				p->ph.b_aec_enable = 0; /* AVS */
			}
			else
			{
				printf("[error]wrong profile id\n");
				p->b_error_flag  = 1;

                return CAVS_ERROR;
			}

#if 0//THREADS_OPT
            cavs_init_picture_context(p);
            cavs_init_picture_ref_list_for_aec(p);
#else    
			p->ph.b_picture_structure = 1; /* NOTE : guarantee frame not change */
            cavs_init_picture(p);
#endif            

            break;

        default:
            return CAVS_ERROR;
    }	
    
    /* weighting quant */
    cavs_init_frame_quant_param( p );

    cavs_frame_update_wq_matrix( p );

    /* decode slice */
    while( frame->slice_num--)
    {
        cur_startcode = frame->slice[slice_count].i_startcode;
        p_buf = frame->slice[slice_count].p_start;
        i_len = frame->slice[slice_count].i_len;
        slice_count++;

        switch( cur_startcode )
        {
            case XAVS_USER_DATA_CODE:
                cavs_bitstream_init(&p->s,p_buf,i_len);
                i_result = cavs_get_user_data(&p->s, &p->user_data);
                break;

            case XAVS_EXTENSION_START_CODE:
                cavs_bitstream_init(&p->s,p_buf,i_len);
                i_result = cavs_get_extension(&p->s, p);
                break;

            default:
#if 1//!THREADS_OPT
                if( !p->cur.p_data[0] ) /* store current decoded frame */
                {
                    break;
                }

                if( p->ph.i_picture_coding_type == XAVS_B_PICTURE )
                {
                    if( !p->p_save[1] || !p->p_save[2] ) /* need L0 and L1 reference frame */
                    {
                        break;
                    }
                }
                
                if( p->ph.i_picture_coding_type == XAVS_P_PICTURE )
                {
                    if( !p->p_save[1] ) /* need reference frame */
                    {
                        break;
                    }
                }
#endif

                if( cur_startcode >= XAVS_SLICE_MIN_START_CODE 
                    && cur_startcode <= XAVS_SLICE_MAX_START_CODE )
                {
                    if ( 0 == p->param.i_thread_model )
                    {
                        int i_next_mb_y;

#if 0//THREADS_OPT
                        cavs_bitstream_init(&p_aec->s, p_buf, i_len); /* init handle */                        
                        p_aec->i_mb_y = cur_startcode & 0x000000FF;                       
                        
                        if( frame->slice_num != 0 )
                            i_next_mb_y = frame->slice[slice_count].i_startcode & 0x000000FF; /*slice_count++ before*/
                        else
                            i_next_mb_y = 0xFF;
                        if (i_next_mb_y>=0 && i_next_mb_y<=0xAF)
                            p_aec->i_slice_mb_end = i_next_mb_y * p_aec->i_mb_width;
                        else
                            p_aec->i_slice_mb_end = p_aec->i_mb_num;
                        
                        /* AEC */
#if CREATE_THREADS
                        ret = cavs_pthread_create( &p_aec->id, NULL, (void*)cavs_get_slice_aec, p_aec );
                        if ( ret == 0 )
                        {
                            p_aec->b_thread_active = 1;
                        }
                        else
                        {
                            printf("creat multi-thread of AEC failed!\n");
                            return 0;
                        }
#else
                        cavs_get_slice_aec(p_aec);
#endif

                        /* creat independent ref-list for AEC module */
                        if( p_aec->ph.i_picture_coding_type != XAVS_B_PICTURE )
                        {
                            xavs_image *p_image = p_aec->p_save_aec[0];

                            if(!p_aec->p_save_aec[1])
                            {
                                p_aec->p_save_aec[0] = &p_aec->image_aec[1];
                            }
                            else
                            {
                                if(!p_aec->p_save_aec[2])
                                {
                                    p_aec->p_save_aec[0] = &p_aec->image_aec[2];	 
                                }
                                else
                                {
                                    p_aec->p_save_aec[0] = p_aec->p_save_aec[2];
                                }
                            }
                            p_aec->p_save_aec[2] = p_aec->p_save_aec[1];
                            p_aec->p_save_aec[1] = p_image;
                            //memset(&p_aec->cur_aec, 0, sizeof(xavs_image));
                        }

                        /* update AEC handle */
                        p = p_m; /* switch to main threads */
                        p->unused[1]->p_save_aec[0] = p_aec->p_save_aec[0];
                        p->unused[1]->p_save_aec[1] = p_aec->p_save_aec[1];
                        p->unused[1]->p_save_aec[2] = p_aec->p_save_aec[2];
                        p->unused[0] = p->unused[1];
                        p->unused[1] = p_aec;
#if !CREATE_THREADS
                        /* copy col type to next handle */
                        memcpy( p->unused[0]->p_col_mv, p->unused[1]->p_col_mv,p->i_mb_width*(p->i_mb_height)*4*sizeof(xavs_vector));
                        memcpy( p->unused[0]->p_col_type_base, p->unused[1]->p_col_type_base, p->i_mb_width*(p->i_mb_height));
#endif
                        p->current[0] = p->current[1];
                        p->current[1] = p_aec;

                        /* REC */
                        /*init REC handle */
                        p_rec = p->current[0];                        
                        if( p_rec != NULL )
                        {                        
                            cavs_get_slice_rec_threads( p_rec );

                            if( p_rec->ph.i_picture_coding_type != XAVS_B_PICTURE )
                                p->last_delayed_pframe = p_rec->p_save[1];
                            
#if DEBUG
                            switch(p_rec->ph.i_picture_coding_type)
                            {
                                case XAVS_I_PICTURE:
                                    printf("rec ============= I frame %d================\n", p->i_frame_decoded);
                                    break;
                                case XAVS_P_PICTURE:
                                    printf("rec ============= P frame %d================\n", p->i_frame_decoded);
                                    break;
                                case XAVS_B_PICTURE:
                                    printf("rec ============= B frame %d================\n", p->i_frame_decoded);
                                    break;
                            }
#endif

                            
                            /* this module will effect ref-list creature and update */
                            if(p_rec->b_complete && p_rec->ph.i_picture_coding_type != XAVS_B_PICTURE)
                            {
                                xavs_image *p_image = p_rec->p_save[0];
                                if(p_rec->vsh.b_low_delay)
                                {
                                    i_result = CAVS_FRAME_OUT;

                                    /* set for ffmpeg interface */
                                    param->output_type = p_rec->p_save[0]->i_code_type;
                                    //printf("prepare one frame yuv: I or P frame\n");

                                    /* when b_low_delay is 1, output current frame when finish decoded current */
                                    cavs_out_image_yuv420(p, p_rec->p_save[0], param->p_out_yuv, 0);
                                }
                                if(!p_rec->p_save[1])
                                {
                                    p_rec->p_save[0] = &p_rec->image[1];
                                }
                                else
                                {
                                    if( p_rec->vsh.b_low_delay == 0 /*&& p_rec->ph.i_picture_coding_type != XAVS_I_PICTURE*/ )
                                    {
                                        i_result = CAVS_FRAME_OUT;

                                        /* set for ffmpeg interface */
                                        param->output_type = p_rec->p_save[1]->i_code_type;
                                        
                                        //printf("prepare one frame yuv : I or P frame\n");

                                        /* when b_low_delay is 0, current frame should delay one frame to output */
                                        cavs_out_image_yuv420(p, p_rec->p_save[1], param->p_out_yuv, 0);
                                    }
                                    else
                                    {
                    	                param->output_type = -1;
                                    }

                                    if(!p_rec->p_save[2])
                                    {
                                        p_rec->p_save[0] = &p_rec->image[2];	 
                                    }
                                    else
                                    {
                                        p_rec->p_save[0] = p_rec->p_save[2];
                                    }
                                }
                                p_rec->p_save[2] = p_rec->p_save[1];
                                p_rec->p_save[1] = p_image;
                                memset(&p_rec->cur, 0, sizeof(xavs_image));
                                p_rec->b_complete = 0;

                                /* copy ref-list to anther threads */
                                p->current[1]->p_save[0] = p_rec->p_save[0];
                                p->current[1]->p_save[1] = p_rec->p_save[1];
                                p->current[1]->p_save[2] = p_rec->p_save[2];
                            }
                            else if(p_rec->b_complete)
                            {
                                i_result = CAVS_FRAME_OUT;

                                /* set for ffmpeg interface */
                                param->output_type = p_rec->p_save[0]->i_code_type;
                                
                                //printf("prepare one frame yuv : B frame\n");

                                /* ouput B-frame no delay */
                                cavs_out_image_yuv420(p, p_rec->p_save[0], param->p_out_yuv, 0);
                                memset(&p_rec->cur,0,sizeof(xavs_image));
                                p_rec->b_complete=0;
                            }
                        }

                        /* waiting for end of AEC threads */
#if CREATE_THREADS
                        if( p_aec->b_thread_active == 1 )
                        {   
                            cavs_pthread_join(p_aec->id, NULL);
                            p_aec->b_thread_active = 0;
                        }
                        
                        /* copy col type to next handle */
                        memcpy( p->unused[0]->p_col_mv, p->unused[1]->p_col_mv,p->i_mb_width*(p->i_mb_height)*4*sizeof(xavs_vector));
                        memcpy( p->unused[0]->p_col_type_base, p->unused[1]->p_col_type_base, p->i_mb_width*(p->i_mb_height));
#endif
                                      
#else /* old version */                                           
                        cavs_bitstream_init(&p->s, p_buf, i_len);                        
                        p->i_mb_y = cur_startcode & 0x000000FF;                       
                        
                        if( frame->slice_num != 0 )
                            i_next_mb_y = frame->slice[slice_count].i_startcode & 0x000000FF; /*slice_count++ before*/
                        else
                            i_next_mb_y = 0xFF;
                        if (i_next_mb_y>=0 && i_next_mb_y<=0xAF)
                            p->i_slice_mb_end = i_next_mb_y * p->i_mb_width;
                        else
                            p->i_slice_mb_end = p->i_mb_num;
                        
                        if( cavs_get_slice(p) != 0 )
                        {
                            printf("[error] decoding slice is wrong\n");
                            p->b_complete = 1; /* set for pic exchange */
                            //return CAVS_ERROR;
                        }

                        /* this module will effect ref-list creature and update */
                        if(p->b_complete && p->ph.i_picture_coding_type != XAVS_B_PICTURE)
                        {
                            xavs_image *p_image = p->p_save[0];
                            if(p->vsh.b_low_delay)
                            {
                                i_result = CAVS_FRAME_OUT;

                                /* set for ffmpeg interface */
                                param->output_type = p->p_save[0]->i_code_type;
                                  
                                //printf("prepare one frame yuv: I or P frame\n");
                                

                                /* when b_low_delay is 1, output current frame when finish decoded current */
                                cavs_out_image_yuv420(p, p->p_save[0], param->p_out_yuv, 0);
                            }
                            if(!p->p_save[1])
                            {
                                p->p_save[0] = &p->image[1];
                            }
                            else
                            {
                                if( p->vsh.b_low_delay == 0 /*&& p->ph.i_picture_coding_type != XAVS_I_PICTURE*/ )
                                {
                                    i_result = CAVS_FRAME_OUT;

                                    /* set for ffmpeg interface */
                                    param->output_type = p->p_save[1]->i_code_type;
                                    
                                    //printf("prepare one frame yuv : I or P frame\n");

                                    /* when b_low_delay is 0, current frame should delay one frame to output */
                                    cavs_out_image_yuv420(p, p->p_save[1], param->p_out_yuv, 0);
                                }

                                if(!p->p_save[2])
                                {
                                    p->p_save[0] = &p->image[2];	 
                                }
                                else
                                {
                                    p->p_save[0] = p->p_save[2];
                                }
                            }
                            p->p_save[2] = p->p_save[1];
                            p->p_save[1] = p_image;
                            memset(&p->cur, 0, sizeof(xavs_image));
                            p->b_complete = 0;
                        }
                        else if(p->b_complete)
                        {
                            i_result = CAVS_FRAME_OUT;

                            /* set for ffmpeg interface */
                            param->output_type = p->p_save[0]->i_code_type;
                              
                            //printf("prepare one frame yuv : B frame\n");

                            /* ouput B-frame no delay */
                            cavs_out_image_yuv420(p, p->p_save[0], param->p_out_yuv, 0);
                            memset(&p->cur,0,sizeof(xavs_image));
                            p->b_complete=0;
                        }	
#endif

                       
                    } 
                    else if( 1 == p->param.i_thread_model )
                    {
                        if ( XAVS_THREAD_MAX >= sliceNum )
                        {
                            int i_next_mb_y;

                            /* sync context of handle */
                            cavs_slice_thread_sync_context( p->thread[threadNum], p );
                            if( threadNum !=0 ) /* NOTE : need to init new handle */
                            {
								p->thread[threadNum]->ph.b_picture_structure = 1; /* NOTE : guarantee frame not change */
                            	cavs_init_picture(p->thread[threadNum]);
                            }
                            cavs_bitstream_init(&p->thread[threadNum]->s, p_buf, i_len);
                            p->thread[threadNum]->i_mb_y = cur_startcode & 0x000000FF;

                            if( frame->slice_num != 0 )
                                i_next_mb_y = frame->slice[slice_count].i_startcode & 0x000000FF; /*slice_count++ before*/
                            else
                                i_next_mb_y = 0xFF;
                            if (i_next_mb_y>=0 && i_next_mb_y<=0xAF)
                                p->thread[threadNum]->i_slice_mb_end = i_next_mb_y * p->thread[threadNum]->i_mb_width;
                            else
                                p->thread[threadNum]->i_slice_mb_end = p->thread[threadNum]->i_mb_num;

#if TEST_POOL
                            cavs_threadpool_run( p->threadpool, (void*)cavs_get_slice, p->thread[threadNum] );
                            p->thread[threadNum]->b_thread_active = 1;
#else
                            int ret = cavs_pthread_create( &p->thread[threadNum]->id, NULL, (void*)cavs_get_slice, p->thread[threadNum] );
                            if ( ret == 0 )
                            {
                                p->thread[threadNum]->b_thread_active = 1;
                            }
                            else
                            {
                                printf("creat multi-thread failed!\n");
                                return 0;
                            }
#endif
                            threadNum++;
                        }
                        else
                        {
                            printf("[error]threads exceed limit\n");

                            return CAVS_ERROR;
                        }
                                    
                        if ( threadNum == sliceNum )
                        {
                            for ( i = 0; i < threadNum; i++ )
                            {
                                if( p->thread[i]->b_thread_active )
                                {  
#if TEST_POOL
                                    if(cavs_threadpool_wait( p->threadpool, p->thread[i]))
                		                ;//return -1;
#else                                
                                    cavs_pthread_join(p->thread[i]->id, NULL);
#endif                                    
                                    p->thread[i]->b_thread_active = 0;
                                    if( p->thread[i]->b_error_flag )
                                    {
                                        p->b_error_flag = 1;
                                        p->thread[threadNum - 1]->b_complete = 1;
                                        printf("[error] decoding slice is wrong\n");
                                        
                                        //return CAVS_ERROR;
                                    }
                                }
                            }

                            if( p->thread[threadNum - 1]->b_complete && p->ph.i_picture_coding_type != XAVS_B_PICTURE )
                            {
                                xavs_image *p_image = p->p_save[0];
                                
                                if( p->vsh.b_low_delay ) /* */
                                {
                                    i_result = CAVS_FRAME_OUT;

                                    /* set for ffmpeg interface */
                                    param->output_type = p->p_save[0]->i_code_type;
                                      
                                    cavs_out_image_yuv420( p, p->p_save[0], param->p_out_yuv, 0 );
                                }
                                
                                if( !p->p_save[1] )
                                {
                                    p->p_save[0] = &p->image[1];
                                }
                                else
                                {
                                    if( p->vsh.b_low_delay == 0 /*&& p->ph.i_picture_coding_type != XAVS_I_PICTURE*/ )
                                    {
                                        i_result = CAVS_FRAME_OUT;

                                        /* set for ffmpeg interface */
                                        param->output_type = p->p_save[1]->i_code_type;
                                     
                                            
                                        cavs_out_image_yuv420( p, p->p_save[1], param->p_out_yuv, 0 );
                                    }
                                    if( !p->p_save[2] )
                                    {
                                        p->p_save[0] = &p->image[2];	 
                                    }
                                    else
                                    {
                                        p->p_save[0] = p->p_save[2];
                                    }
                                }
                                p->p_save[2] = p->p_save[1];
                                p->p_save[1] = p_image; /* p->p_save[0] to p->p_save[1] */
                                memset( &p->cur, 0, sizeof(xavs_image) );
                                p->thread[threadNum - 1]->b_complete = 0;
                            }
                            else /* output B frame of reconstructed */
                            {
                                i_result = CAVS_FRAME_OUT;

                                /* set for ffmpeg interface */
                                param->output_type = p->p_save[0]->i_code_type;
                                  
                                cavs_out_image_yuv420( p, p->p_save[0], param->p_out_yuv, 0);
                                memset( &p->cur, 0, sizeof(xavs_image) );                              
                                p->thread[threadNum - 1]->b_complete = 0;
                            }	
                        }
                    }				
                }
                else
                {
                    printf("[error] startcode is wrong\n");
                    
                    return CAVS_ERROR;
                }
        }
    }

    if( p->b_error_flag )
    {
    	i_result = CAVS_ERROR;
    }

    return i_result;
}

#if FAR_ACCELERATE

int frame_decode_aec_threads_far( cavs_decoder *p_aec )
{
    cavs_decoder *p_m = p_aec->p_m;
    frame_pack *frame = p_aec->fld;
    uint8_t   *p_buf;
    int    i_len, i_result = 0;
    int slice_count = p_aec->slice_count;
    //int field_count = p_aec->field_count;
    uint32_t cur_startcode;
    uint32_t i_ret = 0;

    /* pack  */
    i_ret = cavs_frame_pack( p_aec->p_stream, frame );
    if( i_ret == CAVS_ERROR )
    {
        p_aec->b_error_flag = 1;
        return CAVS_ERROR;
    }
    
    cur_startcode = frame->slice[0].i_startcode;
    p_buf = frame->slice[0].p_start;
    i_len = frame->slice[0].i_len;
    frame->slice_num--;
    
    /* decode header of pic */
    switch( cur_startcode )
    {
        case XAVS_I_PICUTRE_START_CODE:
            cavs_bitstream_init(&p_aec->s, p_buf, i_len);
            cavs_get_i_picture_header(&p_aec->s, &p_aec->ph, &p_aec->vsh);
            
			if( p_aec->vsh.i_profile_id == PROFILE_GUANGDIAN )
			{	
				//p_aec->ph.b_aec_enable = 1; /* AEC only for AVS+ */
				p_aec->ph.b_aec_enable = p_m->vsh.b_aec_enable;
			}
			else if(p_aec->vsh.i_profile_id == PROFILE_JIZHUN)
			{
				p_aec->ph.b_aec_enable = 0; /* AVS */
			}
			else
			{
				printf("[error]wrong profile id\n");
				p_aec->b_error_flag  = 1;

                return CAVS_ERROR;
			}
			p_aec->ph.b_picture_structure = 1; /* NOTE : guarantee frame not change */

            i_ret = cavs_init_picture_context(p_aec);
            if(i_ret == CAVS_ERROR)
            {
                p_aec->b_error_flag  = 1;
                return CAVS_ERROR;
            }
            cavs_init_picture_ref_list_for_aec(p_aec);

            break;

        case XAVS_PB_PICUTRE_START_CODE:
            cavs_bitstream_init(&p_aec->s, p_buf, i_len);
            cavs_get_pb_picture_header(&p_aec->s, &p_aec->ph, &p_aec->vsh);

			if( p_aec->vsh.i_profile_id == PROFILE_GUANGDIAN )
			{	
				//p_aec->ph.b_aec_enable = 1; /* AEC only for AVS+ */
				p_aec->ph.b_aec_enable = p_m->vsh.b_aec_enable;
			}
			else if(p_aec->vsh.i_profile_id == PROFILE_JIZHUN)
			{
				p_aec->ph.b_aec_enable = 0; /* AVS */
			}
			else
			{
				printf("[error]wrong profile id\n");
				p_aec->b_error_flag  = 1;

                return CAVS_ERROR;
			}
			p_aec->ph.b_picture_structure = 1; /* NOTE : guarantee frame not change */

            i_ret = cavs_init_picture_context(p_aec);
            if( i_ret == CAVS_ERROR )
            {
                p_aec->b_error_flag  = 1;
                return CAVS_ERROR;
            }
            cavs_init_picture_ref_list_for_aec(p_aec);
         
            break;

        default:
            return CAVS_ERROR;
    }	
    
    /* weighting quant */
    cavs_init_frame_quant_param( p_aec );

    cavs_frame_update_wq_matrix( p_aec );
    
    while( frame->slice_num--)
    {
        cur_startcode = frame->slice[slice_count].i_startcode;
        p_buf = frame->slice[slice_count].p_start;
        i_len = frame->slice[slice_count].i_len;
        slice_count++;

        switch( cur_startcode )
        {
            case XAVS_USER_DATA_CODE:
                cavs_bitstream_init(&p_aec->s,p_buf,i_len);
                i_result = cavs_get_user_data(&p_aec->s, &p_aec->user_data);
                break;

            case XAVS_EXTENSION_START_CODE:
                cavs_bitstream_init(&p_aec->s,p_buf,i_len);
                i_result = cavs_get_extension(&p_aec->s, p_aec);
                break;

            default:

                if( cur_startcode >= XAVS_SLICE_MIN_START_CODE 
                    && cur_startcode <= XAVS_SLICE_MAX_START_CODE )
                {
                    if ( 0 == p_aec->param.i_thread_model )
                    {
                        int i_next_mb_y;

                        cavs_bitstream_init(&p_aec->s, p_buf, i_len); /* init handle */                        
                        p_aec->i_mb_y = cur_startcode & 0x000000FF;                       
                        
                        if( frame->slice_num != 0 )
                            i_next_mb_y = frame->slice[slice_count].i_startcode & 0x000000FF; /*slice_count++ before*/
                        else
                            i_next_mb_y = 0xFF;
                        if (i_next_mb_y>=0 && i_next_mb_y<=0xAF)
                            p_aec->i_slice_mb_end = i_next_mb_y * p_aec->i_mb_width;
                        else
                            p_aec->i_slice_mb_end = p_aec->i_mb_num;
                        //p_aec->b_bottom = 0;
                        //p_aec->i_slice_mb_end_fld[p_aec->b_bottom] =  p_aec->i_slice_mb_end;
                        
                        /* AEC */
                        cavs_get_slice_aec(p_aec);
                        if(p_aec->b_error_flag == 1)
                            return  CAVS_ERROR;

                        /* creat independent ref-list for AEC module */
                        if( p_aec->ph.i_picture_coding_type != XAVS_B_PICTURE )
                        {
                            xavs_image *p_image = p_aec->p_save_aec[0];

                            if(!p_aec->p_save_aec[1])
                            {
                                p_aec->p_save_aec[0] = &p_aec->image_aec[1];
                            }
                            else
                            {
                                if(!p_aec->p_save_aec[2])
                                {
                                    p_aec->p_save_aec[0] = &p_aec->image_aec[2];	 
                                }
                                else
                                {
                                    p_aec->p_save_aec[0] = p_aec->p_save_aec[2];
                                }
                            }
                            p_aec->p_save_aec[2] = p_aec->p_save_aec[1];
                            p_aec->p_save_aec[1] = p_image;
                            memset(&p_aec->cur_aec, 0, sizeof(xavs_image));
                        }

#if !CREATE_THREADS
                            p_m->unused[1]->p_save_aec[0] = p_aec->p_save_aec[0];/* should proc like field */
                            p_m->unused[1]->p_save_aec[1] = p_aec->p_save_aec[1];
                            p_m->unused[1]->p_save_aec[2] = p_aec->p_save_aec[2];
                            p_m->unused[0] = p_m->unused[1];
                            p_m->unused[1] = p_aec;
    
                            /* copy col type to next handle */
                            memcpy( p_m->unused[0]->p_col_mv, p_aec->p_col_mv,p_m->i_mb_width*(p_m->i_mb_height)*4*sizeof(xavs_vector));
                            memcpy( p_m->unused[0]->p_col_type_base, p_aec->p_col_type_base, p_m->i_mb_width*(p_m->i_mb_height));
                            p_m->current[0] = p_m->current[1];
                            p_m->current[1] = p_aec;
#endif

                    }                     				
                }
                else
                {
                    printf("[error] startcode is wrong\n");
                    p_aec->b_error_flag = 1;

                    return CAVS_ERROR;
                }
        }
    }

	return 0;
}

int decode_one_frame_far(void *p_decoder, frame_pack *frame, cavs_param *param )
{
    cavs_decoder *p = (cavs_decoder *)p_decoder;
    cavs_decoder *p_m = p;/* backup main threads */
    cavs_decoder *p_aec, *p_rec;

    int i_result=0;
    int slice_count = 1; /* 0 is for pic header */

    /* FIXIT : should init handle here */
    p_aec = p_m->unused[0];
    p = p_aec; /* switch to aec threads */

    /* decode slice */
    p_aec->p_m = p_m;
    p_aec->fld = frame;
    p_aec->slice_count = slice_count;
    //p_aec->field_count = field_count;
    
#if !CREATE_THREADS
    frame_decode_aec_threads_far( p_aec );
#else

#if TEST_POOL
    cavs_threadpool_run( p_m->threadpool, (void*)frame_decode_aec_threads_far, p_aec );
    p_aec->b_thread_active = 1;
#else
    int ret = cavs_pthread_create( &p_aec->id, NULL, (void*)frame_decode_aec_threads_far, p_aec );
    if ( ret == 0 )
    {
        p_aec->b_thread_active = 1;
    }
    else
    {
        printf("creat field multi-thread of AEC failed!\n");
        return 0;
    }
#endif
    
#endif

#if CREATE_THREADS
    /* handle switch */
    //p = p_m; /* switch to main threads */
    //p->unused[1]->p_save_aec[0] = p_aec->p_save_aec[0];/* should proc like field */
    //p->unused[1]->p_save_aec[1] = p_aec->p_save_aec[1];
    //p->unused[1]->p_save_aec[2] = p_aec->p_save_aec[2];
    p_m->unused[0] = p_m->unused[1];
    p_m->unused[1] = p_aec;

    p_m->current[0] = p_m->current[1];
    p_m->current[1] = p_aec;
#endif

    /* REC */
    /*init REC handle */
    p_rec = p_m->current[0];                        
    if( p_rec != NULL )
    {                 
        cavs_init_picture_context_frame(p_rec);
        i_result = cavs_get_slice_rec_threads( p_rec );
        if( i_result == -1 )
        {
            i_result = CAVS_ERROR;
            //printf("[cavs error] rec handle is wrong\n");

            /* close AEC at same time */
            if( p_aec->b_thread_active == 1 )
            {
#if TEST_POOL
                if(cavs_threadpool_wait( p_m->threadpool, p_aec))
                    ;//return -1;
#else            
                cavs_pthread_join(p_aec->id, NULL);
#endif                
                p_aec->b_thread_active = 0;
            }
            return i_result;
        }

        if( p_rec->ph.i_picture_coding_type != XAVS_B_PICTURE )
            p_m->last_delayed_pframe = p_rec->p_save[1];
       
        /* this module will effect ref-list creature and update */
        if(p_rec->b_complete && p_rec->ph.i_picture_coding_type != XAVS_B_PICTURE)
        {
            xavs_image *p_image = p_rec->p_save[0];
            if(p_rec->vsh.b_low_delay)
            {
                i_result = CAVS_FRAME_OUT;

                /* set for ffmpeg interface */
                param->output_type = p_rec->p_save[0]->i_code_type;
                //printf("prepare one frame yuv: I or P frame\n");

                /* when b_low_delay is 1, output current frame when finish decoded current */
                cavs_out_image_yuv420(p_m, p_rec->p_save[0], param->p_out_yuv, 0);
            }
            if(!p_rec->p_save[1])
            {
                p_rec->p_save[0] = &p_rec->image[1];
            }
            else
            {
                if( p_rec->vsh.b_low_delay == 0 /*&& p_rec->ph.i_picture_coding_type != XAVS_I_PICTURE*/ )
                {
                    i_result = CAVS_FRAME_OUT;

                    /* set for ffmpeg interface */
                    param->output_type = p_rec->p_save[1]->i_code_type;
                    
                    //printf("prepare one frame yuv : I or P frame\n");

                    /* when b_low_delay is 0, current frame should delay one frame to output */
                    cavs_out_image_yuv420(p_m, p_rec->p_save[1], param->p_out_yuv, 0);
                }
                else
                {
                	param->output_type = -1;
                }

                if(!p_rec->p_save[2])
                {
                    p_rec->p_save[0] = &p_rec->image[2];	 
                }
                else
                {
                    p_rec->p_save[0] = p_rec->p_save[2];
                }
            }
            p_rec->p_save[2] = p_rec->p_save[1];
            p_rec->p_save[1] = p_image;
            memset(&p_rec->cur, 0, sizeof(xavs_image));
            p_rec->b_complete = 0;

            /* copy ref-list to anther threads */
            p_m->current[1]->p_save[0] = p_rec->p_save[0];
            p_m->current[1]->p_save[1] = p_rec->p_save[1];
            p_m->current[1]->p_save[2] = p_rec->p_save[2];
        }
        else if(p_rec->b_complete)
        {
            i_result = CAVS_FRAME_OUT;

            /* set for ffmpeg interface */
            param->output_type = p_rec->p_save[0]->i_code_type;
            
            //printf("prepare one frame yuv : B frame\n");

            /* ouput B-frame no delay */
            cavs_out_image_yuv420(p_m, p_rec->p_save[0], param->p_out_yuv, 0);
            memset(&p_rec->cur,0,sizeof(xavs_image));
            p_rec->b_complete=0;
        }
    }

#if CREATE_THREADS
    /* waiting for end of AEC threads */
    if( p_aec->b_thread_active == 1 )
    {  
#if TEST_POOL
       if(cavs_threadpool_wait( p_m->threadpool, p_aec))
           ;//return -1;
#else    
        cavs_pthread_join(p_aec->id, NULL);
#endif        
        p_aec->b_thread_active = 0;
    }
    
    if( p_aec->b_error_flag )
    {
        //printf("[cavs error] AEC handle is wrong\n");
        return CAVS_ERROR;
    }

    p_m->unused[0]->p_save_aec[0] = p_aec->p_save_aec[0];/* should proc like field */
    p_m->unused[0]->p_save_aec[1] = p_aec->p_save_aec[1];
    p_m->unused[0]->p_save_aec[2] = p_aec->p_save_aec[2];
    
    /* copy col type to next handle */
    memcpy( p_m->unused[0]->p_col_mv, p_aec->p_col_mv,p_m->i_mb_width*(p_m->i_mb_height)*4*sizeof(xavs_vector));
    memcpy( p_m->unused[0]->p_col_type_base, p_aec->p_col_type_base, p_m->i_mb_width*(p_m->i_mb_height));
#endif

    if( p->b_error_flag )
    {
    	i_result = CAVS_ERROR;
    }

    return i_result;
}

#endif

/* cavs_topfield_pack */
uint32_t cavs_topfield_pack( InputStream*p, frame_pack *field )
{
    //uint32_t i_startcode;
    int i_len;
    /*unsigned char *m_buf;

    m_buf = (unsigned char *)cavs_malloc((MAX_CODED_FRAME_SIZE/2)*sizeof(unsigned char));
    memset(m_buf, 0, (MAX_CODED_FRAME_SIZE/2)*sizeof(unsigned char));*/

	uint32_t i_startcode = field->slice[0].i_startcode; /* pic header */

    while( i_startcode )
    {  
        cavs_bitstream_save( field, p );
		i_startcode = cavs_get_one_nal(p, field->p_cur/*p->m_buf*/, &i_len, (MAX_CODED_FRAME_SIZE / 2) - field->i_len);

        if( ((i_startcode & 0xff) > (uint32_t)field->i_mb_end[0])
			&& i_startcode != XAVS_USER_DATA_CODE
			&& i_startcode != XAVS_EXTENSION_START_CODE ) /* next slice start mb address exceed field limit */
        {
            cavs_bitstream_restore( p, field );
            //cavs_free(m_buf);
			 
            return i_startcode;
        }

        switch( i_startcode )
        {
#if 0        
            case XAVS_VIDEO_SEQUENCE_START_CODE:
                cavs_bitstream_restore( p, field );
                return i_startcode;
            case XAVS_VIDEO_SEQUENCE_END_CODE:
                cavs_bitstream_restore( p, field );
                return i_startcode;
            case XAVS_VIDEO_EDIT_CODE:
                cavs_bitstream_restore( p, field );
                return i_startcode;
            case XAVS_I_PICUTRE_START_CODE:
                cavs_bitstream_restore( p, field );
                return i_startcode;
            case XAVS_PB_PICUTRE_START_CODE:
                cavs_bitstream_restore( p, field );
                return i_startcode;
#endif
            case XAVS_USER_DATA_CODE:
                break;
            case XAVS_EXTENSION_START_CODE:
                break;
			case -1:
				field->p_cur = field->p_start;
				field->i_len = 0;
				field->slice_num = 0;
				break;
            default:
                //memcpy(field->p_cur, p->m_buf, i_len ); /* pack slice into one frame */

                /* set slice info */
                field->slice[field->slice_num].i_startcode = i_startcode;
                field->slice[field->slice_num].p_start = field->p_cur + 4; /* for skip current startcode */
                field->slice[field->slice_num].i_len = i_len-4;
        
                /*update frame info */
                field->p_cur = field->p_cur + i_len;
                field->i_len += i_len;
                field->slice_num++;               
                break;
    	 }
    }

    return 0;
}

/* decode_top_field */
int decode_top_field(void *p_decoder, frame_pack *field, cavs_param *param )
{
    cavs_decoder *p=(cavs_decoder *)p_decoder;
    uint32_t  cur_startcode;
    uint8_t   *p_buf;
    int    i_len, i_result=0;
    int i, sliceNum, threadNum;
    int ret = 0;
    int slice_count = 1; /* 0 is for pic header */
    //int b_interlaced = p->param.b_interlaced;
    
    p->b_bottom = field->b_bottom;
    sliceNum = field->slice_num - 1; /* exclude pic header */
    threadNum = 0;

    cur_startcode = field->slice[0].i_startcode;
    p_buf = field->slice[0].p_start;
    i_len = field->slice[0].i_len;
    field->slice_num--;

    /* decode header of pic */
    switch( cur_startcode )
    {
        case XAVS_I_PICUTRE_START_CODE:
            cavs_bitstream_init(&p->s, p_buf, i_len);
            cavs_get_i_picture_header(&p->s,&p->ph,&p->vsh);
			if( p->vsh.i_profile_id == PROFILE_GUANGDIAN )
			{	
				//p->ph.b_aec_enable = 1; /* AEC only for AVS+ */
				p->ph.b_aec_enable = p->vsh.b_aec_enable;
			}
			else if(p->vsh.i_profile_id == PROFILE_JIZHUN)
			{
				p->ph.b_aec_enable = 0; /* AVS */
			}
			else
			{
				printf("[error]wrong profile id\n");
				p->b_error_flag  = 1;

                return CAVS_ERROR;
			}
			p->ph.b_picture_structure = 0; /* NOTE : guarantee field not change */
            cavs_init_picture(p);

            /* add for no-seq header before I-frame */
            p->last_delayed_pframe = p->p_save[1];
            break;

        case XAVS_PB_PICUTRE_START_CODE:
            cavs_bitstream_init(&p->s, p_buf, i_len);
            cavs_get_pb_picture_header(&p->s,&p->ph,&p->vsh);
			if( p->vsh.i_profile_id == PROFILE_GUANGDIAN )
			{	
				//p->ph.b_aec_enable = 1; /* AEC only for AVS+ */
				p->ph.b_aec_enable = p->vsh.b_aec_enable;
			}
			else if(p->vsh.i_profile_id == PROFILE_JIZHUN)
			{
				p->ph.b_aec_enable = 0; /* AVS */
			}
			else
			{
				printf("[error]wrong profile id\n");
				p->b_error_flag  = 1;

                return CAVS_ERROR;
			}
			p->ph.b_picture_structure = 0; /* NOTE : guarantee field not change */
            cavs_init_picture(p);
            break;

        default:
            return CAVS_ERROR;
    }	
    
    /*add for weighting quant */
    cavs_init_frame_quant_param( p );

    cavs_frame_update_wq_matrix( p );

    /* decode slice */
    while( field->slice_num--)
    {
        cur_startcode = field->slice[slice_count].i_startcode;
        p_buf = field->slice[slice_count].p_start;
        i_len = field->slice[slice_count].i_len;
        slice_count++;

        switch( cur_startcode )
        {
            case XAVS_USER_DATA_CODE:
                cavs_bitstream_init(&p->s,p_buf,i_len);
                i_result = cavs_get_user_data(&p->s, &p->user_data);
                break;

            case XAVS_EXTENSION_START_CODE:
                cavs_bitstream_init(&p->s,p_buf,i_len);
                i_result = cavs_get_extension(&p->s, p);
                break;

            default:
                if( !p->cur.p_data[0] ) /* store current decoded frame */
                {
                    break;
                }

                if( p->ph.i_picture_coding_type == XAVS_B_PICTURE )
                {
                    if( !p->p_save[1] || !p->p_save[2] ) /* need L0 and L1 reference frame */
                    {
                        break;
                    }
                }
                
                if( p->ph.i_picture_coding_type == XAVS_P_PICTURE )
                {
                    if( !p->p_save[1] ) /* need reference frame */
                    {
                        break;
                    }
                }

                if( cur_startcode >= XAVS_SLICE_MIN_START_CODE 
                    && cur_startcode <= XAVS_SLICE_MAX_START_CODE )
                {
                    if ( 0 == p->param.i_thread_model )
                    {
                        int i_next_mb_y;

                        p->b_error_flag = 0;
                        cavs_bitstream_init(&p->s, p_buf, i_len);                        
                        p->i_mb_y = cur_startcode & 0x000000FF;                       
                        
                        if( field->slice_num != 0 )
                            i_next_mb_y = field->slice[slice_count].i_startcode & 0x000000FF; /*slice_count++ before*/
                        else
                            i_next_mb_y = 0xFF;

                        if((uint32_t)i_next_mb_y > (p->i_mb_height>>1)
                            && field->slice_num != 0 )
                        {
                            p->b_error_flag = 1;
                        }
                        
                        if (i_next_mb_y>=0 && i_next_mb_y<=0xAF)
                            p->i_slice_mb_end = i_next_mb_y * p->i_mb_width;
                        else
                        {
                            p->i_slice_mb_end = (p->i_mb_num>>1);
                        }

                        if( ! p->b_error_flag )
                        {
                            if( cavs_get_slice(p) != 0 )
                            {
                                printf("[error] decoding slice is wrong\n");
                                p->b_complete = 1; /* set for pic exchange */
                                //return CAVS_ERROR;
                            }
                        }
                        else
                            p->b_complete = 1; /* set for pic exchange */

                        if(p->b_complete && p->ph.i_picture_coding_type != XAVS_B_PICTURE)
                        {
                            //xavs_image *p_image = p->p_save[0];
                            if(p->vsh.b_low_delay)
                            {
                                i_result = CAVS_FRAME_OUT;

                                /* set for ffmpeg interface */
                                param->output_type = p->p_save[0]->i_code_type;
                                  
                                cavs_out_image_yuv420(p, p->p_save[0], param->p_out_yuv, 0);
                            }
                            if(!p->p_save[1])
                            {
                                //p->p_save[0] = &p->image[1];
                            }
                            else
                            {
                                if(p->vsh.b_low_delay == 0 /*&& p->ph.i_picture_coding_type != XAVS_I_PICTURE*/ )
                                {
                                    i_result = CAVS_FRAME_OUT;

                                    /* set for ffmpeg interface */
                                    param->output_type = p->p_save[1]->i_code_type;
                          
                                    cavs_out_image_yuv420(p, p->p_save[1], param->p_out_yuv, 0);
                                }

                                //if(!p->p_save[2])
                                //{
                                    //p->p_save[0] = &p->image[2];	 
                                //}
                                //else
                                //{
                                   // p->p_save[0] = p->p_save[2];
                                //}
                            }
                            //p->p_save[2] = p->p_save[1];
                            //p->p_save[1] = p_image;
                            //memset(&p->cur, 0, sizeof(xavs_image));
                            p->b_complete = 0;
                        }
                        else if( p->b_complete )
                        {
                            i_result = CAVS_FRAME_OUT;

                            /* set for ffmpeg interface */
                            param->output_type =  p->p_save[0]->i_code_type;
                              
                            cavs_out_image_yuv420(p, p->p_save[0], param->p_out_yuv, 0);
                            memset(&p->cur,0,sizeof(xavs_image));
                            p->b_complete=0;
                        }	
                    } 
                    else if( 1 == p->param.i_thread_model )
                    {
                        if ( XAVS_THREAD_MAX >= sliceNum )
                        {
                            int i_next_mb_y;

							p->b_error_flag = 0;
                            /* sync context of handle */
                            cavs_slice_thread_sync_context( p->thread[threadNum], p );
                            if( threadNum !=0 ) /* NOTE : need to init new handle */
                            {
								p->thread[threadNum]->ph.b_picture_structure = 0; /* NOTE : guarantee field not change */
                            	cavs_init_picture(p->thread[threadNum]);
                            }
                            cavs_bitstream_init(&p->thread[threadNum]->s, p_buf, i_len);
                            p->thread[threadNum]->i_mb_y = cur_startcode & 0x000000FF;

                            if( field->slice_num != 0 )
                                i_next_mb_y = field->slice[slice_count].i_startcode & 0x000000FF; /*slice_count++ before*/
                            else
                                i_next_mb_y = 0xFF;

                            if((uint32_t)i_next_mb_y > (p->i_mb_height>>1)
                                && field->slice_num != 0 )
                            {
                                p->b_error_flag = 1;
                            }
                                
                            if (i_next_mb_y>=0 && i_next_mb_y<=0xAF)
                                p->thread[threadNum]->i_slice_mb_end = i_next_mb_y * p->thread[threadNum]->i_mb_width;
                            else
                            {
                                p->thread[threadNum]->i_slice_mb_end = (p->thread[threadNum]->i_mb_num>>1);
                            }

                            if( !p->b_error_flag )
                            {
#if TEST_POOL
                                cavs_threadpool_run( p->threadpool, (void*)cavs_get_slice, p->thread[threadNum] );
                                p->thread[threadNum]->b_thread_active = 1;
#else                            
                                ret = cavs_pthread_create( &p->thread[threadNum]->id, NULL, (void*)cavs_get_slice, p->thread[threadNum] );

#endif
                            }
                            else
                            {  
                                p->thread[threadNum]->b_thread_active = 0;
                                threadNum = sliceNum;
                                p->thread[threadNum - 1]->b_complete = 1;
                            }

							if ( ret == 0 )
							{
								p->thread[threadNum]->b_thread_active = 1;
							}

                            if( threadNum != sliceNum )
								threadNum++;
                        }
                        else
                        {
                            printf("[error]threads exceed limit\n");

                            return CAVS_ERROR;
                        }
                                    
                        if ( threadNum == sliceNum )
                        {
                            for ( i = 0; i < threadNum; i++ )
                            {
                                if( p->thread[i]->b_thread_active )
                                {  
#if TEST_POOL
                                    if(cavs_threadpool_wait( p->threadpool, p->thread[i]))
                		                ;//return -1;
#else
                                    cavs_pthread_join(p->thread[i]->id, NULL);
#endif                                    
                                    p->thread[i]->b_thread_active = 0;
                                    if( p->thread[i]->b_error_flag )
                                    {
						                p->b_error_flag = 1;
                                        p->thread[threadNum - 1]->b_complete = 1;
                                        printf("[error] decoding slice is wrong\n");
                                        
                                        //return CAVS_ERROR;
                                    }
                                }
                            }

                            //p->b_complete = p->thread[threadNum - 1]->b_complete;
                            if( p->thread[threadNum - 1]->b_complete && p->ph.i_picture_coding_type != XAVS_B_PICTURE )
                            {
                                //xavs_image *p_image = p->p_save[0];
                                
                                if( p->vsh.b_low_delay ) /* */
                                {
                                    i_result = CAVS_FRAME_OUT;

                                    /* set for ffmpeg interface */
                                    param->output_type =  p->p_save[0]->i_code_type;
                                      
                                    cavs_out_image_yuv420( p, p->p_save[0], param->p_out_yuv, 0 );
                                }
                                
                                if( !p->p_save[1] )
                                {
                                    //p->p_save[0] = &p->image[1];
                                }
                                else
                                {
                                    if( p->vsh.b_low_delay == 0 /*&& p->ph.i_picture_coding_type != XAVS_I_PICTURE*/ )
                                    {
                                        i_result = CAVS_FRAME_OUT;

                                        /* set for ffmpeg interface */
                                        param->output_type =  p->p_save[1]->i_code_type;
                                          
                                        cavs_out_image_yuv420( p, p->p_save[1], param->p_out_yuv, 0 );
                                    }
                                    //if( !p->p_save[2] )
                                    //{
                                    //    p->p_save[0] = &p->image[2];	 
                                    //}
                                    //else
                                    //{
                                    //    p->p_save[0] = p->p_save[2];
                                    //}
                                }
                                //p->p_save[2] = p->p_save[1];
                                //p->p_save[1] = p_image; /* p->p_save[0] to p->p_save[1] */
                                //memset( &p->cur, 0, sizeof(xavs_image) );
                                p->thread[threadNum - 1]->b_complete = 0;
                            }
                            else /* output B frame of reconstructed */
                            {
                                i_result = CAVS_FRAME_OUT;

                                /* set for ffmpeg interface */
                                param->output_type = p->p_save[0]->i_code_type;
                                
                                cavs_out_image_yuv420( p, p->p_save[0], param->p_out_yuv, 0 );
                                memset( &p->cur, 0, sizeof(xavs_image) );                              
                                p->thread[threadNum - 1]->b_complete = 0;
                            }	
                        }
                    }				
                }
                else
                {
                    return CAVS_ERROR;
                }
        }
    }

	if( p->b_error_flag )
	{
		i_result = CAVS_ERROR;
	}

    return i_result;
}

/* cavs_botfield_pack */
uint32_t cavs_botfield_pack( InputStream*p, frame_pack *field )
{
    //uint32_t i_startcode;
    int i_len;
    /*unsigned char *m_buf;

    m_buf = (unsigned char *)cavs_malloc((MAX_CODED_FRAME_SIZE/2)*sizeof(unsigned char));
    memset(m_buf, 0, (MAX_CODED_FRAME_SIZE/2)*sizeof(unsigned char));*/

	uint32_t i_startcode = 1; /* use to start loop */
    while( i_startcode )
    {  
        cavs_bitstream_save( field, p );
		i_startcode = cavs_get_one_nal(p, field->p_cur/*p->m_buf*/, &i_len, (MAX_CODED_FRAME_SIZE / 2) - field->i_len);

        switch( i_startcode )
        {	
            case XAVS_VIDEO_SEQUENCE_START_CODE:
                cavs_bitstream_restore( p, field );
                //cavs_free(m_buf);
                return i_startcode;
            case XAVS_VIDEO_SEQUENCE_END_CODE:
                cavs_bitstream_restore( p, field );
                //cavs_free(m_buf);
                return i_startcode;
            case XAVS_VIDEO_EDIT_CODE:
                cavs_bitstream_restore( p, field );
				//cavs_free(m_buf);
                return i_startcode;
            case XAVS_I_PICUTRE_START_CODE:
                cavs_bitstream_restore( p, field );
                //cavs_free(m_buf);
                return i_startcode;
            case XAVS_PB_PICUTRE_START_CODE:
                cavs_bitstream_restore( p, field );
                //cavs_free(m_buf);
                return i_startcode;

            case XAVS_USER_DATA_CODE:
                break;
            case XAVS_EXTENSION_START_CODE:
                break;
            case 0x000001fe: // FIXIT
                cavs_bitstream_restore( p, field );
                //cavs_free(m_buf);
                return i_startcode;
                break;
			case -1:
				field->p_cur = field->p_start;
				field->i_len = 0;
				field->slice_num = 0;
				break;
            default:
                //memcpy(field->p_cur, p->m_buf, i_len ); /* pack slice into one frame */

                /* set slice info */
                field->slice[field->slice_num].i_startcode = i_startcode;
                field->slice[field->slice_num].p_start = field->p_cur + 4; /* for skip current startcode */
                field->slice[field->slice_num].i_len = i_len-4;
        
                /*update frame info */
                field->p_cur = field->p_cur + i_len;
                field->i_len += i_len;
                field->slice_num++;               
                break;
    	 }
    }

    //cavs_free(m_buf);

    return 0;
}

/* decode_bot_field */
int decode_bot_field(void *p_decoder, frame_pack *field, cavs_param *param )
{
    cavs_decoder *p=(cavs_decoder *)p_decoder;
    uint32_t  cur_startcode;
    uint8_t   *p_buf;
    int    i_len, i_result=0;
    int i, sliceNum, threadNum;
    int ret = 0;
    int slice_count = 0; /* bot pack has no pic header */
    //int b_interlaced = p->param.b_interlaced;

    p->b_bottom = field->b_bottom;
    sliceNum = field->slice_num; /* bot pack has no pic header */
    threadNum = 0;

    cur_startcode = field->i_startcode_type;

    /* decode header of pic */
    switch( cur_startcode )
    {
        case XAVS_I_PICUTRE_START_CODE:
            //cavs_bitstream_init(&p->s, p_buf, i_len);
            //cavs_get_i_picture_header(&p->s,&p->ph,&p->vsh);
            cavs_init_picture(p);
            break;

        case XAVS_PB_PICUTRE_START_CODE:
            //cavs_bitstream_init(&p->s, p_buf, i_len);
            //cavs_get_pb_picture_header(&p->s,&p->ph,&p->vsh);
            cavs_init_picture(p);
            break;

        default:
            return CAVS_ERROR;
    }	
    
    /* decode slice */
    while( field->slice_num--)
    {
        cur_startcode = field->slice[slice_count].i_startcode;
        p_buf = field->slice[slice_count].p_start;
        i_len = field->slice[slice_count].i_len;
        slice_count++;

        switch( cur_startcode )
        {
            case XAVS_USER_DATA_CODE:
                cavs_bitstream_init(&p->s,p_buf,i_len);
                i_result = cavs_get_user_data(&p->s, &p->user_data);
                break;

            case XAVS_EXTENSION_START_CODE:
                cavs_bitstream_init(&p->s,p_buf,i_len);
                i_result = cavs_get_extension(&p->s, p);
                break;

            default:
                if( !p->cur.p_data[0] ) /* store current decoded frame */
                {
                    break;
                }

                if( p->ph.i_picture_coding_type == XAVS_B_PICTURE )
                {
                    if( !p->p_save[1] || !p->p_save[2] ) /* need L0 and L1 reference frame */
                    {
                        break;
                    }
                }
                
                if( p->ph.i_picture_coding_type == XAVS_P_PICTURE )
                {
                    if( !p->p_save[1] ) /* need reference frame */
                    {
                        break;
                    }
                }

                if( cur_startcode >= XAVS_SLICE_MIN_START_CODE 
                    && cur_startcode <= XAVS_SLICE_MAX_START_CODE )
                {
                    if ( 0 == p->param.i_thread_model )
                    {
                        int i_next_mb_y;

                        p->b_error_flag = 0;
                        cavs_bitstream_init(&p->s, p_buf, i_len);                        
                        p->i_mb_y = cur_startcode & 0x000000FF;                       
                        
                        if( field->slice_num != 0 )
                            i_next_mb_y = field->slice[slice_count].i_startcode & 0x000000FF; /*slice_count++ before*/
                        else
                            i_next_mb_y = 0xFF;

                        
                        if((uint32_t)i_next_mb_y < (p->i_mb_height>>1))
                        {
                            p->b_error_flag = 1;
                        }
                        
                        if (i_next_mb_y>=0 && i_next_mb_y<=0xAF)
                            p->i_slice_mb_end = i_next_mb_y * p->i_mb_width;
                        else
                        {  
                            p->i_slice_mb_end = p->i_mb_num;
                        }                 

                        if( !p->b_error_flag )
                        {
                            if( cavs_get_slice(p) != 0 )
                            {
                                printf("[error] decoding slice is wrong\n");
                                p->b_complete = 1; /* set for pic exchange */
                                //return CAVS_ERROR;
                            }
                        }
                        else
                             p->b_complete = 1; /* set for pic exchange */
                        
                        if(p->b_complete && p->ph.i_picture_coding_type != XAVS_B_PICTURE)
                        {
                            xavs_image *p_image = p->p_save[0];
                            if( p->vsh.b_low_delay )
                            {
                                i_result = CAVS_FRAME_OUT;

                                /* set for ffmpeg interface */
                                param->output_type = p->p_save[0]->i_code_type;
                                
                                cavs_out_image_yuv420(p, p->p_save[0], param->p_out_yuv, 1);
                            }
                            if(!p->p_save[1])
                            {
                                p->p_save[0] = &p->image[1];
                            }
                            else
                            {
                                if(p->vsh.b_low_delay == 0 /*&& p->ph.i_picture_coding_type != XAVS_I_PICTURE*/ )
                                {
                                    i_result = CAVS_FRAME_OUT;

                                    /* set for ffmpeg interface */
                                    param->output_type = p->p_save[1]->i_code_type;
                                      
                                    //printf("prepare one frame yuv : I or P frame\n");
                                     
                                    cavs_out_image_yuv420(p, p->p_save[1], param->p_out_yuv, 1);
                                }

                                if(!p->p_save[2])
                                {
                                    p->p_save[0] = &p->image[2];	 
                                }
                                else
                                {
                                    p->p_save[0] = p->p_save[2];
                                }
                            }
                            p->p_save[2] = p->p_save[1];
                            p->p_save[1] = p_image;
                            memset(&p->cur, 0, sizeof(xavs_image));
                            p->b_complete = 0;
                        }
                        else if(p->b_complete)
                        {
                            i_result = CAVS_FRAME_OUT;

                            /* set for ffmpeg interface */
                            param->output_type = p->p_save[0]->i_code_type;
                            
                            cavs_out_image_yuv420(p, p->p_save[0], param->p_out_yuv, 1);
                            memset(&p->cur,0,sizeof(xavs_image));
                            p->b_complete=0;
                        }	
                    } 
                    else if( 1 == p->param.i_thread_model )
                    {
                        if ( XAVS_THREAD_MAX >= sliceNum )
                        {
                            int i_next_mb_y;

							p->b_error_flag = 0;
                            /* sync context of handle */
                            cavs_slice_thread_sync_context( p->thread[threadNum], p );
                            if( threadNum !=0 ) /* NOTE : need to init new handle */
                            {
								p->thread[threadNum]->ph.b_picture_structure = 0; /* NOTE : guarantee field not change */
                            	cavs_init_picture(p->thread[threadNum]);
                            }
                            cavs_bitstream_init(&p->thread[threadNum]->s, p_buf, i_len);
                            p->thread[threadNum]->i_mb_y = cur_startcode & 0x000000FF;

                            if( field->slice_num != 0 )
                                i_next_mb_y = field->slice[slice_count].i_startcode & 0x000000FF; /*slice_count++ before*/
                            else
                                i_next_mb_y = 0xFF;

                            if((uint32_t)i_next_mb_y < (p->i_mb_height>>1))
                            {
                                p->b_error_flag = 1;
                            }
                                  
                            if (i_next_mb_y>=0 && i_next_mb_y<=0xAF)
                                p->thread[threadNum]->i_slice_mb_end = i_next_mb_y * p->thread[threadNum]->i_mb_width;
                            else
                            {
                                p->thread[threadNum]->i_slice_mb_end = p->thread[threadNum]->i_mb_num;
                            }

                            if(!p->b_error_flag)
                            {
#if TEST_POOL
                                cavs_threadpool_run( p->threadpool, (void*)cavs_get_slice, p->thread[threadNum] );
                                p->thread[threadNum]->b_thread_active = 1;
#else                    
                                ret = cavs_pthread_create( &p->thread[threadNum]->id, NULL, (void*)cavs_get_slice, p->thread[threadNum] );

#endif
                            }
                            else
                            {
                                p->thread[threadNum]->b_thread_active = 0;
                                threadNum = sliceNum;
                                p->thread[threadNum - 1]->b_complete = 1;
                            }
                            
                            if ( ret == 0 )
                            {
                                p->thread[threadNum]->b_thread_active = 1;
                            }

                            if( threadNum != sliceNum )
								threadNum++;

                        }
                        else
                        {
                            printf("[error]threads exceed limit\n");

                            return CAVS_ERROR;
                        }
                                    
                        if ( threadNum == sliceNum )
                        {
                            for ( i = 0; i < threadNum; i++ )
                            {
                                if( p->thread[i]->b_thread_active )
                                {
#if TEST_POOL
                                    if(cavs_threadpool_wait( p->threadpool, p->thread[i]))
                		                ;//return -1;
#else                                
                                    cavs_pthread_join(p->thread[i]->id, NULL);
#endif                                    

                                    p->thread[i]->b_thread_active = 0;
                                    if( p->thread[i]->b_error_flag )
                                    {
                                        p->b_error_flag = 1;
                                        p->thread[threadNum - 1]->b_complete = 1;
                                        printf("[error] decoding slice is wrong\n");
                                        
                                        //return CAVS_ERROR;
                                    }
                                }
                            }

                            p->b_complete = p->thread[threadNum - 1]->b_complete;
                            if( p->thread[threadNum - 1]->b_complete && p->ph.i_picture_coding_type != XAVS_B_PICTURE )
                            {
                                xavs_image *p_image = p->p_save[0];
                                
                                if( p->vsh.b_low_delay ) /* */
                                {
                                    i_result = CAVS_FRAME_OUT;

                                    /* set for ffmpeg interface */
                                    param->output_type = p->p_save[0]->i_code_type;
                                    
                                    cavs_out_image_yuv420( p, p->p_save[0], param->p_out_yuv, 1 );
                                }
                                
                                if( !p->p_save[1] )
                                {
                                    p->p_save[0] = &p->image[1];
                                }
                                else
                                {
                                    if( p->vsh.b_low_delay == 0 /*&& p->ph.i_picture_coding_type != XAVS_I_PICTURE*/ )
                                    {
                                        i_result = CAVS_FRAME_OUT;

                                        /* set for ffmpeg interface */
                                        param->output_type = p->p_save[1]->i_code_type;
                                        
                                        //printf("prepare one frame yuv : I or P frame\n");
                                         
                                        cavs_out_image_yuv420( p, p->p_save[1], param->p_out_yuv, 1 );
                                    }
                                    if( !p->p_save[2] )
                                    {
                                        p->p_save[0] = &p->image[2];	 
                                    }
                                    else
                                    {
                                        p->p_save[0] = p->p_save[2];
                                    }
                                }
                                p->p_save[2] = p->p_save[1];
                                p->p_save[1] = p_image; /* p->p_save[0] to p->p_save[1] */
                                memset( &p->cur, 0, sizeof(xavs_image) );
                                p->thread[threadNum - 1]->b_complete = 0;
                            }
                            else /* output B frame of reconstructed */
                            {
                                i_result = CAVS_FRAME_OUT;

                                /* set for ffmpeg interface */
                                param->output_type = p->p_save[0]->i_code_type;
                                
                                cavs_out_image_yuv420( p, p->p_save[0], param->p_out_yuv, 1 );
                                memset( &p->cur, 0, sizeof(xavs_image) );                              
                                p->thread[threadNum - 1]->b_complete = 0;
                            }	
                        }
                    }				
                }
                else
                {
                    return CAVS_ERROR;
                }
        }
    }
	
    if( p->b_error_flag )
    {
        i_result = CAVS_ERROR;
    }

    return i_result;
}

#if THREADS_OPT
/* pack top-field and bot-field into one frame */
uint32_t cavs_field_pack( InputStream*p, frame_pack *field )
{
    uint32_t i_startcode=1, pre_startcode = 0;
    int i_len;
    /*unsigned char *m_buf;

    m_buf = (unsigned char *)cavs_malloc((MAX_CODED_FRAME_SIZE)*sizeof(unsigned char));
    memset(m_buf, 0, (MAX_CODED_FRAME_SIZE)*sizeof(unsigned char));*/

	//uint32_t i_startcode = 1; /* use to start loop */
    while( i_startcode )
    {  
        cavs_bitstream_save( field, p );
		i_startcode = cavs_get_one_nal(p, field->p_cur/*p->m_buf*/, &i_len, MAX_CODED_FRAME_SIZE- field->i_len);
		
        switch( i_startcode )
        {	
            case XAVS_VIDEO_SEQUENCE_START_CODE:
                cavs_bitstream_restore( p, field );
                //cavs_free(m_buf);
                return i_startcode;
            case XAVS_VIDEO_SEQUENCE_END_CODE:
                cavs_bitstream_restore( p, field );
                //cavs_free(m_buf);
                return i_startcode;
            case XAVS_VIDEO_EDIT_CODE:
                cavs_bitstream_restore( p, field );
				//cavs_free(m_buf);
                return i_startcode;
            case XAVS_I_PICUTRE_START_CODE:
                cavs_bitstream_restore( p, field );
                //cavs_free(m_buf);
                return i_startcode;
            case XAVS_PB_PICUTRE_START_CODE:
                cavs_bitstream_restore( p, field );
                //cavs_free(m_buf);
                return i_startcode;

            case XAVS_USER_DATA_CODE:
                break;
            case XAVS_EXTENSION_START_CODE:
                break;
            case 0x000001fe: // FIXIT
                cavs_bitstream_restore( p, field );
                //cavs_free(m_buf);
                return i_startcode;
                break;
			case -1:
				field->p_cur = field->p_start;
				field->i_len = 0;
				field->slice_num = 0;
				break;
            default:
				if( i_startcode == pre_startcode ) /* NOTE : repeat slice should discard */
					break;
				if ((i_startcode & 0xff) > (uint32_t)field->i_mb_end[1]) 
				{
					cavs_bitstream_restore( p, field );
					//cavs_free(m_buf);

					return i_startcode;
				}
				
                //memcpy(field->p_cur, p->m_buf, i_len ); /* pack slice into one frame */

                /* set slice info */
                field->slice[field->slice_num].i_startcode = i_startcode;
                field->slice[field->slice_num].p_start = field->p_cur + 4; /* for skip current startcode */
                field->slice[field->slice_num].i_len = i_len-4;
        
                /*update frame info */
                field->p_cur = field->p_cur + i_len;
                field->i_len += i_len;
                field->slice_num++;               
                break;
    	 }

		pre_startcode = i_startcode;
    }

    //cavs_free(m_buf);

    return 0;
}

int field_decode_aec_threads( cavs_decoder *p_aec )
{
    //cavs_decoder *p_m = p_aec->p_m;
    frame_pack *field = p_aec->fld;
    uint8_t   *p_buf;
    int    i_len, i_result = 0;
    int slice_count = p_aec->slice_count;
    int field_count = p_aec->field_count;
    uint32_t  cur_startcode;
    uint32_t i_ret =0;

    while( field->slice_num--)
    {
        cur_startcode = field->slice[slice_count].i_startcode;
        p_buf = field->slice[slice_count].p_start;
        i_len = field->slice[slice_count].i_len;
        slice_count++;
        field_count++;
        
        switch( cur_startcode )
        {
            case XAVS_USER_DATA_CODE:
                cavs_bitstream_init(&p_aec->s,p_buf,i_len);
                i_result = cavs_get_user_data(&p_aec->s, &p_aec->user_data);
                break;

            case XAVS_EXTENSION_START_CODE:
                cavs_bitstream_init(&p_aec->s,p_buf,i_len);
                i_result = cavs_get_extension(&p_aec->s, p_aec);
                break;

            default:

                if( cur_startcode >= XAVS_SLICE_MIN_START_CODE 
                    && cur_startcode <= XAVS_SLICE_MAX_START_CODE )
                {
                    if ( 0 == p_aec->param.i_thread_model )
                    {
                        int i_next_mb_y;
                        uint32_t i_mb_y_ori = 0;

                        cavs_bitstream_init(&p_aec->s, p_buf, i_len);                        
                        p_aec->i_mb_y = cur_startcode & 0x000000FF;
                        i_mb_y_ori = p_aec->i_mb_y;                    
                        
                        /* set b_bottom */
                        p_aec->b_bottom = 0;
                        if( p_aec->i_mb_y >= (p_aec->i_mb_height>>1))
                        {
                            p_aec->b_bottom = 1;
                            
                            i_ret = cavs_init_picture_context(p_aec);
                            if( i_ret == CAVS_ERROR )
                            {
                                p_aec->b_error_flag  = 1;
                                return CAVS_ERROR;
                            }
                            cavs_init_picture_ref_list_for_aec(p_aec);
                        }
                        p_aec->i_mb_y = i_mb_y_ori;
             
                        if( field->slice_num != 0 )
                            i_next_mb_y = field->slice[slice_count].i_startcode & 0x000000FF; /*slice_count++ before*/
                        else
                            i_next_mb_y = 0xFF;

                        if( !p_aec->b_bottom )
                        {
                            if((uint32_t)i_next_mb_y<(p_aec->i_mb_height>>1))
                            {
                                p_aec->b_error_flag = 1;
                                return  CAVS_ERROR;
                            }
                        }                       
                        
                        if (i_next_mb_y>=0 && i_next_mb_y<=0xAF)
                            p_aec->i_slice_mb_end = i_next_mb_y * p_aec->i_mb_width;
                        else
                        {
                            p_aec->i_slice_mb_end = (p_aec->i_mb_num>>(!p_aec->b_bottom));
                        }
                        p_aec->i_slice_mb_end_fld[p_aec->b_bottom] =  p_aec->i_slice_mb_end;
                        
                        cavs_get_slice_aec(p_aec);
                        if(p_aec->b_error_flag == 1)
                            return  CAVS_ERROR;

                        /* creat independent ref-list for AEC module */
                        if( p_aec->ph.i_picture_coding_type != XAVS_B_PICTURE )
                        {
                            xavs_image *p_image = p_aec->p_save_aec[0];

                            if(!p_aec->p_save_aec[1])
                            {
                                if( p_aec->b_bottom )
                                    p_aec->p_save_aec[0] = &p_aec->image_aec[1];
                            }
                            else
                            {
                                if( p_aec->b_bottom )
                                {
                                    if(!p_aec->p_save_aec[2])
                                    {
                                        p_aec->p_save_aec[0] = &p_aec->image_aec[2];	 
                                    }
                                    else
                                    {
                                        p_aec->p_save_aec[0] = p_aec->p_save_aec[2];
                                    }
                                }
                            }

                            if( p_aec->b_bottom )
                            {
                                p_aec->p_save_aec[2] = p_aec->p_save_aec[1];
                                p_aec->p_save_aec[1] = p_image;
                                memset(&p_aec->cur_aec, 0, sizeof(xavs_image));
                            }
                        }

#if !CREATE_THREADS
                        /* update AEC handle when bottom-field */
                        if( p_aec->b_bottom )
                        {
                            /* switch to main threads */
                            p_m->unused[1]->p_save_aec[0] = p_aec->p_save_aec[0];
                            p_m->unused[1]->p_save_aec[1] = p_aec->p_save_aec[1];
                            p_m->unused[1]->p_save_aec[2] = p_aec->p_save_aec[2];

                            p_m->unused[0] = p_m->unused[1];
                            p_m->unused[1] = p_aec;
                            
                            /* copy col type to next handle */
                            memcpy( p_m->unused[0]->p_col_mv, p_m->unused[1]->p_col_mv,p_m->i_mb_width*(p_m->i_mb_height)*4*sizeof(xavs_vector));
                            memcpy( p_m->unused[0]->p_col_type_base, p_m->unused[1]->p_col_type_base, p_m->i_mb_width*(p_m->i_mb_height));
                            
                            p_m->current[0] = p_m->current[1];
                            p_m->current[1] = p_aec;
                        }   
#endif
                        
                        
                    } 
                    else if( 1 == p_aec->param.i_thread_model )
                    {
                        printf("[error] not support multi-slice!\n");
                        return  CAVS_ERROR;      
                    }
                }
                else
                {
                    return CAVS_ERROR;
                }
        }
    }

    return 0;
}


/* decode top-field and bot-field orderly in one function */
int cavs_field_decode(void *p_decoder, frame_pack *field, cavs_param *param )
{
#if !THREADS_OPT
    cavs_decoder *p=(cavs_decoder *)p_decoder;
#else
    cavs_decoder *p = (cavs_decoder *)p_decoder;
    cavs_decoder *p_m = p;/* backup main threads */
    cavs_decoder *p_aec, *p_rec;
    int field_count = 0;
#endif
    
    uint32_t  cur_startcode;
    uint8_t   *p_buf;
    int    i_len, i_ret = 0, i_result=0;
    int sliceNum, threadNum;
    int slice_count = 1; /* 0 is for pic header */
    
    sliceNum = field->slice_num - 1; /* exclude pic header */
    threadNum = 0;

    cur_startcode = field->slice[0].i_startcode;
    p_buf = field->slice[0].p_start;
    i_len = field->slice[0].i_len;
    field->slice_num--;

#if THREADS_OPT
    /* init aec handle */
    p_aec = p_m->unused[0];
    p = p_aec;
    p_aec->b_bottom = 0; /* init bottom flag */
    p_aec->i_frame_decoded = p_m->i_frame_decoded; //just for debug
#endif    

    /* decode header of pic */
    switch( cur_startcode )
    {
        case XAVS_I_PICUTRE_START_CODE:
            cavs_bitstream_init(&p->s, p_buf, i_len);
            cavs_get_i_picture_header(&p->s,&p->ph,&p->vsh);
			if( p->vsh.i_profile_id == PROFILE_GUANGDIAN )
			{
				//p->ph.b_aec_enable = 1; /* AEC only for AVS+ */
				p->ph.b_aec_enable = p->vsh.b_aec_enable;
			}
			else if(p->vsh.i_profile_id == PROFILE_JIZHUN)
			{
				p->ph.b_aec_enable = 0; /* AVS */				
			}
			else
			{
				printf("[error]wrong profile id\n");
				p->b_error_flag  = 1;

                return CAVS_ERROR;
			}
			p->ph.b_picture_structure = 0; /* NOTE : guarantee field not change */
#if THREADS_OPT
            i_ret = cavs_init_picture_context(p);
            if( i_ret == CAVS_ERROR )
            {
                p->b_error_flag  = 1;
                return CAVS_ERROR;
            }
            cavs_init_picture_ref_list_for_aec(p);
#else
            cavs_init_picture(p);
#endif

#if !THREADS_OPT
            /* add for no-seq header before I-frame */
            p->last_delayed_pframe = p->p_save[1];
#endif
            break;

        case XAVS_PB_PICUTRE_START_CODE:
            cavs_bitstream_init(&p->s, p_buf, i_len);
            cavs_get_pb_picture_header(&p->s,&p->ph,&p->vsh);
			if( p->vsh.i_profile_id == PROFILE_GUANGDIAN )
			{
				//p->ph.b_aec_enable = 1; /* AEC only for AVS+ */
				p->ph.b_aec_enable = p->vsh.b_aec_enable;
			}
			else if(p->vsh.i_profile_id == PROFILE_JIZHUN)
			{
				p->ph.b_aec_enable = 0; /* AVS */
			}
			else
			{
				printf("[error]wrong profile id\n");
				p->b_error_flag  = 1;

                return CAVS_ERROR;
			}
            p->ph.b_picture_structure = 0; /* NOTE : guarantee field not change */
#if THREADS_OPT
            i_ret = cavs_init_picture_context(p);
            if( i_ret == CAVS_ERROR )
            {
                p->b_error_flag  = 1;
                return CAVS_ERROR;
            }
            cavs_init_picture_ref_list_for_aec(p);
#else
            cavs_init_picture(p);
#endif

            break;

        default:
            return CAVS_ERROR;
    }	
    
    /*add for weighting quant */
    cavs_init_frame_quant_param( p );

    cavs_frame_update_wq_matrix( p );

#if THREADS_OPT /*NOTE : not support multi-slice of field */

#if CREATE_THREADS
    p_aec->p_m = p_m;
    p_aec->fld = field;
    p_aec->slice_count = slice_count;
    p_aec->field_count = field_count;

#if TEST_POOL
    cavs_threadpool_run( p_m->threadpool, (void*)field_decode_aec_threads, p_aec );
    p_aec->b_thread_active = 1;
#else
    int ret = cavs_pthread_create( &p_aec->id, NULL, (void*)field_decode_aec_threads, p_aec );
    if ( ret == 0 )
    {
        p_aec->b_thread_active = 1;
    }
    else
    {
        printf("creat field multi-thread of AEC failed!\n");
        return 0;
    }
#endif

#else
    /* aec decode loop */
    /* decode slice */
    while( field->slice_num--)
    {
        cur_startcode = field->slice[slice_count].i_startcode;
        p_buf = field->slice[slice_count].p_start;
        i_len = field->slice[slice_count].i_len;
        slice_count++;
        field_count++;
        
        switch( cur_startcode )
        {
            case XAVS_USER_DATA_CODE:
                cavs_bitstream_init(&p->s,p_buf,i_len);
                i_result = cavs_get_user_data(&p->s, &p->user_data);
                break;

            case XAVS_EXTENSION_START_CODE:
                cavs_bitstream_init(&p->s,p_buf,i_len);
                i_result = cavs_get_extension(&p->s, p);
                break;

            default:

                if( cur_startcode >= XAVS_SLICE_MIN_START_CODE 
                    && cur_startcode <= XAVS_SLICE_MAX_START_CODE )
                {
                    if ( 0 == p->param.i_thread_model )
                    {
                        int i_next_mb_y;
                        uint32_t i_mb_y_ori;

                        cavs_bitstream_init(&p_aec->s, p_buf, i_len);                        
                        p_aec->i_mb_y = cur_startcode & 0x000000FF;                       
                        i_mb_y_ori = p_aec->i_mb_y;
                        
                        /* set b_bottom */
                        p_aec->b_bottom = 0;
                        if( p_aec->i_mb_y >= (p_m->i_mb_height>>1))
                        {
                            p_aec->b_bottom = 1;
                            
                            i_ret = cavs_init_picture_context(p_aec);
                            if( i_ret == CAVS_ERROR )
                            {
                                p_aec->b_error_flag  = 1;
                                return CAVS_ERROR;
                            }
                            cavs_init_picture_ref_list_for_aec(p_aec);
                        }
                        p_aec->i_mb_y = i_mb_y_ori;
                        
                        if( field->slice_num != 0 )
                            i_next_mb_y = field->slice[slice_count].i_startcode & 0x000000FF; /*slice_count++ before*/
                        else
                            i_next_mb_y = 0xFF;
                        if (i_next_mb_y>=0 && i_next_mb_y<=0xAF)
                            p_aec->i_slice_mb_end = i_next_mb_y * p_m->i_mb_width;
                        else
                        {
                            p_aec->i_slice_mb_end = (p_m->i_mb_num>>(!p_aec->b_bottom));
                        }
                        p_aec->i_slice_mb_end_fld[p_aec->b_bottom] =  p_aec->i_slice_mb_end;
                        
                        cavs_get_slice_aec(p_aec);
                        if(p_aec->b_error_flag == 1)
                            return  CAVS_ERROR;

                        /* creat independent ref-list for AEC module */
                        if( p_aec->ph.i_picture_coding_type != XAVS_B_PICTURE )
                        {
                            xavs_image *p_image = p_aec->p_save_aec[0];

                            if(!p_aec->p_save_aec[1])
                            {
                                if(p_aec->b_bottom)
                                    p_aec->p_save_aec[0] = &p_aec->image_aec[1];
                            }
                            else
                            {
                                if(p_aec->b_bottom)
                                {
                                    if(!p_aec->p_save_aec[2])
                                    {
                                        p_aec->p_save_aec[0] = &p_aec->image_aec[2];	 
                                    }
                                    else
                                    {
                                        p_aec->p_save_aec[0] = p_aec->p_save_aec[2];
                                    }
                                }
                            }

                            if(p_aec->b_bottom)
                            {
                                p_aec->p_save_aec[2] = p_aec->p_save_aec[1];
                                p_aec->p_save_aec[1] = p_image;
                                memset(&p_aec->cur_aec, 0, sizeof(xavs_image));
                            }
                        }
                        
                        /* update AEC handle when bottom-field */
                        if( p_aec->b_bottom )
                        {
                            /* switch to main threads */
                            p_m->unused[1]->p_save_aec[0] = p_aec->p_save_aec[0];
                            p_m->unused[1]->p_save_aec[1] = p_aec->p_save_aec[1];
                            p_m->unused[1]->p_save_aec[2] = p_aec->p_save_aec[2];
                            p_m->unused[0] = p_m->unused[1];
                            p_m->unused[1] = p_aec;
                            
                            /* copy col type to next handle */
                            memcpy( p_m->unused[0]->p_col_mv, p_m->unused[1]->p_col_mv,p_m->i_mb_width*(p_m->i_mb_height)*4*sizeof(xavs_vector));
                            memcpy( p_m->unused[0]->p_col_type_base, p_m->unused[1]->p_col_type_base, p_m->i_mb_width*(p_m->i_mb_height));
                            
                            p_m->current[0] = p_m->current[1];
                            p_m->current[1] = p_aec;   
                        }                        
                    } 
                    else if( 1 == p->param.i_thread_model )
                    {
                        printf("[error] not support multi-slice!\n");
                        return  CAVS_ERROR;      
                    }
                }
                else
                {
                    return CAVS_ERROR;
                }
        }
    }
#endif

#if CREATE_THREADS
    /* switch to main threads */
    //p_m->unused[1]->p_save_aec[0] = p_aec->p_save_aec[0]; /* will crash,why?*/
    //p_m->unused[1]->p_save_aec[1] = p_aec->p_save_aec[1];
    //p_m->unused[1]->p_save_aec[2] = p_aec->p_save_aec[2];
    p_m->unused[0] = p_m->unused[1];
    p_m->unused[1] = p_aec;
    
    p_m->current[0] = p_m->current[1];
    p_m->current[1] = p_aec;

#endif

    /* rec decode loop */
    p_rec = p_m->current[0];
    field_count =  2; /* only support two field */
    if( p_rec != NULL )
    {
        while( field_count-- )
        {
            p_rec->b_bottom = !field_count; /* first-top second-botttom */

#if DEBUG

            if(field_count==1)
            {
                switch(p_rec->ph.i_picture_coding_type)
                {
                    case XAVS_I_PICTURE:
                        printf("rec ============= I frame %d ================\n", p->i_frame_decoded);
                        break;
                    case XAVS_P_PICTURE:
                        printf("rec ============= P frame %d================\n", p->i_frame_decoded);
                        break;
                    case XAVS_B_PICTURE:
                        printf("rec ============= B frame %d================\n", p->i_frame_decoded);
                        break;
                }
            }

#endif   

            cavs_init_picture_context_fld( p_rec );
            cavs_get_slice_rec_threads( p_rec );
            
            if( p_rec->b_bottom )
            {
                if( p_rec->ph.i_picture_coding_type != XAVS_B_PICTURE )
                    p_m->last_delayed_pframe = p_rec->p_save[1];
            }
            
            if(p_rec->b_complete && p_rec->ph.i_picture_coding_type != XAVS_B_PICTURE)
            {
                xavs_image *p_image = p_rec->p_save[0];
                if(p_rec->vsh.b_low_delay)
                {
                    i_result = CAVS_FRAME_OUT;

                    /* set for ffmpeg interface */
                    param->output_type = p_rec->p_save[0]->i_code_type;
                    
                    cavs_out_image_yuv420(p_m, p_rec->p_save[0], param->p_out_yuv,  p_rec->b_bottom);
                }
                if(!p_rec->p_save[1])
                {
                    if( p_rec->b_bottom )
                        p_rec->p_save[0] = &p_rec->image[1];
                }
                else
                {
                    if(p_rec->vsh.b_low_delay == 0 /*&& p_rec->ph.i_picture_coding_type != XAVS_I_PICTURE*/ )
                    {
                        i_result = CAVS_FRAME_OUT;

                        /* set for ffmpeg interface */
                        param->output_type = p_rec->p_save[1]->i_code_type;
                          
                        cavs_out_image_yuv420(p_m, p_rec->p_save[1], param->p_out_yuv,  p_rec->b_bottom);
                    }
                    else
                    {
                    	param->output_type = -1;
                    }
   
                    if( p_rec->b_bottom )
                    {
                        if(!p_rec->p_save[2])
                        {
                            p_rec->p_save[0] = &p_rec->image[2];	 
                        }
                        else
                        {
                            p_rec->p_save[0] = p_rec->p_save[2];
                        }    
                    }
                }

                if( p_rec->b_bottom )
                {
                    p_rec->p_save[2] = p_rec->p_save[1];
                    p_rec->p_save[1] = p_image;
                    memset(&p_rec->cur, 0, sizeof(xavs_image));

                    /* copy ref-list to anther threads */
                    p_m->current[1]->p_save[0] = p_rec->p_save[0];
                    p_m->current[1]->p_save[1] = p_rec->p_save[1];
                    p_m->current[1]->p_save[2] = p_rec->p_save[2];
                }
                p_rec->b_complete = 0;
                p_rec->b_bottom = 0;
            }
            else if( p_rec->b_complete )
            {
                i_result = CAVS_FRAME_OUT;

                /* set for ffmpeg interface */
                param->output_type =  p_rec->p_save[0]->i_code_type;
                 
                cavs_out_image_yuv420(p_m, p_rec->p_save[0], param->p_out_yuv,  p_rec->b_bottom);
                memset(&p_rec->cur,0,sizeof(xavs_image));
                p_rec->b_complete=0;
                p_rec->b_bottom = 0;
            }	                         
        }
    }

    /*wait for end of AEC handle */
#if CREATE_THREADS/*when creat threads, this module should put behind to parallel */

    if( p_aec->b_thread_active == 1 )
    {  

#if TEST_POOL
        if(cavs_threadpool_wait( p_m->threadpool, p_aec))
            ;//return -1;
#else    
        cavs_pthread_join(p_aec->id, NULL);
#endif    

        p_aec->b_thread_active = 0;
    }
    
    p_m->unused[0]->p_save_aec[0] = p_aec->p_save_aec[0];
    p_m->unused[0]->p_save_aec[1] = p_aec->p_save_aec[1];
    p_m->unused[0]->p_save_aec[2] = p_aec->p_save_aec[2];

    /* copy col type to next handle */
    memcpy( p_m->unused[0]->p_col_mv, p_aec->p_col_mv,p_m->i_mb_width*(p_m->i_mb_height)*4*sizeof(xavs_vector));
    memcpy( p_m->unused[0]->p_col_type_base, p_aec->p_col_type_base, p_m->i_mb_width*(p_m->i_mb_height));

#if B_MB_WEIGHTING
	{
		int i, j;
		for( j = 0; j < 2; j++ )
		{
            p_m->unused[0]->b_slice_weighting_flag[j] = p_aec->b_slice_weighting_flag[j];
			p_m->unused[0]->b_mb_weighting_flag[j] = p_aec->b_mb_weighting_flag[j];
			for( i = 0; i < 4; i++ )
			{
				p_m->unused[0]->i_luma_scale[j][i] = p_aec->i_luma_scale[j][i];
				p_m->unused[0]->i_luma_shift[j][i] = p_aec->i_luma_shift[j][i];
				p_m->unused[0]->i_chroma_scale[j][i] = p_aec->i_chroma_scale[j][i];
				p_m->unused[0]->i_chroma_shift[j][i] = p_aec->i_chroma_shift[j][i];
			}
		}
	}
#endif	
#endif

#else
    /* decode slice */
    while( field->slice_num--)
    {
        cur_startcode = field->slice[slice_count].i_startcode;
        p_buf = field->slice[slice_count].p_start;
        i_len = field->slice[slice_count].i_len;
        slice_count++;

        switch( cur_startcode )
        {
            case XAVS_USER_DATA_CODE:
                cavs_bitstream_init(&p->s,p_buf,i_len);
                i_result = cavs_get_user_data(&p->s, &p->user_data);
                break;

            case XAVS_EXTENSION_START_CODE:
                cavs_bitstream_init(&p->s,p_buf,i_len);
                i_result = cavs_get_extension(&p->s, p);
                break;

            default:

                if( cur_startcode >= XAVS_SLICE_MIN_START_CODE 
                    && cur_startcode <= XAVS_SLICE_MAX_START_CODE )
                {
                    if ( 0 == p->param.i_thread_model )
                    {
                        int i_next_mb_y;
                        uint32_t i_mb_y_ori;

                        cavs_bitstream_init(&p->s, p_buf, i_len);                        
                        p->i_mb_y = cur_startcode & 0x000000FF;                       
                        i_mb_y_ori = p->i_mb_y;
                        
                        /* set b_bottom *///FIXIT: wrong for this place
                        p->b_bottom = 0;
                        if( p->i_mb_y >= (p->i_mb_height>>1))
                        {
                            p->b_bottom = 1;
                            cavs_init_picture(p);
                        }
                        p->i_mb_y = i_mb_y_ori;
                        
                        if( field->slice_num != 0 )
                            i_next_mb_y = field->slice[slice_count].i_startcode & 0x000000FF; /*slice_count++ before*/
                        else
                            i_next_mb_y = 0xFF;
                        if (i_next_mb_y>=0 && i_next_mb_y<=0xAF)
                            p->i_slice_mb_end = i_next_mb_y * p->i_mb_width;
                        else
                        {
                            p->i_slice_mb_end = (p->i_mb_num>>(!p->b_bottom));
                        }
                        
                        if( cavs_get_slice(p) != 0 )
                        {
                            printf("[error] decoding slice is wrong\n");
                            p->b_complete = 1; /* set for pic exchange */
                        }

                        if(p->b_complete && p->ph.i_picture_coding_type != XAVS_B_PICTURE)
                        {
                            xavs_image *p_image = p->p_save[0];
                            if(p->vsh.b_low_delay)
                            {
                                i_result = CAVS_FRAME_OUT;

                                /* set for ffmpeg interface */
                                param->output_type =  p->p_save[0]->i_code_type;
                                 
                                cavs_out_image_yuv420(p, p->p_save[0], param->p_out_yuv, p->b_bottom);
                            }
                            if(!p->p_save[1])
                            {
                                if( p->b_bottom )
                                    p->p_save[0] = &p->image[1];
                            }
                            else
                            {
                                if(p->vsh.b_low_delay == 0 /*&& p->ph.i_picture_coding_type != XAVS_I_PICTURE*/ )
                                {
                                    i_result = CAVS_FRAME_OUT;

                                    /* set for ffmpeg interface */
                                    param->output_type = p->p_save[1]->i_code_type;
                                      
                                    cavs_out_image_yuv420(p, p->p_save[1], param->p_out_yuv, p->b_bottom);
                                }

                                if( p->b_bottom )
                                {
                                    if(!p->p_save[2])
                                    {
                                        p->p_save[0] = &p->image[2];	 
                                    }
                                    else
                                    {
                                        p->p_save[0] = p->p_save[2];
                                    }    
                                }
                            }

                            if( p->b_bottom )
                            {
                                p->p_save[2] = p->p_save[1];
                                p->p_save[1] = p_image;
                                memset(&p->cur, 0, sizeof(xavs_image));
                            }
                            p->b_complete = 0;
                            p->b_bottom = 0;
                        }
                        else if( p->b_complete )
                        {
                            i_result = CAVS_FRAME_OUT;

                            /* set for ffmpeg interface */
                            param->output_type =  p->p_save[0]->i_code_type;
                             
                            cavs_out_image_yuv420(p, p->p_save[0], param->p_out_yuv, p->b_bottom );
                            memset(&p->cur,0,sizeof(xavs_image));
                            p->b_complete=0;
                            p->b_bottom = 0;
                        }	
                    } 
                    else if( 1 == p->param.i_thread_model )
                    {
                        if ( XAVS_THREAD_MAX >= sliceNum )
                        {
                            int i_next_mb_y;
                            int i_mb_y_ori;

                            /* sync context of handle */
                            cavs_slice_thread_sync_context( p->thread[threadNum], p );
                            //if( threadNum !=0 ) /* NOTE : need to init new handle */
                            //{
                            //	cavs_init_picture(p->thread[threadNum]);
                            //}
                            cavs_bitstream_init(&p->thread[threadNum]->s, p_buf, i_len);
                            p->thread[threadNum]->i_mb_y = cur_startcode & 0x000000FF;
                            i_mb_y_ori = p->thread[threadNum]->i_mb_y;
                            
                            /* set b_bottom */
                            p->thread[threadNum]->b_bottom = 0;
                            if( p->thread[threadNum]->i_mb_y >= (p->i_mb_height>>1))
                            {
                                p->thread[threadNum]->b_bottom = 1;
                                cavs_init_picture(p->thread[threadNum]);
                                p->thread[threadNum]->i_mb_y = i_mb_y_ori;
                            }
                            
                            if( field->slice_num != 0 )
                                i_next_mb_y = field->slice[slice_count].i_startcode & 0x000000FF; /*slice_count++ before*/
                            else
                                i_next_mb_y = 0xFF;
                            if (i_next_mb_y>=0 && i_next_mb_y<=0xAF)
                                p->thread[threadNum]->i_slice_mb_end = i_next_mb_y * p->thread[threadNum]->i_mb_width;
                            else
                            {
                                p->thread[threadNum]->i_slice_mb_end = (p->thread[threadNum]->i_mb_num>>(!p->thread[threadNum]->b_bottom));
                            }

#if TEST_POOL
                            cavs_threadpool_run( p->threadpool, (void*)cavs_get_slice, p->thread[threadNum] );
                            p->thread[threadNum]->b_thread_active = 1;
#else
                            ret = cavs_pthread_create( &p->thread[threadNum]->id, NULL, (void*)cavs_get_slice, p->thread[threadNum] );                            
                            if ( ret == 0 )
                            {
                                p->thread[threadNum]->b_thread_active = 1;
                            }
#endif

                            threadNum++;
                        }
                        else
                        {
                            printf("[error]threads exceed limit\n");

                            return CAVS_ERROR;
                        }
                                    
                        if ( threadNum == (sliceNum>>1) )/* top and bottom */
                        {
                            for ( i = 0; i < threadNum; i++ )
                            {
                                if( p->thread[i]->b_thread_active )
                                {  
#if TEST_POOL
                                    if(cavs_threadpool_wait( p->threadpool, p->thread[i]))
                		                ;//return -1;
#else
                                    cavs_pthread_join(p->thread[i]->id, NULL);
#endif                                    
                                    p->thread[i]->b_thread_active = 0;
                                    if( p->thread[i]->b_error_flag )
                                    {
                                        p->b_error_flag = 1;
                                        p->thread[threadNum - 1]->b_complete = 1;
                                        printf("[error] decoding slice is wrong\n");
                                    }
                                }
                            }

                            if( p->thread[threadNum - 1]->b_complete && p->ph.i_picture_coding_type != XAVS_B_PICTURE )
                            {
                                xavs_image *p_image = p->p_save[0];
                                
                                if( p->vsh.b_low_delay ) /* */
                                {
                                    i_result = CAVS_FRAME_OUT;

                                    /* set for ffmpeg interface */
                                    param->output_type = p->p_save[0]->i_code_type;
                                     
                                    cavs_out_image_yuv420( p, p->p_save[0], param->p_out_yuv, p->thread[threadNum]->b_bottom );
                                }
                                
                                if( !p->p_save[1] )
                                {
                                    if( p->thread[threadNum - 1]->b_bottom )
                                        p->p_save[0] = &p->image[1];
                                }
                                else
                                {
                                    if( p->vsh.b_low_delay == 0 /*&& p->ph.i_picture_coding_type != XAVS_I_PICTURE*/ )
                                    {
                                        i_result = CAVS_FRAME_OUT;

                                         /* set for ffmpeg interface */
                                        param->output_type = p->p_save[1]->i_code_type;
                                          
                                        cavs_out_image_yuv420( p, p->p_save[1], param->p_out_yuv, p->thread[threadNum]->b_bottom );
                                    }

                                    if( p->thread[threadNum - 1]->b_bottom )
                                    {
                                        if( !p->p_save[2] )
                                        {
                                            p->p_save[0] = &p->image[2];	 
                                        }
                                        else
                                        {
                                            p->p_save[0] = p->p_save[2];
                                        }        
                                    }
                                }

                                if( p->thread[threadNum - 1]->b_bottom )
                                {
                                    p->p_save[2] = p->p_save[1];
                                    p->p_save[1] = p_image; /* p->p_save[0] to p->p_save[1] */
                                    memset( &p->cur, 0, sizeof(xavs_image) );    
                                }
                                
                                p->thread[threadNum - 1]->b_complete = 0;
                                p->thread[threadNum - 1]->b_bottom = 0;
                            }
                            else /* output B frame of reconstructed */
                            {
                                i_result = CAVS_FRAME_OUT;

                                /* set for ffmpeg interface */
                                param->output_type =  p->p_save[0]->i_code_type;
                                 
                                cavs_out_image_yuv420( p, p->p_save[0], param->p_out_yuv, p->thread[threadNum]->b_bottom );
                                memset( &p->cur, 0, sizeof(xavs_image) );                              
                                p->thread[threadNum - 1]->b_complete = 0;
                                p->thread[threadNum - 1]->b_bottom = 0;
                            }	

                            threadNum = 0; /* reset for bottom-field */
                        }
                    }				
                }
                else
                {
                    return CAVS_ERROR;
                }
        }
    }
#endif

    if( p->b_error_flag )
    {
        i_result = CAVS_ERROR;
    }

    return i_result;
}
#endif

#if FAR_ACCELERATE

int field_decode_aec_threads_far( cavs_decoder *p_aec )
{
    cavs_decoder *p_m = p_aec->p_m;
    frame_pack *field = p_aec->fld;
    uint8_t   *p_buf;
    int    i_len, i_result = 0;
    int slice_count = p_aec->slice_count;
    int field_count = p_aec->field_count;
    uint32_t   cur_startcode;
    uint32_t i_ret = 0;

    /* pack slice */
    cavs_field_pack( p_aec->p_stream, field );

    cur_startcode = field->slice[0].i_startcode;
    p_buf = field->slice[0].p_start;
    i_len = field->slice[0].i_len;
    field->slice_num--;

    /* decode header of pic */
    switch( cur_startcode )
    {
        case XAVS_I_PICUTRE_START_CODE:
            cavs_bitstream_init(&p_aec->s, p_buf, i_len);
            cavs_get_i_picture_header(&p_aec->s,&p_aec->ph,&p_aec->vsh);
			
			if( p_aec->vsh.i_profile_id == PROFILE_GUANGDIAN )
			{
				//p_aec->ph.b_aec_enable = 1; /* AEC only for AVS+ */
				p_aec->ph.b_aec_enable = p_m->vsh.b_aec_enable;
			}
			else if(p_aec->vsh.i_profile_id == PROFILE_JIZHUN)
			{
				p_aec->ph.b_aec_enable = 0; /* AVS */
			}
			else
			{
				printf("[error]wrong profile id\n");
				p_aec->b_error_flag  = 1;

                return CAVS_ERROR;
			}

			p_aec->ph.b_picture_structure = 0; /* NOTE : guarantee field or frame not change */
#if THREADS_OPT
            i_ret = cavs_init_picture_context(p_aec);
            if( i_ret == CAVS_ERROR )
            {
                p_aec->b_error_flag  = 1;
                return CAVS_ERROR;
            }
            cavs_init_picture_ref_list_for_aec(p_aec);
#else
            cavs_init_picture(p_aec);
#endif

#if !THREADS_OPT
            /* add for no-seq header before I-frame */
            p_aec->last_delayed_pframe = p_aec->p_save[1];
#endif
            break;

        case XAVS_PB_PICUTRE_START_CODE:
            cavs_bitstream_init(&p_aec->s, p_buf, i_len);
            cavs_get_pb_picture_header(&p_aec->s, &p_aec->ph, &p_aec->vsh);

			if( p_aec->vsh.i_profile_id == PROFILE_GUANGDIAN )
			{
				//p_aec->ph.b_aec_enable = 1; /* AEC only for AVS+ */
				p_aec->ph.b_aec_enable = p_m->vsh.b_aec_enable;
			}
			else if(p_aec->vsh.i_profile_id == PROFILE_JIZHUN)
			{
				p_aec->ph.b_aec_enable = 0; /* AVS */
			}
			else
			{
				printf("[error]wrong profile id\n");
				p_aec->b_error_flag  = 1;

                return CAVS_ERROR;
			}
            p_aec->ph.b_picture_structure = 0; /* NOTE : guarantee field not change */

#if THREADS_OPT
            i_ret = cavs_init_picture_context(p_aec);
            if( i_ret == CAVS_ERROR )
            {
                p_aec->b_error_flag  = 1;
                return CAVS_ERROR;
            }
            cavs_init_picture_ref_list_for_aec(p_aec);
#else
            cavs_init_picture(p_aec);
#endif

            break;

        default:
            return CAVS_ERROR;
    }	
    
    /*add for weighting quant */
    cavs_init_frame_quant_param( p_aec );

    cavs_frame_update_wq_matrix( p_aec );
    while( field->slice_num--)
    {
        cur_startcode = field->slice[slice_count].i_startcode;
        p_buf = field->slice[slice_count].p_start;
        i_len = field->slice[slice_count].i_len;
        slice_count++;
        field_count++;
        
        switch( cur_startcode )
        {
            case XAVS_USER_DATA_CODE:
                cavs_bitstream_init(&p_aec->s,p_buf,i_len);
                i_result = cavs_get_user_data(&p_aec->s, &p_aec->user_data);
                break;

            case XAVS_EXTENSION_START_CODE:
                cavs_bitstream_init(&p_aec->s,p_buf,i_len);
                i_result = cavs_get_extension(&p_aec->s, p_aec);
                break;

            default:

                if( cur_startcode >= XAVS_SLICE_MIN_START_CODE 
                    && cur_startcode <= XAVS_SLICE_MAX_START_CODE )
                {
                    if ( 0 == p_aec->param.i_thread_model )
                    {
                        int i_next_mb_y;
                        uint32_t i_mb_y_ori = 0;

                        cavs_bitstream_init(&p_aec->s, p_buf, i_len);                        
                        p_aec->i_mb_y = cur_startcode & 0x000000FF;
                        i_mb_y_ori = p_aec->i_mb_y;                    
                        
                        /* set b_bottom */
                        p_aec->b_bottom = 0;
                        if( p_aec->i_mb_y >= (p_aec->i_mb_height>>1))
                        {
                            p_aec->b_bottom = 1;
                            
                            i_ret = cavs_init_picture_context(p_aec);
                            if( i_ret == CAVS_ERROR )
                            {
                                p_aec->b_error_flag  = 1;
                                return CAVS_ERROR;
                            }
                            cavs_init_picture_ref_list_for_aec(p_aec);
                        }
                        p_aec->i_mb_y = i_mb_y_ori;
             
                        if( field->slice_num != 0 )
                            i_next_mb_y = field->slice[slice_count].i_startcode & 0x000000FF; /*slice_count++ before*/
                        else
                            i_next_mb_y = 0xFF;/* bot-field */
                        if (i_next_mb_y>=0 && i_next_mb_y<=0xAF)
                            p_aec->i_slice_mb_end = i_next_mb_y * p_aec->i_mb_width;
                        else
                        {
                            p_aec->i_slice_mb_end = (p_aec->i_mb_num>>(!p_aec->b_bottom));
                        }
                        p_aec->i_slice_mb_end_fld[p_aec->b_bottom] =  p_aec->i_slice_mb_end;

                        cavs_get_slice_aec(p_aec);
                        if(p_aec->b_error_flag == 1)
                            return  CAVS_ERROR;

                        /* creat independent ref-list for AEC module */
                        if( p_aec->ph.i_picture_coding_type != XAVS_B_PICTURE )
                        {
                            xavs_image *p_image = p_aec->p_save_aec[0];

                            if(!p_aec->p_save_aec[1])
                            {
                                if( p_aec->b_bottom )
                                    p_aec->p_save_aec[0] = &p_aec->image_aec[1];
                            }
                            else
                            {
                                if( p_aec->b_bottom )
                                {
                                    if(!p_aec->p_save_aec[2])
                                    {
                                        p_aec->p_save_aec[0] = &p_aec->image_aec[2];	 
                                    }
                                    else
                                    {
                                        p_aec->p_save_aec[0] = p_aec->p_save_aec[2];
                                    }
                                }
                            }

                            if( p_aec->b_bottom )
                            {
                                p_aec->p_save_aec[2] = p_aec->p_save_aec[1];
                                p_aec->p_save_aec[1] = p_image;
                                memset(&p_aec->cur_aec, 0, sizeof(xavs_image));
                            }
                        }                        
                    } 
                    else if( 1 == p_aec->param.i_thread_model )
                    {
                        printf("[error] not support multi-slice!\n");
                        return  CAVS_ERROR;      
                    }
                }
                else
                {
					p_aec->b_error_flag = 1;
                    return CAVS_ERROR;
                }
        }
    }

    return 0;
}

int cavs_field_decode_far(void *p_decoder, frame_pack *field, cavs_param *param )
{
#if !THREADS_OPT
    cavs_decoder *p=(cavs_decoder *)p_decoder;
#else
    cavs_decoder *p = (cavs_decoder *)p_decoder;
    cavs_decoder *p_m = p;/* backup main threads */
    cavs_decoder *p_aec, *p_rec;
    int field_count = 0;
#endif
    
    int i_result = 0;
    int sliceNum, threadNum;
    int slice_count = 1; /* 0 is for pic header */
    
    sliceNum = field->slice_num - 1; /* exclude pic header */
    threadNum = 0;

#if 0
    cur_startcode = field->slice[0].i_startcode;
    p_buf = field->slice[0].p_start;
    i_len = field->slice[0].i_len;
    field->slice_num--;
#endif

#if THREADS_OPT
    /* init aec handle */
    p_aec = p_m->unused[0];
    p = p_aec;
    p_aec->b_bottom = 0; /* init bottom flag */
    p_aec->i_frame_decoded = p_m->i_frame_decoded; //just for debug
#endif    

#if 0
    /* decode header of pic */
    switch( cur_startcode )
    {
        case XAVS_I_PICUTRE_START_CODE:
            cavs_bitstream_init(&p->s, p_buf, i_len);
            cavs_get_i_picture_header(&p->s,&p->ph,&p->vsh);

#if THREADS_OPT
            cavs_init_picture_context(p);
            cavs_init_picture_ref_list_for_aec(p);
#else
            cavs_init_picture(p);
#endif

#if !THREADS_OPT
            /* add for no-seq header before I-frame */
            p->last_delayed_pframe = p->p_save[1];
#endif
            break;

        case XAVS_PB_PICUTRE_START_CODE:
            cavs_bitstream_init(&p->s, p_buf, i_len);
            cavs_get_pb_picture_header(&p->s,&p->ph,&p->vsh);
            
#if THREADS_OPT
            cavs_init_picture_context(p);
            cavs_init_picture_ref_list_for_aec(p);
#else
            cavs_init_picture(p);
#endif

            break;

        default:
            return CAVS_ERROR;
    }	
    
    /*add for weighting quant */
    cavs_init_frame_quant_param( p );

    cavs_frame_update_wq_matrix( p );
#endif    

    /* init p_aec*/
    p_aec->p_m = p_m;
    p_aec->fld = field;
    p_aec->slice_count = slice_count;
    p_aec->field_count = field_count; 

#if TEST_POOL
    cavs_threadpool_run( p_m->threadpool, (void*)field_decode_aec_threads_far, p_aec );
    p_aec->b_thread_active = 1;
#else
    int ret = cavs_pthread_create( &p_aec->id, NULL, (void*)field_decode_aec_threads_far, p_aec );
    if ( ret == 0 )
    {
        p_aec->b_thread_active = 1;
    }
    else
    {
        printf("creat field multi-thread of AEC failed!\n");
        return 0;
    }
#endif

    /* switch to main threads */
    //p_m->unused[1]->p_save_aec[0] = p_aec->p_save_aec[0]; /* will crash,why?*/
    //p_m->unused[1]->p_save_aec[1] = p_aec->p_save_aec[1];
    //p_m->unused[1]->p_save_aec[2] = p_aec->p_save_aec[2];
    p_m->unused[0] = p_m->unused[1];
    p_m->unused[1] = p_aec;
    
    p_m->current[0] = p_m->current[1];
    p_m->current[1] = p_aec;

    /* rec decode loop */
    p_rec = p_m->current[0];
    field_count =  2; /* only support two field */
    if( p_rec != NULL )
    {
        while( field_count-- )
        {
            p_rec->b_bottom = !field_count; /* first-top second-botttom */  

            cavs_init_picture_context_fld( p_rec );
            i_result = cavs_get_slice_rec_threads( p_rec );
            if( i_result == -1 )
            {
                /* close AEC at same time */
                if( p_aec->b_thread_active == 1 )
                {
#if TEST_POOL
                	if(cavs_threadpool_wait( p_m->threadpool, p_aec ) )
                		;//return -1;

#else
                    cavs_pthread_join(p_aec->id, NULL);
#endif                    
                    p_aec->b_thread_active = 0;
                }
                
                return CAVS_ERROR;
            }
            
            if( p_rec->b_bottom )
            {
                if( p_rec->ph.i_picture_coding_type != XAVS_B_PICTURE )
                    p_m->last_delayed_pframe = p_rec->p_save[1];
            }
            
            if(p_rec->b_complete && p_rec->ph.i_picture_coding_type != XAVS_B_PICTURE)
            {
                xavs_image *p_image = p_rec->p_save[0];
                if(p_rec->vsh.b_low_delay)
                {
                    i_result = CAVS_FRAME_OUT;

                    /* set for ffmpeg interface */
                    param->output_type = p_rec->p_save[0]->i_code_type;
                    
                    cavs_out_image_yuv420(p_m, p_rec->p_save[0], param->p_out_yuv,  p_rec->b_bottom);
                }
                if(!p_rec->p_save[1])
                {
                    if( p_rec->b_bottom )
                        p_rec->p_save[0] = &p_rec->image[1];
                }
                else
                {
                    if(p_rec->vsh.b_low_delay == 0 /*&& p_rec->ph.i_picture_coding_type != XAVS_I_PICTURE*/ )
                    {
                        i_result = CAVS_FRAME_OUT;

                        /* set for ffmpeg interface */
                        param->output_type = p_rec->p_save[1]->i_code_type;
                          
                        cavs_out_image_yuv420(p_m, p_rec->p_save[1], param->p_out_yuv,  p_rec->b_bottom);
                    }
                    else
                    {
                    	param->output_type = -1;
                    }

                    if( p_rec->b_bottom )
                    {
                        if(!p_rec->p_save[2])
                        {
                            p_rec->p_save[0] = &p_rec->image[2];	 
                        }
                        else
                        {
                            p_rec->p_save[0] = p_rec->p_save[2];
                        }    
                    }
                }

                if( p_rec->b_bottom )
                {
                    p_rec->p_save[2] = p_rec->p_save[1];
                    p_rec->p_save[1] = p_image;
                    memset(&p_rec->cur, 0, sizeof(xavs_image));

                    /* copy ref-list to anther threads */
                    p_m->current[1]->p_save[0] = p_rec->p_save[0];
                    p_m->current[1]->p_save[1] = p_rec->p_save[1];
                    p_m->current[1]->p_save[2] = p_rec->p_save[2];
                }
                p_rec->b_complete = 0;
                p_rec->b_bottom = 0;
            }
            else if( p_rec->b_complete )
            {
                i_result = CAVS_FRAME_OUT;

                /* set for ffmpeg interface */
                param->output_type =  p_rec->p_save[0]->i_code_type;
                 
                cavs_out_image_yuv420(p_m, p_rec->p_save[0], param->p_out_yuv,  p_rec->b_bottom);
                memset(&p_rec->cur,0,sizeof(xavs_image));
                p_rec->b_complete=0;
                p_rec->b_bottom = 0;
            }	                         
        }
    }

    /*wait for end of AEC handle */
    if( p_aec->b_thread_active == 1 )
    {  
#if TEST_POOL
	if(cavs_threadpool_wait( p_m->threadpool, p_aec ) )
        		;//return -1;
#else    
        cavs_pthread_join(p_aec->id, NULL);
#endif

        p_aec->b_thread_active = 0;
    }
    
    if( p_aec->b_error_flag )
    {
        return CAVS_ERROR;
    }
    
    p_m->unused[0]->p_save_aec[0] = p_aec->p_save_aec[0];
    p_m->unused[0]->p_save_aec[1] = p_aec->p_save_aec[1];
    p_m->unused[0]->p_save_aec[2] = p_aec->p_save_aec[2];

    /* copy col type to next handle */
    memcpy( p_m->unused[0]->p_col_mv, p_aec->p_col_mv,p_m->i_mb_width*(p_m->i_mb_height)*4*sizeof(xavs_vector));
    memcpy( p_m->unused[0]->p_col_type_base, p_aec->p_col_type_base, p_m->i_mb_width*(p_m->i_mb_height));

#if B_MB_WEIGHTING
	{
		int i, j;
		for( j = 0; j < 2; j++ )
		{
            p_m->unused[0]->b_slice_weighting_flag[j] = p_aec->b_slice_weighting_flag[j];
			p_m->unused[0]->b_mb_weighting_flag[j] = p_aec->b_mb_weighting_flag[j];
			for( i = 0; i < 4; i++ )
			{
				p_m->unused[0]->i_luma_scale[j][i] = p_aec->i_luma_scale[j][i];
				p_m->unused[0]->i_luma_shift[j][i] = p_aec->i_luma_shift[j][i];
				p_m->unused[0]->i_chroma_scale[j][i] = p_aec->i_chroma_scale[j][i];
				p_m->unused[0]->i_chroma_shift[j][i] = p_aec->i_chroma_shift[j][i];
			}
		}
	}
#endif

    if( p->b_error_flag )
    {
        i_result = CAVS_ERROR;
    }

    return i_result;
}

#endif

int cavs_decoder_create(void **pp_decoder, cavs_param *param)
{
    cavs_decoder *p;
    int counter = 0;
	int loop_cnt = 0;
    unsigned int i_feature_value = 0;
    int i_result = 0;

    p = (cavs_decoder *)malloc(sizeof(cavs_decoder));
    if( !p )
    {
        return -1;
    }
    memset( p, 0, sizeof(cavs_decoder) );
    memcpy (&p->param, param, sizeof (cavs_param));
    p->i_video_edit_code_flag=0;
    *pp_decoder = p;

    p->param.b_accelerate = param->b_accelerate = 1; 
    p->param.output_type = param->output_type = -1;

    cavs_decoder_init( p );

    p->p_stream = (InputStream*)malloc(sizeof(InputStream));;
	
    /* threading */
    /* use threadpool to optimize */

    return 0;
    
}



int cavs_decoder_get_seq(void *p_decoder, cavs_seq_info *p_si)
{
    cavs_decoder *p=(cavs_decoder *)p_decoder;

    if(!p)
    {
    	return -1;
    }

    if(!p->b_get_video_sequence_header)
    {
    	return -1;
    }
    
    p_si->lWidth=p->vsh.i_horizontal_size;
    p_si->lHeight=p->vsh.i_vertical_size;

    p_si->i_frame_rate_den = frame_rate_tab[p->vsh.i_frame_rate_code][0];
    p_si->i_frame_rate_num = frame_rate_tab[p->vsh.i_frame_rate_code][1];
    p_si->b_interlaced = !p->vsh.b_progressive_sequence;

    p_si->profile = p->vsh.i_profile_id;
    p_si->level = p->vsh.i_level_id;
    p_si->aspect_ratio = p->vsh.i_aspect_ratio;
    p_si->low_delay = p->vsh.b_low_delay;
    p_si->frame_rate_code = p->vsh.i_frame_rate_code;
    
    return 0;
}

int cavs_decoder_buffer_init(cavs_param *param)
{
    int w = param->seqsize.lWidth;
    int h = param->seqsize.lHeight;
    
    param->p_out_yuv[0]=(unsigned char *)cavs_malloc(w*h*3/2);
    param->p_out_yuv[1] = param->p_out_yuv[0] + w*h;
    param->p_out_yuv[2] = param->p_out_yuv[1] + w*h/4;
    
    if ( param->p_out_yuv[0] == NULL )
    	return -1;
    return 0;
}

int cavs_decoder_buffer_end(cavs_param *param)
{
    cavs_free(param->p_out_yuv[0]);
    
    return 0;
}

void cavs_decoder_destroy( void *p_decoder )
{
    cavs_decoder *p=(cavs_decoder *)p_decoder;
    if(!p)
    {
    	return ;
    }

#if TEST_POOL
    if( p->param.i_thread_num >= 1 )
	{
        cavs_threadpool_delete( p->threadpool );		
	}
#endif

#if B_MB_WEIGHTING

	if(p->mb_line_buffer[0])
    {
    	p->mb_line_buffer[0] -= p->image[0].i_stride[0]*XAVS_EDGE+XAVS_EDGE;
    	XAVS_SAFE_FREE(p->mb_line_buffer[0]);
    }
    	
    if(p->mb_line_buffer[1])
    {
    	p->mb_line_buffer[1] -= p->image[0].i_stride[1]*XAVS_EDGE/2+XAVS_EDGE/2;
    	XAVS_SAFE_FREE(p->mb_line_buffer[1]);
    }
    	
    if(p->mb_line_buffer[2])
    {
    	p->mb_line_buffer[2] -= p->image[0].i_stride[2]*XAVS_EDGE/2+XAVS_EDGE/2;
    	XAVS_SAFE_FREE(p->mb_line_buffer[2]);	
    }
#endif

    if( p->param.i_thread_num > 1)
    {
         int k;
         for( k = p->param.i_thread_num - 1; k >= 0 && p->thread[k] != NULL ; k-- )
         {
            /* destroy threads */
            cavs_pthread_mutex_destroy( &p->thread[k]->mutex );
            cavs_pthread_cond_destroy( &p->thread[k]->cv );
            
            cavs_free_resource( p->thread[k] );

            if( k == 0) /* NOTE : only free once */
            {
                int i;
                for( i = 0; i < 3; i++ )
                {
                    if( p->image[i].p_data[0] )
                    {
                        p->image[i].p_data[0] -= p->image[i].i_stride[0]*XAVS_EDGE+XAVS_EDGE;
                        XAVS_SAFE_FREE(p->image[i].p_data[0]);
                    }

                    if( p->image[i].p_data[1] )
                    {
                        p->image[i].p_data[1] -= p->image[i].i_stride[1]*XAVS_EDGE/2+XAVS_EDGE/2;
                        XAVS_SAFE_FREE(p->image[i].p_data[1]);
                    }

                    if( p->image[i].p_data[2] )
                    {
                        p->image[i].p_data[2] -= p->image[i].i_stride[2]*XAVS_EDGE/2+XAVS_EDGE/2;
                        XAVS_SAFE_FREE(p->image[i].p_data[2]);	
                    }

                    memset(&p->image[i],0,sizeof(xavs_image));
                }
            }
			else
				XAVS_SAFE_FREE(p->thread[k]);

         }  
    }
#if THREADS_OPT
    /* FIXIT : free threads of AEC and REC used */
    else if( p->param.b_accelerate )
    {
         int k;
         for( k = 1; k >= 0 && p->unused_backup[k] != NULL ; k-- )
         {
            /* destroy threads */
            cavs_pthread_mutex_destroy( &p->unused_backup[k]->mutex );
            cavs_pthread_cond_destroy( &p->unused_backup[k]->cv );
    
            cavs_free_resource( p->unused_backup[k] );

            if( k == 0) /* NOTE : only free once */
            {
                int i;
                for( i = 0; i < 3; i++ )
                {
                    if( p->image[i].p_data[0] )
                    {
                        p->image[i].p_data[0] -= p->image[i].i_stride[0]*XAVS_EDGE+XAVS_EDGE;
                        XAVS_SAFE_FREE(p->image[i].p_data[0]);
                    }

                    if( p->image[i].p_data[1] )
                    {
                        p->image[i].p_data[1] -= p->image[i].i_stride[1]*XAVS_EDGE/2+XAVS_EDGE/2;
                        XAVS_SAFE_FREE(p->image[i].p_data[1]);
                    }

                    if( p->image[i].p_data[2] )
                    {
                        p->image[i].p_data[2] -= p->image[i].i_stride[2]*XAVS_EDGE/2+XAVS_EDGE/2;
                        XAVS_SAFE_FREE(p->image[i].p_data[2]);	
                    }

                    memset(&p->image[i],0,sizeof(xavs_image));

                }

                /* free image_aec */
                {
                    for( i = 0; i < 3; i++ )
                    {
                    	if(p->image_aec[i].p_data[0])
                    	{
                    		p->image_aec[i].p_data[0]-=p->image_aec[i].i_stride[0]*XAVS_EDGE+XAVS_EDGE;
                    		XAVS_SAFE_FREE(p->image_aec[i].p_data[0]);
                    	}

                    	if(p->image_aec[i].p_data[1])
                    	{
                    		p->image_aec[i].p_data[1]-=p->image_aec[i].i_stride[1]*XAVS_EDGE/2+XAVS_EDGE/2;
                    		XAVS_SAFE_FREE(p->image_aec[i].p_data[1]);
                    	}

                    	if(p->image_aec[i].p_data[2])
                    	{
                    		p->image_aec[i].p_data[2]-=p->image_aec[i].i_stride[2]*XAVS_EDGE/2+XAVS_EDGE/2;
                    		XAVS_SAFE_FREE(p->image_aec[i].p_data[2]);	
                    	}

                    	memset(&p->image_aec[i], 0, sizeof(xavs_image));
                    }
                }
            }
			else
				XAVS_SAFE_FREE(p->unused_backup[k]);
         }  
    }
#endif
	else
		cavs_free_resource(p);

    free( p->p_stream );
    free( p );
}

void cavs_decoder_slice_destroy( void *p_decoder )
{
    cavs_decoder *p=(cavs_decoder *)p_decoder;
    if(!p)
    {
    	return ;
    }

#if TEST_POOL
    if( p->param.i_thread_num >= 1 )
        cavs_threadpool_delete( p->threadpool );
#endif

    if( p->param.i_thread_num > 1)
    {
         int k;
         for( k = p->param.i_thread_num - 1; k >= 0; k-- )
         {
            /* destroy threads */
            cavs_pthread_mutex_destroy( &p->thread[k]->mutex );
            cavs_pthread_cond_destroy( &p->thread[k]->cv );
            
            cavs_free_resource( p->thread[k] );

            if( k == 0) /* NOTE : only free once */
            {
                int i;
                for( i = 0; i < 3; i++ )
                {
                    if( p->image[i].p_data[0] )
                    {
                        p->image[i].p_data[0] -= p->image[i].i_stride[0]*XAVS_EDGE+XAVS_EDGE;
                        XAVS_SAFE_FREE(p->image[i].p_data[0]);
                    }

                    if( p->image[i].p_data[1] )
                    {
                        p->image[i].p_data[1] -= p->image[i].i_stride[1]*XAVS_EDGE/2+XAVS_EDGE/2;
                        XAVS_SAFE_FREE(p->image[i].p_data[1]);
                    }

                    if( p->image[i].p_data[2] )
                    {
                        p->image[i].p_data[2] -= p->image[i].i_stride[2]*XAVS_EDGE/2+XAVS_EDGE/2;
                        XAVS_SAFE_FREE(p->image[i].p_data[2]);	
                    }

                    memset(&p->image[i],0,sizeof(xavs_image));
                }
            }
			else
				XAVS_SAFE_FREE(p->thread[k]);
         }  
    }
#if THREADS_OPT
    /* FIXIT : free threads of AEC and REC used */
    else if( p->param.b_accelerate )
    {
         int k;
         for( k = 1; k >= 0; k-- )
         {
            /* destroy threads */
            cavs_pthread_mutex_destroy( &p->unused_backup[k]->mutex );
            cavs_pthread_cond_destroy( &p->unused_backup[k]->cv );
    
            cavs_free_resource( p->unused_backup[k] );

            if( k == 0) /* NOTE : only free once */
            {
                int i;
                for( i = 0; i < 3; i++ )
                {
                    if( p->image[i].p_data[0] )
                    {
                        p->image[i].p_data[0] -= p->image[i].i_stride[0]*XAVS_EDGE+XAVS_EDGE;
                        XAVS_SAFE_FREE(p->image[i].p_data[0]);
                    }

                    if( p->image[i].p_data[1] )
                    {
                        p->image[i].p_data[1] -= p->image[i].i_stride[1]*XAVS_EDGE/2+XAVS_EDGE/2;
                        XAVS_SAFE_FREE(p->image[i].p_data[1]);
                    }

                    if( p->image[i].p_data[2] )
                    {
                        p->image[i].p_data[2] -= p->image[i].i_stride[2]*XAVS_EDGE/2+XAVS_EDGE/2;
                        XAVS_SAFE_FREE(p->image[i].p_data[2]);	
                    }

                    memset(&p->image[i],0,sizeof(xavs_image));

                }

                /* free image_aec */
                {
                    for( i = 0; i < 3; i++ )
                    {
                    	if(p->image_aec[i].p_data[0])
                    	{
                    		p->image_aec[i].p_data[0]-=p->image_aec[i].i_stride[0]*XAVS_EDGE+XAVS_EDGE;
                    		XAVS_SAFE_FREE(p->image_aec[i].p_data[0]);
                    	}

                    	if(p->image_aec[i].p_data[1])
                    	{
                    		p->image_aec[i].p_data[1]-=p->image_aec[i].i_stride[1]*XAVS_EDGE/2+XAVS_EDGE/2;
                    		XAVS_SAFE_FREE(p->image_aec[i].p_data[1]);
                    	}

                    	if(p->image_aec[i].p_data[2])
                    	{
                    		p->image_aec[i].p_data[2]-=p->image_aec[i].i_stride[2]*XAVS_EDGE/2+XAVS_EDGE/2;
                    		XAVS_SAFE_FREE(p->image_aec[i].p_data[2]);	
                    	}

                    	memset(&p->image_aec[i], 0, sizeof(xavs_image));
                    }
                }
            }
			else
				XAVS_SAFE_FREE(p->unused_backup[k]);
         }  
    }
#endif
	else
		cavs_free_resource(p);

    //free(p);
}


int cavs_decoder_reset(void *p_decoder)
{
    cavs_decoder *p = (cavs_decoder *)p_decoder;

    if( !p )
    {
        return -1;
    }
    p->i_video_edit_code_flag = 0;
    p->b_get_i_picture_header = 0;
    
    if( p->param.b_accelerate )
    {
        memset(&p->ref_aec[0], 0, sizeof(xavs_image));
        memset(&p->ref_aec[1], 0, sizeof(xavs_image));
        memset(&p->ref_aec[2], 0, sizeof(xavs_image));
        memset(&p->ref_aec[3], 0, sizeof(xavs_image));
        p->image_aec[0].i_distance_index = 0;
        p->image_aec[1].i_distance_index = 0;
        p->image_aec[2].i_distance_index = 0;

        p->p_save_aec[0] = &p->image_aec[0];
        //p->last_delayed_pframe = p->p_save[1];
        p->p_save_aec[1] = 0; // FIXIT : can't shut down this line for open gop
        p->p_save_aec[2] = 0;
    }   

    memset(&p->ref[0], 0, sizeof(xavs_image));
    memset(&p->ref[1], 0, sizeof(xavs_image));
    memset(&p->ref[2], 0, sizeof(xavs_image));
    memset(&p->ref[3], 0, sizeof(xavs_image));
    p->image[0].i_distance_index = 0;
    p->image[1].i_distance_index = 0;
    p->image[2].i_distance_index = 0;

    p->p_save[0] = &p->image[0];
    p->last_delayed_pframe = p->p_save[1];
    p->p_save[1] = 0; // FIXIT : can't shut down this line for open gop
    p->p_save[2] = 0;

    memset(p->p_block, 0, 64*sizeof(DCTELEM));

    return 0;
}

int cavs_out_delay_frame(void *p_decoder, uint8_t *p_out_yuv[3])
{
    cavs_decoder * p = (cavs_decoder *)p_decoder;
    int b_interlaced = p->param.b_interlaced;

	 /* B frame exists */
    if ( !p->b_low_delay 
		&& ((cavs_decoder *)p_decoder)->last_delayed_pframe /* delay frame exists */
		)
    {
        if( !b_interlaced )
            cavs_out_image_yuv420((cavs_decoder *)p_decoder, ((cavs_decoder *)p_decoder)->last_delayed_pframe, p_out_yuv, 0);
        else
        {
            //p->b_bottom = 0;
            cavs_out_image_yuv420((cavs_decoder *)p_decoder, ((cavs_decoder *)p_decoder)->last_delayed_pframe, p_out_yuv,  0);
            //p->b_bottom = 1;
            cavs_out_image_yuv420((cavs_decoder *)p_decoder, ((cavs_decoder *)p_decoder)->last_delayed_pframe, p_out_yuv, 1);
        }

        /* reset for next delay frame */
        ((cavs_decoder *)p_decoder)->last_delayed_pframe = NULL;

        return 1;
    }

    return 0;
}

int cavs_set_last_delay_frame( void *p_decoder )
{
    cavs_decoder * p = (cavs_decoder *)p_decoder;
    
#if THREADS_OPT
    p->last_delayed_pframe = p->p_save[1];
#endif
    
    return 0;
}

int cavs_out_delay_frame_end(void *p_decoder, unsigned char *p_out_yuv[3])
{
    cavs_decoder * p = (cavs_decoder *)p_decoder;
    int b_interlaced = p->param.b_interlaced;
    
	if ( !/*p->vsh.b_low_delay*/p->b_low_delay ) /* B frame exists */
    {
        p->last_delayed_pframe = p->p_save[1];

		if(p->last_delayed_pframe == NULL)
		{
			return 0;
		}

        if( !b_interlaced )
            cavs_out_image_yuv420((cavs_decoder *)p_decoder, ((cavs_decoder *)p_decoder)->last_delayed_pframe, p_out_yuv, 0);
        else
        {
            //p->b_bottom = 0;
            cavs_out_image_yuv420((cavs_decoder *)p_decoder, ((cavs_decoder *)p_decoder)->last_delayed_pframe, p_out_yuv, 0);
            //p->b_bottom = 1;
            cavs_out_image_yuv420((cavs_decoder *)p_decoder, ((cavs_decoder *)p_decoder)->last_delayed_pframe, p_out_yuv, 1);
        }

        return 1;
    }

    return 0;
}

int cavs_decoder_seq_init( void *p_decoder , cavs_param *param )
{
    cavs_decoder *p = (cavs_decoder *)p_decoder;

#if 0  
    int b_interlaced = param->b_interlaced;

    if( !b_interlaced ) /* creat frame_pack */
    {
        /* creat frame pack */
        p->Frame = (frame_pack* )cavs_malloc(sizeof(frame_pack));
        p->Frame->p_start = (uint8_t* )cavs_malloc(MAX_CODED_FRAME_SIZE*sizeof(uint8_t));
        memset(p->Frame->p_start, 0, MAX_CODED_FRAME_SIZE*sizeof(unsigned char));    
    }
    else
    {
        /* creat field pack */
        p->Frame = (frame_pack* )cavs_malloc(sizeof(frame_pack));
        p->Frame->p_start = (uint8_t* )cavs_malloc(MAX_CODED_FRAME_SIZE*sizeof(uint8_t));
        memset(p->Frame->p_start, 0, MAX_CODED_FRAME_SIZE*sizeof(unsigned char));  

        param->fld_mb_end[0] = (param->seqsize.lHeight>>5) - 1; /* top field mb end */
        param->fld_mb_end[1] = (param->seqsize.lHeight>>4) - 1; /* bot field mb end */
    }
#else
    /* NOTE : we can't distinguish frame or field coding frame seq header */
    p->Frame = (frame_pack* )cavs_malloc(sizeof(frame_pack));
	memset(p->Frame, 0, sizeof(frame_pack));
    p->Frame->p_start = (uint8_t* )cavs_malloc(MAX_CODED_FRAME_SIZE*sizeof(uint8_t));
    //memset(p->Frame->p_start, 0, MAX_CODED_FRAME_SIZE*sizeof(unsigned char));  
	
    param->fld_mb_end[0] = (((param->seqsize.lHeight+31)& 0xffffffe0)>>5) - 1; /* top field mb end */
    param->fld_mb_end[1] = (((param->seqsize.lHeight+31)& 0xffffffe0)>>4) - 1; /* bot field mb end */

#endif

    return 0;
}

int cavs_decoder_seq_end( void *p_decoder )
{
    cavs_decoder *p = (cavs_decoder *)p_decoder;

	if(p && p->Frame)
	{
		if(p->Frame->p_start)
			cavs_free( p->Frame->p_start );
		cavs_free( p->Frame );
	}
    
    return 0;
}


int cavs_decode_one_frame( void *p_decoder, int i_startcode, cavs_param *param, uint8_t* buf, int length )
{
    cavs_decoder *p = (cavs_decoder *)p_decoder;
    InputStream *p_stream = p->p_stream;
    int b_interlaced = p->param.b_interlaced;
    int i_result;
	
    p->i_frame_decoded++;
    if( !b_interlaced ) /* decode one frame or one field */
    {
        /* init frame pack */
        //memset(p->Frame->p_start, 0, MAX_CODED_FRAME_SIZE*sizeof(unsigned char));

        /* init */
        p->Frame->p_cur = p->Frame->p_start;
        p->Frame->i_len = 0;
        p->Frame->slice_num = 0;
        p->Frame->i_startcode_type = i_startcode;
        memcpy(p->Frame->p_cur, buf, length ); /* pack header of pic into one frame */

        /* set pic header info, use position of slice_num = 0 */
        p->Frame->slice[p->Frame->slice_num].i_startcode = i_startcode;
        p->Frame->slice[p->Frame->slice_num].p_start = p->Frame->p_cur + 4; /* for skip current startcode */
        p->Frame->slice[p->Frame->slice_num].i_len = length-4;

        /* update frame info */
        p->Frame->p_cur = p->Frame->p_cur + length;
        p->Frame->i_len += length;
        p->Frame->slice_num++;

#if 0
#if FAR_ACCELERATE
        //i_startcode = cavs_frame_pack(p_stream, p->Frame );

        i_result = decode_one_frame_far( p_decoder, p->Frame, param );

#else
        i_startcode = cavs_frame_pack(p_stream, p->Frame );

        i_result = decode_one_frame( p_decoder, p->Frame, param );
#endif       

#else
        if( !p->param.b_accelerate )
        {
            i_startcode = cavs_frame_pack(p_stream, p->Frame );
            if(i_startcode == CAVS_ERROR)
                return CAVS_ERROR;

            i_result = decode_one_frame( p_decoder, p->Frame, param );
        }
        else
        {
#if FAR_ACCELERATE
             i_result = decode_one_frame_far( p_decoder, p->Frame, param );
#endif
        }
        
#endif
    }
    else
    {
        uint32_t frame_type = i_startcode;
        
        /* init field pack of top */
        //memset(p->Frame->p_start, 0, MAX_CODED_FRAME_SIZE*sizeof(unsigned char));
        p->Frame->b_bottom = 0;
        p->Frame->i_mb_end[0] = param->fld_mb_end[0];
        p->Frame->p_cur = p->Frame->p_start;
        p->Frame->i_len = 0;
        p->Frame->slice_num = 0;
        p->Frame->i_startcode_type = i_startcode;
        memcpy( p->Frame->p_cur, buf, length ); /* pack header of pic into one frame */

        /* set pic header info , use position of slice_num = 0 */
        p->Frame->slice[p->Frame->slice_num].i_startcode = i_startcode;
        p->Frame->slice[p->Frame->slice_num].p_start = p->Frame->p_cur + 4; /* for skip current startcode */
        p->Frame->slice[p->Frame->slice_num].i_len = length-4;
        
        /* update field info of top */
        p->Frame->p_cur = p->Frame->p_cur + length;
        p->Frame->i_len += length;
        p->Frame->slice_num++;

        if( p->param.b_accelerate ) /* entropy decoded parallel */
        {
#if THREADS_OPT

#if FAR_ACCELERATE
            p->Frame->i_mb_end[0] = param->fld_mb_end[0];
            p->Frame->i_mb_end[1] = param->fld_mb_end[1];
            p->b_bottom = 0;

            /* pack one image as two field */
            //i_startcode = cavs_field_pack(p_stream, p->Frame );

            /* decode one image as two field */
            i_result = cavs_field_decode_far( p_decoder, p->Frame, param );

#else     
            p->Frame->i_mb_end[0] = param->fld_mb_end[0];
            p->Frame->i_mb_end[1] = param->fld_mb_end[1];
            p->b_bottom = 0;

            /* pack one image as two field */
            i_startcode = cavs_field_pack(p_stream, p->Frame );

            /* decode one image as two field */
            i_result = cavs_field_decode( p_decoder, p->Frame, param );
#endif

#endif            
        }
        else
        {
            /* pack top-field */
            i_startcode = cavs_topfield_pack(p_stream, p->Frame );

            /* decode top-field */
            i_result = decode_top_field( p_decoder, p->Frame, param );               
            if( i_result == CAVS_ERROR )
            {
                return i_result;
            }

            /* init field pack of bot */
            /* NOTE : bot-field has no pic header */
            //memset(p->Frame->p_start, 0, MAX_CODED_FRAME_SIZE*sizeof(unsigned char));
            p->Frame->b_bottom = 1;
            p->Frame->i_mb_end[0] = param->fld_mb_end[0];
            p->Frame->i_mb_end[1] = param->fld_mb_end[1];
            p->Frame->p_cur = p->Frame->p_start;
            p->Frame->i_len = 0;
            p->Frame->slice_num = 0;
            p->Frame->i_startcode_type = frame_type;

            /* pack bot-field */
            i_startcode = cavs_botfield_pack(p_stream, p->Frame );

            /* decode bot-field */
            i_result = decode_bot_field( p_decoder, p->Frame, param );    
        }
    }

    return i_result;
}

int cavs_decoder_init_stream( void *p_decoder, unsigned char *rawstream, unsigned int i_len)
{
    cavs_decoder *p = (cavs_decoder *)p_decoder;

    cavs_init_bitstream( p->p_stream , rawstream, i_len );

    return 0;
}

int cavs_decoder_get_one_nal( void *p_decoder, unsigned char *buf, int *length, int buf_len)
{
    cavs_decoder *p = (cavs_decoder *)p_decoder;
    int i_ret;
    
	i_ret = cavs_get_one_nal(p->p_stream, buf, length, buf_len/*MAX_CODED_FRAME_SIZE*/);

    return i_ret;
}

int cavs_decoder_cur_frame_type( void* p_decoder )
{
    cavs_decoder *p = (cavs_decoder *)p_decoder;

    if( p->param.b_accelerate )
    {
        return p->unused[0]->ph.i_picture_coding_type;
    } 
    else
        return p->ph.i_picture_coding_type;
}

int cavs_decoder_thread_param_init( void* p_decoder )
{
    cavs_decoder *p = (cavs_decoder *)p_decoder;

    p->param.i_thread_model = 0; /* NOTE: don't modify */
    p->param.i_thread_num = 128;//64;

    return 0;    
}




int cavs_decoder_probe_seq(void *p_decoder,  unsigned char  *p_in, int i_in_length)
{
    cavs_decoder *p=(cavs_decoder *)p_decoder;
    uint32_t   i_startcode;
    uint8_t   *p_buf;
    int    i_len, i_result=0;

    p_buf = p_in;
    i_len = i_in_length;	

    i_startcode = ((p_buf[0]<<24))|((p_buf[1]<<16))|(p_buf[2]<<8)|p_buf[3];
    p_buf += 4; /* skip startcode */
    i_len -= 4;
    if(i_startcode==0)
    {
    	return CAVS_ERROR;
    }

    if(i_startcode!=XAVS_VIDEO_SEQUENCE_START_CODE&&!p->b_get_video_sequence_header)
    {
    	return  CAVS_ERROR;
    } 

    switch(i_startcode)
    {
    case XAVS_VIDEO_SEQUENCE_START_CODE:
    	cavs_bitstream_init(&p->s,p_buf,i_len);
    	if(cavs_get_video_sequence_header(&p->s,&p->vsh)==0)
    	{
            p->b_get_video_sequence_header = 1;

            return CAVS_SEQ_HEADER;
    	}
		else
		{
			return CAVS_ERROR;
		}

    	break;
    case XAVS_VIDEO_SEQUENCE_END_CODE:
    	p->last_delayed_pframe = p->p_save[1];
    	i_result = CAVS_SEQ_END;
    	break;
    case XAVS_USER_DATA_CODE:
    	cavs_bitstream_init(&p->s,p_buf,i_len);
    	i_result = cavs_get_user_data(&p->s, &p->user_data);
    	break;
    case XAVS_EXTENSION_START_CODE:
    	cavs_bitstream_init(&p->s,p_buf,i_len);
    	i_result = cavs_get_extension(&p->s, p);
    	break;
    case XAVS_VIDEO_EDIT_CODE:
    	p->i_video_edit_code_flag=0;
    	break;

    }

    return i_result;

//fail:
//    return -1;
}


int cavs_decoder_pic_header( void* p_decoder,  unsigned char  *p_buf,  int i_len, cavs_param* param, unsigned int cur_startcode )
{
    cavs_decoder *p = (cavs_decoder *)p_decoder;
    int i_ret = 0;

    /* decode header of pic */
    switch( cur_startcode )
    {
        case XAVS_I_PICUTRE_START_CODE:
            cavs_bitstream_init(&p->s, p_buf+4, i_len-4);
            cavs_get_i_picture_header(&p->s,&p->ph,&p->vsh);
            break;

        case XAVS_PB_PICUTRE_START_CODE:
            cavs_bitstream_init(&p->s, p_buf+4, i_len-4);
       
			i_ret = cavs_get_pb_picture_header(&p->s,&p->ph,&p->vsh);
            if(i_ret == -1)
                return CAVS_ERROR;
				
            break;

        default:
            return CAVS_ERROR;
    }

    /* decide frame or field */
    param->b_interlaced = !p->ph.b_picture_structure;
    if( !param->b_interlaced )
    {
        printf("[cavs info] decode as frame\n");    
    }
    else
    {
        printf("[cavs info] decode as field\n");   
    }
    p->b_get_video_sequence_header = 0;

    return 0;
}

int cavs_decoder_set_format_type( void* p_decoder, cavs_param *param )
{
    cavs_decoder *p = (cavs_decoder *)p_decoder;

	p->param.b_interlaced = param->b_interlaced;

    return 0;
}

int cavs_decoder_reset_pipeline( cavs_decoder *p_decoder )
{
    cavs_decoder *p = p_decoder;

    if( !p )
    {
        return -1;
    }
    p->i_video_edit_code_flag = 0;
    p->b_get_i_picture_header = 0;
    
    if( p->param.b_accelerate )
    {
        memset(&p->ref_aec[0], 0, sizeof(xavs_image));
        memset(&p->ref_aec[1], 0, sizeof(xavs_image));
        memset(&p->ref_aec[2], 0, sizeof(xavs_image));
        memset(&p->ref_aec[3], 0, sizeof(xavs_image));
        p->image_aec[0].i_distance_index = 0;
        p->image_aec[1].i_distance_index = 0;
        p->image_aec[2].i_distance_index = 0;

        p->p_save_aec[0] = &p->image_aec[0];
        //p->last_delayed_pframe = p->p_save[1];
        p->p_save_aec[1] = NULL; // FIXIT : can't shut down this line for open gop
        p->p_save_aec[2] = NULL;
    }  

    memset(&p->ref[0], 0, sizeof(xavs_image));
    memset(&p->ref[1], 0, sizeof(xavs_image));
    memset(&p->ref[2], 0, sizeof(xavs_image));
    memset(&p->ref[3], 0, sizeof(xavs_image));
    p->image[0].i_distance_index = 0;
    p->image[1].i_distance_index = 0;
    p->image[2].i_distance_index = 0;

    p->p_save[0] = &p->image[0];
    p->last_delayed_pframe = p->p_save[1];
    p->p_save[1] = NULL; // FIXIT : can't shut down this line for open gop
    p->p_save[2] = NULL;

    memset(p->p_block, 0, 64*sizeof(DCTELEM));

    return 0;
}

int cavs_decoder_seq_header_reset( void* p_decoder )
{
    cavs_decoder *p = (cavs_decoder *)p_decoder;
    
    if(p->b_low_delay != p->vsh.b_low_delay)
    {
        p->b_low_delay = p->vsh.b_low_delay;

        if( !p->b_low_delay )
        {
			//cavs_decoder_reset(p);
			//p->p_save[1] = NULL;
	    }
    }
    
    return 0;
}

int cavs_decoder_seq_header_reset_pipeline( void* p_decoder )
{
    cavs_decoder *p = (cavs_decoder *)p_decoder;

    /* reset for pipeline */
    cavs_decoder_reset_pipeline( p->unused_backup[0] );
    cavs_decoder_reset_pipeline( p->unused_backup[1] );
#if THREADS_OPT
    p->unused_backup[1]->p_save_aec[0] = &p->image_aec[0];
#endif    
    p->unused_backup[1]->p_save[0] = &p->image[0];  

    p->current[0] = NULL;
    p->current[1] = NULL;
    
    return 0;
}

int cavs_decoder_low_delay_value( void* p_decoder )
{
	cavs_decoder *p = (cavs_decoder *)p_decoder;

	return  p->vsh.b_low_delay;
}

int cavs_decoder_slice_num_probe( void* p_decoder,  int i_startcode, cavs_param *param, unsigned char  *buf,  int length )
{
    cavs_decoder *p = (cavs_decoder *)p_decoder;

    int b_interlaced = p->param.b_interlaced;

    p->Frame = (frame_pack* )cavs_malloc(sizeof(frame_pack));
    p->Frame->p_start = (uint8_t* )cavs_malloc(MAX_CODED_FRAME_SIZE*sizeof(uint8_t));
    //memset(p->Frame->p_start, 0, MAX_CODED_FRAME_SIZE*sizeof(unsigned char));  

    if( !b_interlaced )
    {
        /* init frame pack */
        //memset(p->Frame->p_start, 0, MAX_CODED_FRAME_SIZE*sizeof(unsigned char));

        /* init */
        p->Frame->p_cur = p->Frame->p_start;
        p->Frame->i_len = 0;
        p->Frame->slice_num = 0;
        p->Frame->i_startcode_type = i_startcode;
        memcpy(p->Frame->p_cur, buf, length ); /* pack header of pic into one frame */

        /* set pic header info, use position of slice_num = 0 */
        p->Frame->slice[p->Frame->slice_num].i_startcode = i_startcode;
        p->Frame->slice[p->Frame->slice_num].p_start = p->Frame->p_cur + 4; /* for skip current startcode */
        p->Frame->slice[p->Frame->slice_num].i_len = length-4;

        /* update frame info */
        p->Frame->p_cur = p->Frame->p_cur + length;
        p->Frame->i_len += length;
        p->Frame->slice_num++;

        /* pack  */
        cavs_frame_pack( p->p_stream,  p->Frame );

		p->param.b_accelerate = 0; /* accelerate version don't support multi-slice */
		p->param.i_thread_model = 1;
		p->param.i_thread_num = 128; //64;
		param->b_accelerate = 0;
    }
    else
    {
        //uint32_t frame_type = i_startcode;

        /* init field pack of top */
        //memset(p->Frame->p_start, 0, MAX_CODED_FRAME_SIZE*sizeof(unsigned char));
        p->Frame->b_bottom = 0;
        //p->Frame->i_mb_end[0] = param->fld_mb_end[0];
        p->Frame->p_cur = p->Frame->p_start;
        p->Frame->i_len = 0;
        p->Frame->slice_num = 0;
        p->Frame->i_startcode_type = i_startcode;
        memcpy( p->Frame->p_cur, buf, length ); /* pack header of pic into one frame */

        /* set pic header info , use position of slice_num = 0 */
        p->Frame->slice[p->Frame->slice_num].i_startcode = i_startcode;
        p->Frame->slice[p->Frame->slice_num].p_start = p->Frame->p_cur + 4; /* for skip current startcode */
        p->Frame->slice[p->Frame->slice_num].i_len = length-4;

        /* update field info of top */
        p->Frame->p_cur = p->Frame->p_cur + length;
        p->Frame->i_len += length;
        p->Frame->slice_num++;

		p->Frame->i_mb_end[0] = (((p->vsh.i_vertical_size+31)& 0xffffffe0)>>5) - 1; /* top field mb end */
		p->Frame->i_mb_end[1] = (((p->vsh.i_vertical_size+31)& 0xffffffe0)>>4) - 1; /* bot field mb end */

        /* pack  */
        cavs_field_pack( p->p_stream, p->Frame );

		p->param.b_accelerate = 0; /* accelerate version don't support multi-slice */
		p->param.i_thread_model = 1;
		p->param.i_thread_num = 128; //64;
		param->b_accelerate = 0;
    }

    cavs_free( p->Frame->p_start );
	p->Frame->p_start = NULL;
    cavs_free( p->Frame );
	p->Frame = NULL;
    
    return 0;
}


#if THREADS_OPT
/* Opt for multi-thread of AEC module */
/* AEC module : aec or 2d-vlc */
/* REC module : reconstructed processdure */

/*================================= MB level ========================================================*/

/* read coeffs */
static int get_residual_block_aec_opt( cavs_decoder *p, const xavs_vlc *p_vlc_table, 
	int i_escape_order, int b_chroma, int i_block )
{
    int *level_buf = p->level_buf;
    int *run_buf = p->run_buf;
    int i;
    int pos = -1;
    //DCTELEM *block = p->p_block;

    i = p->bs_read_coeffs(p, p_vlc_table, i_escape_order, b_chroma);
    if(p->b_error_flag )
    {
        printf("[error]MB coeffs is wrong\n");
        return -1;
    }
      
#if DEBUG_INTRA|DEBUG_P_INTER|DEBUG_B_INTER
    {
    	int tmp;
    	FILE *fp;

    	fp = fopen("data_coeff.dat","ab");
    	fprintf(fp,"frame %d MB %d i = %d\n", p->i_frame_decoded, p->i_mb_index, i);
    	for( tmp = i-1; tmp >= 0; tmp-- )
    		fprintf(fp,"%d,",run_buf[tmp]);
    	fprintf(fp,"\n");
    	fclose(fp);
    }
#endif

    /* copy buf to tab */
    memcpy( &(p->num_buf_tab[p->i_mb_index][i_block]), &i, 1*sizeof(int));
    memcpy( p->run_buf_tab[p->i_mb_index][i_block], run_buf, 64*sizeof(int));
    memcpy( p->level_buf_tab[p->i_mb_index][i_block], level_buf, 64*sizeof(int));

    if( i == -1 )
    {
        printf("[error]MB coeffs is wrong\n");
        return -1;
    }

    /* add for detecting pos error */
    while(--i >= 0)
    {
        pos += /*run_buf[i]*/p->run_buf_tab[p->i_mb_index][i_block][i];
        if(pos > 63 || pos < 0) 
        {
        	printf("[error]aec MB run pos is too long\n");
        	p->b_error_flag = 1;

        	return -1;
        }

        //block[zigzag[pos]] = (/*level_buf[i]*/p->level_buf_tab[p->i_mb_index][i_block][i]*dqm + dqa) >> dqs;
    }
        
    return 0;
}

static int get_residual_block_rec_opt(cavs_decoder *p,const xavs_vlc *p_vlc_table, 
	int i_escape_order,int i_qp, uint8_t *p_dest, int i_stride, int b_chroma, int i_block )
{
    int i, pos = -1;
    //int *level_buf = p->level_buf;
    //int *run_buf = p->run_buf;
    int dqm = dequant_mul[i_qp];
    int dqs = dequant_shift[i_qp];
    int dqa = 1 << (dqs - 1);
    const uint8_t *zigzag = p->ph.b_picture_structure==0 ? zigzag_field : zigzag_progressive;
    DCTELEM *block = p->p_block;

    DECLARE_ALIGNED_16(short dct8x8[8][8]);
    DECLARE_ALIGNED_16(UNUSED int idct8x8[8][8]);
    int j, k;
    
    /* read level */
    i= p->num_buf_tab[p->i_mb_index][i_block];

    if( i == -1 )
    {
        printf("[error]MB coeff is wrong\n");
        p->b_error_flag = 1;

        return -1;
    }    

    if( !p->b_weight_quant_enable )
    {
     	while(--i >= 0)
    	{
    		pos += p->run_buf_tab[p->i_mb_index][i_block][i];
    		if(pos > 63 || pos < 0) 
    		{
    			printf("[error]MB run pos is too long\n");
    			p->b_error_flag = 1;
            
    			return -1;
    		}

    		block[zigzag[pos]] = (p->level_buf_tab[p->i_mb_index][i_block][i]*dqm + dqa) >> dqs;
    	}
    }
    else
    {
    	int m, n;

        while(--i >= 0)
    	{
    		pos += p->run_buf_tab[p->i_mb_index][i_block][i];
    		if(pos > 63 || pos < 0 ) 
    		{
    			printf("[error]MB run pos is too long\n");
    			p->b_error_flag = 1;
            
    			return -1;
    		}

    		m = SCAN[p->ph.b_picture_structure][pos][0];
    		n = SCAN[p->ph.b_picture_structure][pos][1];

             /* NOTE : bug will conflict with idct asm when Win32-Release, don't why now */
    		block[zigzag[pos]] = (((((int)(p->level_buf_tab[p->i_mb_index][i_block][i]*p->cur_wq_matrix[n*8+m])>>3)*dqm)>>4) + dqa) >> dqs;
    	}
    }

#if WIN32//!HAVE_MMX

#if 1 // bug:    
	xavs_idct8_add(p_dest, block, i_stride);
#else
    for( j = 0 ; j < 8; j++ )
        for( k = 0; k < 8; k++ )
        {
            idct8x8[j][k] = (int)block[j*8+k];
        }
    memset(block, 0, 64*sizeof(DCTELEM));
        
    /* idct 8x8 */    
    cavs_inv_transform_B8( idct8x8 );

#if 0 // FIXIT
    if( /*p->i_mb_index == 0*/0)
    {
        int i;

        printf("block coeffs after idct:\n");

        for( j = 0; j < 8; j++ )
        {
            for( k = 0; k < 8; k++ )
            {
                printf("%3d,",idct8x8[j][k] );
            }
            printf("\n");
        }
    }
#endif

    /* reconstruct = residual + predidct */
    xavs_add_c( p_dest, idct8x8, i_stride );

#endif

#else /* bug : asm is wrong when worst situation,  */	
    
#if 0	
	for( j = 0 ; j < 8; j++ )
    	for( k = 0; k < 8; k++ )
    	{
    		dct8x8[k][j] = block[j*8+k];
    	}
    memset(block,0,64*sizeof(DCTELEM));
    cavs_add8x8_idct8_sse2( p_dest, dct8x8,i_stride );
#else
	cavs_add8x8_idct8_sse2( p_dest, block,i_stride );
	memset(block,0,64*sizeof(DCTELEM));
#endif

#endif

    return 0;
}

static inline int get_residual_inter_aec_opt(cavs_decoder *p,int i_mb_type) 
{      
    int block;
    //int i_stride = p->cur.i_stride[0];
    uint8_t *p_d;
    int i_cbp;
    int i_ret = 0;

    i_cbp = p->i_cbp_tab[p->i_mb_index];   

    /* luma */
    for(block = 0; block < 4; block++)
    {
        if( i_cbp & (1<<block))
        {
            p_d = p->p_y + p->i_luma_offset[block];               
            
            i_ret = get_residual_block_aec_opt( p, inter_2dvlc, 0, 0, block);
            if(i_ret == -1)
                return -1;
        }
    }

    /* cb */
    if( i_cbp & /*(1<<4)*/16)
    {
        i_ret = get_residual_block_aec_opt( p, chroma_2dvlc, 0, 1, 4);
        if(i_ret == -1)
            return -1;

    }
    
    /* cr */
    if( i_cbp & /*(1<<5)*/32)
    {
        i_ret = get_residual_block_aec_opt( p, chroma_2dvlc, 0, 1, 5);
        if(i_ret == -1)
            return -1;
    }

    return 0;
}

static inline int get_residual_inter_rec_opt(cavs_decoder *p,int i_mb_type) 
{      
    int block;
    int i_stride = p->cur.i_stride[0];
    uint8_t *p_d;
    int i_qp_cb = 0, i_qp_cr = 0;
    int i_cbp;
    int i_ret = 0;

    i_cbp = p->i_cbp_tab[p->i_mb_index];

	if(i_cbp <0 || i_cbp >63 || p->i_qp_tab[p->i_mb_index] < 0
		|| p->i_qp_tab[p->i_mb_index] > 63 )
	{
		return -1;
	}

    /* luma */
    for(block = 0; block < 4; block++)
    {
        if( i_cbp & (1<<block))
        {
            p_d = p->p_y + p->i_luma_offset[block];
               
            i_ret = get_residual_block_rec_opt( p, inter_2dvlc, 0, p->i_qp_tab[p->i_mb_index], p_d, i_stride, 0, block );
            if( i_ret == -1 )
                return -1;
        }
    }

    /* FIXIT : weighting quant qp for chroma
        NOTE : not modify p->i_qp directly
    */
    if( p->b_weight_quant_enable && !p->ph.chroma_quant_param_disable )
    { 
        i_qp_cb = clip3_int( p->i_qp_tab[p->i_mb_index] + p->ph.chroma_quant_param_delta_u, 0, 63  );
        i_qp_cr = clip3_int( p->i_qp_tab[p->i_mb_index] + p->ph.chroma_quant_param_delta_v, 0, 63 );
    }

    /* cr */
    if( i_cbp & /*(1<<4)*/16)
    {
        i_ret = get_residual_block_rec_opt(p, chroma_2dvlc, 0, 
                              chroma_qp[p->b_weight_quant_enable && !p->ph.chroma_quant_param_disable ? i_qp_cb : p->i_qp_tab[p->i_mb_index]],
                              p->p_cb, p->cur.i_stride[1], 1, 4);
        if( i_ret == -1 )
            return -1;
    }

    /* cr */
    if( i_cbp & /*(1<<5)*/32)
    {
        i_ret = get_residual_block_rec_opt(p, chroma_2dvlc, 0, 
                              chroma_qp[ p->b_weight_quant_enable && !p->ph.chroma_quant_param_disable ? i_qp_cr : p->i_qp_tab[p->i_mb_index]],
                              p->p_cr, p->cur.i_stride[2], 1, 5);
        if( i_ret == -1 )
            return -1;
    }

    return 0;
}

static inline void rec_init_mb( cavs_decoder *p )
{
    //int i = 0;
    int i_offset = p->i_mb_x<<1;
    int i_offset_b4 = p->i_mb_x<<2;

    if(!(p->i_mb_flags & A_AVAIL))
    {
        p->mv[MV_FWD_A1] = MV_NOT_AVAIL;
        p->mv[MV_FWD_A3] = MV_NOT_AVAIL;
        p->mv[MV_BWD_A1] = MV_NOT_AVAIL;
        p->mv[MV_BWD_A3] = MV_NOT_AVAIL;

        p->i_intra_pred_mode_y[5] = p->i_intra_pred_mode_y[10] = 
        p->i_intra_pred_mode_y[15] = p->i_intra_pred_mode_y[20] = NOT_AVAIL;
    }

    if(p->i_mb_flags & B_AVAIL)
    {
        p->mv[MV_FWD_B2] = p->p_top_mv[0][i_offset];
        p->mv[MV_BWD_B2] = p->p_top_mv[1][i_offset];
        p->mv[MV_FWD_B3] = p->p_top_mv[0][i_offset+1];
        p->mv[MV_BWD_B3] = p->p_top_mv[1][i_offset+1];

        p->i_intra_pred_mode_y[1] = p->p_top_intra_pred_mode_y[i_offset_b4];
        p->i_intra_pred_mode_y[2] = p->p_top_intra_pred_mode_y[i_offset_b4+1];
        p->i_intra_pred_mode_y[3] = p->p_top_intra_pred_mode_y[i_offset_b4+2];
        p->i_intra_pred_mode_y[4] = p->p_top_intra_pred_mode_y[i_offset_b4+3];
    }
    else
    {
        p->mv[MV_FWD_B2] = MV_NOT_AVAIL;
        p->mv[MV_FWD_B3] = MV_NOT_AVAIL;
        p->mv[MV_BWD_B2] = MV_NOT_AVAIL;
        p->mv[MV_BWD_B3] = MV_NOT_AVAIL;
        
        p->i_intra_pred_mode_y[1] = p->i_intra_pred_mode_y[2] = 
        p->i_intra_pred_mode_y[3] = p->i_intra_pred_mode_y[4] = NOT_AVAIL;
    }

    if(p->i_mb_flags & C_AVAIL)
    {
        p->mv[MV_FWD_C2] = p->p_top_mv[0][i_offset+2];
        p->mv[MV_BWD_C2] = p->p_top_mv[1][i_offset+2];
    }
    else
    {
        p->mv[MV_FWD_C2] = MV_NOT_AVAIL;
        p->mv[MV_BWD_C2] = MV_NOT_AVAIL;
    }

    if(!(p->i_mb_flags & D_AVAIL))
    {
        p->mv[MV_FWD_D3] = MV_NOT_AVAIL;
        p->mv[MV_BWD_D3] = MV_NOT_AVAIL;	
    }

    p->p_col_type = &p->p_col_type_base[p->i_mb_y*p->i_mb_width + p->i_mb_x];

    if (p->ph.b_aec_enable)
    {
        if (!p->i_mb_x)
        {
            p->i_chroma_pred_mode_left = 0;
            p->i_cbp_left = -1;

            M32(p->p_mvd[0][0]) 
                = M32(p->p_mvd[0][3]) 
                = M32(p->p_mvd[1][0]) 
                = M32(p->p_mvd[1][3]) = 0;
            
            p->p_ref[0][3] 
                = p->p_ref[0][6] 
                = p->p_ref[1][3] 
                = p->p_ref[1][6] = -1;                
        }
        else
        {
            M32(p->p_mvd[0][0]) = M32(p->p_mvd[0][2]);
            M32(p->p_mvd[0][3]) = M32(p->p_mvd[0][5]);
            M32(p->p_mvd[1][0]) = M32(p->p_mvd[1][2]);
            M32(p->p_mvd[1][3]) = M32(p->p_mvd[1][5]);

            p->p_ref[0][3] = p->p_ref[0][5];
            p->p_ref[0][6] = p->p_ref[0][8];
            p->p_ref[1][3] = p->p_ref[1][5];
            p->p_ref[1][6] = p->p_ref[1][8];
        
        }

        CP16(&p->p_ref[0][1], p->p_ref_top[p->i_mb_x][0]);
        CP16(&p->p_ref[1][1], p->p_ref_top[p->i_mb_x][1]);

        M64(p->p_mvd[0][1]) 
            = M64(p->p_mvd[1][1]) 
            = M64(p->p_mvd[0][4]) 
            = M64(p->p_mvd[1][4]) = 0;

        M16(&p->p_ref[0][4]) 
            = M16(&p->p_ref[0][7]) 
            = M16(&p->p_ref[1][4]) 
            = M16(&p->p_ref[1][7]) = -1;
        
        //p->i_pred_mode_chroma_tab[p->i_mb_index] = 0;
    }    
}    

static inline int aec_next_mb( cavs_decoder *p )
{
    int i, i_y;
    int i_offset = p->i_mb_x<<1;

    p->i_mb_flags |= A_AVAIL;

    for( i = 0; i <= 20; i += 4 )
    {
        p->mv[i] = p->mv[i+2];
    }
   
    p->mv[MV_FWD_D3] = p->p_top_mv[0][i_offset+1];
    p->mv[MV_BWD_D3] = p->p_top_mv[1][i_offset+1];
    p->p_top_mv[0][i_offset+0] = p->mv[MV_FWD_X2];
    p->p_top_mv[0][i_offset+1] = p->mv[MV_FWD_X3];
    p->p_top_mv[1][i_offset+0] = p->mv[MV_BWD_X2];
    p->p_top_mv[1][i_offset+1] = p->mv[MV_BWD_X3];

    /* for AEC */
    if (p->ph.b_aec_enable)
    {
        p->p_mb_type_top[p->i_mb_x] = p->i_mb_type_tab[p->i_mb_index];

#if 0
        if( p->ph.b_skip_mode_flag  || p->ph.i_picture_coding_type == 0 )
        {
            p->i_chroma_pred_mode_left = p->p_chroma_pred_mode_top[p->i_mb_x] = p->i_pred_mode_chroma_tab[p->i_mb_index];
            p->i_cbp_left = p->p_cbp_top[p->i_mb_x] = p->i_cbp;
        }
        else
        {
             if( p->i_mb_type_tab[p->i_mb_index] != P_SKIP && p->i_mb_type_tab[p->i_mb_index] != B_SKIP )
            {
                p->i_chroma_pred_mode_left = p->p_chroma_pred_mode_top[p->i_mb_x] = p->i_pred_mode_chroma_tab[p->i_mb_index];
                p->i_cbp_left = p->p_cbp_top[p->i_mb_x] = p->i_cbp;
            } 
            else
            {   
                p->i_chroma_pred_mode_left = p->p_chroma_pred_mode_top[p->i_mb_x] = 0;
                p->i_cbp_left = p->p_cbp_top[p->i_mb_x] = 0; /* skip has not cbp */
                p->i_last_dqp = 0;
            }
        }
#else
		if( p->ph.i_picture_coding_type == 0 && p->b_bottom == 0)
        {
            p->i_chroma_pred_mode_left = p->p_chroma_pred_mode_top[p->i_mb_x] = p->i_pred_mode_chroma_tab[p->i_mb_index];
            p->i_cbp_left = p->p_cbp_top[p->i_mb_x] = p->i_cbp;
        }
        else
        {
            if( p->i_mb_type_tab[p->i_mb_index] != P_SKIP && p->i_mb_type_tab[p->i_mb_index] != B_SKIP )
            {
                p->i_chroma_pred_mode_left = p->p_chroma_pred_mode_top[p->i_mb_x] = p->i_pred_mode_chroma_tab[p->i_mb_index];
                p->i_cbp_left = p->p_cbp_top[p->i_mb_x] = p->i_cbp;
            } 
            else
            {   
                p->i_chroma_pred_mode_left = p->p_chroma_pred_mode_top[p->i_mb_x] = 0;
                p->i_cbp_left = p->p_cbp_top[p->i_mb_x] = 0; /* skip has not cbp */
                p->i_last_dqp = 0;
            }
        }
#endif

        CP16(p->p_ref_top[p->i_mb_x][0], &p->p_ref[0][7]);
        CP16(p->p_ref_top[p->i_mb_x][1], &p->p_ref[1][7]);
    }

    /* Move to next MB */
    p->i_mb_x++;
    if( p->i_mb_x == p->i_mb_width ) 
    { 
        p->i_mb_flags = B_AVAIL|C_AVAIL;

        p->i_intra_pred_mode_y[5] = p->i_intra_pred_mode_y[10] = 
        p->i_intra_pred_mode_y[15] = p->i_intra_pred_mode_y[20] = NOT_AVAIL;
          
        for( i = 0; i <= 20; i += 4 )
        {
            p->mv[i] = MV_NOT_AVAIL;
        }
        p->i_mb_x = 0;
        p->i_mb_y++;
        i_y = p->i_mb_y - p->i_mb_offset;
        p->b_first_line = 0;

        /* for AEC */
        p->i_mb_type_left = -1;
    }
    else
    {
        /* for AEC */
        p->i_mb_type_left = p->i_mb_type_tab[p->i_mb_index];
    }

    if( p->i_mb_x == p->i_mb_width-1 )
    {
        p->i_mb_flags &= ~C_AVAIL;
    }

    if( p->i_mb_x && p->i_mb_y && p->b_first_line==0 ) 
    {
        p->i_mb_flags |= D_AVAIL;
    }

    /* set for AEC decode of frame level */
    if( (p->i_mb_type_tab[p->i_mb_index] != I_8X8 )
		&& (p->ph.i_picture_coding_type != 0 
		|| (p->ph.i_picture_coding_type == XAVS_I_PICTURE && p->b_bottom) ) )
    {
        /* set intra prediction modes to default values */
        p->i_intra_pred_mode_y[5] =  p->i_intra_pred_mode_y[10] = 
        p->i_intra_pred_mode_y[15] =  p->i_intra_pred_mode_y[20] = NOT_AVAIL;

        p->p_top_intra_pred_mode_y[(i_offset<<1)+0] = p->p_top_intra_pred_mode_y[(i_offset<<1)+1] = 
        p->p_top_intra_pred_mode_y[(i_offset<<1)+2] = p->p_top_intra_pred_mode_y[(i_offset<<1)+3] = NOT_AVAIL;    
    }
    
    return 0;
}

static inline int rec_next_mb( cavs_decoder *p )
{
    int i, i_y;
    //int i_offset = p->i_mb_x<<1;

    p->i_mb_flags |= A_AVAIL;
    p->p_y += 16;
    p->p_cb += 8;
    p->p_cr += 8;

    /* Move to next MB */
    p->i_mb_x++;
    if(p->i_mb_x == p->i_mb_width) 
    { 
        p->i_mb_flags = B_AVAIL|C_AVAIL;

        for( i = 0; i <= 20; i += 4 )
        {
            p->mv[i] = MV_NOT_AVAIL;
        }
        p->i_mb_x = 0;
        p->i_mb_y++;
        i_y = p->i_mb_y - p->i_mb_offset;
        p->p_y = p->cur.p_data[0] + i_y*XAVS_MB_SIZE*p->cur.i_stride[0];
        p->p_cb = p->cur.p_data[1] + i_y*(XAVS_MB_SIZE>>1)*p->cur.i_stride[1];
        p->p_cr = p->cur.p_data[2] + i_y*(XAVS_MB_SIZE>>1)*p->cur.i_stride[2];

#if B_MB_WEIGHTING
        p->p_back_y = p->mb_line_buffer[0] + i_y*XAVS_MB_SIZE*p->cur.i_stride[0];
        p->p_back_cb = p->mb_line_buffer[1] + i_y*(XAVS_MB_SIZE>>1)*p->cur.i_stride[1];
        p->p_back_cr = p->mb_line_buffer[2] + i_y*(XAVS_MB_SIZE>>1)*p->cur.i_stride[2];
#endif

        p->b_first_line = 0;

        /* for AEC */
        p->i_mb_type_left = -1;
    }
    else
    {
        /* for AEC */
        p->i_mb_type_left = p->i_mb_type_tab[p->i_mb_index];
    }

    if( p->i_mb_x == p->i_mb_width-1 )
    {
        p->i_mb_flags &= ~C_AVAIL;
    }

    if( p->i_mb_x && p->i_mb_y && p->b_first_line==0 ) 
    {
        p->i_mb_flags |= D_AVAIL;
    }
    
    return 0;
}    


static int get_mb_i_aec_opt( cavs_decoder *p )
{
    //uint8_t *p_left = NULL;

    static const uint8_t scan5x5[4][4] = {
    	{6, 7, 11, 12},
    	{8, 9, 13, 14},
    	{16, 17, 21, 22},
    	{18, 19, 23, 24}
    };

    int i;
    int i_offset = p->i_mb_x<<2;
    //int i_mb_type = p->i_cbp_code;
    int i_rem_mode;
    int i_pred_mode;
    
    int i_ret = 0;

    init_mb(p);

#if HAVE_MMX
    cavs_mb_neighbour_avail( p );
#endif

#if DEBUG_INTRA
            {
                int tmp;
                FILE *fp;

                fp = fopen("data_luma.dat","ab");
                fprintf(fp,"frame %d MB %d \n", p->i_frame_decoded, p->i_mb_index );
                fprintf(fp,"left side\n");  
                if(p->i_mb_flags & A_AVAIL)
                {
                    for(i = 5; i < 25; i+=5 )
                    {    
                        
                        fprintf(fp,"%d \n", p->i_intra_pred_mode_y[i]);
                        //if((i+1)%5==0)
                        //     fprintf(fp,"%d\n");    
                    }
                }
                fprintf(fp,"top side\n");
                if(p->i_mb_flags & B_AVAIL)
                {
                    for(i = 1; i < 5; i++ )
                    {    
                        
                        fprintf(fp,"%d \n", p->i_intra_pred_mode_y[i]);
                        //if((i+1)%5==0)
                        //     fprintf(fp,"%d\n");    
                    }
                }
#if 0
                fprintf(fp,"cur side\n");
                for(i = 6; i < 10; i++ )
                {    
                        
                    fprintf(fp,"%d \n", p->i_intra_pred_mode_y[i]); 
                }
                fprintf(fp,"\n");
                for(i = 11; i < 15; i++ )
                {    
                        
                    fprintf(fp,"%d \n", p->i_intra_pred_mode_y[i]); 
                }
                fprintf(fp,"\n");
                for(i = 16; i < 20; i++ )
                {    
                        
                    fprintf(fp,"%d \n", p->i_intra_pred_mode_y[i]); 
                }
                fprintf(fp,"\n");
                for(i = 21; i < 25; i++ )
                {    
                        
                    fprintf(fp,"%d \n", p->i_intra_pred_mode_y[i]); 
                }
#endif                
                fprintf(fp,"\n");
                fclose(fp);
            }
#endif

    /* luma predict mode */
    for( i = 0; i < 4; i++ ) 
    {
        {
            int i_pos = scan5x5[i][0];

            i_rem_mode = p->bs_read[SYNTAX_INTRA_LUMA_PRED_MODE](p);
            if( p->b_error_flag )
            {
                printf("[error]MB pred_mode_luma is wrong\n");
                return -1;
            }
          
            i_pred_mode = cavs_mb_predict_intra8x8_mode(p, i_pos);
            if(!p->pred_mode_flag)
            {
                i_pred_mode = i_rem_mode + (i_rem_mode >= i_pred_mode);
            }
            if( i_pred_mode > 4) 
            {
                printf("[error]MB pred_mode_luma is wrong\n");
                p->b_error_flag = 1;
                
                return -1;
            }

#if DEBUG_INTRA
            {
                int tmp;
                FILE *fp;

                fp = fopen("data_luma.dat","ab");
                fprintf(fp,"frame %d MB %d\n", p->i_frame_decoded, p->i_mb_index);
                //for( tmp = i; tmp >= 0; tmp-- )
                fprintf(fp,"luma = %d\n", i_pred_mode);
                //fprintf(fp,"\n");
                fclose(fp);
            }
#endif

            p->i_intra_pred_mode_y[scan5x5[i][0]] =
            p->i_intra_pred_mode_y[scan5x5[i][1]] =
            p->i_intra_pred_mode_y[scan5x5[i][2]] =
            p->i_intra_pred_mode_y[scan5x5[i][3]] = i_pred_mode;
            p->i_intra_pred_mode_y_tab[p->i_mb_index][scan5x5[i][0]] =
            p->i_intra_pred_mode_y_tab[p->i_mb_index][scan5x5[i][1]] =
            p->i_intra_pred_mode_y_tab[p->i_mb_index][scan5x5[i][2]] =
            p->i_intra_pred_mode_y_tab[p->i_mb_index][scan5x5[i][3]] = i_pred_mode;
        }
    }

    /* chroma predict mode */
    //p->i_pred_mode_chroma = p->bs_read[SYNTAX_INTRA_CHROMA_PRED_MODE](p);
    //if(p->i_pred_mode_chroma > 6) 
    //{
    //    printf("[error]MB pred_mode_chroma is wrong\n");
    //    p->b_error_flag = 1;
                
    //    return -1;
    //}
    p->i_pred_mode_chroma_tab[p->i_mb_index] = p->bs_read[SYNTAX_INTRA_CHROMA_PRED_MODE](p);
    if(p->i_pred_mode_chroma_tab[p->i_mb_index] > 6 || p->b_error_flag ) 
    {
        printf("[error]MB pred_mode_chroma is wrong\n");
        p->b_error_flag = 1;
                
        return -1;
    }
    
#if DEBUG_INTRA
    {
        int tmp;
        FILE *fp;

        fp = fopen("data_chroma.dat","ab");
        fprintf(fp,"frame %d MB %d \n", p->i_frame_decoded, p->i_mb_index );
        //for( tmp = i; tmp >= 0; tmp-- )
        fprintf(fp,"chroma = %d\n",p->i_pred_mode_chroma_tab[p->i_mb_index]);
        //fprintf(fp,"\n");
        fclose(fp);
    }
#endif

    p->i_intra_pred_mode_y[5] =  p->i_intra_pred_mode_y[9];
    p->i_intra_pred_mode_y[10] =  p->i_intra_pred_mode_y[14];
    p->i_intra_pred_mode_y[15] =  p->i_intra_pred_mode_y[19];
    p->i_intra_pred_mode_y[20] =  p->i_intra_pred_mode_y[24];

    p->p_top_intra_pred_mode_y[i_offset+0] = p->i_intra_pred_mode_y[21];
    p->p_top_intra_pred_mode_y[i_offset+1] = p->i_intra_pred_mode_y[22];
    p->p_top_intra_pred_mode_y[i_offset+2] = p->i_intra_pred_mode_y[23];
    p->p_top_intra_pred_mode_y[i_offset+3] = p->i_intra_pred_mode_y[24];

    if(!(p->i_mb_flags & A_AVAIL))
    {
        adapt_pred_mode(LEFT_ADAPT_INDEX_L, &p->i_intra_pred_mode_y[6] );
        adapt_pred_mode(LEFT_ADAPT_INDEX_L, &p->i_intra_pred_mode_y[11] );
        adapt_pred_mode(LEFT_ADAPT_INDEX_L, &p->i_intra_pred_mode_y[16] );
        adapt_pred_mode(LEFT_ADAPT_INDEX_L, &p->i_intra_pred_mode_y[21] );
        adapt_pred_mode(LEFT_ADAPT_INDEX_C, &p->i_pred_mode_chroma );
    }
    if(!(p->i_mb_flags & B_AVAIL))
    {
        adapt_pred_mode(TOP_ADAPT_INDEX_L, &p->i_intra_pred_mode_y[6] );
        adapt_pred_mode(TOP_ADAPT_INDEX_L, &p->i_intra_pred_mode_y[7] );
        adapt_pred_mode(TOP_ADAPT_INDEX_L, &p->i_intra_pred_mode_y[8] );
        adapt_pred_mode(TOP_ADAPT_INDEX_L, &p->i_intra_pred_mode_y[9] );
        adapt_pred_mode(TOP_ADAPT_INDEX_C, &p->i_pred_mode_chroma);
    }
    
    if(!(p->i_mb_flags & A_AVAIL))
    {
        adapt_pred_mode(LEFT_ADAPT_INDEX_L, &p->i_intra_pred_mode_y_tab[p->i_mb_index][6] );
        adapt_pred_mode(LEFT_ADAPT_INDEX_L, &p->i_intra_pred_mode_y_tab[p->i_mb_index][11] );
        adapt_pred_mode(LEFT_ADAPT_INDEX_L, &p->i_intra_pred_mode_y_tab[p->i_mb_index][16] );
        adapt_pred_mode(LEFT_ADAPT_INDEX_L, &p->i_intra_pred_mode_y_tab[p->i_mb_index][21] );
        adapt_pred_mode(LEFT_ADAPT_INDEX_C, &p->i_pred_mode_chroma_tab[p->i_mb_index] );
    }
    if(!(p->i_mb_flags & B_AVAIL))
    {
        adapt_pred_mode(TOP_ADAPT_INDEX_L, &p->i_intra_pred_mode_y_tab[p->i_mb_index][6] );
        adapt_pred_mode(TOP_ADAPT_INDEX_L, &p->i_intra_pred_mode_y_tab[p->i_mb_index][7] );
        adapt_pred_mode(TOP_ADAPT_INDEX_L, &p->i_intra_pred_mode_y_tab[p->i_mb_index][8] );
        adapt_pred_mode(TOP_ADAPT_INDEX_L, &p->i_intra_pred_mode_y_tab[p->i_mb_index][9] );
        adapt_pred_mode(TOP_ADAPT_INDEX_C, &p->i_pred_mode_chroma_tab[p->i_mb_index] );
    }

    /* cbp */
    p->i_cbp_tab[p->i_mb_index] = p->bs_read[SYNTAX_INTRA_CBP](p);
    if( p->b_error_flag )
    {
        printf("[error]MB cbp is wrong\n");
        return -1;
    }
    
    p->i_cbp = p->i_cbp_tab[p->i_mb_index];
    if( p->i_cbp_tab[p->i_mb_index] > 63 ) 
    {
        printf("[error]MB cbp is wrong\n");
        p->b_error_flag = 1;

        return -1;
    }

#if DEBUG_INTRA
    {
    	int tmp;
    	FILE *fp;

    	fp = fopen("data_cbp.dat","ab");
    	fprintf(fp,"frame %d MB %d cbp = %d\n", p->i_frame_decoded, p->i_mb_index, p->i_cbp_tab[p->i_mb_index] );
    	//for( tmp = i; tmp >= 0; tmp-- )
    	//	fprintf(fp,"cbp = %d\n",p->i_cbp_tab[p->i_mb_index]);
    	//fprintf(fp,"\n");
    	fclose(fp);
    }
#endif

    /* qp */
    p->i_qp_tab[p->i_mb_index] = p->i_qp;
    if(p->i_cbp_tab[p->i_mb_index] && !p->b_fixed_qp)
    {
#if 1
        int delta_qp =  p->bs_read[SYNTAX_DQP](p);
        
		p->i_qp_tab[p->i_mb_index] = (p->i_qp_tab[p->i_mb_index] -MIN_QP + delta_qp + (MAX_QP-MIN_QP+1))%(MAX_QP-MIN_QP+1)+MIN_QP;

        p->i_qp = p->i_qp_tab[p->i_mb_index];

        if( p->i_qp < 0 || p->i_qp > 63  || p->b_error_flag )
        {
            printf("[error]MB qp delta is wrong\n");
			p->b_error_flag = 1;
            return -1;  
        }
#else    
        p->i_qp = p->i_qp_tab[p->i_mb_index] = (p->i_qp_tab[p->i_mb_index] + p->bs_read[SYNTAX_DQP](p)) & 63;
        if( p->b_error_flag )
        {
            printf("[error]MB qp delta is wrong\n");
            return -1;
        }
#endif        
    }
    else
        p->i_last_dqp  = 0; // for cabac only

#if DEBUG_INTRA
    {
    	int tmp;
    	FILE *fp;

    	fp = fopen("data_qp.dat","ab");
    	fprintf(fp,"frame %d MB %d \n", p->i_frame_decoded, p->i_mb_index );
    	//for( tmp = i; tmp >= 0; tmp-- )
    		fprintf(fp,"qp = %d\n",p->i_qp_tab[p->i_mb_index]);
    	//fprintf(fp,"\n");
    	fclose(fp);
    }	
#endif

    /* luma coeffs */
    for( i = 0; i < 4; i++ )
    {
        if(p->i_cbp_tab[p->i_mb_index]  & (1<<i))
        {
            /* read coeffs from stream */
            i_ret = get_residual_block_aec_opt( p, intra_2dvlc,  1, 0, i );
            if( i_ret == -1 )
                return -1;
        }
    }

    /* chroma coeffs */
    if(p->i_cbp_tab[p->i_mb_index]  & (1<<4))
    {
        /* read cb coeffs from stream */
        i_ret = get_residual_block_aec_opt( p, chroma_2dvlc,  0, 1, 4 );
        if( i_ret == -1 )
            return -1;
    }
    if(p->i_cbp_tab[p->i_mb_index]  & (1<<5))
    {
        /* read cr coeffs from stream */
        i_ret = get_residual_block_aec_opt( p, chroma_2dvlc,  0, 1, 5 );
        if( i_ret == -1 )
            return -1;
    }

    p->mv [MV_FWD_X0] = MV_INTRA;
    copy_mvs(&p->mv[MV_FWD_X0], BLK_16X16);
    p->mv[MV_BWD_X0] = MV_INTRA;
    copy_mvs(&p->mv[MV_BWD_X0], BLK_16X16);

    if(p->ph.i_picture_coding_type != XAVS_B_PICTURE)
    	*p->p_col_type = I_8X8;

    /* need to update mb info */
    return aec_next_mb(p);
}

static int get_mb_p_aec_opt( cavs_decoder *p )
{
    int i_offset;
    int ref[4];
    int i_mb_type = p->i_mb_type;
    int16_t (*p_mvd)[6][2] = p->p_mvd;
    int8_t (*p_ref)[9] = p->p_ref;
    int i_ret = 0;

    init_mb(p);

#define FWD		0
#define MVD_X0  1
#define MVD_X1	2
#define MVD_X2  4
#define MVD_X3	5
#define REF_X0  4
#define REF_X1	5
#define REF_X2	7
#define REF_X3  8

#if DEBUG_P_INTER
   {
        int tmp;
        FILE *fp;
        int i;

        fp = fopen("data_mv.dat","ab");
        fprintf(fp,"frame %d MB %d \n", p->i_frame_decoded, p->i_mb_index );
        for( i = 0; i < 24; i++ )
            fprintf(fp,"mv[%d] = (%d,%d) \n", i, 
                (&p->mv[i])->x,  (&p->mv[i])->y);
        fclose(fp);
   }
#endif

    switch(i_mb_type)
    {
        case P_SKIP:
            mv_pred(p, MV_FWD_X0, MV_FWD_C2, MV_PRED_PSKIP, BLK_16X16, PSKIP_REF, MVD_X0);
            if( p->b_error_flag )
            {
                printf("[error]MB mvd is wrong\n");
                return -1;
            }
#if DEBUG_P_INTER
            {
                int tmp;
                FILE *fp;

                fp = fopen("data_ref.dat","ab");
                fprintf(fp,"MB %d \n", p->i_mb_index );
                fprintf(fp,"frame %d ref = %d \n", p->i_frame_decoded, (&p->mv[MV_FWD_X0])->ref );
                fclose(fp);
            }
#endif

            break;
        case P_16X16:
            ref[0] = p->bs_read_ref_p(p, REF_X0);            
            if( ref[0] > 3 || p->b_error_flag )
            {
                printf("[error]P_16X16 MB %d refidx is wrong\n", p->i_mb_index );
                p->b_error_flag = 1;
                
                return -1;
            }
            mv_pred(p, MV_FWD_X0, MV_FWD_C2, MV_PRED_MEDIAN,   BLK_16X16, ref[0], MVD_X0);
            if( p->b_error_flag )
            {
                printf("[error]MB mvd is wrong\n");
                return -1;
            }
#if DEBUG_P_INTER
            {
                int tmp;
                FILE *fp;

                fp = fopen("data_ref.dat","ab");
                fprintf(fp,"MB %d \n", p->i_mb_index );
                //fprintf(fp,"mv = (%d, %d)\n", (&p->mv[MV_FWD_X0])->x, (&p->mv[MV_FWD_X0])->y);
                fprintf(fp,"frame %d ref = %d \n", p->i_frame_decoded, (&p->mv[MV_FWD_X0])->ref );
                fclose(fp);
            }
#endif

            M32(p_mvd[FWD][MVD_X1]) = M32(p_mvd[FWD][MVD_X3]) = M32(p_mvd[FWD][MVD_X0]);
            p_ref[FWD][REF_X1] = p_ref[FWD][REF_X2] = p_ref[FWD][REF_X3] = p_ref[FWD][REF_X0];
            break;
        case P_16X8:
            ref[0] = p->bs_read_ref_p(p, REF_X0);
            if(  ref[0] > 3 || p->b_error_flag )
            {
                printf("[error]MB refidx is wrong\n");
                p->b_error_flag = 1;
                
                return -1;
            }
            
            ref[2] = p->bs_read_ref_p(p, REF_X2);
            if( ref[2] > 3 || p->b_error_flag )
            {
                printf("[error]P_16X8 MB %d refidx is wrong\n", p->i_mb_index );
                p->b_error_flag = 1;
                
                return -1;
            }
            
            mv_pred(p, MV_FWD_X0, MV_FWD_C2, MV_PRED_TOP,      BLK_16X8, ref[0], MVD_X0);
            if( p->b_error_flag )
            {
                printf("[error]MB %d mvd is wrong\n", p->i_mb_index );
                
                return -1;
            }
            mv_pred(p, MV_FWD_X2, MV_FWD_A1, MV_PRED_LEFT,     BLK_16X8, ref[2], MVD_X2);
            if( p->b_error_flag )
            {
                printf("[error]MB %d mvd is wrong\n", p->i_mb_index );
                
                return -1;
            }

#if DEBUG_P_INTER
            {
                int tmp;
                FILE *fp;

                fp = fopen("data_ref.dat","ab");
                fprintf(fp,"MB %d \n", p->i_mb_index );
                //fprintf(fp,"mv = (%d, %d)\n", (&p->mv[MV_FWD_X0])->x, (&p->mv[MV_FWD_X0])->y);
                fprintf(fp,"frame %d ref = %d \n", p->i_frame_decoded, (&p->mv[MV_FWD_X0])->ref );
                fprintf(fp,"frame %d ref = %d \n", p->i_frame_decoded, (&p->mv[MV_FWD_X2])->ref );
                fclose(fp);
            }
#endif
            
            CP32(p_mvd[FWD][MVD_X1], p_mvd[FWD][MVD_X0]);
            CP32(p_mvd[FWD][MVD_X3], p_mvd[FWD][MVD_X2]);
            p_ref[FWD][REF_X1] = p_ref[FWD][REF_X0];
            p_ref[FWD][REF_X3] = p_ref[FWD][REF_X2];
            break;
        case P_8X16:
            ref[0] = p->bs_read_ref_p(p, REF_X0);
            if( ref[0] > 3 || p->b_error_flag )
            {
                printf("[error] P_8X16 MB %d refidx is wrong\n", p->i_mb_index );
                p->b_error_flag = 1;
                
                return -1;
            }
            
            ref[1] = p->bs_read_ref_p(p, REF_X1);
            if( ref[1] > 3 || p->b_error_flag )
            {
                printf("[error] P_8X16 MB %d refidx is wrong\n", p->i_mb_index );
                p->b_error_flag = 1;
                
                return -1;
            }
            
            mv_pred(p, MV_FWD_X0, MV_FWD_B3, MV_PRED_LEFT,     BLK_8X16, ref[0], MVD_X0);
            if( p->b_error_flag )
            {
                printf("[error]MB %d mvd is wrong\n", p->i_mb_index );
                
                return -1;
            }
            mv_pred(p, MV_FWD_X1, MV_FWD_C2, MV_PRED_TOPRIGHT, BLK_8X16, ref[1], MVD_X1);
            if( p->b_error_flag )
            {
                printf("[error]MB %d mvd is wrong\n", p->i_mb_index );
                
                return -1;
            }

#if DEBUG_P_INTER
            {
                int tmp;
                FILE *fp;

                fp = fopen("data_ref.dat","ab");
                fprintf(fp,"MB %d \n", p->i_mb_index );
                //fprintf(fp,"mv = (%d, %d)\n", (&p->mv[MV_FWD_X0])->x, (&p->mv[MV_FWD_X0])->y);
                fprintf(fp,"frame %d ref = %d \n", p->i_frame_decoded, (&p->mv[MV_FWD_X0])->ref );
                fprintf(fp,"frame %d ref = %d \n", p->i_frame_decoded, (&p->mv[MV_FWD_X1])->ref );
                fclose(fp);
            }
#endif

            CP32(p_mvd[FWD][MVD_X3], p_mvd[FWD][MVD_X1]);
            p_ref[FWD][REF_X2] = p_ref[FWD][REF_X0];
            p_ref[FWD][REF_X3] = p_ref[FWD][REF_X1];
            break;
        case P_8X8:
            ref[0] = p->bs_read_ref_p(p, REF_X0);
            if( ref[0] > 3 || p->b_error_flag )
            {
                printf("[error]P_8X8 MB %d refidx is wrong\n", p->i_mb_index );
                p->b_error_flag = 1;
                
                return -1;
            }
                    
            ref[1] = p->bs_read_ref_p(p, REF_X1);
            if( ref[1] > 3  || p->b_error_flag )
            {
                printf("[error]P_8X8 MB %d refidx is wrong\n", p->i_mb_index );
                p->b_error_flag = 1;
                
                return -1;
            }
            
            ref[2] = p->bs_read_ref_p(p, REF_X2);
            if( ref[2] > 3  || p->b_error_flag  )
            {
                printf("[error]P_8X8 MB %d refidx is wrong\n", p->i_mb_index );
                p->b_error_flag = 1;
                
                return -1;
            }
            
            ref[3] = p->bs_read_ref_p(p, REF_X3);              
            if( ref[3] > 3 || p->b_error_flag )
            {
                printf("[error]P_8X8 MB %d refidx is wrong\n", p->i_mb_index );
                p->b_error_flag = 1;
                
                return -1;
            }
            
            mv_pred(p, MV_FWD_X0, MV_FWD_B3, MV_PRED_MEDIAN,   BLK_8X8, ref[0], MVD_X0);
            if( p->b_error_flag )
            {
                printf("[error]MB %d mvd is wrong\n", p->i_mb_index );
                
                return -1;
            }
            mv_pred(p, MV_FWD_X1, MV_FWD_C2, MV_PRED_MEDIAN,   BLK_8X8, ref[1], MVD_X1);
            if( p->b_error_flag )
            {
                printf("[error]MB %d mvd is wrong\n", p->i_mb_index );
                
                return -1;
            }
            mv_pred(p, MV_FWD_X2, MV_FWD_X1, MV_PRED_MEDIAN,   BLK_8X8, ref[2], MVD_X2);
            if( p->b_error_flag )
            {
                printf("[error]MB %d mvd is wrong\n", p->i_mb_index );
                
                return -1;
            }
            mv_pred(p, MV_FWD_X3, MV_FWD_X0, MV_PRED_MEDIAN,   BLK_8X8, ref[3], MVD_X3);
            if( p->b_error_flag )
            {
                printf("[error]MB %d mvd is wrong\n", p->i_mb_index );
                
                return -1;
            }

#if DEBUG_P_INTER
            {
                int tmp;
                FILE *fp;

                fp = fopen("data_ref.dat","ab");
                fprintf(fp,"MB %d \n", p->i_mb_index );
                //fprintf(fp,"mv = (%d, %d)\n", (&p->mv[MV_FWD_X0])->x, (&p->mv[MV_FWD_X0])->y);
                fprintf(fp,"frame %d ref = %d \n", p->i_frame_decoded, (&p->mv[MV_FWD_X0])->ref );
                fprintf(fp,"frame %d ref = %d \n", p->i_frame_decoded, (&p->mv[MV_FWD_X1])->ref );
                fprintf(fp,"frame %d ref = %d \n", p->i_frame_decoded, (&p->mv[MV_FWD_X2])->ref );
                fprintf(fp,"frame %d ref = %d \n", p->i_frame_decoded, (&p->mv[MV_FWD_X3])->ref );
                fclose(fp);
            }
#endif
            
#if DEBUG_P_INTER
   {
        int tmp;
        FILE *fp;
        int i;

        fp = fopen("data_mv_8x8.dat","ab");
        fprintf(fp,"frame %d MB %d \n", p->i_frame_decoded, p->i_mb_index );
        for( i = 0; i < 24; i++ )
            fprintf(fp,"mv[%d] = (%d,%d) \n", i, 
                (&p->mv[i])->x,  (&p->mv[i])->y);
        fclose(fp);
   }
#endif

            break;

        default:
            printf("[error]MB %d type is wrong\n", p->i_mb_index);
            p->b_error_flag = 1;
            return -1;
            break;
    }
 
    /* save current mv, mvd, refidx into a frame, ignore neighbor info */
    /* mv copy */
    memcpy(p->mv_tab[p->i_mb_index], p->mv, 24*sizeof(xavs_vector));

    /* mvd copy */
    memcpy(p->p_mvd_tab[p->i_mb_index], p->p_mvd, 2*6*2*sizeof(int16_t));
    
    /* refidx copy */
    memcpy(p->p_ref_tab[p->i_mb_index], p->p_ref, 2*9*sizeof(int8_t));
    
#undef FWD
#undef MVD_X3
#undef MVD_X2
#undef MVD_X1
#undef MVD_X0
#undef REF_X3
#undef REF_X2
#undef REF_X1
#undef REF_X0

#if B_MB_WEIGHTING
	p->weighting_prediction = 0;
#endif
    if (i_mb_type != P_SKIP)
    {
#if B_MB_WEIGHTING
        if ( p->sh.b_slice_weighting_flag && p->sh.b_mb_weighting_flag )
        {
            if( p->ph.b_aec_enable )
			{
				p->weighting_prediction = cavs_cabac_get_mb_weighting_prediction( p );
			}
			else
				p->weighting_prediction = cavs_bitstream_get_bit1(&p->s);	//weighting_prediction
        }
#else 
        if (p->sh.b_mb_weighting_flag)
        {
            cavs_bitstream_get_bit1(&p->s); /* weighting_prediction */
        }
#endif
    }

#if B_MB_WEIGHTING
    p->weighting_prediction_tab[p->i_mb_index] = p->weighting_prediction;
#endif

    i_offset = (p->i_mb_y*p->i_mb_width + p->i_mb_x)*4;
    p->p_col_mv[i_offset+ 0] = p->mv[MV_FWD_X0];
    p->p_col_mv[i_offset + 1] = p->mv[MV_FWD_X1];
    p->p_col_mv[i_offset + 2] = p->mv[MV_FWD_X2];
    p->p_col_mv[i_offset + 3] = p->mv[MV_FWD_X3];

    p->i_qp_tab[p->i_mb_index] = p->i_qp; /* init for skip */
    if (i_mb_type != P_SKIP)
    {
        /* get coded block pattern */
        p->i_cbp_tab[p->i_mb_index] = p->bs_read[SYNTAX_INTER_CBP](p);
        if( p->b_error_flag )
        {
            printf("[error]MB cbp is wrong\n");
            return -1;
        }
        p->i_cbp = p->i_cbp_tab[p->i_mb_index];
        if( p->i_cbp_tab[p->i_mb_index] > 63) 
        {
            printf("[error]MB cbp is wrong\n");
            p->b_error_flag = 1;

            return -1;
        }
        
#if DEBUG_P_INTER
	{
		int tmp;
		FILE *fp;

		fp = fopen("data_cbp.dat","ab");
		fprintf(fp,"frame %d MB %d cbp = %d\n", p->i_frame_decoded, p->i_mb_index, p->i_cbp_tab[p->i_mb_index]);
		
		fclose(fp);
	}
#endif
        
        /* get quantizer */
        p->i_qp_tab[p->i_mb_index] = p->i_qp;
        if(p->i_cbp_tab[p->i_mb_index]  && !p->b_fixed_qp)
        {
#if 1
            int delta_qp =  p->bs_read[SYNTAX_DQP](p);
            
			p->i_qp_tab[p->i_mb_index] = (p->i_qp_tab[p->i_mb_index] -MIN_QP + delta_qp + (MAX_QP-MIN_QP+1))%(MAX_QP-MIN_QP+1)+MIN_QP;

            p->i_qp = p->i_qp_tab[p->i_mb_index];

            if( p->i_qp < 0 || p->i_qp > 63  || p->b_error_flag )
            {
                printf("[error]MB qp delta is wrong\n");
				p->b_error_flag = 1;
                return -1;  
            }
#else        
            p->i_qp = p->i_qp_tab[p->i_mb_index]  = (p->i_qp_tab[p->i_mb_index] + p->bs_read[SYNTAX_DQP](p)) & 63;
            if( p->b_error_flag )
            {
                printf("[error]MB qp delta is wrong\n");
                return -1;
            }
#endif            
        }
        else
            p->i_last_dqp = 0;

#if DEBUG_P_INTER
    {
		int tmp;
		FILE *fp;

		fp = fopen("data_qp.dat","ab");
		fprintf(fp,"frame %d MB %d \n", p->i_frame_decoded, p->i_mb_index);
		fprintf(fp,"qp = %d\n",p->i_qp_tab[p->i_mb_index]);
		
		fclose(fp);
	}
#endif

        i_ret = get_residual_inter_aec_opt(p, i_mb_type);
        if( i_ret == -1 )
            return -1;
    }

    *p->p_col_type = i_mb_type;

    return aec_next_mb(p);
}

static int get_mb_b_aec_opt( cavs_decoder *p )
{   
    int kk = 0; 
    xavs_vector mv[24];
    int block;
    enum xavs_mb_sub_type sub_type[4];
    int flags;
    int ref[4];    
    int i_ref_offset = p->ph.b_picture_structure == 0 ? 2 : 1;
    uint8_t i_col_type;
    xavs_vector *p_mv;
    int i_mb_type = p->i_mb_type_tab[p->i_mb_index];
    int16_t (*p_mvd)[6][2] = p->p_mvd;
    int8_t	(*p_ref)[9] = p->p_ref;
    int i_ret = 0;
    
    init_mb(p);

    p->mv[MV_FWD_X0] = MV_REF_DIR;
    copy_mvs(&p->mv[MV_FWD_X0], BLK_16X16);
    p->mv[MV_BWD_X0] = MV_REF_DIR;
    copy_mvs(&p->mv[MV_BWD_X0], BLK_16X16);

#define FWD		0
#define BWD		1
#define MVD_X0	1
#define MVD_X1	2
#define MVD_X2	4
#define MVD_X3	5
#define REF_X0  4
#define REF_X1  5
#define REF_X2  7
#define REF_X3  8

#if DEBUG_B_INTER
   {
        int tmp;
        FILE *fp;
        int i;

        fp = fopen("data_mv.dat","ab");
        fprintf(fp,"frame %d MB %d \n", p->i_frame_decoded, p->i_mb_index );
        for( i = 0; i < 24; i++ )
        {    
           fprintf(fp,"fmv[%d] = (%d,%d) \n", i, 
                (&p->mv[MV_FWD_X0])->x,  (&p->mv[MV_FWD_X0])->y);
           fprintf(fp,"bmv[%d] = (%d,%d) \n", i, 
                (&p->mv[MV_BWD_X0])->x,  (&p->mv[MV_BWD_X0])->y);
        }
        fclose(fp);
   }
#endif

    /* The MVD of pos X[0-3] have been initialized as 0
        The REF of pos X[0-3] have been initialized as -1 */
    switch(i_mb_type) 
    {
        case -1:
            printf("[error]MB %d type is wrong\n", p->i_mb_index);
            return -1;
            break;
        
        case B_SKIP:
        case B_DIRECT:
            get_b_direct_skip_mb(p);
            if(p->b_error_flag)
            {
                printf("[error]MB %d mv direct is wrong\n", p->i_mb_index );
                return -1;
            }
        
#if DEBUG_B_INTER
           {
                int tmp;
                FILE *fp;
                int i;

                fp = fopen("data_directmv.dat","ab");
                fprintf(fp,"frame %d MB %d \n", p->i_frame_decoded, p->i_mb_index );
                for( i = 0; i < 24; i++ )
                {    
                   fprintf(fp,"fmv[%d] = (%d,%d) \n", i, 
                        (&p->mv[MV_FWD_X0])->x,  (&p->mv[MV_FWD_X0])->y);
                   fprintf(fp,"bmv[%d] = (%d,%d) \n", i, 
                        (&p->mv[MV_BWD_X0])->x,  (&p->mv[MV_BWD_X0])->y);
                }
                fclose(fp);
           }
                        
            {
                int tmp;
                FILE *fp;

                fp = fopen("data_ref.dat","ab");
                fprintf(fp,"ref l0= %d \n", (&p->mv[MV_FWD_X0])->ref );
                fprintf(fp,"ref l1= %d \n", (&p->mv[MV_BWD_X0])->ref );
                fclose(fp);
            }
#endif

            break;
            
        case B_FWD_16X16:
            ref[0] = p->bs_read_ref_b(p, FWD, REF_X0);            
            if( ref[0] > 1 || p->b_error_flag )
            {
                printf("[error]MB %d refidx is wrong\n", p->i_mb_index );
                p->b_error_flag = 1;
                
                return -1;
            }
            
            mv_pred(p, MV_FWD_X0, MV_FWD_C2, MV_PRED_MEDIAN, BLK_16X16, ref[0]+i_ref_offset, MVD_X0);
            if( p->b_error_flag )
            {
                printf("[error]MB mvd is wrong\n");
                return -1;
            }
              
#if DEBUG_B_INTER
            {
                int tmp;
                FILE *fp;

                fp = fopen("data_ref.dat","ab");
                fprintf(fp,"frame %d Mb %d\n",p->i_frame_decoded, p->i_mb_index);
                fprintf(fp,"ref l0= %d \n", (&p->mv[MV_FWD_X0])->ref );
                fprintf(fp,"ref l1= %d \n", (&p->mv[MV_BWD_X0])->ref );
                fclose(fp);
            }
#endif
                        
            M32(p_mvd[FWD][MVD_X1]) = M32(p_mvd[FWD][MVD_X3]) = M32(p_mvd[FWD][MVD_X0]);
            p_ref[FWD][REF_X1] = p_ref[FWD][REF_X2] = p_ref[FWD][REF_X3] = p_ref[FWD][REF_X0];
            break;
            
        case B_SYM_16X16:
            ref[0] = p->bs_read_ref_b(p, FWD, REF_X0);
            if( ref[0] > 1 || p->b_error_flag )
            {
                printf("[error]MB %d refidx is wrong\n", p->i_mb_index );
                p->b_error_flag = 1;
                
                return -1;
            }
            mv_pred(p, MV_FWD_X0, MV_FWD_C2, MV_PRED_MEDIAN, BLK_16X16, ref[0]+i_ref_offset, MVD_X0);
            if( p->b_error_flag )
            {
                printf("[error]MB mvd is wrong\n");
                return -1;
            }
              
            mv_pred_sym(p, &p->mv[MV_FWD_X0], BLK_16X16,ref[0]);
            if(p->b_error_flag)
            {
                printf("[error]MB %d mv sym is wrong\n", p->i_mb_index );
                return -1;
            }
            M32(p_mvd[FWD][MVD_X1]) = M32(p_mvd[FWD][MVD_X3]) = M32(p_mvd[FWD][MVD_X0]);
            p_ref[FWD][REF_X1] = p_ref[FWD][REF_X2] = p_ref[FWD][REF_X3] = p_ref[FWD][REF_X0];
            break;
            
        case B_BWD_16X16:
            ref[0] = p->bs_read_ref_b(p, BWD, REF_X0);
            if( ref[0] > 1 || p->b_error_flag )
            {
                printf("[error]MB %d refidx is wrong\n", p->i_mb_index );
                p->b_error_flag = 1;
                
                return -1;
            }
            mv_pred(p, MV_BWD_X0, MV_BWD_C2, MV_PRED_MEDIAN, BLK_16X16, ref[0], MVD_X0);
            if( p->b_error_flag )
            {
                printf("[error]MB mvd is wrong\n");
                return -1;
            }
            
            M32(p_mvd[BWD][MVD_X1]) = M32(p_mvd[BWD][MVD_X3]) = M32(p_mvd[BWD][MVD_X0]);
            p_ref[BWD][REF_X1] = p_ref[BWD][REF_X2] = p_ref[BWD][REF_X3] = p_ref[BWD][REF_X0];
            break;
            
        case B_8X8:
            mv[MV_FWD_A1] = p->mv[MV_FWD_A1];
            mv[MV_FWD_B2] = p->mv[MV_FWD_B2];
            mv[MV_FWD_C2] = p->mv[MV_FWD_C2];
            mv[MV_FWD_D3] = p->mv[MV_FWD_D3];
            mv[MV_BWD_A1] = p->mv[MV_BWD_A1];
            mv[MV_BWD_B2] = p->mv[MV_BWD_B2];
            mv[MV_BWD_C2] = p->mv[MV_BWD_C2];
            mv[MV_BWD_D3] = p->mv[MV_BWD_D3];

            for(block = 0; block < 4; block++)
            {    
                sub_type[block] = (enum xavs_mb_sub_type)p->bs_read[SYNTAX_MB_PART_TYPE](p);
                if(sub_type[block] < 0 || sub_type[block] > 3 || p->b_error_flag )
                {
                    printf("[error]MB %d sub_type is wrong\n", p->i_mb_index );
                    p->b_error_flag = 1;
                
                    return -1;
                }
            }
            
            for( block = 0; block < 4; block++)
            {
                if(sub_type[block] == B_SUB_DIRECT || sub_type[block] == B_SUB_BWD)
                    ref[block] = 0;
                else
                    ref[block] = p->bs_read_ref_b(p, FWD, ref_scan[block]);
                  
                if( ref[block] > 1 || p->b_error_flag )
                {
                    printf("[error]MB %d refidx is wrong\n", p->i_mb_index );
                    p->b_error_flag = 1;
                
                    return -1;
                }
            }
            
            for(block = 0; block < 4; block++)
            {
                if(sub_type[block] == B_SUB_BWD)
                {
                    ref[block] = p->bs_read_ref_b(p, BWD, ref_scan[block]);
                }
                
                if( ref[block] > 1 || p->b_error_flag )
                {
                    printf("[error]MB %d refidx is wrong\n", p->i_mb_index );
                    p->b_error_flag = 1;
                
                    return -1;
                }
            }
            
            for( block = 0; block < 4; block++ ) 
            {
                switch(sub_type[block])
                {
                    case B_SUB_DIRECT:
                        get_col_info(p, &i_col_type, &p_mv, block);
                        if(!i_col_type)
                        {
                            mv_pred_sub_direct(p, mv, 0, mv_scan[block], mv_scan[block]-3,
                                    MV_PRED_BSKIP, BLK_8X8, i_ref_offset);
							if(p->b_error_flag)
								return -1;
                            mv_pred_sub_direct(p, mv, MV_BWD_OFFS, mv_scan[block]+MV_BWD_OFFS,
                                    mv_scan[block]-3+MV_BWD_OFFS,
                                    MV_PRED_BSKIP, BLK_8X8, 0);
                        } 
                        else
                        {
                            get_b_direct_skip_sub_mb(p, block, p_mv);
                        }
						                            
                        if(p->b_error_flag)
                        {
                            printf("[error]MB %d mv direct is wrong\n", p->i_mb_index );
                            return -1;
                        }

                        break;
                        
                    case B_SUB_FWD:
                        mv_pred(p, mv_scan[block], mv_scan[block]-3,
                                    MV_PRED_MEDIAN, BLK_8X8, ref[block]+i_ref_offset, mvd_scan[block]);
                        if( p->b_error_flag )
                        {
                            printf("[error]MB mvd is wrong\n");
                            return -1;
                        }
                        break;
                        
                    case B_SUB_SYM:
                        mv_pred(p, mv_scan[block], mv_scan[block]-3,
                                        MV_PRED_MEDIAN, BLK_8X8, ref[block]+i_ref_offset, mvd_scan[block]);
                        if( p->b_error_flag )
                        {
                            printf("[error]MB mvd is wrong\n");
                            return -1;
                        }
                        mv_pred_sym(p, &p->mv[mv_scan[block]], BLK_8X8,ref[block]);
                        if(p->b_error_flag)
                        {
                            printf("[error]MB %d mv sym is wrong\n", p->i_mb_index );
                            return -1;
                        }
                        break;
                    default:
                    	break;
                }
            }
            for( block = 0; block < 4; block++ )
            {
                if(sub_type[block] == B_SUB_BWD)
                {
                    mv_pred(p, mv_scan[block]+MV_BWD_OFFS,
                            mv_scan[block]+MV_BWD_OFFS-3,
                            MV_PRED_MEDIAN, BLK_8X8, ref[block], mvd_scan[block]);
                    if( p->b_error_flag )
                    {
                        printf("[error]MB mvd is wrong\n");
                        return -1;
                    }
                }
            }
            break;
        default:
            flags = partition_flags[i_mb_type];
            if( i_mb_type <= B_SYM_16X16 )
            {
                printf("[error]MB %d mb_type is wrong\n", p->i_mb_index );
                p->b_error_flag = 1;

                return -1;
            }
            if(i_mb_type & 1) /* 16x8 macroblock types */
            { 
                int i = 0, k = 0;
                
                if (flags & FWD0)
                	ref[i++] = p->bs_read_ref_b(p, FWD, REF_X0);
                if (flags & FWD1)
                	ref[i++] = p->bs_read_ref_b(p, FWD, REF_X2);
                if (flags & BWD0)
                	ref[i++] = p->bs_read_ref_b(p, BWD, REF_X0);
                if (flags & BWD1)
                	ref[i++] = p->bs_read_ref_b(p, BWD, REF_X2);
                if( p->b_error_flag )
                {
                    printf("[error]MB refidx is wrong\n");
                    return -1;
                }

                k = i;
                for( i = 0 ; i < k; i++ )
                {
                    if( ref[i] > 1 )
                    {
                        printf("[error]MB %d refidx is wrong\n", p->i_mb_index );
                        p->b_error_flag = 1;
                
                        return -1;
                    }    
                }
                
                if(flags & FWD0)
                {
                    mv_pred(p, MV_FWD_X0, MV_FWD_C2, MV_PRED_TOP,  BLK_16X8, ref[kk++]+i_ref_offset, MVD_X0);
                    if( p->b_error_flag )
                    {
                        printf("[error]MB mvd is wrong\n");
                        return -1;
                    }
                    CP32(p_mvd[FWD][MVD_X1], p_mvd[FWD][MVD_X0]);
                    p_ref[FWD][REF_X1] = p_ref[FWD][REF_X0];
                }
                
                if(flags & SYM0)
                {    
                    mv_pred_sym(p, &p->mv[MV_FWD_X0], BLK_16X8,ref[kk-1]);
                    if(p->b_error_flag)
                    {
                        printf("[error]MB %d mv sym is wrong\n", p->i_mb_index );
                        return -1;
                    }
                }
                if(flags & FWD1)
                {
                    mv_pred(p, MV_FWD_X2, MV_FWD_A1, MV_PRED_LEFT, BLK_16X8, ref[kk++]+i_ref_offset, MVD_X2);
                    if( p->b_error_flag )
                    {
                        printf("[error]MB mvd is wrong\n");
                        return -1;
                    }
                      
                    CP32(p_mvd[FWD][MVD_X3], p_mvd[FWD][MVD_X2]);
                    p_ref[FWD][REF_X3] = p_ref[FWD][REF_X2];
                }
                if(flags & SYM1)
                {    
                    mv_pred_sym(p, &p->mv[MV_FWD_X2], BLK_16X8,ref[kk-1]);
                    if(p->b_error_flag)
                    {
                        printf("[error]MB %d mv sym is wrong\n", p->i_mb_index );
                        return -1;
                    }
                }
                if(flags & BWD0)
                {
                    mv_pred(p, MV_BWD_X0, MV_BWD_C2, MV_PRED_TOP,  BLK_16X8, ref[kk++], MVD_X0);
                    if( p->b_error_flag )
                    {
                        printf("[error]MB mvd is wrong\n");
                        return -1;
                    }
                      
                    CP32(p_mvd[BWD][MVD_X1], p_mvd[BWD][MVD_X0]);
                    p_ref[BWD][REF_X1] = p_ref[BWD][REF_X0];
                }
                if(flags & BWD1)
                {
                    mv_pred(p, MV_BWD_X2, MV_BWD_A1, MV_PRED_LEFT, BLK_16X8, ref[kk++], MVD_X2);
                    if( p->b_error_flag )
                    {
                        printf("[error]MB mvd is wrong\n");
                        return -1;
                    }
                      
                    CP32(p_mvd[BWD][MVD_X3], p_mvd[BWD][MVD_X2]);
                    p_ref[BWD][REF_X3] = p_ref[BWD][REF_X2];
                }
            } 
            else  /* 8x16 macroblock types */
            {
                int i = 0, k = 0;
                
                if (flags & FWD0)
                	ref[i++] = p->bs_read_ref_b(p, FWD, REF_X0);
                if (flags & FWD1)
                	ref[i++] = p->bs_read_ref_b(p, FWD, REF_X1);
                if (flags & BWD0)
                	ref[i++] = p->bs_read_ref_b(p, BWD, REF_X0);
                if (flags & BWD1)
                	ref[i++] = p->bs_read_ref_b(p, BWD, REF_X1);
                if( p->b_error_flag )
                {
                    printf("[error]MB refidx is wrong\n");
                    return -1;
                }
                
                k = i;
                for( i = 0 ; i < k; i++ )
                {
                    if( ref[i] > 1 )
                    {
                        printf("[error]MB %d refidx is wrong\n", p->i_mb_index );
                        p->b_error_flag = 1;
                
                        return -1;
                    }    
                }
                           
                if(flags & FWD0)
                {
                    mv_pred(p, MV_FWD_X0, MV_FWD_B3, MV_PRED_LEFT, BLK_8X16, ref[kk++]+i_ref_offset, MVD_X0);
                    if( p->b_error_flag )
                    {
                        printf("[error]MB mvd is wrong\n");
                        return -1;
                    }
                	 p_ref[FWD][REF_X2] = p_ref[FWD][REF_X0];
                }
                if(flags & SYM0)
                {    
                    mv_pred_sym(p, &p->mv[MV_FWD_X0], BLK_8X16,ref[kk-1]);
                    if(p->b_error_flag)
                    {
                        printf("[error]MB %d mv sym is wrong\n", p->i_mb_index );
                        return -1;
                    }
                }
                if(flags & FWD1)
                {
                    mv_pred(p, MV_FWD_X1, MV_FWD_C2, MV_PRED_TOPRIGHT,BLK_8X16, ref[kk++]+i_ref_offset, MVD_X1);
                    if( p->b_error_flag )
                    {
                        printf("[error]MB mvd is wrong\n");
                        return -1;
                    }
                	CP32(p_mvd[FWD][MVD_X3], p_mvd[FWD][MVD_X1]);
                	p_ref[FWD][REF_X3] = p_ref[FWD][REF_X1];
                }
                if(flags & SYM1)
                {
                    mv_pred_sym(p, &p->mv[MV_FWD_X1], BLK_8X16,ref[kk-1]);
                    if(p->b_error_flag)
                    {
                        printf("[error]MB %d mv sym is wrong\n", p->i_mb_index );
                        return -1;
                    }
                }
                
                if(flags & BWD0)
                {
                    mv_pred(p, MV_BWD_X0, MV_BWD_B3, MV_PRED_LEFT, BLK_8X16, ref[kk++], MVD_X0);
                    if( p->b_error_flag )
                    {
                        printf("[error]MB mvd is wrong\n");
                        return -1;
                    }
                	p_ref[BWD][REF_X2] = p_ref[BWD][REF_X0];
                }
                if(flags & BWD1)
                {
                	mv_pred(p, MV_BWD_X1, MV_BWD_C2, MV_PRED_TOPRIGHT,BLK_8X16, ref[kk++], MVD_X1);
                    if( p->b_error_flag )
                    {
                        printf("[error]MB mvd is wrong\n");
                        return -1;
                    }
                	CP32(p_mvd[BWD][MVD_X3], p_mvd[BWD][MVD_X1]);
                	p_ref[BWD][REF_X3] = p_ref[BWD][REF_X1];
                }
            }
    }

    /* save current mv, mvd, refidx into a frame, ignore neighbor info */
    /* mv copy */
    memcpy(p->mv_tab[p->i_mb_index], p->mv, 24*sizeof(xavs_vector));

    /* mvd copy */
    memcpy(p->p_mvd_tab[p->i_mb_index], p->p_mvd, 2*6*2*sizeof(int16_t));

    /* refidx copy */
    memcpy(p->p_ref_tab[p->i_mb_index], p->p_ref, 2*9*sizeof(int8_t));
    
#undef FWD
#undef BWD
#undef MVD_X3
#undef MVD_X2
#undef MVD_X1
#undef MVD_X0
#undef REF_X3
#undef REF_X2
#undef REF_X1
#undef REF_X0

#if B_MB_WEIGHTING
	p->weighting_prediction = 0;
#endif
    if (i_mb_type != B_SKIP)
    {
#if B_MB_WEIGHTING
        if ( p->sh.b_slice_weighting_flag && p->sh.b_mb_weighting_flag )
        {
            if( p->ph.b_aec_enable )
            {
                p->weighting_prediction = cavs_cabac_get_mb_weighting_prediction( p );
            }
            else
                p->weighting_prediction = cavs_bitstream_get_bit1(&p->s);   //weighting_prediction
        }
#else 
        if (p->sh.b_mb_weighting_flag)
        {
            cavs_bitstream_get_bit1(&p->s); /* weighting_prediction */
        }
#endif

    }

#if B_MB_WEIGHTING
	p->weighting_prediction_tab[p->i_mb_index] = p->weighting_prediction;
#endif
    
    p->i_qp_tab[p->i_mb_index] = p->i_qp;  /* init for skip */
    if(i_mb_type != B_SKIP)
    {
        /* get coded block pattern */
        p->i_cbp_tab[p->i_mb_index] = p->bs_read[SYNTAX_INTER_CBP](p);
        if( p->b_error_flag )
        {
            printf("[error]MB cbp is wrong\n");
            return -1;
        }
        p->i_cbp = p->i_cbp_tab[p->i_mb_index];
        if( p->i_cbp_tab[p->i_mb_index] > 63) 
        {
            printf("[error]MB cbp is wrong\n");
            p->b_error_flag = 1;

            return -1;
        }
        
#if DEBUG_B_INTER
	 {
		int tmp;
		FILE *fp;

		fp = fopen("data_cbp.dat","ab");
		fprintf(fp,"frame %d MB %d \n", p->i_frame_decoded, p->i_mb_index);
		fprintf(fp,"cbp = %d\n",p->i_cbp_tab[p->i_mb_index]);
		
		fclose(fp);
	 }
#endif

        /* get quantizer */
        p->i_qp_tab[p->i_mb_index] = p->i_qp;
        if(p->i_cbp_tab[p->i_mb_index]  && !p->b_fixed_qp)
        {  
#if 1
            int delta_qp =  p->bs_read[SYNTAX_DQP](p);
            
			p->i_qp_tab[p->i_mb_index] = (p->i_qp_tab[p->i_mb_index] -MIN_QP + delta_qp + (MAX_QP-MIN_QP+1))%(MAX_QP-MIN_QP+1)+MIN_QP;

            p->i_qp = p->i_qp_tab[p->i_mb_index];

            if( p->i_qp < 0 || p->i_qp > 63  || p->b_error_flag )
            {
                printf("[error]MB qp delta is wrong\n");
				p->b_error_flag = 1;

                return -1;  
            }
#else        
            p->i_qp = p->i_qp_tab[p->i_mb_index]  = (p->i_qp_tab[p->i_mb_index] + p->bs_read[SYNTAX_DQP](p)) & 63;
            if( p->b_error_flag )
            {
                printf("[error]MB qp delta is wrong\n");
                return -1;
            }
#endif            
        }
        else
            p->i_last_dqp = 0;

#if DEBUG_B_INTER
    	{
		int tmp;
		FILE *fp;

		fp = fopen("data_qp.dat","ab");
		fprintf(fp,"frame %d MB %d \n", p->i_frame_decoded, p->i_mb_index);
		fprintf(fp,"qp = %d\n",p->i_qp_tab[p->i_mb_index]);
		
		fclose(fp);
	 }        
#endif
       
        i_ret = get_residual_inter_aec_opt(p, i_mb_type);
        if( i_ret == -1 )
            return -1;
    }
    
    return aec_next_mb(p);
}

static int get_mb_i_rec_opt( cavs_decoder *p )
{
    uint8_t i_top[18];
    uint8_t *p_left = NULL;
    uint8_t *p_d;

    static const uint8_t scan5x5[4][4] = {
    	{6, 7, 11, 12},
    	{8, 9, 13, 14},
    	{16, 17, 21, 22},
    	{18, 19, 23, 24}
    };

    int i;
    //int i_offset = p->i_mb_x<<2;
    //int i_mb_type = p->i_cbp_code;
    DECLARE_ALIGNED_16(uint8_t edge_8x8[33]);
    int i_qp_cb = 0, i_qp_cr = 0;
    int ret = 0;

    //init_mb(p); // not need this in this module
	if(p->i_cbp_tab[p->i_mb_index] < 0 || p->i_cbp_tab[p->i_mb_index] > 63 
		|| p->i_qp_tab[p->i_mb_index] < 0
		|| p->i_qp_tab[p->i_mb_index] > 63 )
	{
		printf("[error] wrong qp or cbp \n");
		return -1;
	}

#if HAVE_MMX
    cavs_mb_neighbour_avail( p );
#endif

    for( i = 0; i < 4; i++ )
    {
        int i_mode;

        p_d = p->p_y + p->i_luma_offset[i];

        load_intra_pred_luma(p, i_top, &p_left, i);

#if HAVE_MMX
        cavs_predict_8x8_filter(p, edge_8x8, i_top, p_left, p->mb.i_neighbour8[i]);
#endif

        {
            i_mode = p->i_intra_pred_mode_y_tab[p->i_mb_index][scan5x5[i][0]];

            if( i_mode > 7 || i_mode < 0 )
            {
                printf("rec i_pred_mode_luma is wrong\n");
                return -1;
            }
            
#if !HAVE_MMX
            intra_pred_lum[i_mode](p_d, edge_8x8, p->cur.i_stride[0], i_top, p_left);
#else
            p->cavs_intra_luma[i_mode](p_d, edge_8x8, p->cur.i_stride[0], i_top, p_left);
#endif

            if(p->i_cbp_tab[p->i_mb_index] & (1<<i))
            {
                ret = get_residual_block_rec_opt( p, intra_2dvlc, 1, p->i_qp_tab[p->i_mb_index], p_d, p->cur.i_stride[0], 0, i);
                if(ret == -1)
                {
                	return -1;
                }
            }
        }
    }

    load_intra_pred_chroma(p);

    if(p->i_pred_mode_chroma_tab[p->i_mb_index]>6 || p->i_pred_mode_chroma_tab[p->i_mb_index] < 0)
    {
        printf("rec i_pred_mode_chroma is wrong\n");
        return -1;
    }

#if !HAVE_MMX
       intra_pred_chroma[p->i_pred_mode_chroma_tab[p->i_mb_index]](p->p_cb, p->mb.i_neighbour, p->cur.i_stride[1], &p->p_top_border_cb[p->i_mb_x*10], p->i_left_border_cb);

       intra_pred_chroma[p->i_pred_mode_chroma_tab[p->i_mb_index]](p->p_cr, p->mb.i_neighbour, p->cur.i_stride[2], &p->p_top_border_cr[p->i_mb_x*10], p->i_left_border_cr);
#else
       p->cavs_intra_chroma[p->i_pred_mode_chroma_tab[p->i_mb_index]](p->p_cb, p->mb.i_neighbour, p->cur.i_stride[1], &p->p_top_border_cb[p->i_mb_x*10], p->i_left_border_cb);
       p->cavs_intra_chroma[p->i_pred_mode_chroma_tab[p->i_mb_index]](p->p_cr, p->mb.i_neighbour, p->cur.i_stride[2], &p->p_top_border_cr[p->i_mb_x*10], p->i_left_border_cr);
#endif
    
    /* FIXIT : weighting quant qp for chroma
        NOTE : not modify p->i_qp directly
    */
    if( p->b_weight_quant_enable && !p->ph.chroma_quant_param_disable )
    { 
        i_qp_cb = clip3_int( p->i_qp_tab[p->i_mb_index] + p->ph.chroma_quant_param_delta_u, 0, 63 );
        i_qp_cr = clip3_int( p->i_qp_tab[p->i_mb_index] + p->ph.chroma_quant_param_delta_v, 0, 63 );
    }

    if(p->i_cbp_tab[p->i_mb_index] & (1<<4))
    {
        ret = get_residual_block_rec_opt( p, chroma_2dvlc, 0, 
                chroma_qp[ p->b_weight_quant_enable 
                && !p->ph.chroma_quant_param_disable? i_qp_cb : p->i_qp_tab[p->i_mb_index]],
                p->p_cb,p->cur.i_stride[1], 1, 4);
        if(ret == -1)
        {
            return -1;
        }
    }
    if(p->i_cbp_tab[p->i_mb_index] & (1<<5))
    {
        ret = get_residual_block_rec_opt( p, chroma_2dvlc, 0, 
                 chroma_qp[ p->b_weight_quant_enable 
                 && !p->ph.chroma_quant_param_disable? i_qp_cr : p->i_qp_tab[p->i_mb_index]],
                 p->p_cr, p->cur.i_stride[2], 1, 5);
        if(ret == -1)
        {
            return -1;
        }
    }

    filter_mb(p, I_8X8);
    p->mv[MV_FWD_X0] = MV_INTRA;
    copy_mvs(&p->mv[MV_FWD_X0], BLK_16X16);
    p->mv[MV_BWD_X0] = MV_INTRA;
    copy_mvs(&p->mv[MV_BWD_X0], BLK_16X16);

    return rec_next_mb(p); 
}

static int get_mb_p_rec_opt( cavs_decoder *p )
{
    int i_mb_type = p->i_mb_type_tab[p->i_mb_index];
    int ret = 0;

#define FWD		0
#define MVD_X0  1
#define MVD_X1	2
#define MVD_X2  4
#define MVD_X3	5
#define REF_X0  4
#define REF_X1	5
#define REF_X2	7
#define REF_X3  8

#if 0
    /* read current mv, mvd , refidx */
    /** mv cache 
    0:    D3  B2  B3  C2
    4:    A1  X0  X1   -
    8:    A3  X2  X3   - */
    p->mv[MV_FWD_X0] = p->mv_tab[p->i_mb_index][MV_FWD_X0];
    p->mv[MV_FWD_X1] = p->mv_tab[p->i_mb_index][MV_FWD_X1];
    p->mv[MV_FWD_X2] = p->mv_tab[p->i_mb_index][MV_FWD_X2];
    p->mv[MV_FWD_X3] = p->mv_tab[p->i_mb_index][MV_FWD_X3];
    p->mv[MV_FWD_A1] = p->mv_tab[p->i_mb_index][MV_FWD_A1];/* following four used for deblock */
    p->mv[MV_FWD_A3] = p->mv_tab[p->i_mb_index][MV_FWD_A3];
    p->mv[MV_FWD_B2] = p->mv_tab[p->i_mb_index][MV_FWD_B2];
    p->mv[MV_FWD_B3] = p->mv_tab[p->i_mb_index][MV_FWD_B3];
#else
    p->mv[MV_FWD_B2] = p->mv_tab[p->i_mb_index][MV_FWD_B2];
    p->mv[MV_FWD_B3] = p->mv_tab[p->i_mb_index][MV_FWD_B3];
    p->mv[MV_FWD_A1] = p->mv_tab[p->i_mb_index][MV_FWD_A1];/* following four used for deblock */
    p->mv[MV_FWD_X0] = p->mv_tab[p->i_mb_index][MV_FWD_X0];
    p->mv[MV_FWD_X1] = p->mv_tab[p->i_mb_index][MV_FWD_X1];
    p->mv[MV_FWD_A3] = p->mv_tab[p->i_mb_index][MV_FWD_A3];
    p->mv[MV_FWD_X2] = p->mv_tab[p->i_mb_index][MV_FWD_X2];
    p->mv[MV_FWD_X3] = p->mv_tab[p->i_mb_index][MV_FWD_X3];
#endif

    /** mvd cache
    0:    A1  X0  X1
    3:    A3  X2  X3   */
    CP32(p->p_mvd[FWD][MVD_X0], p->p_mvd_tab[p->i_mb_index][FWD][MVD_X0]);
    CP32(p->p_mvd[FWD][MVD_X1], p->p_mvd_tab[p->i_mb_index][FWD][MVD_X1]);
    CP32(p->p_mvd[FWD][MVD_X2], p->p_mvd_tab[p->i_mb_index][FWD][MVD_X2]);
    CP32(p->p_mvd[FWD][MVD_X3], p->p_mvd_tab[p->i_mb_index][FWD][MVD_X3]);

    /** ref cache
    0:    D3  B2  B3
    3:    A1  X0  X1
    6:    A3  X2  X3   */
    p->p_ref[FWD][REF_X0] = p->p_ref_tab[p->i_mb_index][FWD][REF_X0];    
    p->p_ref[FWD][REF_X1] = p->p_ref_tab[p->i_mb_index][FWD][REF_X1];
    p->p_ref[FWD][REF_X2] = p->p_ref_tab[p->i_mb_index][FWD][REF_X2];
    p->p_ref[FWD][REF_X3] = p->p_ref_tab[p->i_mb_index][FWD][REF_X3];
 
#undef FWD
#undef MVD_X3
#undef MVD_X2
#undef MVD_X1
#undef MVD_X0
#undef REF_X3
#undef REF_X2
#undef REF_X1
#undef REF_X0

#if B_MB_WEIGHTING
	p->weighting_prediction = p->weighting_prediction_tab[p->i_mb_index];
#endif
    
    inter_pred(p, i_mb_type);

    if (i_mb_type != P_SKIP)
    {
        ret = get_residual_inter_rec_opt(p, i_mb_type);
        if(ret == -1)
        {
            return -1;
        }
    }

    filter_mb(p, i_mb_type);

    return rec_next_mb(p);   
}


static int get_mb_b_rec_opt( cavs_decoder *p )
{   
    int i_mb_type = p->i_mb_type_tab[p->i_mb_index];//p->i_mb_type;
    int ret = 0;

#define FWD		0
#define BWD		1
#define MVD_X0	1
#define MVD_X1	2
#define MVD_X2	4
#define MVD_X3	5
#define REF_X0  4
#define REF_X1  5
#define REF_X2  7
#define REF_X3  8

#if 0
    /* read current mv, mvd , refidx */
    /** mv cache 
    0:    D3  B2  B3  C2
    4:    A1  X0  X1   -
    8:    A3  X2  X3   - */
    p->mv[MV_FWD_X0] = p->mv_tab[p->i_mb_index][MV_FWD_X0];
    p->mv[MV_FWD_X1] = p->mv_tab[p->i_mb_index][MV_FWD_X1];
    p->mv[MV_FWD_X2] = p->mv_tab[p->i_mb_index][MV_FWD_X2];
    p->mv[MV_FWD_X3] = p->mv_tab[p->i_mb_index][MV_FWD_X3];
    p->mv[MV_BWD_X0] = p->mv_tab[p->i_mb_index][MV_BWD_X0];
    p->mv[MV_BWD_X1] = p->mv_tab[p->i_mb_index][MV_BWD_X1];
    p->mv[MV_BWD_X2] = p->mv_tab[p->i_mb_index][MV_BWD_X2];
    p->mv[MV_BWD_X3] = p->mv_tab[p->i_mb_index][MV_BWD_X3];

    p->mv[MV_FWD_A1] = p->mv_tab[p->i_mb_index][MV_FWD_A1];/* following four used for deblock */
    p->mv[MV_FWD_A3] = p->mv_tab[p->i_mb_index][MV_FWD_A3];
    p->mv[MV_FWD_B2] = p->mv_tab[p->i_mb_index][MV_FWD_B2];
    p->mv[MV_FWD_B3] = p->mv_tab[p->i_mb_index][MV_FWD_B3];
    p->mv[MV_BWD_A1] = p->mv_tab[p->i_mb_index][MV_BWD_A1];/* following four used for deblock */
    p->mv[MV_BWD_A3] = p->mv_tab[p->i_mb_index][MV_BWD_A3];
    p->mv[MV_BWD_B2] = p->mv_tab[p->i_mb_index][MV_BWD_B2];
    p->mv[MV_BWD_B3] = p->mv_tab[p->i_mb_index][MV_BWD_B3];
#else
    p->mv[MV_FWD_B2] = p->mv_tab[p->i_mb_index][MV_FWD_B2];
    p->mv[MV_FWD_B3] = p->mv_tab[p->i_mb_index][MV_FWD_B3];
    p->mv[MV_FWD_A1] = p->mv_tab[p->i_mb_index][MV_FWD_A1];/* following four used for deblock */
    p->mv[MV_FWD_X0] = p->mv_tab[p->i_mb_index][MV_FWD_X0];
    p->mv[MV_FWD_X1] = p->mv_tab[p->i_mb_index][MV_FWD_X1];
    p->mv[MV_FWD_A3] = p->mv_tab[p->i_mb_index][MV_FWD_A3];
    p->mv[MV_FWD_X2] = p->mv_tab[p->i_mb_index][MV_FWD_X2];
    p->mv[MV_FWD_X3] = p->mv_tab[p->i_mb_index][MV_FWD_X3];

    p->mv[MV_BWD_B2] = p->mv_tab[p->i_mb_index][MV_BWD_B2];
    p->mv[MV_BWD_B3] = p->mv_tab[p->i_mb_index][MV_BWD_B3];
    p->mv[MV_BWD_A1] = p->mv_tab[p->i_mb_index][MV_BWD_A1];/* following four used for deblock */
    p->mv[MV_BWD_X0] = p->mv_tab[p->i_mb_index][MV_BWD_X0];
    p->mv[MV_BWD_X1] = p->mv_tab[p->i_mb_index][MV_BWD_X1];
    p->mv[MV_BWD_A3] = p->mv_tab[p->i_mb_index][MV_BWD_A3];
    p->mv[MV_BWD_X2] = p->mv_tab[p->i_mb_index][MV_BWD_X2];
    p->mv[MV_BWD_X3] = p->mv_tab[p->i_mb_index][MV_BWD_X3];

#endif

    /** mvd cache
    0:    A1  X0  X1
    3:    A3  X2  X3   */
    CP32(p->p_mvd[FWD][MVD_X0], p->p_mvd_tab[p->i_mb_index][FWD][MVD_X0]);
    CP32(p->p_mvd[FWD][MVD_X1], p->p_mvd_tab[p->i_mb_index][FWD][MVD_X1]);
    CP32(p->p_mvd[FWD][MVD_X2], p->p_mvd_tab[p->i_mb_index][FWD][MVD_X2]);
    CP32(p->p_mvd[FWD][MVD_X3], p->p_mvd_tab[p->i_mb_index][FWD][MVD_X3]);
    CP32(p->p_mvd[BWD][MVD_X0], p->p_mvd_tab[p->i_mb_index][BWD][MVD_X0]);
    CP32(p->p_mvd[BWD][MVD_X1], p->p_mvd_tab[p->i_mb_index][BWD][MVD_X1]);
    CP32(p->p_mvd[BWD][MVD_X2], p->p_mvd_tab[p->i_mb_index][BWD][MVD_X2]);
    CP32(p->p_mvd[BWD][MVD_X3], p->p_mvd_tab[p->i_mb_index][BWD][MVD_X3]);

    /** ref cache
    0:    D3  B2  B3
    3:    A1  X0  X1
    6:    A3  X2  X3   */
    p->p_ref[FWD][REF_X0] = p->p_ref_tab[p->i_mb_index][FWD][REF_X0];    
    p->p_ref[FWD][REF_X1] = p->p_ref_tab[p->i_mb_index][FWD][REF_X1];
    p->p_ref[FWD][REF_X2] = p->p_ref_tab[p->i_mb_index][FWD][REF_X2];
    p->p_ref[FWD][REF_X3] = p->p_ref_tab[p->i_mb_index][FWD][REF_X3];
    p->p_ref[BWD][REF_X0] = p->p_ref_tab[p->i_mb_index][BWD][REF_X0];    
    p->p_ref[BWD][REF_X1] = p->p_ref_tab[p->i_mb_index][BWD][REF_X1];
    p->p_ref[BWD][REF_X2] = p->p_ref_tab[p->i_mb_index][BWD][REF_X2];
    p->p_ref[BWD][REF_X3] = p->p_ref_tab[p->i_mb_index][BWD][REF_X3];

#undef FWD
#undef BWD
#undef MVD_X3
#undef MVD_X2
#undef MVD_X1
#undef MVD_X0
#undef REF_X3
#undef REF_X2
#undef REF_X1
#undef REF_X0

#if B_MB_WEIGHTING
	p->weighting_prediction = p->weighting_prediction_tab[p->i_mb_index];
#endif

    inter_pred(p, i_mb_type);

    if(i_mb_type != B_SKIP)
    {
        ret = get_residual_inter_rec_opt(p, i_mb_type);
        if(ret == -1)
        {
            return -1;
        }
    }
    
    filter_mb(p, i_mb_type);
   
    return rec_next_mb(p);
}

/*================================= slice level ========================================================*/

#endif
