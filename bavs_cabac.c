/*****************************************************************************
* cabac.c: arithmetic coder
*****************************************************************************
*/


#include "bavs_globe.h"

#define TEST_CABAC 1

static const uint8_t sub_mb_type_b_to_golomb[13] = {
	10, 4, 5, 1, 11, 6, 7, 2, 12, 8, 9, 3, 0
};

static const uint8_t mb_type_b_to_golomb[3][9] = {
	{4, 8, 12, 10, 6, 14, 16, 18, 20},    /* D_16x8 */
	{5, 9, 13, 11, 7, 15, 17, 19, 21},    /* D_8x16 */
	{1, -1, -1, -1, 2, -1, -1, -1, 3}     /* D_16x16 */
};


#define BIARI_CTX_INIT1_LOG(jj,ctx)\
{\
	for (j=0; j<jj; j++)\
{\
	ctx[j].lg_pmps = 1023;\
	ctx[j].mps = 0;\
	ctx[j].cycno = 0;\
	}\
}

#define BIARI_CTX_INIT2_LOG(ii,jj,ctx)\
{\
	for (i=0; i<ii; i++)\
	for (j=0; j<jj; j++)\
{\
	ctx[i][j].lg_pmps = 1023;\
	ctx[i][j].mps = 0;\
	ctx[i][j].cycno = 0;\
	}\
}
#if 0 // FIXIT
//take pseudo startcode into account
#define get_byte(){                         \
	cb->buffer = *(cb->bs.p++);				\
	cb->bs.i_startcode = (cb->bs.i_startcode << 8) + cb->buffer; \
	if((cb->bs.i_startcode&0xffffff)!=0x000002) \
		cb->bits_to_go = 7;                     \
	else										\
	{											\
		cb->buffer = 0;							\
		cb->bits_to_go = 5;						\
	}											\
}
#else
#define get_byte(){                         \
	if(cb->bs.p > cb->bs.p_end )\
	{\
		cb->b_cabac_error = 1;\
		return 0; \
	}\
	cb->buffer = *(cb->bs.p++);				\
	cb->bits_to_go = 7;                     \
}

#endif

void biari_init_context_logac (bi_ctx_t* ctx)
{
	ctx->lg_pmps = (QUARTER<<LG_PMPS_SHIFTNO)-1; //10 bits precision
	ctx->mps	 = 0;	
	ctx->cycno  =  0;
}

/*****************************************************************************
*
*****************************************************************************/
void cavs_cabac_context_init( cavs_decoder * h)
{
    int i, j;

    //--- motion coding contexts ---
    BIARI_CTX_INIT2_LOG (3, NUM_MB_TYPE_CTX,   h->cabac.mb_type_contexts);
    BIARI_CTX_INIT2_LOG (2, NUM_B8_TYPE_CTX,   h->cabac.b8_type_contexts);
    BIARI_CTX_INIT2_LOG (2, NUM_MV_RES_CTX,    h->cabac.mv_res_contexts);
    BIARI_CTX_INIT2_LOG (2, NUM_REF_NO_CTX,    h->cabac.ref_no_contexts);
    BIARI_CTX_INIT1_LOG (   NUM_DELTA_QP_CTX,  h->cabac.delta_qp_contexts);
    BIARI_CTX_INIT1_LOG (   NUM_MB_AFF_CTX,    h->cabac.mb_aff_contexts);

    //--- texture coding contexts ---
    BIARI_CTX_INIT1_LOG (                 NUM_IPR_CTX,  h->cabac.ipr_contexts);
    BIARI_CTX_INIT1_LOG (                 NUM_CIPR_CTX, h->cabac.cipr_contexts);
    BIARI_CTX_INIT2_LOG (3,               NUM_CBP_CTX,  h->cabac.cbp_contexts);
    BIARI_CTX_INIT2_LOG (NUM_BLOCK_TYPES, NUM_BCBP_CTX, h->cabac.bcbp_contexts);
    BIARI_CTX_INIT2_LOG (NUM_BLOCK_TYPES, NUM_ONE_CTX,  h->cabac.one_contexts);
    BIARI_CTX_INIT2_LOG (NUM_BLOCK_TYPES, NUM_ABS_CTX,  h->cabac.abs_contexts);
    BIARI_CTX_INIT2_LOG (NUM_BLOCK_TYPES, NUM_MAP_CTX,  h->cabac.fld_map_contexts);
    BIARI_CTX_INIT2_LOG (NUM_BLOCK_TYPES, NUM_LAST_CTX, h->cabac.fld_last_contexts);
    BIARI_CTX_INIT2_LOG (NUM_BLOCK_TYPES, NUM_MAP_CTX,  h->cabac.map_contexts);
    BIARI_CTX_INIT2_LOG (NUM_BLOCK_TYPES, NUM_LAST_CTX, h->cabac.last_contexts);

#if B_MB_WEIGHTING
    biari_init_context_logac( &h->cabac.mb_weighting_pred);
#endif
}

int cavs_cabac_start_decoding(cavs_cabac_t * cb, cavs_bitstream *s)
{
    int i;

    cb->bs = *s;
    cb->s1 = 0;
    cb->t1 = 0xFF;
    cb->value_s = cb->value_t = 0;
    cb->bits_to_go = 0;
    for (i = 0; i< B_BITS - 1; i++)
    {
        if (--cb->bits_to_go < 0)
            get_byte();
        cb->value_t = (cb->value_t<<1)  | ((cb->buffer >> cb->bits_to_go) & 0x01); 
    }

    while (cb->value_t < QUARTER)
    {
        if (--cb->bits_to_go < 0) 
            get_byte();   
        // Shift in next bit and add to value 
        cb->value_t = (cb->value_t << 1) | ((cb->buffer >> cb->bits_to_go) & 0x01);
        cb->value_s++;
    }
    cb->value_t = cb->value_t & 0xff;

    //cb->dec_bypass  = 0;
    cb->b_cabac_error = 0;

	return 0;
}

