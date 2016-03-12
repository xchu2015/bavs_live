/*****************************************************************************
 * cavlc.h: 2d-VLC
 *****************************************************************************
*/
 
#ifndef _CAVS_VLC_H
#define _CAVS_VLC_H
#include<stdio.h>
#include<stdint.h>

#define MAX_CODED_FRAME_SIZE 2000000         //!< bytes for one frame
//#define SVA_STREAM_BUF_SIZE 1024

typedef signed char int8_t;
typedef unsigned char  uint8_t;
typedef short  int16_t;
typedef unsigned short  uint16_t;
typedef int  int32_t;
typedef unsigned  int uint32_t;
#if 0 
#ifdef __GNUC__
typedef long long int int64_t;
typedef unsigned  long long int uint64_t;
#else
typedef __int64  int64_t;
typedef unsigned __int64 uint64_t;
#endif
#endif
typedef short DCTELEM;
#define unaligned16(a) (*(const uint16_t*)(a))
#define unaligned32(a) (*(const uint32_t*)(a))
#define unaligned64(a) (*(const uint64_t*)(a))

#ifdef _MSC_VER
#define inline __inline
#endif

enum profile_e
{
  PROFILE_JIZHUN	  = 0x20,
  PROFILE_SHENZHAN	  = 0x24,
  PROFILE_YIDONG	  = 0x34,
  PROFILE_GUANGDIAN   = 0x48,
  PROFILE_JIAQIANG	  = 0x88,
};

//Tech. of GuangDian Profile
#define	PSKIP_REF	 p->ph.b_pb_field_enhanced
#define NEW_DIRECT	 p->ph.b_pb_field_enhanced

#define XAVS_EDGE    16
#define XAVS_MB_SIZE  16
#define XAVS_MB4x4_SIZE 8

#define XAVS_I_PICTURE 0
#define XAVS_P_PICTURE 1
#define XAVS_B_PICTURE 2  
#define BACKGROUND_IMG 3
#define   BP_IMG 4

#define MAX_NEG_CROP 1024//384

#define XAVS_MIN(a,b) ( (a)<(b) ? (a) : (b) )
#define XAVS_MAX(a,b) ( (a)>(b) ? (a) : (b) )
#define XAVS_MIN3(a,b,c) XAVS_MIN((a),XAVS_MIN((b),(c)))
#define XAVS_MAX3(a,b,c) XAVS_MAX((a),XAVS_MAX((b),(c)))
#define XAVS_MIN4(a,b,c,d) XAVS_MIN((a),XAVS_MIN3((b),(c),(d)))
#define XAVS_MAX4(a,b,c,d) XAVS_MAX((a),XAVS_MAX3((b),(c),(d)))
#define XAVS_XCHG(type,a,b) { type t = a; a = b; b = t; }
#define XAVS_FIX8(f) ((int)(f*(1<<8)+.5))
#define XAVS_SAFE_FREE(p) if(p){cavs_free(p);p=0;}
#define LEFT_ADAPT_INDEX_L 0
#define TOP_ADAPT_INDEX_L  1
#define LEFT_ADAPT_INDEX_C 2
#define TOP_ADAPT_INDEX_C  3
#define XAVS_THREAD_MAX 128 //64
#define XAVS_BFRAME_MAX 16

enum xavs_intra_lum_4x4_pred_mode
{
	I_PRED_4x4_V  = 8,
	I_PRED_4x4_H  = 9,
	I_PRED_4x4_DC = 10,
	I_PRED_4x4_DDL= 11,
	I_PRED_4x4_DDR= 12,
	I_PRED_4x4_HD = 13,
	I_PRED_4x4_VL = 14,
	I_PRED_4x4_HU = 15,
	I_PRED_4x4_VR = 16,

	I_PRED_4x4_DC_LEFT = 17,
	I_PRED_4x4_DC_TOP  = 18,
	I_PRED_4x4_DC_128  = 19,
};
enum xavs_intra_lum_pred_mode {
     INTRA_L_VERT=0,
     INTRA_L_HORIZ,
     INTRA_L_LP,
     INTRA_L_DOWN_LEFT,
     INTRA_L_DOWN_RIGHT,
     INTRA_L_LP_LEFT,
     NTRA_L_LP_TOP,
     INTRA_L_DC_128
};
enum xavs_intra_chrom_pred_mode {
     INTRA_C_DC=0,
     INTRA_C_HORIZ,
     INTRA_C_VERT,
     INTRA_C_PLANE,
     INTRA_C_DC_LEFT,
     INTRA_C_DC_TOP,
     INTRA_C_DC_128,
};

