/*****************************************************************************
 * cavlc.c: 2d-VLC
 *****************************************************************************
 */
 
#include "bavs_globe.h"
#include <stdlib.h>
#include <memory.h>
#include <stdio.h>
#define CACHE_LINE        16
#define CACHE_LINE_MASK   15

#if defined(__GNUC__) && (__GNUC__ > 3 || __GNUC__ == 3 && __GNUC_MINOR__ > 0)
#else
#pragma warning(disable:4996)
#endif

#if CAVS_TRACE
FILE *trace_fp;
#endif

/****************************************************************************
 * * cavs_malloc:
 ****************************************************************************/
void *cavs_malloc( int i_size )
{
    uint8_t * buf;
    uint8_t * align_buf = NULL;

#if 0
    buf = (uint8_t *) malloc( i_size + CACHE_LINE_MASK + sizeof( void ** ) +
    	sizeof( int ) );
    align_buf = buf + CACHE_LINE_MASK + sizeof( void ** ) + sizeof( int );
    align_buf -= (long) align_buf & CACHE_LINE_MASK;
    *( (void **) ( align_buf - sizeof( void ** ) ) ) = buf;
    *( (int *) ( align_buf - sizeof( void ** ) - sizeof( int ) ) ) = i_size;
#else
	buf = (uint8_t *) malloc( i_size + CACHE_LINE_MASK + sizeof( void ** ));
    if( buf )
	{
		align_buf = buf + CACHE_LINE_MASK + sizeof( void ** );
		align_buf -= (uint8_t)align_buf & CACHE_LINE_MASK;
		*( (void **) ( align_buf - sizeof( void ** ) ) ) = buf;
	}
    
	if( !align_buf )
        printf("malloc of size %d failed\n", i_size );
#endif

    return align_buf;
}

/****************************************************************************
 * * cavs_free:
 ****************************************************************************/
void cavs_free( void *p )
{
    if( p )
    {
        free( *( ( ( void **) p ) - 1 ) );
    }
}

/****************************************************************************
 * cavs_realloc:
 ****************************************************************************/
void *cavs_realloc( void *p, int i_size )
{
    int       i_old_size = 0;
    uint8_t * p_new;
    if( p )
    {
        i_old_size = *( (int*) ( (uint8_t*) p  - sizeof( void ** ) -
                     sizeof( int ) ) );
    }
    p_new = (uint8_t *)cavs_malloc( i_size );
    if( i_old_size > 0 && i_size > 0 )
    {
        memcpy( p_new, p, ( i_old_size < i_size ) ? i_old_size : i_size );
    }
    cavs_free( p );

    return p_new;
}


int cavs_get_sequence_display_extension(cavs_bitstream *s, xavs_sequence_display_extension *e)
{
#if CAVS_TRACE
    fprintf(trace_fp,"序列显示扩展信息读取开始\n\n\n\n");
    fprintf(trace_fp,"*******************************************************\n");
    fflush(trace_fp);
#endif

    e->i_video_format = cavs_bitstream_get_bits(s, 3);
    e->b_sample_range = cavs_bitstream_get_bits(s, 1);
    e->b_colour_description = cavs_bitstream_get_bit1(s);
    if(e->b_colour_description)
    {
        e->i_colour_primaries = cavs_bitstream_get_bits(s, 8);
        e->i_transfer_characteristics = cavs_bitstream_get_bits(s, 8);
        e->i_matrix_coefficients = cavs_bitstream_get_bits(s, 8);
    }
    e->i_display_horizontal_size = cavs_bitstream_get_bits(s, 14);
    cavs_bitstream_clear_bits(s, 1);
    e->i_display_vertical_size=cavs_bitstream_get_bits(s, 14);
	e->i_stereo_packing_mode = cavs_bitstream_get_bits(s, 2);
    
#if CAVS_TRACE
    fprintf(trace_fp,"序列显示扩展信息读取完毕\n\n\n\n");
    fprintf(trace_fp,"*******************************************************\n");
    fflush(trace_fp);
#endif

    return 0;
}