static inline unsigned int cavs_biari_decode_symbol(cavs_cabac_t *cb, bi_ctx_t *bi_ct)
{
	//unsigned char s_flag;
    unsigned char cycno = bi_ct->cycno;
	unsigned char cwr = cwr_trans[cycno];
	unsigned char bit = bi_ct->mps;
    unsigned short lg_pmps = bi_ct->lg_pmps;
    unsigned short t_rlps = lg_pmps>>2;
    unsigned short s2 = cb->s1;
#if TEST_CABAC
	int16_t t1_d = cb->t1 - t_rlps; // lg_pmps_shift2[lg_pmps];
	unsigned short t2 = (uint8_t)t1_d;
	//if ( t1_d >= 0 ){
	//    //s2 = cb->s1;
	//    t2 = t1_d; //s_flag = 0;
	//}else
	if (t1_d<0){
		s2++; //s2 = cb->s1+1; //t2 = (uint8_t)t1_d; // 256 + t1_d;
		t_rlps += cb->t1; //s_flag = 1;
	}
#endif

	if( s2 < cb->value_s || (s2 == cb->value_s && cb->value_t < t2)) //MPS
	{
		cb->s1 = s2;
		cb->t1 = t2;

		//if (cb->dec_bypass) return(bit);
		//update other parameters
		bi_ct->cycno = cycno_trans1[cycno]; //if (cycno == 0)cycno = 1;(cycno | (!cycno));
		bi_ct->lg_pmps = lg_pmps_tab_mps[cwr][lg_pmps];
		return bit;
	}
    else //if (s2 > cb->value_s || ( s2 == cb->value_s && cb->value_t >= t2)) //LPS
    {
		bit ^= 0x01; //bit=!bit;//LPS 
#if TEST_CABAC
        /*t_rlps = (s_flag==0)? lg_pmps_shift2[lg_pmps]
        	:(cb->t1 + lg_pmps_shift2[lg_pmps]);*/
#endif

        if (s2==cb->value_s) cb->value_t = (cb->value_t-t2);
        else{
        	if (--cb->bits_to_go < 0) get_byte();
        	// Shift in next bit and add to value 
        	cb->value_t = ((cb->value_t << 1) | ((cb->buffer >> cb->bits_to_go) & 0x01)) + 256-t2;
        	//cb->value_t = 256 + cb->value_t - t2;
        }

        //restore range		
        //while (t_rlps < QUARTER){
        //	t_rlps=t_rlps<<1;
        //	if (--cb->bits_to_go < 0) get_byte();   
        //	// Shift in next bit and add to value 
        //	cb->value_t = (cb->value_t << 1) | ((cb->buffer >> cb->bits_to_go) & 0x01);
        //}
		int wn = log2_tab[t_rlps];
		uint8_t rsd = (uint8_t)((cb->buffer << (8 - cb->bits_to_go)) | (*(cb->bs.p) >> (cb->bits_to_go))) >> (8 - wn);
		cb->value_t = (cb->value_t << wn) | rsd; //+rsd;
		cb->bits_to_go -= wn;
		if (cb->bits_to_go < 0){
			cb->buffer = *(cb->bs.p++);
			cb->bits_to_go += 8;
		}

        cb->s1 = 0;
		cb->t1 = (uint8_t)(t_rlps << wn); //t_rlps & 0xff;

        //restore value
        cb->value_s = 0;
        //while (cb->value_t<QUARTER){
        //	int j;
        //	if (--cb->bits_to_go < 0) get_byte();   
        //	j=(cb->buffer >> cb->bits_to_go) & 0x01;
        //	// Shift in next bit and add to value 
        //	cb->value_t = (cb->value_t << 1) | j;
        //	cb->value_s++;
        //}
		while (!cb->value_t){
			if (--cb->bits_to_go < 0) get_byte();
			// Shift in next bit and add to value 
			cb->value_t = (cb->value_t << 1) | ((cb->buffer >> cb->bits_to_go) & 0x01);
			cb->value_s++;
		}
		if (cb->value_t < QUARTER){
			int wn = log2_tab[cb->value_t];
			uint8_t rsd = (cb->buffer << (8 - cb->bits_to_go)) | (*(cb->bs.p) >> (cb->bits_to_go));
			cb->value_t = (cb->value_t << wn) | (rsd >> (8 - wn));
			cb->value_s += wn;
			cb->bits_to_go -= wn;
			if (cb->bits_to_go < 0){
				cb->buffer = *(cb->bs.p++);
				cb->bits_to_go += 8;
			}
		}
		cb->value_t = (uint8_t)cb->value_t; // &0xff;

		//if (cb->dec_bypass) return(bit);
		//update other parameters
		bi_ct->cycno = cycno_trans2[cycno];
		lg_pmps = lg_pmps_tab[cwr][lg_pmps];
		if (lg_pmps >= 1024){
			lg_pmps = 2047 - lg_pmps;
			bi_ct->mps = bit; // !(bi_ct->mps);
		}
		bi_ct->lg_pmps = lg_pmps;
		return bit;
    }

    //bi_ct->lg_pmps = lg_pmps;
    //return(bit);
}