static const int xavs_mb_pred_mode8x8c[7] = {
	INTRA_C_DC, INTRA_C_HORIZ, INTRA_C_VERT, INTRA_C_PLANE,
	INTRA_C_DC, INTRA_C_DC, INTRA_C_DC
};

#define NO_INTRA_PMODE 8
# define INTRA_PMODE_4x4 8
static const int8_t pred4x4[10][10] = {
	{2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
	{2, 0, 0, 0, 0, 0, 0, 0, 0, 0},   // mdou new 3
	{2, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{2, 0, 1, 2, 3, 4, 5, 6, 7, 8},
	{2, 0, 0, 0, 3, 3, 3, 3, 3, 3},
	{2, 0, 1, 4, 4, 4, 4, 4, 4, 4},
	{2, 0, 1, 5, 5, 5, 5, 5, 5, 5},
	{2, 0, 0, 0, 0, 0, 0, 6, 0, 0},
	{2, 0, 1, 7, 7, 7, 7, 7, 7, 7},
	{2, 0, 0, 0, 0, 4, 5, 6, 7, 8}//*/
};
static const char pred_4x4to8x8[9] = 
{
	0,1,2,3,4,1,0,1,0
};

#define NO_INTRA_8x8_MODE 8
#define IS_INTRA_4x4_PRED_MODE(mode) ((mode)>=NO_INTRA_8x8_MODE)
#define xavs_mb_pred_mode(t) xavs_mb_pred_mode[(t)+1]
static const int xavs_mb_pred_mode[21] = {
	-1, INTRA_L_VERT, INTRA_L_HORIZ, INTRA_L_LP,
	INTRA_L_DOWN_LEFT, INTRA_L_DOWN_RIGHT, INTRA_L_LP,
	INTRA_L_LP, INTRA_L_LP,
	I_PRED_4x4_V-NO_INTRA_8x8_MODE,   I_PRED_4x4_H-NO_INTRA_8x8_MODE,   I_PRED_4x4_DC-NO_INTRA_8x8_MODE,
	I_PRED_4x4_DDL-NO_INTRA_8x8_MODE, I_PRED_4x4_DDR-NO_INTRA_8x8_MODE, I_PRED_4x4_HD-NO_INTRA_8x8_MODE,
	I_PRED_4x4_VL-NO_INTRA_8x8_MODE,  I_PRED_4x4_HU-NO_INTRA_8x8_MODE,  I_PRED_4x4_VR-NO_INTRA_8x8_MODE,
	I_PRED_4x4_DC-NO_INTRA_8x8_MODE,  I_PRED_4x4_DC-NO_INTRA_8x8_MODE,  I_PRED_4x4_DC-NO_INTRA_8x8_MODE
};

static const int8_t xavs_pred_mode_8x8to4x4[5] =
{
	0, 1, 2, 3, 4
};
static const int8_t xavs_pred_mode_4x4to8x8[9] =
{
	0, 1, 2, 3, 4, 1, 0, 1, 0
};



#define ESCAPE_CODE_PART1             39
#define ESCAPE_CODE_PART2             71
#define ESCAPE_CODE                   59
#define FWD0                          0x01
#define FWD1                          0x02
#define BWD0                          0x04
#define BWD1                          0x08
#define SYM0                          0x10
#define SYM1                          0x20
#define SPLITH                        0x40
#define SPLITV                        0x80


#define MV_BWD_OFFS                     12
#define MV_STRIDE                        4

#define CAVS_TRACE				   0
#if CAVS_TRACE
extern FILE *trace_fp;
#endif

enum xavs_mb_type 
{    
     I_8X8=0 ,
     P_SKIP,
     P_16X16,
     P_16X8,
     P_8X16,
     P_8X8,
     B_SKIP,
     B_DIRECT,
     B_FWD_16X16,
     B_BWD_16X16,
     B_SYM_16X16,
     B_8X8 = 29
};

enum xavs_mb_sub_type {
     B_SUB_DIRECT,
     B_SUB_FWD,
     B_SUB_BWD,
     B_SUB_SYM
};
enum xavs_block_size{
   BLK_16X16,
   BLK_16X8,
   BLK_8X16,
	BLK_8X8,
   BLK_4X4
};
enum xavs_mv_intra_mode_location {
INTRA_MODE_D3 =0,
INTRA_MODE_B2,
INTRA_MODE_B3,
INTRA_MODE_A1,
INTRA_MODE_X0,
INTRA_MODE_X1,
INTRA_MODE_A3,
INTRA_MODE_X2,
INTRA_MODE_X3
};

enum xavs_mv_pred {
MV_PRED_MEDIAN,
MV_PRED_LEFT,
MV_PRED_TOP,
MV_PRED_TOPRIGHT,
MV_PRED_PSKIP,
MV_PRED_BSKIP
};

enum xavs_mv_block_location {
MV_FWD_D3 = 0,
MV_FWD_B2,
MV_FWD_B3,
MV_FWD_C2,
MV_FWD_A1,
MV_FWD_X0,
MV_FWD_X1,
MV_FWD_A3 = 8,
MV_FWD_X2,
MV_FWD_X3,
MV_BWD_D3 = MV_BWD_OFFS,
MV_BWD_B2,
MV_BWD_B3,
MV_BWD_C2,
MV_BWD_A1,
MV_BWD_X0,
MV_BWD_X1,
MV_BWD_A3 = MV_BWD_OFFS+8,
MV_BWD_X2,
MV_BWD_X3
};
#if 0
enum macroblock_position_e
{
	MB_LEFT = 0x01,
	MB_TOP = 0x02,
	MB_TOPRIGHT = 0x04,
	MB_TOPLEFT = 0x08,
	MB_DOWNLEFT = 0x10,

	//MB_PRIVATE  = 0x10,
	ALL_NEIGHBORS = 0x1f,
};
#endif

#define A_AVAIL                          1
#define B_AVAIL                          2
#define C_AVAIL                          4
#define D_AVAIL                          8
#define NOT_AVAIL                       -1
#define REF_INTRA                       -2
#define REF_DIR                         -3

#define XAVS_ICACHE_STRIDE	5
#define XAVS_ICACHE_START	6


#define XAVS_SEQUENCE_DISPLAY_EXTENTION     0x00000002
#define XAVS_COPYRIGHT_EXTENTION            0x00000004
#define XAVS_PICTURE_DISPLAY_EXTENTION      0x00000007
#define XAVS_CAMERA_PARAMETERS_EXTENTION    0x0000000B
#define XAVS_SLICE_MIN_START_CODE           0x00000100
#define XAVS_SLICE_MAX_START_CODE           0x000001AF
#define XAVS_VIDEO_SEQUENCE_START_CODE      0x000001B0
#define XAVS_VIDEO_SEQUENCE_END_CODE        0x000001B1
#define XAVS_USER_DATA_CODE                 0x000001B2
#define XAVS_I_PICUTRE_START_CODE           0x000001B3
#define XAVS_EXTENSION_START_CODE           0x000001B5
#define XAVS_PB_PICUTRE_START_CODE          0x000001B6
#define XAVS_VIDEO_EDIT_CODE                0x000001B7
#define XAVS_VIDEO_TIME_CODE                0x000001E0

#define pixel uint8_t
/* Unions for type-punning.
 * Mn: load or store n bits, aligned, native-endian
 * CPn: copy n bits, aligned, native-endian
 * we don't use memcpy for CPn because memcpy's args aren't assumed to be aligned */
typedef union { uint16_t i; uint8_t  c[2]; }  xavs_union16_t;
typedef union { uint32_t i; uint16_t b[2]; uint8_t  c[4]; }  xavs_union32_t;
typedef union { uint64_t i; uint32_t a[2]; uint16_t b[4]; uint8_t c[8]; }  xavs_union64_t;
typedef struct { uint64_t i[2]; } xavs_uint128_t;
typedef union { xavs_uint128_t i; uint64_t a[2]; uint32_t b[4]; uint16_t c[8]; uint8_t d[16]; }  xavs_union128_t;
#define M16(src) (((xavs_union16_t*)(src))->i)
#define M32(src) (((xavs_union32_t*)(src))->i)
#define M64(src) (((xavs_union64_t*)(src))->i)
#define M128(src) (((xavs_union128_t*)(src))->i)
#define M128_ZERO ((xavs_uint128_t){{0,0}})
#define CP16(dst,src) M16(dst) = M16(src)
#define CP32(dst,src) M32(dst) = M32(src)
#define CP64(dst,src) M64(dst) = M64(src)
#define CP128(dst,src) M128(dst) = M128(src)

#define MPIXEL_X4(src) M32(src)
#define PIXEL_SPLAT_X4(x) ((x)*0x01010101U)
#define CPPIXEL_X4(dst,src) MPIXEL_X4(dst) = MPIXEL_X4(src)

#define PACK8TO16(a, b)	(a + ((b)<<8))

//#define DECLARE_ALIGNED( var, n ) __declspec(align(n)) var
#define DECLARE_ALIGNED_16( var ) DECLARE_ALIGNED( var, 16 )
#define DECLARE_ALIGNED_8( var )  DECLARE_ALIGNED( var, 8 )
#define DECLARE_ALIGNED_4( var )  DECLARE_ALIGNED( var, 4 )

/*
	 0  1  4  5
	 2  3  6  7
	 8  9 12 13
	10 11 14 15
*/
static const int offset_block4x4[16][2] = {
	{0,0}, {1,0}, {0,1}, {1,1},
	{2,0}, {3,0}, {2,1}, {3,1},
	{0,2}, {1,2}, {0,3}, {1,3},
	{2,2}, {3,2}, {2,3}, {3,3}
};

typedef struct tagxavs_video_sequence_header
{
	/************************************************************************/
	/* jizhun_profile and yidong_profile       0x34                                                         */
	/************************************************************************/
	uint8_t  i_profile_id;/*8bits u(8)*/
	uint8_t  i_level_id;/*8bits u(8)*/
	uint8_t  b_progressive_sequence;/*1bit u(1)*/
	uint32_t i_horizontal_size;/*14bits u(14)*/
	uint32_t i_vertical_size;/*14bits u(14)*/
	uint8_t  i_chroma_format;/*2bits u(2)*/
	uint8_t  i_sample_precision;/*3bits u(3)*/
	uint8_t  i_aspect_ratio;/*4bits u(4)*/
	uint8_t  i_frame_rate_code;/*4bits u(4)*/
	uint32_t i_bit_rate;
	/*30bits u(30) 分两部分获得
	 18bits bit_rate_lower;
	 12bits bit_rate_upper。
	其间有1bit的marker_bit防止起始码的竞争*/
	uint8_t  b_low_delay;/*1bit u(1)*/
	//这里有1bit的marker_bit
	uint32_t i_bbv_buffer_size;/*18bits u(18)*/
	//还有3bits的reserved_bits，固定为'000' 
		/************************************************************************/
	/* yidong_profile       0x34                                                                                       */
	/************************************************************************/
	
	uint32_t  weighting_quant_flag;
	uint32_t  mb_adapt_wq_disable;
	uint32_t  chroma_quant_param_disable;
	uint8_t   chroma_quant_param_delta_u;	
	uint8_t   chroma_quant_param_delta_v;
	uint8_t  weighting_quant_param_index;
	uint8_t   weighting_quant_param_delta1[6];
	uint8_t   weighting_quant_param_delta2[6];
	uint32_t   scene_adapt_disable;
	uint32_t   scene_model_update_flag;
	uint32_t  weighting_quant_model;
	uint32_t  vbs_enable;
	uint32_t  vbs_qp_shift;
	uint32_t asi_enable;
	uint32_t limitDC_refresh_frame;

	uint8_t  b_interlaced;

	uint8_t b_flag_aec; /* used for check stream change */
	uint32_t b_aec_enable; /* backup aec enable */
}xavs_video_sequence_header;

typedef struct tagxavs_picture_header
{
    uint16_t i_bbv_delay;/*16bits u(16)*/

    uint8_t  i_picture_coding_type;/*2bits u(2) pb_piture_header专有*/
    uint8_t  b_time_code_flag;/*1bit u(1) i_piture_header专有*/
    uint32_t i_time_code;/*24bits u(24) i_piture_header专有*/
    uint8_t  i_picture_distance;/*8bits u(8)*/
    uint32_t i_bbv_check_times;/*ue(v)*/
    uint8_t  b_progressive_frame;/*1bit u(1)*/
    uint8_t  b_picture_structure;/*1bit u(1)*/
    uint8_t  b_advanced_pred_mode_disable;/*1bit u(1) pb_piture_header专有*/
    uint8_t  b_top_field_first;/*1bit u(1)*/
    uint8_t  b_repeat_first_field;/*1bit u(1)*/
    uint8_t  b_fixed_picture_qp;/*1bit u(1)*/
    uint8_t  i_picture_qp;/*6bits u(6)*/
    uint8_t  b_picture_reference_flag;/*1bit u(1) pb_piture_header专有*/
    /*4bits保留字节reserved_bits固定为'0000'*/
    uint8_t  b_no_forward_reference_flag;
    uint8_t  b_skip_mode_flag;/*1bit u(1)*/
    uint8_t  b_loop_filter_disable;/*1bit u(1)*/
    uint8_t  b_loop_filter_parameter_flag;/*1bit u(1)*/
    uint32_t  i_alpha_c_offset;/*se(v)*/
    uint32_t  i_beta_offset;/*se(v)*/

    /************************************************************************/
    /* yidong_profile       0x34                                                                                   */
    /************************************************************************/

    uint32_t  weighting_quant_flag;
    uint32_t  mb_adapt_wq_disable;
    uint32_t  chroma_quant_param_disable;
    uint8_t   chroma_quant_param_delta_u;	
    uint8_t   chroma_quant_param_delta_v;
    uint8_t   weighting_quant_param_index;
    uint8_t   weighting_quant_param_delta1[6];
    uint8_t   weighting_quant_param_delta2[6];
	
	uint8_t   weighting_quant_param_undetail[6];
	uint8_t   weighting_quant_param_detail[6];

    uint32_t  scene_adapt_disable;
    uint32_t  scene_model_update_flag;
    uint32_t  weighting_quant_model;
    uint32_t  vbs_enable;
    uint32_t  vbs_qp_shift;
    uint32_t  asi_enable;
    uint32_t  limitDC_refresh_frame;

    /************************************************************************/
    /* guangdian profile       0x48       add by jcma                       */
    /************************************************************************/
    uint32_t  b_aec_enable;
    uint32_t  b_pb_field_enhanced;
}xavs_picture_header;

typedef struct tagxavs_slice_header
{
	uint8_t  i_slice_vertical_position_extension;/*3bits u(3)*/
	uint8_t  i_slice_horizontal_position_extension;/*2bits u(2)*/
	uint8_t i_slice_horizontal_position;/*8bits u(8)*/
	uint8_t  b_fixed_slice_qp;/*1bit u(1)*/
	uint8_t  i_slice_qp;/*6bits u(6)*/
	uint8_t  b_slice_weighting_flag;/*1bit u(1)*/
	uint32_t i_luma_scale[4];/*8bits u(8)*/
	int32_t  i_luma_shift[4];/*8bits i(8)*/
	uint32_t i_chroma_scale[4];/*8bits u(8)*/
	int32_t  i_chroma_shift[4];/*8bits i(8)*/
	uint8_t  b_mb_weighting_flag;/*1bit u(1)*/
	uint32_t i_mb_skip_run;/*ue(v)*/
	uint32_t i_profile_id;
	uint8_t  i_number_of_reference;
	uint32_t quant_coff_pred_flag;
}xavs_slice_header;

typedef struct tagxavs_sequence_display_extension
{
	uint8_t  i_video_format;/*3bits u(3)*/
	uint8_t  b_sample_range;/*1bit u(1)*/
	uint8_t  b_colour_description;/*1bit u(1)*/
	uint8_t  i_colour_primaries;/*8bits u(8)*/
	uint8_t  i_transfer_characteristics;/*8bits u(8)*/
	uint8_t  i_matrix_coefficients;/*8bits u(8)*/
	uint32_t i_display_horizontal_size;/*14bits u(14)*/
	uint32_t i_display_vertical_size;/*14bits u(14)*/
	//还有2bits的reserved_bits，
	uint8_t  i_stereo_packing_mode;
}xavs_sequence_display_extension;

typedef struct tagxavs_copyright_extension
{
	uint8_t  b_copyright_flag;/*1bit u(1)*/
	uint8_t  i_copyright_id;/*8bits u(8)*/
	uint8_t  b_original_or_copy;/*1bit u(1)*/
	//这里有保留7bits的reserved_bits
	uint64_t i_copyright_number;
	/*64bits u(64)分三个部分出现，
	20bits copyright_number_1;
	22bits copyright_number_2;
	22bits copyright_number_3
	其间分别嵌入2个1bit的marker_bit*/
}xavs_copyright_extension;

typedef struct tagxavs_camera_parameters_extension
{
	//1bit的reserved_bits
	uint8_t  i_camera_id;/*7bits u(7)*/
	//这里有1bit的marker_bit
	uint32_t i_height_of_image_device;/*22bits u(22)*/
	//这里有1bit的marker_bit
	uint32_t i_focal_length;/*22bits u(22)*/
	//这里有1bit的marker_bit
	uint32_t i_f_number;/*22bits u(22)*/
	//这里有1bit的marker_bit
	uint32_t i_vertical_angle_of_view;/*22bits u(22)*/
	//这里有1bit的marker_bit
	int32_t  i_camera_position_x;
	/*32bits i(32) 分两部分获得
	16bits camera_position_x_upper;
	16bits camera_position_x_lower;
    其间有1bit的marker_bit防止起始码的竞争*/
	int32_t  i_camera_position_y;
	/*32bits i(32) 分两部分获得
	16bits camera_position_x_upper;
	16bits camera_position_x_lower;
    其间有1bit的marker_bit防止起始码的竞争*/
	int32_t  i_camera_position_z;
	/*32bits i(32) 分两部分获得
	16bits camera_position_x_upper;
	16bits camera_position_x_lower;
    其间有1bit的marker_bit防止起始码的竞争*/
	//这里有1bit的marker_bit
	int32_t i_camera_direction_x;/*22bits i(22)*/
	//这里有1bit的marker_bit
	int32_t i_camera_direction_y;/*22bits i(22)*/
	//这里有1bit的marker_bit
	int32_t i_camera_direction_z;/*22bits i(22)*/

	//这里有1bit的marker_bit
	int32_t i_image_plane_vertical_x;/*22bits i(22)*/
	//这里有1bit的marker_bit
	int32_t i_image_plane_vertical_y;/*22bits i(22)*/
	//这里有1bit的marker_bit
	int32_t i_image_plane_vertical_z;/*22bits i(22)*/
	//这里有1bit的marker_bit
	//还有32bits的reserved_bits，
}xavs_camera_parameters_extension;

typedef struct tagxavs_picture_display_extension
{
	uint32_t i_number_of_frame_centre_offsets;
	/*元素个数，由语法派生获得xavs_sequence_display_extension，不占用bits*/
	uint32_t i_frame_centre_horizontal_offset[4];/*16bits i(16)*/
	uint32_t i_frame_centre_vertical_offset[4];/*16bits i(16)*/
}xavs_picture_display_extension;
 typedef struct tagxavs_vector
{
     int16_t x;
     int16_t y;
     int16_t dist;
     int16_t ref;
} xavs_vector;

typedef struct tagxavs_vlc 
{
	int8_t rltab[59][3];
	int8_t level_add[27];
	int8_t golomb_order;
	int inc_limit;
	int8_t max_run;

} xavs_vlc;

typedef struct tagxavs_image
{
	uint32_t   i_colorspace;
	uint32_t   i_width;
	uint32_t   i_height;
	uint8_t   *p_data[4];
	uint32_t   i_stride[4];
	uint32_t   i_distance_index;
	uint32_t   i_code_type;
	uint32_t   b_top_field_first;
	uint32_t   i_ref_distance[2]; /* two ref-frames at most */
	uint32_t   b_picture_structure;

}xavs_image;

typedef struct tagcavs_bitstream
{
    uint8_t *p_start;
    uint8_t *p;
    uint8_t *p_end;
    int      i_left;
    uint32_t i_startcode;
}cavs_bitstream;

static inline void cavs_bitstream_init( cavs_bitstream *s, void *p_data, int i_data )
{
    s->p_start = (uint8_t *)p_data;
    s->p       = (uint8_t *)p_data;
    s->p_end   = s->p + i_data;
    s->i_left  = 8;
    s->i_startcode=*s->p;
}

static inline int cavs_bitstream_pos( cavs_bitstream *s )
{
    return( 8 * ( s->p - s->p_start ) + 8 - s->i_left );
}
static inline int cavs_bitstream_eof( cavs_bitstream *s )
{
    return( s->p >= s->p_end -1? 1: 0 ) && (*(s->p-1) & ((1<<s->i_left)-1)) == (1<<(s->i_left-1));
}

static inline void cavs_bitstream_align(cavs_bitstream *s)
{
    if (s->i_left < 8)
    {
        s->p++;
        s->i_left = 8;
    }
}

static inline void cavs_bitstream_next_byte( cavs_bitstream *s)
{
    s->p++;
	s->i_startcode = (s->i_startcode << 8) + *s->p;
#if 0 // FIXIT
	if((s->i_startcode&0xffffff)!=0x000002)
    {
        s->i_left = 8;
    }
    else
    {
        *s->p>>=2;
        s->i_left = 6;
    }
#else
	s->i_left = 8;
#endif

}

static inline uint32_t cavs_bitstream_get_bits( cavs_bitstream *s, int i_count )
{
    static uint32_t i_mask[33] ={0x00,
    	0x01,      0x03,      0x07,      0x0f,
    	0x1f,      0x3f,      0x7f,      0xff,
    	0x1ff,     0x3ff,     0x7ff,     0xfff,
    	0x1fff,    0x3fff,    0x7fff,    0xffff,
    	0x1ffff,   0x3ffff,   0x7ffff,   0xfffff,
    	0x1fffff,  0x3fffff,  0x7fffff,  0xffffff,
    	0x1ffffff, 0x3ffffff, 0x7ffffff, 0xfffffff,
    	0x1fffffff,0x3fffffff,0x7fffffff,0xffffffff};
    int      i_shr;
    uint32_t i_result = 0;

#if CAVS_TRACE
    int count_xiaoC=i_count;	
#endif

    while( i_count > 0 )
    {
        if( s->p >= s->p_end )
        {

#if CAVS_TRACE
    		fprintf(trace_fp,"%d bits	(%d) error 读bit溢出\n",count_xiaoC,i_result);
    		fprintf(trace_fp,"**************************************************\n");
    		fflush(trace_fp);
#endif

            break;
        }
        if( ( i_shr = s->i_left - i_count ) >= 0 )
        {
            i_result |= ( *s->p >> i_shr )&i_mask[i_count];
            s->i_left -= i_count;
            if( s->i_left == 0 )
            {
                cavs_bitstream_next_byte(s);
            }
            
#if CAVS_TRACE
    		fprintf(trace_fp,"%d bits	(%d)\n",count_xiaoC,i_result);
    		fprintf(trace_fp,"**************************************************\n");
    		fflush(trace_fp);
#endif

            return( i_result );
        }
        else
        {
            i_result |= (*s->p&i_mask[s->i_left]) << -i_shr;
            i_count  -= s->i_left;
            cavs_bitstream_next_byte(s);
       }
    }

#if CAVS_TRACE
    fprintf(trace_fp,"%d bits	(%d)\n",count_xiaoC,i_result);
    fprintf(trace_fp,"**************************************************\n");
    fflush(trace_fp);

#endif

    return( i_result );
}


static inline uint32_t cavs_bitstream_get_bit1( cavs_bitstream *s )
{
    if( s->p < s->p_end )
    {
        uint32_t i_result;

        s->i_left--;
        i_result = ( *s->p >> s->i_left )&0x01;
        if( s->i_left == 0 )
        {
            cavs_bitstream_next_byte(s);  
        }
        
#if CAVS_TRACE
    	fprintf(trace_fp,"%d bit	(%d)\n",1,i_result);
    	fprintf(trace_fp,"**************************************************\n");
    	fflush(trace_fp);
#endif

        return i_result;
    }

#if CAVS_TRACE
    fprintf(trace_fp,"%d bit	(%d) error 读bit溢出\n",1,0);
    fprintf(trace_fp,"**************************************************\n");
    fflush(trace_fp);
#endif

    return 0;
}



static inline int32_t cavs_bitstream_get_int( cavs_bitstream *s, int i_count )
{
    uint32_t i_temp=1<<(i_count-1);
    uint32_t i_result=cavs_bitstream_get_bits(s,i_count);
    
    if(i_result&i_temp)
    {
        return 0-(int)((~i_result+1)<<( 32 - i_count)>>(32 - i_count));
    }
    else
    {
        return i_result;
    }
}

static inline void cavs_bitstream_clear_bits( cavs_bitstream *s, int i_count )
{

#if CAVS_TRACE
    int count_xiaoC=i_count;
#endif

    while( i_count > 0 )
    {
        if( s->p >= s->p_end )
        {

#if CAVS_TRACE
    		fprintf(trace_fp,"%d bits	  error 清除读bit溢出\n",count_xiaoC);
    		fprintf(trace_fp,"**************************************************\n");
    		fflush(trace_fp);
#endif

            break;
        }
        if( (  s->i_left - i_count ) >= 0 )
        {
            s->i_left -= i_count;
            if( s->i_left == 0 )
            {
                cavs_bitstream_next_byte(s);
            }
            break;
        }
        else
        {  
            i_count  -= s->i_left;
            cavs_bitstream_next_byte(s);
       }
    }
}

static inline int cavs_bitstream_get_ue( cavs_bitstream *s )
{
    int i = 0;
    int temp;

    while( cavs_bitstream_get_bit1(s) == 0 && s->p < s->p_end && i < 32)
    {
        i++;
    }
    temp=( ( 1 << i) - 1 + cavs_bitstream_get_bits( s, i) );

#if CAVS_TRACE
    fprintf(trace_fp,"%d bits	 (%d)得到ue\n",i*2+1,temp);
    fprintf(trace_fp,"**************************************************\n");
    fflush(trace_fp);
#endif

    return( temp );
}

static inline int cavs_bitstream_get_se( cavs_bitstream *s)
{
    int val = cavs_bitstream_get_ue( s);
    int temp=val&0x01 ? (val+1)/2 : -(val/2);
    
#if CAVS_TRACE
    fprintf(trace_fp,"			(%d)得到se\n",temp);
    fprintf(trace_fp,"**************************************************\n");
    fflush(trace_fp);
#endif

    return temp;
}

static inline int cavs_bitstream_get_ue_k( cavs_bitstream *s, int k)
{
    int i = 0;
    int temp;

    while( cavs_bitstream_get_bit1( s ) == 0 && s->p < s->p_end && i < 32)
    {
        i++;
    }
    temp = ( 1 << (i+k)) - (1<<k) + cavs_bitstream_get_bits( s, i+k);
    
#if CAVS_TRACE
    fprintf(trace_fp,"			(%d)得到ue_k\n",temp);
    fprintf(trace_fp,"**************************************************\n");
#endif

    return( temp );
}

static inline int cavs_bitstream_get_se_k( cavs_bitstream *s, int k)
{
    int val = cavs_bitstream_get_ue_k( s,k);

    return val&0x01 ? (val+1)/2 : -(val/2);
}

void *cavs_malloc( int );
void *cavs_realloc( void *p, int i_size );
void  cavs_free( void * );
int cavs_get_video_sequence_header(cavs_bitstream *s,xavs_video_sequence_header *h);
int cavs_get_sequence_display_extension(cavs_bitstream *s, xavs_sequence_display_extension *sde);
int cavs_get_copyright_extension(cavs_bitstream *s, xavs_copyright_extension *ce);
int cavs_get_camera_parameters_extension(cavs_bitstream *s, xavs_camera_parameters_extension *cpe);
int cavs_get_user_data(cavs_bitstream *s, uint8_t *user_data);
int cavs_get_i_picture_header(cavs_bitstream *s,xavs_picture_header *h,xavs_video_sequence_header *sh);
int cavs_get_pb_picture_header(cavs_bitstream *s,xavs_picture_header *h,xavs_video_sequence_header *sh);

int cavs_cavlc_get_ue(cavs_decoder *p);
int cavs_cavlc_get_se(cavs_decoder *p);
int cavs_cavlc_get_mb_type_p(cavs_decoder *p);
int cavs_cavlc_get_mb_type_b(cavs_decoder *p);
int cavs_cavlc_get_intra_luma_pred_mode(cavs_decoder *p);
int cavs_cavlc_get_intra_cbp(cavs_decoder *p);
int cavs_cavlc_get_inter_cbp(cavs_decoder *p);
int cavs_cavlc_get_ref_p(cavs_decoder *p, int ref_scan_idx);
int cavs_cavlc_get_ref_b(cavs_decoder *p, int i_list, int ref_scan_idx);
int cavs_cavlc_get_mb_part_type(cavs_decoder *p);
int cavs_cavlc_get_mvd(cavs_decoder *p, int i_list, int i_down, int xy_idx);
int cavs_cavlc_get_coeffs(cavs_decoder *p, const xavs_vlc *p_vlc_table, int i_escape_order, int b_chroma);
#endif