int cavs_get_copyright_extension(cavs_bitstream *s,xavs_copyright_extension *e)
{

#if CAVS_TRACE
    fprintf(trace_fp,"版权扩展信息读取开始\n\n\n\n");
    fprintf(trace_fp,"*******************************************************\n");
    fflush(trace_fp);
#endif

    e->b_copyright_flag = cavs_bitstream_get_bit1(s);
    e->i_copyright_id = cavs_bitstream_get_bits(s, 8);
    e->b_original_or_copy = cavs_bitstream_get_bit1(s);
    cavs_bitstream_clear_bits(s, 7);
    cavs_bitstream_clear_bits(s, 1);
    e->i_copyright_number = cavs_bitstream_get_bits(s, 20);
    e->i_copyright_number <<= 22;
    cavs_bitstream_clear_bits(s, 1);
    e->i_copyright_number += cavs_bitstream_get_bits(s, 22);
    e->i_copyright_number <<= 22;
    cavs_bitstream_clear_bits(s, 1);
    e->i_copyright_number += cavs_bitstream_get_bits(s, 22);

#if CAVS_TRACE
    fprintf(trace_fp,"版权扩展信息读取完毕\n\n\n\n");
    fprintf(trace_fp,"*******************************************************\n");
    fflush(trace_fp);
#endif

    return 0;
}

int cavs_get_camera_parameters_extension(cavs_bitstream *s,xavs_camera_parameters_extension *e)
{	
    uint32_t i_value;

#if CAVS_TRACE
    fprintf(trace_fp,"摄像机参数扩展信息读取开始\n\n\n\n");
    fprintf(trace_fp,"*******************************************************\n");
    fflush(trace_fp);
#endif

    cavs_bitstream_clear_bits(s, 1);
    e->i_camera_id = cavs_bitstream_get_bits(s, 7);
    cavs_bitstream_clear_bits(s, 1);
    e->i_height_of_image_device = cavs_bitstream_get_bits(s, 22);
    cavs_bitstream_clear_bits(s, 1);
    e->i_focal_length = cavs_bitstream_get_bits(s, 22);
    cavs_bitstream_clear_bits(s, 1);
    e->i_f_number = cavs_bitstream_get_bits(s, 22);
    cavs_bitstream_clear_bits(s, 1);
    e->i_vertical_angle_of_view = cavs_bitstream_get_bits(s, 22);
    cavs_bitstream_clear_bits(s, 1);

    i_value = cavs_bitstream_get_bits(s, 16);
    i_value <<= 16;
    cavs_bitstream_clear_bits(s, 1);
    i_value += cavs_bitstream_get_bits(s, 16);
    cavs_bitstream_clear_bits(s, 1);
    e->i_camera_position_x = (int32_t)i_value;

    i_value = cavs_bitstream_get_bits(s, 16);
    i_value <<= 16;
    cavs_bitstream_clear_bits(s, 1);
    i_value += cavs_bitstream_get_bits(s, 16);
    cavs_bitstream_clear_bits(s, 1);
    e->i_camera_position_y = (int32_t)i_value;

    i_value = cavs_bitstream_get_bits(s, 16);
    i_value <<= 16;
    cavs_bitstream_clear_bits(s, 1);
    i_value += cavs_bitstream_get_bits(s, 16);
    cavs_bitstream_clear_bits(s, 1);
    e->i_camera_position_z = (int32_t)i_value;

    e->i_camera_direction_x = cavs_bitstream_get_int(s, 22);
    cavs_bitstream_clear_bits(s, 1);
    e->i_camera_direction_y = cavs_bitstream_get_int(s, 22);
    cavs_bitstream_clear_bits(s, 1);
    e->i_camera_direction_z = cavs_bitstream_get_int(s, 22);
    cavs_bitstream_clear_bits(s, 1);

    e->i_image_plane_vertical_x = cavs_bitstream_get_int(s, 22);
    cavs_bitstream_clear_bits(s, 1);
    e->i_image_plane_vertical_y = cavs_bitstream_get_int(s, 22);
    cavs_bitstream_clear_bits(s, 1);
    e->i_image_plane_vertical_z = cavs_bitstream_get_int(s, 22);
    cavs_bitstream_clear_bits(s, 1);

#if CAVS_TRACE
    fprintf(trace_fp,"摄像机参数扩展信息读取完毕\n\n\n\n");
    fprintf(trace_fp,"*******************************************************\n");
    fflush(trace_fp);
#endif

    return 0;
}