static inline unsigned int cavs_biari_decode_symbol_bypass(cavs_cabac_t *cb)
{
	//bi_ctx_t ctx;
	//uint32_t bit;
	//ctx.cycno = 0; // added by xun
	//ctx.lg_pmps = (QUARTER << LG_PMPS_SHIFTNO) - 1;
	//ctx.mps = 0;
	//cb->dec_bypass = 1;
	//bit = cavs_biari_decode_symbol(cb, &ctx);

	//unsigned short lg_pmps = (QUARTER << LG_PMPS_SHIFTNO) - 1;
	unsigned short t_rlps = 255; // lg_pmps >> 2;
	unsigned short s2 = cb->s1;
#if TEST_CABAC  
	int16_t t1_d = cb->t1 - t_rlps;  //t_rlps; // lg_pmps_shift2[lg_pmps];	
	unsigned short t2 = (uint8_t)t1_d;
	//if ( t1_d >= 0 ){
	//    //s2 = cb->s1;
	//    t2 = t1_d; //s_flag = 0;
	//}else
	if (t1_d<0){
		s2++; //s2 = cb->s1+1; //t2 = (uint8_t)t1_d; // 256 + t1_d;
		t_rlps += cb->t1; //s_flag = 1;
	}
#endif

	if (s2 < cb->value_s || (s2 == cb->value_s && cb->value_t < t2)) //MPS
	{
		cb->s1 = s2;
		cb->t1 = t2;
		return 0;
	}
	else //if (s2 > cb->value_s || (s2 == cb->value_s && cb->value_t >= t2)) //LPS
	{
#if TEST_CABAC
		/*t_rlps = (s_flag==0)? lg_pmps_shift2[lg_pmps]
		:(cb->t1 + lg_pmps_shift2[lg_pmps]);*/
#endif
		if (s2 == cb->value_s) cb->value_t = (cb->value_t - t2);
		else{
			if (--cb->bits_to_go < 0) get_byte();
			// Shift in next bit and add to value 
			cb->value_t = ((cb->value_t << 1) | ((cb->buffer >> cb->bits_to_go) & 0x01)) + 256 - t2;
			//cb->value_t = 256 + cb->value_t - t2;
		}

		//restore range		
		if (t_rlps < QUARTER){
			t_rlps <<= 1;
			if (--cb->bits_to_go < 0) get_byte();
			// Shift in next bit and add to value 
			cb->value_t = (cb->value_t << 1) | ((cb->buffer >> cb->bits_to_go) & 0x01);
		}
		cb->s1 = 0;
		cb->t1 = (uint8_t)t_rlps; //t_rlps & 0xff;

		//restore value
		cb->value_s = 0;
		while (!cb->value_t){
			if (--cb->bits_to_go < 0) get_byte();
			// Shift in next bit and add to value 
			cb->value_t = (cb->value_t << 1) | ((cb->buffer >> cb->bits_to_go) & 0x01);
			cb->value_s++;
		}
		if (cb->value_t < QUARTER){
			int wn = log2_tab[cb->value_t];
			uint8_t rsd = (cb->buffer << (8 - cb->bits_to_go)) | (*(cb->bs.p) >> (cb->bits_to_go));
			cb->value_t = (cb->value_t << wn) | (rsd >> (8 - wn));
			cb->value_s += wn;
			cb->bits_to_go -= wn;
			if (cb->bits_to_go < 0){
				cb->buffer = *(cb->bs.p++);
				cb->bits_to_go += 8;
			}
		}
		cb->value_t = (uint8_t)cb->value_t; // &0xff;
		return 1;
	}
	//cb->dec_bypass = 0;
	//return bit;
}

int cavs_biari_decode_stuffing_bit(cavs_cabac_t *cb)
{
    bi_ctx_t ctx;
    uint32_t bit;
	ctx.cycno = 0;	// added by xun
    ctx.lg_pmps = 1 << LG_PMPS_SHIFTNO;
    ctx.mps = 0;
    //cb->dec_bypass = 1; // FIXIT
    bit = cavs_biari_decode_symbol(cb, &ctx);
    //cb->dec_bypass = 0;
    
#if CAVS_TRACE
    fprintf(trace_fp,"Stuffingbit\t\t\t%d\n",bit);
    fflush(trace_fp);
#endif

    return bit;
}

/*unsigned*/ int cavs_biari_decode_symbol_w(cavs_cabac_t *cb, bi_ctx_t *bi_ct1 ,bi_ctx_t *bi_ct2 )
{
    uint8_t bit1,bit2; 
    uint8_t pred_MPS,bit;
    uint32_t lg_pmps;
    uint8_t cwr1,cycno1=bi_ct1->cycno;
    uint8_t cwr2,cycno2=bi_ct2->cycno;
    uint32_t lg_pmps1= bi_ct1->lg_pmps,lg_pmps2= bi_ct2->lg_pmps;
    uint32_t t_rlps;
    uint8_t s_flag,is_LPS=0;
    uint32_t s2,t2;
    int32_t t1_d;
    bit1 = bi_ct1->mps;
    bit2 = bi_ct2->mps;

#if TEST_CABAC
    if( cycno1 > 3 )
    {
        return -1;
    }
    cwr1 = cwr_trans[cycno1];
    if( cycno2 > 3 )
    {
        return -1;
    }
    cwr2 = cwr_trans[cycno2];
#else    
    cwr1 = (cycno1<=1)?3:(cycno1==2)?4:5;
    cwr2 = (cycno2<=1)?3:(cycno2==2)?4:5; 
#endif

    if (bit1 == bit2) 
    {
    	pred_MPS = bit1;
    	lg_pmps = (lg_pmps1 + lg_pmps2)/2;
    }
    else 
    {
    	if (lg_pmps1<lg_pmps2) {
    		pred_MPS = bit1;
    		lg_pmps = 1023 - ((lg_pmps2 - lg_pmps1)>>1);
    	}
    	else {
    		pred_MPS = bit2;
    		lg_pmps = 1023 - ((lg_pmps1 - lg_pmps2)>>1);
    	}
    }

#if TEST_CABAC
	if(lg_pmps>1023)
	{
		return -1;
	}
    t1_d = cb->t1 - lg_pmps_shift2[lg_pmps];
    if ( t1_d >= 0 )
    {
    	s2 = cb->s1;
    	t2 = t1_d;
    	s_flag = 0;
    }
    else
    {
    	s2 = cb->s1 + 1;
    	t2 = 256 + t1_d;
    	s_flag = 1;
    }
#else
    if (cb->t1>=(lg_pmps>>LG_PMPS_SHIFTNO))
    {
    	s2 = cb->s1;
    	t2 = cb->t1 - (lg_pmps>>LG_PMPS_SHIFTNO);
    	s_flag = 0;
    }
    else
    {
    	s2 = cb->s1 + 1;
    	t2 = 256 + cb->t1 - (lg_pmps>>LG_PMPS_SHIFTNO);
    	s_flag = 1;
    }
#endif

    bit = pred_MPS;
    if(s2 > cb->value_s || (s2 == cb->value_s && cb->value_t >= t2))//LPS
    {			
    	is_LPS = 1;
    	bit=!bit;//LPS

#if TEST_CABAC
       t_rlps = (s_flag==0)? lg_pmps_shift2[lg_pmps]:(cb->t1 + lg_pmps_shift2[lg_pmps]);	
#else
    	t_rlps = (s_flag==0)? (lg_pmps>>LG_PMPS_SHIFTNO):(cb->t1 + (lg_pmps>>LG_PMPS_SHIFTNO));		
#endif

    	if (s2 == cb->value_s)
    		cb->value_t = (cb->value_t-t2);
    	else
    	{		
    		if (--cb->bits_to_go < 0) 
    			get_byte();   
    		// Shift in next bit and add to value 
    		cb->value_t = (cb->value_t << 1) | ((cb->buffer >> cb->bits_to_go) & 0x01);
    		cb->value_t = 256 + cb->value_t - t2;
    	}

    	//restore range		
    	while (t_rlps < QUARTER){
    		t_rlps=t_rlps<<1;
    		if (--cb->bits_to_go < 0) 
    			get_byte();   
    		// Shift in next bit and add to value 
    		cb->value_t = (cb->value_t << 1) | ((cb->buffer >> cb->bits_to_go) & 0x01);
    	}
    	cb->s1 = 0;
    	cb->t1 = t_rlps & 0xff;

    	//restore value
    	cb->value_s = 0;
    	while (cb->value_t<QUARTER){
    		int j;
    		if (--cb->bits_to_go < 0) 
    			get_byte();   
    		j=(cb->buffer >> cb->bits_to_go) & 0x01;
    		// Shift in next bit and add to value 

    		cb->value_t = (cb->value_t << 1) | j;
    		cb->value_s++;
    	}
    	cb->value_t = cb->value_t & 0xff;			
    }//--LPS 
    else //MPS
    {
    	cb->s1 = s2;
    	cb->t1 = t2;
    }

#if TEST_CABAC
    cycno1 = cycno_trans_2d[bit!=bit1][cycno1];
    cycno2 = cycno_trans_2d[bit!=bit2][cycno2];
#else
    if (bit!=bit1)
    {			
    	cycno1 = (cycno1<=2)?(cycno1+1):3;//LPS occurs
    }
    else{
    	if (cycno1==0) cycno1 =1;
    }

    if (bit !=bit2)
    {			
    	cycno2 = (cycno2<=2)?(cycno2+1):3;//LPS occurs
    }
    else
    {
    	if (cycno2==0) cycno2 =1;
    }
#endif
    
    bi_ct1->cycno = cycno1;
    bi_ct2->cycno = cycno2;

    //update probability estimation
#if TEST_CABAC
    //bi_ct1
    if( lg_pmps1 > 1023 || cwr1 > 5 )
    {
    	return -1;
    }
    
    if (bit==bit1)
    {	
        lg_pmps1 = lg_pmps_tab_mps[cwr1][lg_pmps1];
    }
    else
    {
        lg_pmps1 = lg_pmps_tab[cwr1][lg_pmps1];
        if (lg_pmps1 >= 1024)
        {
            lg_pmps1 = 2047 - lg_pmps1;
            bi_ct1->mps = !(bi_ct1->mps);
        }	
    }
    bi_ct1->lg_pmps = lg_pmps1;

    //bi_ct2
    if( lg_pmps2 > 1023 || cwr2 > 5 )
    {
    	return -1;
    }
    
    if (bit==bit2)
    {        	
        lg_pmps2 = lg_pmps_tab_mps[cwr2][lg_pmps2];
    }
    else
    {
        lg_pmps2 = lg_pmps_tab[cwr2][lg_pmps2];
        if (lg_pmps2 >= 1024)
        {
            lg_pmps2 = 2047 - lg_pmps2;
            bi_ct2->mps = !(bi_ct2->mps);
        }	
    }
    bi_ct2->lg_pmps = lg_pmps2;  
    

#else    
    //bi_ct1
    if (bit==bit1)
    {
        lg_pmps1 = lg_pmps1 - (unsigned int)(lg_pmps1>>cwr1) - (unsigned int)(lg_pmps1>>(cwr1+2));	
    }
    else
    {
        switch(cwr1) {
            case 3:	lg_pmps1 = lg_pmps1 + 197;					
            	break;
            case 4: lg_pmps1 = lg_pmps1 + 95;
            	break;
            default:lg_pmps1 = lg_pmps1 + 46; 
        }

        if (lg_pmps1>=(256<<LG_PMPS_SHIFTNO))
        {
            lg_pmps1 = (512<<LG_PMPS_SHIFTNO) - 1 - lg_pmps1;
            bi_ct1->mps = !(bi_ct1->mps);
        }	
    }
    bi_ct1->lg_pmps = lg_pmps1;

    //bi_ct2
    if (bit==bit2)
    {
        lg_pmps2 = lg_pmps2 - (unsigned int)(lg_pmps2>>cwr2) - (unsigned int)(lg_pmps2>>(cwr2+2));	
    }
    else
    {
        switch(cwr2) {
            case 3:	lg_pmps2 = lg_pmps2 + 197;					
                break;
            case 4: lg_pmps2 = lg_pmps2 + 95;
                break;
            default:lg_pmps2 = lg_pmps2 + 46; 
        }

        if (lg_pmps2>=(256<<LG_PMPS_SHIFTNO))
        {
            lg_pmps2 = (512<<LG_PMPS_SHIFTNO) - 1 - lg_pmps2;
            bi_ct2->mps = !(bi_ct2->mps);
        }	
    }
    bi_ct2->lg_pmps = lg_pmps2;  
#endif

    return(bit);
}