int cavs_get_user_data(cavs_bitstream *s, uint8_t *user_data)
{
#if CAVS_TRACE
    fprintf(trace_fp,"用户信息读取开始\n\n\n\n");
    fprintf(trace_fp,"*******************************************************\n");
    fflush(trace_fp);
#endif

    while (((*(uint32_t*)(s->p)) & 0x00ffffff) != 0x000001/*0x00010000*/&& s->p < s->p_end)
    	*user_data = cavs_bitstream_get_bits(s, 8);

#if CAVS_TRACE
    fprintf(trace_fp,"用户信息读取完毕\n\n\n\n");
    fprintf(trace_fp,"*******************************************************\n");
    fflush(trace_fp);
#endif

    return 0;
}

//void xavs_get_picture_display_extension(cavs_bitstream *s,xavs_picture_display_extension *e,int i_num)
//{
//	int i;
//	uint32_t   i_value=0;
//	for(i=0;i<i_num;i++)
//	{
//		e->i_frame_centre_horizontal_offset[i]=cavs_bitstream_get_int(s,16);
//		cavs_bitstream_clear_bits(s,1);
//		e->i_frame_centre_vertical_offset[i]=cavs_bitstream_get_int(s,16);
//		cavs_bitstream_clear_bits(s,1);
//	}
//}
static int cavs_check_level_id( uint8_t level_id )
{
    if(      level_id != 0x10
        && level_id != 0x12
        && level_id != 0x14
        && level_id != 0x20
        && level_id != 0x22
        && level_id != 0x2A
        && level_id != 0x40
        && level_id != 0x41
        && level_id != 0x42
        && level_id != 0x44
        && level_id != 0x46
    )
    {
        return -1;    
    }
    

    return 0;
}

static int validResu[][2] = 
{
	{128,96}, //0
	{176,144}, //1
	{320,240}, //2
	{640,480}, //3
	{720,480}, //4
	{720,576}, //6
	{800,480}, //7
	{1280,720}, //8
	{1440,1080}, //9
	{1440,1080}, //10
	{1920,1080}, //11
	{1920,1088}, //12
	{960,1080},//13
	{960,1088}, //14
	{544, 576},	//15
	{528, 576},	//16
	{480, 576},	//17
	{704, 576},
	{-1,-1}
};
static int cavs_check_resolution(int width, int height)
{
	int i = 0;
	for(i=0; i<32; i++)
	{
		if (validResu[i][0] < 0) {
			break;
		}
		if(validResu[i][0]==width && validResu[i][1]==height)
			return 0;
	}
	return -1;
}