int cavs_cabac_get_skip(cavs_decoder *p)
{
    int i = 0, symbol = 0;
    bi_ctx_t * ctx = p->cabac.one_contexts[0];

    while( cavs_biari_decode_symbol(&p->cabac, ctx+i)==0 ) {
        symbol += 1;
        i ++;
        if( i >= 3 ) i=3;

		if( symbol > (p->i_mb_num>>p->param.b_interlaced) ) /* NOTE : remove endless loop */
		{
			 p->b_error_flag = 1;
			 return -1;
		}
    }

#if CAVS_TRACE
    fprintf(trace_fp,"Skip run\t\t\t%d\n",symbol);
    fflush(trace_fp);
#endif
    
    p->b_error_flag = (&(p->cabac))->b_cabac_error;
    if( p->b_error_flag )
    {
        printf("[error]MB skip run of AEC is wrong\n");
        return -1;
    }

    return symbol;
}

int cavs_cabac_get_mb_type_p(cavs_decoder *p)
{
    bi_ctx_t * ctx = p->cabac.mb_type_contexts[1];
    int symbol = 0, i = 0;

#if 0//p->ph.b_skip_mode_flag//0
    while( cavs_biari_decode_symbol (&p->cabac, ctx+i)==0 ) {
    	++symbol;
    	++i;
    	if( i>=4 ) i=4;
    }
    
#if CAVS_TRACE
    fprintf(trace_fp,"MB type\t\t\t%d\n",symbol);
    fflush(trace_fp);
#endif

    p->b_error_flag = (&(p->cabac))->b_cabac_error;
    if( p->b_error_flag )
    {
    	printf("[error]MB type P of AEC is wrong\n");
    }
   
    if (symbol)	//symbol == 0 for I8x8
    	return symbol + P_SKIP - !p->ph.b_skip_mode_flag;
    else
    	return I_8X8;
#else
	const int DMapSkip[5] = {4,0,1,2,3};
	const int DMapNonSkip[6] = {5,0, 1,2,3,4};

	while( cavs_biari_decode_symbol (&p->cabac, ctx+i)==0 ) {
            symbol++;
            i++;
            if( i>=4 ) i=4;

			if(symbol > 5) /* NOTE : remove endless loop */
			{
				p->b_error_flag = 1;
				return -1;
			}
       }

	if( p->ph.b_skip_mode_flag )
	{
		if(symbol > 4)
		{
			p->b_error_flag = 1;
			return -1;
		}
		symbol =  DMapSkip[symbol];
	}
	else
	{
		if(symbol > 5)
		{
			p->b_error_flag = 1;
			return -1;
		}
		symbol =  DMapNonSkip[symbol];
	}

    p->b_error_flag = (&(p->cabac))->b_cabac_error;
    if( p->b_error_flag )
    {
    	printf("[error]MB type P of AEC is wrong\n");
    }
   
    if( symbol + p->ph.b_skip_mode_flag == 5  )
        return I_8X8;
    else
    {
        return symbol + P_SKIP + p->ph.b_skip_mode_flag; 
    }
#endif

}

int cavs_cabac_get_mb_type_b(cavs_decoder *p)
{
    const int i_mb_type_left = p->i_mb_type_left;
    const int i_mb_type_top  = p->p_mb_type_top[p->i_mb_x];
    int i = (i_mb_type_left != -1 && i_mb_type_left != B_SKIP && i_mb_type_left != B_DIRECT)
    	+ (i_mb_type_top != -1 && i_mb_type_top != B_SKIP && i_mb_type_top != B_DIRECT);
    
    bi_ctx_t * ctx = p->cabac.mb_type_contexts[2];
    int symbol;

    if( cavs_biari_decode_symbol(&p->cabac, ctx + i)==0 ) {
    	symbol = 0;
    }
    else
    {
        symbol = 1;
        i = 4;
        while( cavs_biari_decode_symbol (&p->cabac, ctx + i)==0 )
        {
            ++symbol;
            ++i;
            if( i>=10 ) i=10;

			if( symbol > 29 ) /* NOTE : remove endless loop */
			{
				p->b_error_flag = 1;
				return -1;
			}
        }
    }

#if CAVS_TRACE
    fprintf(trace_fp,"MB type\t\t\t%d\n",symbol);
    fflush(trace_fp);
#endif

    p->b_error_flag = (&(p->cabac))->b_cabac_error;
    if( p->b_error_flag )
    {
        printf("[error]MB type B of AEC is wrong\n");
        return -1;
    }
    
    if (symbol + p->ph.b_skip_mode_flag == 24 )
    	return I_8X8;
    else
    	return symbol + B_SKIP + p->ph.b_skip_mode_flag;
}