int cavs_get_video_sequence_header(cavs_bitstream *s,xavs_video_sequence_header *h)
{
    //uint32_t   i_result = 0;
	uint32_t   width, height;

#if CAVS_TRACE
    fprintf(trace_fp,"视频序列头信息读取开始\n\n\n\n");
    fprintf(trace_fp,"*******************************************************\n");
    fflush(trace_fp);
#endif

    h->i_profile_id = cavs_bitstream_get_bits(s, 8);

    if((h->i_profile_id!=PROFILE_JIZHUN) && (h->i_profile_id!=PROFILE_GUANGDIAN) )
    {
    	printf("[error]seq header: profile %d is invalid, The decoder only deals with the jizhun profile and guangdian profile\n", h->i_profile_id);

       return -1;
    }

    h->i_level_id = cavs_bitstream_get_bits(s, 8);
    if(cavs_check_level_id(h->i_level_id))
    {
        printf("[error]seq header: level_id is invalid\n");
        
        return -1;
    }
    h->b_progressive_sequence = cavs_bitstream_get_bit1(s);
	if( h->b_progressive_sequence )
	{
		//printf("[cavs info] progressive sequence \n");
	}
	else
	{
		//printf("[cavs info] not progressive sequence \n");
	}
    width = cavs_bitstream_get_bits(s, 14);
    if( width < 176 || width > 1920)
    {
        return -1;    
    }
    height = cavs_bitstream_get_bits(s, 14);
    if( height < 128 || height >  1088)
    {
        return -1;    
    }
	if(cavs_check_resolution(width, height))
	{
		printf("[error]seq header: width or height is invalid\n");
		return -1;
	}

    h->i_chroma_format = cavs_bitstream_get_bits(s, 2);
    if( h->i_chroma_format != 1 )
    {
        printf("[error]seq header: chroma format only support 4:2:0 \n");
        
        return -1;
    }
    h->i_sample_precision = cavs_bitstream_get_bits(s, 3);
    if( h->i_sample_precision != 1)
    {
        printf("[error]seq header: only support 8 bit \n");
        
        return -1;
    }
    h->i_aspect_ratio = cavs_bitstream_get_bits(s, 4);
    if( h->i_aspect_ratio != 0x1
        && h->i_aspect_ratio != 0x2
        && h->i_aspect_ratio != 0x3
        && h->i_aspect_ratio != 0x4 )
    {
        printf("[error]seq header: aspect ratio is invalid \n");
        
        return -1;    
    }
    h->i_frame_rate_code = cavs_bitstream_get_bits(s, 4);
    if( h->i_frame_rate_code != 0x1 
        && h->i_frame_rate_code != 0x2
        && h->i_frame_rate_code != 0x3
        && h->i_frame_rate_code != 0x4
        && h->i_frame_rate_code != 0x5
        && h->i_frame_rate_code != 0x6
        && h->i_frame_rate_code != 0x7
        && h->i_frame_rate_code != 0x8 )
    {
        printf("[error]seq header: frame rate code is invalid \n");
        
        return -1;  
    }
    
    h->i_bit_rate = cavs_bitstream_get_bits(s, 18);//bit_rate_lower
    h->i_bit_rate <<= 12;
    cavs_bitstream_clear_bits(s, 1); //marker_bit
    h->i_bit_rate += cavs_bitstream_get_bits(s, 12);
    h->b_low_delay = cavs_bitstream_get_bit1(s);
   
    cavs_bitstream_clear_bits(s, 1); //marker_bit
    h->i_bbv_buffer_size = cavs_bitstream_get_bits(s, 18);

    if(cavs_bitstream_get_bits(s,3)!=0) //reserved bits
    {/* 3bits的'000' */
        //printf("[error]seq header: 3 reserved bits should be 0 \n");
        
        //return -1;
    }

	h->i_horizontal_size = width;
	h->i_vertical_size = height;

#if CAVS_TRACE
    fprintf(trace_fp,"视频序列头信息读取完毕\n\n\n\n");
    fprintf(trace_fp,"*******************************************************\n");
    fflush(trace_fp);
#endif

    return 0;
}

short wq_param_default[2][6]=
{
	{135,143,143,160,160,213},   
	{128, 98,106,116,116,128}     
};