static const int map_intra_pred_mode[5] = {-1, 1, 2, 3, 0};
int cavs_cabac_get_intra_luma_pred_mode(cavs_decoder *p)
{
    bi_ctx_t * ctx = p->cabac.one_contexts[1];
    int i = 0;
    int symbol = 0;

    while (cavs_biari_decode_symbol(&p->cabac, ctx + i) == 0)
    {
        ++symbol;
        ++i;
        if (i>=3) i = 3;
        if (symbol == 4) break;
    }

#if CAVS_TRACE
    fprintf(trace_fp,"Luma pred mode\t\t\t%d\n",symbol);
    fflush(trace_fp);
#endif
    p->b_error_flag = (&(p->cabac))->b_cabac_error;
    if( p->b_error_flag )
    {
    	printf("[error]MB luma predmode of AEC is wrong\n");
    }
    p->pred_mode_flag = !symbol;

    return map_intra_pred_mode[symbol];
}

int cavs_cabac_get_intra_chroma_pred_mode(cavs_decoder *p)
{
    bi_ctx_t * ctx = p->cabac.cipr_contexts;
    /* 0 for chroma DC and -1 for not available(outside of slice or not intra) */
    int i = (xavs_mb_pred_mode8x8c[p->i_chroma_pred_mode_left] > 0)
    	+ (xavs_mb_pred_mode8x8c[p->p_chroma_pred_mode_top[p->i_mb_x]] > 0);

    int symbol;
    int bit;
    
    bit = cavs_biari_decode_symbol(&p->cabac, ctx + i);

    if (!bit)
        symbol = 0;
    else
    {
        bit = cavs_biari_decode_symbol(&p->cabac, ctx+3);
        if (!bit) symbol = 1;
        else
        {
            bit = cavs_biari_decode_symbol(&p->cabac, ctx+3);
            /*if (!tempval) symbol = 2;
            else symbol = 3;*/
            symbol = 2 + bit;
        }
    }

#if CAVS_TRACE
    fprintf(trace_fp,"Chroma pred mode\t\t\t%d\n",symbol);
    fflush(trace_fp);
#endif
    p->b_error_flag = (&(p->cabac))->b_cabac_error;
    if( p->b_error_flag )
    {
    	printf("[error]MB chroma predmode of AEC is wrong\n");
    }

    return symbol;
}

int cavs_cabac_get_cbp(cavs_decoder *p)
{
    bi_ctx_t * ctx = p->cabac.cbp_contexts[0];
    int bit, a, b;
    int symbol = 0;

    /* -1 for cbp not available(outside of slice) */
    /* block 0 */
    a = !(p->i_cbp_left & (1<<1));
    b = !(p->p_cbp_top[p->i_mb_x] & (1<<2));
    bit = cavs_biari_decode_symbol(&p->cabac, ctx + a + 2*b);
    symbol |= bit;

    /* block 1 */
    a = !bit;	/*we just get the zero count of left block*/
    b = !(p->p_cbp_top[p->i_mb_x] & (1<<3));
    bit = cavs_biari_decode_symbol(&p->cabac, ctx + a + 2*b);
    symbol |= (bit<<1);

    /* block 2 */
    a = !(p->i_cbp_left & (1<<3));
    b = !(symbol & 1);
    bit = cavs_biari_decode_symbol(&p->cabac, ctx + a + 2*b);
    symbol |= (bit<<2);

    /* block 3 */
    a = !bit;
    b = !(symbol & (1<<1));
    bit = cavs_biari_decode_symbol(&p->cabac, ctx + a + 2*b);
    symbol |= (bit<<3);

    ctx = p->cabac.cbp_contexts[1];
    bit = cavs_biari_decode_symbol(&p->cabac, ctx);
    if (bit)
    {
        bit = cavs_biari_decode_symbol(&p->cabac, ctx + 1);
        if (bit)
            symbol |= (3<<4);
        else
        {
            bit = cavs_biari_decode_symbol(&p->cabac, ctx + 1);
            symbol |= 1 << (4 + bit);
        }
    }

#if CAVS_TRACE
    fprintf(trace_fp,"CBP\t\t\t%d\n",symbol);
    fflush(trace_fp);
#endif

    p->b_error_flag = (&(p->cabac))->b_cabac_error;
    if( p->b_error_flag )
    {
        printf("[error]MB cbp of AEC is wrong\n");
        return -1;
    }

    return symbol;
}

int cavs_cabac_get_dqp(cavs_decoder *p)
{
    bi_ctx_t * ctx = p->cabac.delta_qp_contexts;
    int i = !!p->i_last_dqp;

    int dquant, l;
    int symbol = 1 - cavs_biari_decode_symbol(&p->cabac, ctx + i);
    
    if (symbol)
    {
        symbol = 1 - cavs_biari_decode_symbol(&p->cabac, ctx + 2);
        if (symbol)
        {
            symbol = 0;
            do {
                l = 1 - cavs_biari_decode_symbol(&p->cabac, ctx + 3);
                ++symbol;

				if( symbol > 126 ) /* NOTE : qp_delta can not exceed 63 */
				{
					p->b_error_flag = 1;
					return 0;
				}
            } while (l);
        }
        ++symbol;
    }

    dquant = (symbol+1)>>1;
    if((symbol & 0x01)==0)
    	dquant = -dquant;

    p->i_last_dqp = dquant;

#if CAVS_TRACE
    fprintf(trace_fp,"QP delta\t\t\t%d\n",dquant);
    fflush(trace_fp);
#endif

    p->b_error_flag = (&(p->cabac))->b_cabac_error;
    if( p->b_error_flag )
    {
    	printf("[error]MB dqp of AEC is wrong\n");
    }

    return dquant;
}