int cavs_get_i_picture_header(cavs_bitstream *s,xavs_picture_header *h,xavs_video_sequence_header *sh)
{
    //uint32_t i_value=0;

#if CAVS_TRACE
    fprintf(trace_fp,"I帧序列头信息读取开始\n\n\n\n");
    fprintf(trace_fp,"*******************************************************\n");
    fflush(trace_fp);
#endif

    h->i_picture_coding_type = XAVS_I_PICTURE;
    h->b_picture_reference_flag = 1;
    h->b_advanced_pred_mode_disable = 1;
    h->i_bbv_delay = cavs_bitstream_get_bits(s, 16);

    if (sh->i_profile_id == PROFILE_GUANGDIAN)
    {
    	cavs_bitstream_get_bit1(s);	//marker bit
    	cavs_bitstream_get_bits(s, 7);	//bbv_delay extension
    }

    h->b_time_code_flag = cavs_bitstream_get_bit1(s);
    if(h->b_time_code_flag)
    {
        h->i_time_code = cavs_bitstream_get_bits(s, 24);
    }
    cavs_bitstream_get_bit1(s);
    h->i_picture_distance = cavs_bitstream_get_bits(s, 8);

    if(sh->b_low_delay)
    {	
        //ue
        h->i_bbv_check_times = cavs_bitstream_get_ue(s);
    }
    h->b_progressive_frame = cavs_bitstream_get_bit1(s);
    if(h->b_progressive_frame==0)
    {
        h->b_picture_structure = cavs_bitstream_get_bit1(s);
    }
    else
    {
        h->b_picture_structure=1;
    }
    sh->b_interlaced = !h->b_picture_structure;
    h->b_top_field_first = cavs_bitstream_get_bit1(s);
    h->b_top_field_first = !h->b_picture_structure; // FIXIT:not support 0
    h->b_repeat_first_field = cavs_bitstream_get_bit1(s);
    h->b_fixed_picture_qp = cavs_bitstream_get_bit1(s);
    h->i_picture_qp = cavs_bitstream_get_bits(s, 6);

    if( h->b_progressive_frame == 0 && h->b_picture_structure == 0 )
    {
        h->b_skip_mode_flag = cavs_bitstream_get_bit1(s);
    }
    else
    {
        h->b_skip_mode_flag=0;
    }
    if(cavs_bitstream_get_bits(s,4)!=0)
    {
        //return -1;//reserved bits
    }

    h->b_loop_filter_disable = cavs_bitstream_get_bit1(s);

    if(!h->b_loop_filter_disable)
    {
        h->b_loop_filter_parameter_flag = cavs_bitstream_get_bit1(s);
        if(h->b_loop_filter_parameter_flag)
        {
            //se
            h->i_alpha_c_offset = cavs_bitstream_get_se(s);	
            h->i_beta_offset = cavs_bitstream_get_se(s);	
        }
        else
        {
            h->i_alpha_c_offset = 0;	
            h->i_beta_offset = 0;	
        }
    }
    
    if(sh->i_profile_id != PROFILE_JIZHUN)
    {
        h->weighting_quant_flag = cavs_bitstream_get_bit1(s);
        if (h->weighting_quant_flag)
        {
            uint32_t scene_adapt_disable = 0;
            uint32_t scene_model_update_flag = 0;

            h->mb_adapt_wq_disable = cavs_bitstream_get_bit1(s);
            h->chroma_quant_param_disable = cavs_bitstream_get_bit1(s);
            if (! h->chroma_quant_param_disable)
            {
                h->chroma_quant_param_delta_u = cavs_bitstream_get_se(s);	
                h->chroma_quant_param_delta_v = cavs_bitstream_get_se(s);	
            }
            h->weighting_quant_param_index = cavs_bitstream_get_bits(s, 2);

            if (scene_adapt_disable==0 && scene_model_update_flag==0)
            {
                h->weighting_quant_model = cavs_bitstream_get_bits(s, 2);
            }
            else
            {
                h->weighting_quant_model=3;
            }
        }
        if((h->weighting_quant_param_index == 1)||(/*(!h->mb_adapt_wq_disable)&&*/(h->weighting_quant_param_index == 3)))
        {
            int i;
#if 0      
			for(i=0;i<6;i++)
                h->weighting_quant_param_delta1[i]=(uint8_t)cavs_bitstream_get_se(s) + wq_param_default[0][i];
#else
			for(i=0;i<6;i++)
                h->weighting_quant_param_undetail[i]=(uint8_t)cavs_bitstream_get_se(s) + wq_param_default[0][i];
#endif

        }
        if((h->weighting_quant_param_index == 2)||(/*(!h->mb_adapt_wq_disable)&&*/(h->weighting_quant_param_index == 3)))
        {
            int i;
#if 0 
			for(i=0;i<6;i++)
                h->weighting_quant_param_delta2[i]=(uint8_t)cavs_bitstream_get_se(s) + wq_param_default[1][i];//?
#else
			for(i=0;i<6;i++)
                h->weighting_quant_param_detail[i]=(uint8_t)cavs_bitstream_get_se(s) + wq_param_default[1][i];//?
#endif
        }	
    }
    else 
    {
        h->weighting_quant_flag=0;
        h->mb_adapt_wq_disable=1;
        h->chroma_quant_param_delta_u=0;	
        h->chroma_quant_param_delta_v=0;
    }

    //YiDong related
    if (sh->i_profile_id == PROFILE_YIDONG)
    {
        h->vbs_enable=cavs_bitstream_get_bit1(s);
        if (h->vbs_enable)
        {
            h->vbs_qp_shift=cavs_bitstream_get_bits(s,4);
        }
        h->asi_enable=cavs_bitstream_get_bit1(s);
    }
    else
    {
        h->vbs_enable = 0;
        h->asi_enable = 0;
    }

    //GuangDian related
    if (sh->i_profile_id == PROFILE_GUANGDIAN)
        h->b_aec_enable = cavs_bitstream_get_bit1(s);
    else
        h->b_aec_enable = 0;
    
	/* NOTE : just process once to get status of AEC */
	if( !sh->b_flag_aec )
	{
		sh->b_flag_aec = 1;
		sh->b_aec_enable = h->b_aec_enable;
	}

    h->b_pb_field_enhanced = 0;

#if CAVS_TRACE
    fprintf(trace_fp,"I帧序列头信息读取完毕\n\n\n\n");
    fprintf(trace_fp,"*******************************************************\n");
    fflush(trace_fp);
#endif

    return 0;
}

int cavs_get_pb_picture_header(cavs_bitstream *s,xavs_picture_header *h,xavs_video_sequence_header *sh)
{
    //uint32_t   i_value=0;
    
#if CAVS_TRACE
    fprintf(trace_fp,"PB帧序列头信息读取开始\n\n\n\n");
    fprintf(trace_fp,"*******************************************************\n");
    fflush(trace_fp);
#endif

    h->i_bbv_delay = cavs_bitstream_get_bits(s, 16);

    if (sh->i_profile_id == PROFILE_GUANGDIAN)
    {
        cavs_bitstream_get_bit1(s);	//marker bit
        cavs_bitstream_get_bits(s, 7);	//bbv_delay extension
    }

    h->i_picture_coding_type = cavs_bitstream_get_bits(s, 2);
    if(  h->i_picture_coding_type != 1 &&  h->i_picture_coding_type != 2 )
    {
        printf("[error]pic header i_picture_coding_type is wrong \n");
        return -1;
    }
    h->i_picture_distance = cavs_bitstream_get_bits(s, 8);

    if(sh->b_low_delay)
    {
        h->i_bbv_check_times = cavs_bitstream_get_ue(s);	
    }
    h->b_progressive_frame = cavs_bitstream_get_bit1(s);

    if(h->b_progressive_frame==0)
    {
        h->b_picture_structure = cavs_bitstream_get_bit1(s);

        if(h->b_picture_structure==0)
        {
            h->b_advanced_pred_mode_disable = cavs_bitstream_get_bit1(s);
            if(h->b_advanced_pred_mode_disable != 1)
            {
                return -1;
            }
        }
    }
    else
    {
        h->b_picture_structure = 1;
    }
    sh->b_interlaced = !h->b_picture_structure;
    h->b_top_field_first = cavs_bitstream_get_bit1(s);
    h->b_top_field_first = !h->b_picture_structure;// FIXIT:not support 0
    h->b_repeat_first_field = cavs_bitstream_get_bit1(s);
    h->b_fixed_picture_qp = cavs_bitstream_get_bit1(s);
    h->i_picture_qp = cavs_bitstream_get_bits(s, 6);

    if(!(h->i_picture_coding_type==XAVS_B_PICTURE&&h->b_picture_structure==1))
    {
        h->b_picture_reference_flag = cavs_bitstream_get_bit1(s);
    }

    h->b_no_forward_reference_flag = cavs_bitstream_get_bit1(s);
    if (sh->i_profile_id == PROFILE_GUANGDIAN)
    {
        h->b_pb_field_enhanced = cavs_bitstream_get_bit1(s); //pb_field_enhanced_flag
        cavs_bitstream_get_bits(s, 2); //reserved bits
    }
    else
    {
        h->b_pb_field_enhanced = 0;
        if(cavs_bitstream_get_bits(s,3)!=0)
            ;//return -1;
    }
    h->b_skip_mode_flag = cavs_bitstream_get_bit1(s);
    h->b_loop_filter_disable = cavs_bitstream_get_bit1(s);

    if(!h->b_loop_filter_disable)
    {
        h->b_loop_filter_parameter_flag = cavs_bitstream_get_bit1(s);
        if(h->b_loop_filter_parameter_flag)
        {
            //se
            h->i_alpha_c_offset = cavs_bitstream_get_se(s);	
            h->i_beta_offset = cavs_bitstream_get_se(s);	
        }
        else
        {
            h->i_alpha_c_offset = 0;	
            h->i_beta_offset = 0;	
        }
    }
    
    if(sh->i_profile_id != PROFILE_JIZHUN)
    {
        h->weighting_quant_flag = cavs_bitstream_get_bit1(s);
        if (h->weighting_quant_flag)
        {
            uint32_t scene_adapt_disable = 0;
            uint32_t scene_model_update_flag = 0;
            
            h->mb_adapt_wq_disable = cavs_bitstream_get_bit1(s);
            h->chroma_quant_param_disable = cavs_bitstream_get_bit1(s);
            if (! h->chroma_quant_param_disable)
            {
                h->chroma_quant_param_delta_u = cavs_bitstream_get_se(s);	
                h->chroma_quant_param_delta_v = cavs_bitstream_get_se(s);	
            }
            h->weighting_quant_param_index = cavs_bitstream_get_bits(s, 2);

            if (scene_adapt_disable==0&&scene_model_update_flag==0)
            {
                h->weighting_quant_model = cavs_bitstream_get_bits(s, 2);
            }
            else
            {
                h->weighting_quant_model = 3;
            }
            if((h->weighting_quant_param_index == 1)||(/*(!h->mb_adapt_wq_disable)&&*/(h->weighting_quant_param_index == 3)))
            {
                int i;
#if 0                
                for( i = 0; i < 6; i++)
                    h->weighting_quant_param_delta1[i] = (uint8_t)cavs_bitstream_get_se(s) + wq_param_default[0][i];
#else
				for( i = 0; i < 6; i++)
                    h->weighting_quant_param_undetail[i] = (uint8_t)cavs_bitstream_get_se(s) + wq_param_default[0][i];
#endif
            }
            if((h->weighting_quant_param_index == 2)||(/*(!h->mb_adapt_wq_disable)&&*/(h->weighting_quant_param_index == 3)))
            {
                int i;
#if 0                
                for( i = 0; i < 6; i++)
                    h->weighting_quant_param_delta2[i] = (uint8_t)cavs_bitstream_get_se(s) + wq_param_default[1][i];
#else
				for( i = 0; i < 6; i++)
                    h->weighting_quant_param_detail[i] = (uint8_t)cavs_bitstream_get_se(s) + wq_param_default[1][i];
#endif
            }	
        }
    }

    if (sh->i_profile_id == PROFILE_YIDONG)
    {
        if(h->i_picture_coding_type==XAVS_P_PICTURE)
        {
            h->limitDC_refresh_frame = cavs_bitstream_get_bit1(s);
        }
    }

    if (sh->i_profile_id == PROFILE_YIDONG)
    {
        h->vbs_enable = cavs_bitstream_get_bit1(s);
        if (h->vbs_enable)
        {
            h->vbs_qp_shift = cavs_bitstream_get_bits(s, 4);
        }
        h->asi_enable = cavs_bitstream_get_bit1(s);
    }
    else
    {
        h->vbs_enable = 0;
        h->asi_enable = 0;
    }

    if (sh->i_profile_id == PROFILE_GUANGDIAN)
    	h->b_aec_enable = cavs_bitstream_get_bit1(s);
    else
    	h->b_aec_enable = 0;


#if CAVS_TRACE
    fprintf(trace_fp,"PB帧序列头信息读取完毕\n\n\n\n");
    fprintf(trace_fp,"*******************************************************\n");
    fflush(trace_fp);
#endif

    return 0;
}