int cavs_cabac_get_mvd(cavs_decoder *p, int i_list, int mvd_scan_idx, int xy_idx)
{
    const int mda = abs(p->p_mvd[i_list][mvd_scan_idx-1][xy_idx]);

    int i =  mda < 2 ? 0 : mda < 16 ? 1 : 2;
    bi_ctx_t * ctx = p->cabac.mv_res_contexts[xy_idx];
    int symbol, l, golomb_order = 0, bit = 0;

    if (!cavs_biari_decode_symbol(&p->cabac, ctx + i))
        symbol = 0;
    else if (!cavs_biari_decode_symbol(&p->cabac, ctx + 3))
        symbol = 1;
    else if (!cavs_biari_decode_symbol(&p->cabac, ctx + 4))
        symbol = 2;
    else if (!cavs_biari_decode_symbol(&p->cabac, ctx + 5))
    {
        symbol = 0;
        do {
            l = cavs_biari_decode_symbol_bypass(&p->cabac);
            if (!l)
            {
                symbol += (1<<golomb_order);
                ++golomb_order;

				if(symbol > 4096) /*remove endless loop*/
				{
					p->b_error_flag = 1;
					return -1;
				}
            }

            if( l!=0 && l!=1 )
            {
                	p->b_error_flag = 1;
                	return -1;
            }
        } while(l!=1);
        while (golomb_order--)
        {//next binary part
        	l = cavs_biari_decode_symbol_bypass(&p->cabac);
        	if (l == 1)
        		bit |= (1<<golomb_order);
				
								
            //if( l!=0 && l!=1 )
			//{
			//	p->b_error_flag = 1;
			//	return;
			//}
        }
        symbol += bit;
        symbol = 3 + symbol*2;
    }
    else
    {
        symbol = 0;
        do
        {
        	l = cavs_biari_decode_symbol_bypass(&p->cabac);
        	if (!l) 
        	{
        		symbol += (1<<golomb_order); 
        		++golomb_order;

				if(symbol > 4096) /*remove endless loop*/
				{
					p->b_error_flag = 1;
					return -1;
				}
        	}

		    if( l!=0 && l!=1 )
		    {
			    p->b_error_flag = 1;
			    return -1;
		    }
        } while (l!=1);
        while (golomb_order--)
        {//next binary part
        	l = cavs_biari_decode_symbol_bypass(&p->cabac);
        	if (l == 1)
        		bit |= (1<<golomb_order);
				
			//if( l!=0 && l!=1 )
			//{
			//	p->b_error_flag = 1;
			//	return;
			//}	
        }
        symbol += bit;
        symbol = 4 + symbol*2;
    }
    if (symbol)
    {
    	if (cavs_biari_decode_symbol_bypass(&p->cabac))
    		symbol = -symbol;
    }

    p->p_mvd[i_list][mvd_scan_idx][xy_idx] = symbol;

#if CAVS_TRACE
    fprintf(trace_fp,"%sMVD%d\t\t\t%d\n",i_list ? "B" : "F", xy_idx, symbol);
    fflush(trace_fp);
#endif
    p->b_error_flag = (&(p->cabac))->b_cabac_error;
    if( p->b_error_flag )
    {
    	printf("[error]MB mvd of AEC is wrong\n");
    }

    return symbol;
}

int cavs_cabac_get_ref_p(cavs_decoder *p, int ref_scan_idx)
{
    const int a = p->p_ref[0][ref_scan_idx-1] > 0;
    const int b = p->p_ref[0][ref_scan_idx-3] > 0;

    int i = a + (b<<1), symbol;
    bi_ctx_t * ctx = p->cabac.ref_no_contexts[0];

    if(p->ph.b_picture_reference_flag )
    {
        return 0;
    }

    if (cavs_biari_decode_symbol(&p->cabac, ctx + i))
        symbol = 0;
    else
    {
        symbol = 1;
        i = 4;
        while (!cavs_biari_decode_symbol(&p->cabac, ctx + i))
        {
            symbol++;
            i++;
            if (i >= 5) i = 5;

			if( symbol > 3 ) /* remove endless loop */
			{
				p->b_error_flag = 1;
				return -1;
			}
        }
    }

    p->p_ref[0][ref_scan_idx] = symbol;


#if CAVS_TRACE
    fprintf(trace_fp,"P ref\t\t\t%d\n",symbol);
    fflush(trace_fp);
#endif

    p->b_error_flag = (&(p->cabac))->b_cabac_error;
    if( p->b_error_flag )
    {
    	printf("[error]MB refidx P of AEC is wrong\n");
    }

    return symbol;
}

static inline int cavs_cabac_get_ref_b_core(cavs_decoder *p, int i_list, int ref_scan_idx)
{
    const int a = p->p_ref[i_list][ref_scan_idx-1] > 0;
    const int b = p->p_ref[i_list][ref_scan_idx-3] > 0;

    int i = a + (b<<1);
    bi_ctx_t * ctx = p->cabac.ref_no_contexts[0];
    int symbol = !cavs_biari_decode_symbol(&p->cabac, ctx + i);

    p->p_ref[i_list][ref_scan_idx] = symbol;

#if CAVS_TRACE
    fprintf(trace_fp,"B ref\t\t\t%d\n",symbol);
    fflush(trace_fp);
#endif
    p->b_error_flag = (&(p->cabac))->b_cabac_error;
    if( p->b_error_flag )
    {
    	printf("[error]MB refidx B of AEC is wrong\n");
    }

    return symbol;
}