int cavs_cavlc_get_ue(cavs_decoder *p)
{
    return cavs_bitstream_get_ue(&p->s);
}

int cavs_cavlc_get_se(cavs_decoder *p)
{
    return cavs_bitstream_get_se(&p->s);
}

int cavs_cavlc_get_mb_type_p(cavs_decoder *p)
{
    const int i_mb_type = cavs_bitstream_get_ue(&p->s) + P_SKIP + p->ph.b_skip_mode_flag;

    if (i_mb_type > P_8X8)
    {
        p->i_cbp_code = i_mb_type - P_8X8 - 1;
        return I_8X8;
    }
    
    return i_mb_type;
}

int cavs_cavlc_get_mb_type_b(cavs_decoder *p)
{
    const int i_mb_type = cavs_bitstream_get_ue(&p->s) + B_SKIP + p->ph.b_skip_mode_flag;

    if (i_mb_type > B_8X8)
    {
        p->i_cbp_code = i_mb_type - B_8X8 - 1;
        return I_8X8;
    }
    
    return i_mb_type;
}

int cavs_cavlc_get_intra_luma_pred_mode(cavs_decoder *p)
{
    p->pred_mode_flag = cavs_bitstream_get_bit1(&p->s);

    if(!p->pred_mode_flag)
    {
        return cavs_bitstream_get_bits(&p->s, 2);
    }
    else
        return -1;
}

static const uint8_t cbp_tab[64][2] =	/*[cbp_code][inter]*/
{
	{63, 0},{15,15},{31,63},{47,31},{ 0,16},{14,32},{13,47},{11,13},
	{ 7,14},{ 5,11},{10,12},{ 8, 5},{12,10},{61, 7},{ 4,48},{55, 3},
	{ 1, 2},{ 2, 8},{59, 4},{ 3, 1},{62,61},{ 9,55},{ 6,59},{29,62},
	{45,29},{51,27},{23,23},{39,19},{27,30},{46,28},{53, 9},{30, 6},
	{43,60},{37,21},{60,44},{16,26},{21,51},{28,35},{19,18},{35,20},
	{42,24},{26,53},{44,17},{32,37},{58,39},{24,45},{20,58},{17,43},
	{18,42},{48,46},{22,36},{33,33},{25,34},{49,40},{40,52},{36,49},
	{34,50},{50,56},{52,25},{54,22},{41,54},{56,57},{38,41},{57,38}
};

int cavs_cavlc_get_intra_cbp(cavs_decoder *p)
{
    if (!p->b_have_pred)
    {
        p->i_cbp_code = cavs_bitstream_get_ue(&p->s);
    }
    if(p->i_cbp_code > 63 || p->i_cbp_code < 0)
	{
		p->b_error_flag = 1;

		return -1;
	}
    return cbp_tab[p->i_cbp_code][0];
}

int cavs_cavlc_get_inter_cbp(cavs_decoder *p)
{
    p->i_cbp_code = cavs_bitstream_get_ue(&p->s);
    if(p->i_cbp_code > 63 || p->i_cbp_code < 0)
	{
		p->b_error_flag = 1;

		return -1;
	}
    return cbp_tab[p->i_cbp_code][1];
}

int cavs_cavlc_get_ref_p(cavs_decoder *p, int ref_scan_idx)
{
    if(p->ph.b_picture_reference_flag )
        return 0;
    else
    {
        if(p->ph.b_picture_structure==0 /*&& p->ph.i_picture_coding_type==XAVS_P_PICTURE*/)
            return cavs_bitstream_get_bits(&p->s,2);
        else
            return cavs_bitstream_get_bit1(&p->s);
    }
}

int cavs_cavlc_get_ref_b(cavs_decoder *p, int i_list, int ref_scan_idx)
{
    if(p->ph.b_picture_structure == 0 && p->ph.b_picture_reference_flag ==0)
        return cavs_bitstream_get_bit1(&p->s);	//mb_reference_index
    else
        return 0;
}

int cavs_cavlc_get_mb_part_type(cavs_decoder *p)
{
    return cavs_bitstream_get_bits(&p->s, 2);
}

//i_list, i_down and xy_idx is not used here(just for cabac)
int cavs_cavlc_get_mvd(cavs_decoder *p, int i_list, int i_down, int xy_idx)
{
    return cavs_bitstream_get_se(&p->s);
}

int cavs_cavlc_get_coeffs(cavs_decoder *p, const xavs_vlc *p_vlc_table, int i_escape_order, int b_chroma)
{
    int level_code, esc_code, level, run, mask;
    int *level_buf = p->level_buf;
    int *run_buf = p->run_buf;
    int i;
    
    for( i = 0; i < 65; i++ ) 
    {
        level_code = cavs_bitstream_get_ue_k(&p->s,p_vlc_table->golomb_order);
        if(level_code >= ESCAPE_CODE) 
        {
            run = ((level_code - ESCAPE_CODE) >> 1) + 1;
            esc_code = cavs_bitstream_get_ue_k(&p->s,i_escape_order);

            level = esc_code + (run > p_vlc_table->max_run ? 1 : p_vlc_table->level_add[run]);
            while(level > p_vlc_table->inc_limit)
                p_vlc_table++;
            mask = -(level_code & 1);
            level = (level^mask) - mask;
        } 
        else
        {
            level = p_vlc_table->rltab[level_code][0];
            if(!level) //end of block signal
                break;
            run   = p_vlc_table->rltab[level_code][1];
            p_vlc_table += p_vlc_table->rltab[level_code][2];
        }
        level_buf[i] = level;
        run_buf[i] = run;
    }
    
    return i;
}