int cavs_cabac_get_ref_b(cavs_decoder *p, int i_list, int ref_scan_idx)
{
    if(p->ph.b_picture_structure == 0 && p->ph.b_picture_reference_flag ==0)
        return cavs_cabac_get_ref_b_core(p, i_list, ref_scan_idx);
    return 0;
}


int cavs_cabac_get_mb_part_type(cavs_decoder *p)
{
    bi_ctx_t * ctx = p->cabac.b8_type_contexts[0];
    int i, symbol;
    
    if (cavs_biari_decode_symbol(&p->cabac, ctx + 0))
    {
        symbol = 2;
        i = 2;
    }
    else
    {
        symbol = 0;
        i = 1;
    }
    
    if (cavs_biari_decode_symbol(&p->cabac, ctx + i))
        ++symbol;

#if CAVS_TRACE
    fprintf(trace_fp,"Mb part type\t\t\t%d\n",symbol);
    fflush(trace_fp);
#endif
    p->b_error_flag = (&(p->cabac))->b_cabac_error;
    if( p->b_error_flag )
    {
    	printf("[error]MB part type of AEC is wrong\n");
    }

    return symbol;
}

static const int t_chr[5] = { 0,1,2,4,3000};
int cavs_cabac_get_coeffs(cavs_decoder *p, const xavs_vlc *p_vlc_table, int i_escape_order, int b_chroma)
{
    int pairs, rank, pos;
    int run, level, abslevel, symbol;
    int *run_buf = p->run_buf, *level_buf = p->level_buf;

    /* read coefficients for whole block */
    bi_ctx_t (*primary)[NUM_MAP_CTX];
    bi_ctx_t *p_ctx;
    bi_ctx_t *p_ctx2;
    int ctx, ctx2, offset;
	int i_ret = 0;

    if( !b_chroma )
    {
        if( p->ph.b_picture_structure == 0 )
        {
            primary = p->cabac.fld_map_contexts;
        }
        else
        {
            primary = p->cabac.map_contexts;
        }
    }
    else 
    {
        if( p->ph.b_picture_structure == 0 )
        {
            primary = p->cabac.fld_last_contexts;
        }
        else
        {
            primary = p->cabac.last_contexts;
        }
    }
    
    //! Decode 
    rank = 0;
    pos = 0;
    for( pairs=0; pairs<65; pairs++ ) {
    	p_ctx = primary[rank];
    	//! EOB
    	if( rank>0) {
    		p_ctx2 = primary[5+(pos>>5)];
    		ctx2 = (pos>>1)&0x0f;
#if 1
             /* note : can't cross border of array */
             if( pos < 0 || pos > 63 )
             {
                printf("[error]MB coeffs over border\n");
                return -1;   
             }
#endif              
    		ctx = 0;
#if 0    		
			if( cavs_biari_decode_symbol_w(&p->cabac, p_ctx+ctx, p_ctx2+ctx2) )
    			break;
#else
			i_ret = cavs_biari_decode_symbol_w(&p->cabac, p_ctx+ctx, p_ctx2+ctx2);
			if( i_ret == -1)
			{
				p->b_error_flag = 1;
				return -1;
			}
			else if(i_ret!= 0 )
			{
				break;
			}
#endif

    	}
    	//! Level
    	ctx = 1;
    	symbol = 0;
    	while( cavs_biari_decode_symbol(&p->cabac, p_ctx+ctx)==0 ) {
    		++symbol;
    		++ctx;
    		if( ctx>=2 ) ctx =2;

			if(  symbol > (1<<15) ) /* remove endless loop */
			{
				p->b_error_flag = 1;
				return -1;
			}
    	}
    	abslevel = symbol + 1;
    	//! Sign
    	if( cavs_biari_decode_symbol_bypass(&p->cabac) ) {
    		level = - abslevel;	
    	}
    	else {
    		level = abslevel;
    	}

    	//! Run
    	if( abslevel==1 ) { 
    		offset = 4;
    	}
    	else {
    		offset = 6;
    	}
    	symbol = 0;
    	ctx = 0;
    	while( cavs_biari_decode_symbol(&p->cabac, p_ctx+ctx+offset)==0 ) {
    		++symbol;
    		++ctx;
    		if( ctx>=1 ) ctx =1;

			if(  symbol > 63 )	/* remove endless loop */
			{
				p->b_error_flag = 1;
				return -1;
			}
    	}
    	run = symbol;

    	level_buf[pairs] = level;
    	run_buf[pairs] = run + 1;
        
#if CAVS_TRACE
    	fprintf(trace_fp,"level\t\t\t%d\n",level);
    	fprintf(trace_fp,"run  \t\t\t%d\n",run/*, p->cabac.buffer, p->cabac.bits_to_go*/);
    	fflush(trace_fp);
#endif

    	if( abslevel>t_chr[rank] ) {
    		if( abslevel <= 2 )
    			rank = abslevel;
    		else if( abslevel<=4 )
    			rank = 3;
    		else
    			rank = 4;
    	}
    	pos += (run+1);
    	if( pos>=64 ) pos = 63;
    }

    p->b_error_flag = (&(p->cabac))->b_cabac_error;
    if( p->b_error_flag )
    {
    	printf("[error]MB coeffs of AEC is wrong\n");
    }
  
    return pairs;
}

#if B_MB_WEIGHTING
int cavs_cabac_get_mb_weighting_prediction(cavs_decoder *p)
{
	bi_ctx_t * ctx = &p->cabac.mb_weighting_pred;
	int ret = 0;

	ret = cavs_biari_decode_symbol (&p->cabac, ctx);

	return ret;
}
#endif