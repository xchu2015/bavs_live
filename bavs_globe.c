/*****************************************************************************
 * globe.c: bavs decoder library
 *****************************************************************************
*/
 
#include "bavs_globe.h"
#define AH -1
#define BH -2
#define CH 96
#define DH 42
#define EH -7
#define FH 0
#define AV 0 
#define BV -1
#define CV 5
#define DV 5
#define EV -1
#define FV 0
#define MC_W 8
#define MC_H 8
/************************************************************************
D	a	b	c	E
d	e	f	g
h	i	j	k	m
n	p	q	r
************************************************************************/

#define put(a, b)  a = cm[((b)+512)>>10]

static void cavs_put_filt8_hv_ik(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{
    int32_t temp[8*(8+5)];
    int32_t *tmp = temp;
    uint8_t *cm = crop_table + MAX_NEG_CROP;
    int i,j;

    tmp = temp+2;  
    for(j=0;j<MC_H;j++)
    {
        for(i=-2; i<MC_W+3; i++)
        {
            tmp[i]= BV*src1[-1*srcStride+i] + CV*src1[0+i] + DV*src1[1*srcStride+i] + EV*src1[2*srcStride+i];
        }
        tmp+=13;
        src1+=srcStride;
    }
    tmp = temp+2;                         
    for(j=0; j<MC_H; j++)                        
    {
        for(i=0;i<MC_W;i++)
        {
            put(dst[i], AH*tmp[-2+i] + BH*tmp[-1+i] + CH*tmp[0+i] + DH*tmp[1+i] + EH*tmp[2+i] );

        }
        dst+=dstStride;
        tmp+=13;                                                        
    }    
}
#undef put

#define put(a, b)  a = cm[((b)+512)>>10]
static void cavs_put_filt8_hv_ki(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{//k
     int32_t temp[8*(8+5)];
     int32_t *tmp = temp;
     uint8_t *cm = crop_table + MAX_NEG_CROP;
     int i,j;
	 tmp = temp+2;
	 for(j=0;j<MC_H;j++)
	 {
		 for(i=-2; i<MC_W+3; i++)
		 {
			 tmp[i]= BV*src1[-1*srcStride+i] + CV*src1[0+i] + DV*src1[1*srcStride+i] + EV*src1[2*srcStride+i];			 
		 }
		 tmp+=13;
		 src1+=srcStride;
	 }
     tmp = temp+2;                         
	 for(j=0; j<MC_H; j++)                        
	 {
		 for(i=0;i<MC_W;i++)
		 {
			 put(dst[i], EH*tmp[-1+i] + DH*tmp[0+i] + CH*tmp[1+i] + BH*tmp[2+i] + AH*tmp[3+i] ); 
			
		 }
		 dst+=dstStride;
		 tmp+=13;                                                        
	 }
     
}
#undef put



#define put(a, b)  a = cm[((b)+512)>>10]
static void cavs_put_filt8_hv_fq(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{
	//f=-xx-2aa'+96b'+42s'-7dd'
     int32_t temp[8*(8+5)];
     int32_t *tmp = temp;
     uint8_t *cm = crop_table + MAX_NEG_CROP;
     int i,j;
	 tmp = temp;
	 src1-=2*srcStride;
	 for(j=-2;j<MC_H+3;j++)
	 {
		 for(i=0; i<MC_W; i++)
		 {
			 tmp[i]= BV*src1[-1+i] + CV*src1[0+i] + DV*src1[1+i] + EV*src1[2+i];			 
		 }
		 tmp+=8;
		 src1+=srcStride;
	 }
     tmp = temp+2*8;                         
	 for(j=0; j<MC_H; j++)                        
	 {
		 for(i=0;i<MC_W;i++)
		 {
			 put(dst[i], AH*tmp[-2*8+i] + BH*tmp[-1*8+i] + CH*tmp[0+i] + DH*tmp[1*8+i] + EH*tmp[2*8+i] ); 
			
		 }
		 dst+=dstStride;
		 tmp+=8;                                                        
	 }                                                               
     
}
#undef put

#define put(a, b)  a = cm[((b)+512)>>10]
static void cavs_put_filt8_hv_qf(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{
	//q
     int32_t temp[8*(8+5)];
     int32_t *tmp = temp;
     uint8_t *cm = crop_table + MAX_NEG_CROP;
     int i,j;
	 tmp = temp;
	 src1-=2*srcStride;
	 for(j=-2;j<MC_H+3;j++)
	 {
		 for(i=0; i<MC_W; i++)
		 {
			 tmp[i]= BV*src1[-1+i] + CV*src1[0+i] + DV*src1[1+i] + EV*src1[2+i];			 
		 }
		 tmp+=8;
		 src1+=srcStride;
	 }
     tmp = temp+2*8;                         
	 for(j=0; j<MC_H; j++)                        
	 {
		 for(i=0;i<MC_W;i++)
		 {
			 put(dst[i], EH*tmp[-1*8+i] + DH*tmp[0+i] + CH*tmp[1*8+i] + BH*tmp[2*8+i] + AH*tmp[3*8+i] ); 
			
		 }
		 dst+=dstStride;
		 tmp+=8;                                                        
	 }                                                               
     
}
#undef put


#define put(a, b)  a = cm[((b)+32)>>6]
static void cavs_put_filt8_hv_j(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{
	//j
     int32_t temp[8*(8+5)];
     int32_t *tmp = temp;
     uint8_t *cm = crop_table + MAX_NEG_CROP;
     int i,j;
	 tmp = temp+2;
	 for(j=0;j<MC_H;j++)
	 {
		 for(i=-2; i<MC_W+3; i++)
		 {
			 tmp[i]= BV*src1[-1*srcStride+i] + CV*src1[0+i] + DV*src1[1*srcStride+i] + EV*src1[2*srcStride+i];			 
		 }
		 tmp+=13;
		 src1+=srcStride;
	 }
     tmp = temp+2;                         
	 for(j=0; j<MC_H; j++)                        
	 {
		 for(i=0;i<MC_W;i++)
		 {
			 put(dst[i],  BV*tmp[-1+i] + CV*tmp[0+i] + DV*tmp[1+i] + EV*tmp[2+i] ); 
			
		 }
		 dst+=dstStride;
		 tmp+=13;                                                        
	 }
     
}
#undef put


#define put(a, b)  a = cm[((b)+64)>>7]
static void cavs_put_filt8_h_ac(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{ 
	//a

     uint8_t *cm = crop_table + MAX_NEG_CROP;
     int i,j;
	 for(j=0; j<MC_H; j++)                        
	 {
		 for(i=0;i<MC_W;i++)
		 {
			 put(dst[i], AH*src1[-2+i] + BH*src1[-1+i] + CH*src1[0+i] + DH*src1[1+i] + EH*src1[2+i] ); 	//-1 -2 96 42 -7
		 }
		 src1+=srcStride;
		 dst+=dstStride;
	 }                                                               
}
#undef put

#define put(a, b)  a = cm[((b)+64)>>7]
static void cavs_put_filt8_h_ca(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{ 
	//c
     uint8_t *cm = crop_table + MAX_NEG_CROP;
     int i,j;
	 for(j=0; j<MC_H; j++)                        
	 {
		 for(i=0;i<MC_W;i++)
		 {
			 put(dst[i], EH*src1[-1+i] + DH*src1[0+i] + CH*src1[1+i] + BH*src1[2+i] + AH*src1[3+i] ); //-7 42 96 -2 -1
		 }
		 src1+=srcStride;
		 dst+=dstStride;
	 }                                                               
}
#undef put


#define put(a, b)  a = cm[((b)+4)>>3]
static void cavs_put_filt8_h_b(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{ //b -1 5 5 -1
     uint8_t *cm = crop_table + MAX_NEG_CROP;
     int i,j;
	 for(j=0; j<MC_H; j++)                        
	 {
		 for(i=0;i<MC_W;i++)
		 {
			 put(dst[i], BV*src1[-1+i] + CV*src1[0+i] + DV*src1[1+i] + EV*src1[2+i]);			 		
		 }
		 src1+=srcStride;
		 dst+=dstStride;
	 }                                                               
}
#undef put
#define put(a, b)  a = cm[((b)+4)>>3]
static void cavs_put_filt8_v_h(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{ //h
     uint8_t *cm = crop_table + MAX_NEG_CROP;
     int i,j;
	 for(j=0; j<MC_H; j++)                        
	 {
		 for(i=0;i<MC_W;i++)
		 {
			 put(dst[i], BV*src1[-1*srcStride+i] + CV*src1[0+i] + DV*src1[1*srcStride+i] + EV*src1[2*srcStride+i]);			 		
		 }
		 src1+=srcStride;
		 dst+=dstStride;
	 }                                                               
}
#undef put

#define put(a, b)  a = cm[((b)+64)>>7]
static void cavs_put_filt8_v_dn(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{//d
     uint8_t *cm = crop_table + MAX_NEG_CROP;
     int i,j;
	 for(j=0; j<MC_H; j++)                        
	 {
		 for(i=0;i<MC_W;i++)
		 {
			 put(dst[i], AH*src1[-2*srcStride+i] + BH*src1[-1*srcStride+i] + CH*src1[0+i] + DH*src1[1*srcStride+i] + EH*src1[2*srcStride+i] ); //-1 -2 96 42 -7
			
		 }
		 src1+=srcStride;
		 dst+=dstStride;
	 }                                                               

}
#undef put

#define put(a, b)  a = cm[((b)+64)>>7]
static void cavs_put_filt8_v_nd(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{//n
     uint8_t *cm = crop_table + MAX_NEG_CROP;
     int i,j;
	 for(j=0; j<MC_H; j++)                        
	 {
		 for(i=0;i<MC_W;i++)
		 {
			 put(dst[i], EH*src1[-1*srcStride+i] + DH*src1[0+i] + CH*src1[1*srcStride+i] + BH*src1[2*srcStride+i] + AH*src1[3*srcStride+i] ); //-7 42 96 -2 -1
			
		 }
		 src1+=srcStride;
		 dst+=dstStride;
	 }                                                               

}
#undef put


#define put(a, b)  a = cm[((b)+64)>>7]
static void cavs_put_filt8_hv_egpr(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{//e¡¢g¡¢p¡¢r
     int32_t temp[8*(8+5)];
     int32_t *tmp = temp;
     uint8_t *cm = crop_table + MAX_NEG_CROP;
     int i,j;
	 tmp = temp+2;
	 for(j=0;j<MC_H;j++)
	 {
		 for(i=-2; i<MC_W+3; i++)
		 {
			 tmp[i]= BV*src1[-1*srcStride+i] + CV*src1[0+i] + DV*src1[1*srcStride+i] + EV*src1[2*srcStride+i];	//h 
		 }
		 tmp+=13;
		 src1+=srcStride;
	 }
     tmp = temp+2;                         
	 for(j=0; j<MC_H; j++)                        
	 {
		 for(i=0;i<MC_W;i++)
		 {
			 put(dst[i], BV*tmp[-1+i] + CV*tmp[0+i] + DV*tmp[1+i] + EV*tmp[2+i]+64*src2[0+i]); //(h->j+D)/2
			
		 }
		 src2+=dstStride;
		 dst+=dstStride;
		 tmp+=13;                                                        
	 }                                                               
}
#undef put

#define avg(a, b)  a = ((a)+cm[((b)+512)>>10]+1)>>1
static void cavs_avg_filt8_hv_ik(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{	
     int32_t temp[8*(8+5)];
     int32_t *tmp = temp;
     uint8_t *cm = crop_table + MAX_NEG_CROP;
     int i,j;
	 tmp = temp+2;
	 for(j=0;j<MC_H;j++)
	 {
		 for(i=-2; i<MC_W+3; i++)
		 {
			 tmp[i]= BV*src1[-1*srcStride+i] + CV*src1[0+i] + DV*src1[1*srcStride+i] + EV*src1[2*srcStride+i]; //h
		 }
		 tmp+=13;
		 src1+=srcStride;
	 }
     tmp = temp+2;                         
	 for(j=0; j<MC_H; j++)                        
	 {
		 for(i=0;i<MC_W;i++)
		 {
			 avg(dst[i], AH*tmp[-2+i] + BH*tmp[-1+i] + CH*tmp[0+i] + DH*tmp[1+i] + EH*tmp[2+i] ); //?? src1[]
			
		 }
		 dst+=dstStride;
		 tmp+=13;                                                        
	 }
     
     
}
#undef avg

#define avg(a, b)  a = ((a)+cm[((b)+512)>>10]+1)>>1
static void cavs_avg_filt8_hv_ki(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{	
     int32_t temp[8*(8+5)];
     int32_t *tmp = temp;
     uint8_t *cm = crop_table + MAX_NEG_CROP;
     int i,j;
	 tmp = temp+2;
	 for(j=0;j<MC_H;j++)
	 {
		 for(i=-2; i<MC_W+3; i++)
		 {
			 tmp[i]= BV*src1[-1*srcStride+i] + CV*src1[0+i] + DV*src1[1*srcStride+i] + EV*src1[2*srcStride+i];			 
		 }
		 tmp+=13;
		 src1+=srcStride;
	 }
     tmp = temp+2;                         
	 for(j=0; j<MC_H; j++)                        
	 {
		 for(i=0;i<MC_W;i++)
		 {
			 avg(dst[i], EH*tmp[-1+i] + DH*tmp[0+i] + CH*tmp[1+i] + BH*tmp[2+i] + AH*tmp[3+i] ); 
			
		 }
		 dst+=dstStride;
		 tmp+=13;                                                        
	 }
         
}
#undef avg




#define avg(a, b)  a = ((a)+cm[((b)+512)>>10]+1)>>1
static void cavs_avg_filt8_hv_fq(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{
	//f=-xx-2aa'+96b'+42s'-7dd'
     int32_t temp[8*(8+5)];
     int32_t *tmp = temp;
     uint8_t *cm = crop_table + MAX_NEG_CROP;
     int i,j;
	 tmp = temp;
	 src1-=2*srcStride;
	 for(j=-2;j<MC_H+3;j++)
	 {
		 for(i=0; i<MC_W; i++)
		 {
			 tmp[i]= BV*src1[-1+i] + CV*src1[0+i] + DV*src1[1+i] + EV*src1[2+i];			 
		 }
		 tmp+=8;
		 src1+=srcStride;
	 }
     tmp = temp+2*8;                         
	 for(j=0; j<MC_H; j++)                        
	 {
		 for(i=0;i<MC_W;i++)
		 {
			 avg(dst[i], AH*tmp[-2*8+i] + BH*tmp[-1*8+i] + CH*tmp[0+i] + DH*tmp[1*8+i] + EH*tmp[2*8+i] ); 
			
		 }
		 dst+=dstStride;
		 tmp+=8;                                                        
	 }                                                               
     
}
#undef avg

#define avg(a, b)  a = ((a)+cm[((b)+512)>>10]+1)>>1
static void cavs_avg_filt8_hv_qf(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{
	//f=-xx-2aa'+96b'+42s'-7dd'
     int32_t temp[8*(8+5)];
     int32_t *tmp = temp;
     uint8_t *cm = crop_table + MAX_NEG_CROP;
     int i,j;
	 tmp = temp;
	 src1-=2*srcStride;
	 for(j=-2;j<MC_H+3;j++)
	 {
		 for(i=0; i<MC_W; i++)
		 {
			 tmp[i]= BV*src1[-1+i] + CV*src1[0+i] + DV*src1[1+i] + EV*src1[2+i];			 
		 }
		 tmp+=8;
		 src1+=srcStride;
	 }
     tmp = temp+2*8;                         
	 for(j=0; j<MC_H; j++)                        
	 {
		 for(i=0;i<MC_W;i++)
		 {
			 avg(dst[i], EH*tmp[-1*8+i] + DH*tmp[0+i] + CH*tmp[1*8+i] + BH*tmp[2*8+i] + AH*tmp[3*8+i] ); 
			
		 }
		 dst+=dstStride;
		 tmp+=8;                                                        
	 }                                                               
     
}
#undef avg



#define avg(a, b)  a = ((a)+cm[((b)+32)>>6]  +1)>>1
static void cavs_avg_filt8_hv_j(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{	//j
      int32_t temp[8*(8+5)];
     int32_t *tmp = temp;
     uint8_t *cm = crop_table + MAX_NEG_CROP;
     int i,j;
	 tmp = temp+2;
	 for(j=0;j<MC_H;j++)
	 {
		 for(i=-2; i<MC_W+3; i++)
		 {
			 tmp[i]= BV*src1[-1*srcStride+i] + CV*src1[0+i] + DV*src1[1*srcStride+i] + EV*src1[2*srcStride+i];	//h		 
		 }
		 tmp+=13;
		 src1+=srcStride;
	 }
     tmp = temp+2;                         
	 for(j=0; j<MC_H; j++)                        
	 {
		 for(i=0;i<MC_W;i++)
		 {
			 avg(dst[i],  BV*tmp[-1+i] + CV*tmp[0+i] + DV*tmp[1+i] + EV*tmp[2+i] ); //j
			
		 }
		 dst+=dstStride;
		 tmp+=13;                                                        
	 }
     
}
#undef avg


#define avg(a, b)  a = ((a)+cm[((b)+64)>>7]  +1)>>1
static void cavs_avg_filt8_h_ac(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{ //a
     uint8_t *cm = crop_table + MAX_NEG_CROP;
     int i,j;
	 for(j=0; j<MC_H; j++)                        
	 {
		 for(i=0;i<MC_W;i++)
		 {
			 avg(dst[i], AH*src1[-2+i] + BH*src1[-1+i] + CH*src1[0+i] + DH*src1[1+i] + EH*src1[2+i] ); //a
			
		 }
		 src1+=srcStride;
		 dst+=dstStride;
	 }                                                               
}
#undef avg

#define avg(a, b)  a = ((a)+cm[((b)+64)>>7]  +1)>>1
static void cavs_avg_filt8_h_ca(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{ //c
     uint8_t *cm = crop_table + MAX_NEG_CROP;
     int i,j;
	 for(j=0; j<MC_H; j++)                        
	 {
		 for(i=0;i<MC_W;i++)
		 {
			 avg(dst[i], EH*src1[-1+i] + DH*src1[0+i] + CH*src1[1+i] + BH*src1[2+i] + AH*src1[3+i] ); //c
		 }
		 src1+=srcStride;
		 dst+=dstStride;
	 }                                                               
}
#undef avg


#define avg(a, b)  a = ((a)+cm[((b)+4)>>3]   +1)>>1
static void cavs_avg_filt8_h_b(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{ //b
     uint8_t *cm = crop_table + MAX_NEG_CROP;
     int i,j;
	 for(j=0; j<MC_H; j++)                        
	 {
		 for(i=0;i<MC_W;i++)
		 {
			 avg(dst[i], BV*src1[-1+i] + CV*src1[0+i] + DV*src1[1+i] + EV*src1[2+i]);		//b	 		
		 }
		 src1+=srcStride;
		 dst+=dstStride;
	 }                                                               
}
#undef avg



#define avg(a, b)  a = ((a)+cm[((b)+4)>>3]   +1)>>1
static void cavs_avg_filt8_v_h(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{ //h
     uint8_t *cm = crop_table + MAX_NEG_CROP;
     int i,j;
	 for(j=0; j<MC_H; j++)                        
	 {
		 for(i=0;i<MC_W;i++)
		 {
			 avg(dst[i], BV*src1[-1*srcStride+i] + CV*src1[0+i] + DV*src1[1*srcStride+i] + EV*src1[2*srcStride+i]);			 		
		 }
		 src1+=srcStride;
		 dst+=dstStride;
	 }                                                               
}
#undef avg



#define avg(a, b)  a = ((a)+cm[((b)+64)>>7]  +1)>>1
static void cavs_avg_filt8_v_dn(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{//dst+src1:d
     uint8_t *cm = crop_table + MAX_NEG_CROP;
     int i,j;
	 for(j=0; j<MC_H; j++)                        
	 {
		 for(i=0;i<MC_W;i++)
		 {
			 avg(dst[i], AH*src1[-2*srcStride+i] + BH*src1[-1*srcStride+i] + CH*src1[0+i] + DH*src1[1*srcStride+i] + EH*src1[2*srcStride+i] ); 
			
		 }
		 src1+=srcStride;
		 dst+=dstStride;
	 }                                                               

}
#undef avg

#define avg(a, b)  a = ((a)+cm[((b)+64)>>7]  +1)>>1
static void cavs_avg_filt8_v_nd(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{//n
     uint8_t *cm = crop_table + MAX_NEG_CROP;
     int i,j;
	 for(j=0; j<MC_H; j++)                        
	 {
		 for(i=0;i<MC_W;i++)
		 {
			 avg(dst[i], EH*src1[-1*srcStride+i] + DH*src1[0+i] + CH*src1[1*srcStride+i] + BH*src1[2*srcStride+i] + AH*src1[3*srcStride+i] ); 
			
		 }
		 src1+=srcStride;
		 dst+=dstStride;
	 }                                                               

}
#undef avg


#define avg(a, b)  a = ((a)+cm[((b)+64)>>7]  +1)>>1
static void cavs_avg_filt8_hv_egpr(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{  //error
     int32_t temp[8*(8+5)];
     int32_t *tmp = temp;
     uint8_t *cm = crop_table + MAX_NEG_CROP;
     int i,j;
	 tmp = temp+2;
	 for(j=0;j<MC_H;j++)
	 {
		 for(i=-2; i<MC_W+3; i++)
		 {
			 tmp[i]= BV*src1[-1*srcStride+i] + CV*src1[0+i] + DV*src1[1*srcStride+i] + EV*src1[2*srcStride+i];  //h	 
		 }
		 tmp+=13;
		 src1+=srcStride;
	 }
     tmp = temp+2;                         
	 for(j=0; j<MC_H; j++)                        
	 {
		 for(i=0;i<MC_W;i++)
		 {
			 avg(dst[i], BV*tmp[-1+i] + CV*tmp[0+i] + DV*tmp[1+i] + EV*tmp[2+i]+64*src2[0+i]); // j??[-1 5 5 -1]
			
		 }
		 src2+=dstStride;
		 dst+=dstStride;
		 tmp+=13;                                                        
	 }                                                               
}
#undef avg

#undef AH 
#undef BH 
#undef CH 
#undef DH 
#undef EH 
#undef FH 
#undef AV  
#undef BV 
#undef CV 
#undef DV 
#undef EV 
#undef FV 
#undef MC_W 
#undef MC_H 

static void cavs_put_filt16_hv_fq(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{
    cavs_put_filt8_hv_fq(dst  , src1,   src2  , dstStride, srcStride);
    cavs_put_filt8_hv_fq(dst +8 , src1+8,   src2+8  , dstStride, srcStride);
    src1 += 8*srcStride;
    src2 += 8*srcStride;
    dst += 8*dstStride;
    cavs_put_filt8_hv_fq(dst  , src1,   src2  , dstStride, srcStride);
    cavs_put_filt8_hv_fq(dst +8, src1+8,   src2+8  , dstStride, srcStride);
}


static void cavs_put_filt16_hv_qf(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{
    cavs_put_filt8_hv_qf(dst  , src1,   src2  , dstStride, srcStride);
    cavs_put_filt8_hv_qf(dst +8 , src1+8,   src2+8  , dstStride, srcStride);
    src1 += 8*srcStride;
    src2 += 8*srcStride;
    dst += 8*dstStride;
    cavs_put_filt8_hv_qf(dst  , src1,   src2  , dstStride, srcStride);
    cavs_put_filt8_hv_qf(dst +8, src1+8,   src2+8  , dstStride, srcStride);
}


static void cavs_put_filt16_v_dn(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{
    cavs_put_filt8_v_dn(dst  , src1,   src2  , dstStride, srcStride);
    cavs_put_filt8_v_dn(dst +8 , src1+8,   src2+8  , dstStride, srcStride);
    src1 += 8*srcStride;
    src2 += 8*srcStride;
    dst += 8*dstStride;
    cavs_put_filt8_v_dn(dst  , src1,   src2  , dstStride, srcStride);
    cavs_put_filt8_v_dn(dst +8, src1+8,   src2+8  , dstStride, srcStride);
}

static void cavs_put_filt16_v_nd(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{
    cavs_put_filt8_v_nd(dst  , src1,   src2  , dstStride, srcStride);
    cavs_put_filt8_v_nd(dst +8 , src1+8,   src2+8  , dstStride, srcStride);
    src1 += 8*srcStride;
    src2 += 8*srcStride;
    dst += 8*dstStride;
    cavs_put_filt8_v_nd(dst  , src1,   src2  , dstStride, srcStride);
    cavs_put_filt8_v_nd(dst +8, src1+8,   src2+8  , dstStride, srcStride);
}


static void cavs_put_filt16_hv_egpr(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{
    cavs_put_filt8_hv_egpr(dst  , src1,   src2  , dstStride, srcStride);
    cavs_put_filt8_hv_egpr(dst +8 , src1+8,   src2+8  , dstStride, srcStride);
    src1 += 8*srcStride;
    src2 += 8*srcStride;
    dst += 8*dstStride;
    cavs_put_filt8_hv_egpr(dst  , src1,   src2  , dstStride, srcStride);
    cavs_put_filt8_hv_egpr(dst +8, src1+8,   src2+8  , dstStride, srcStride);
}
static void cavs_put_filt16_hv_j(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{
    cavs_put_filt8_hv_j(dst  , src1,   src2  , dstStride, srcStride);
    cavs_put_filt8_hv_j(dst +8 , src1+8,   src2+8  , dstStride, srcStride);
    src1 += 8*srcStride;
    src2 += 8*srcStride;
    dst += 8*dstStride;
    cavs_put_filt8_hv_j(dst  , src1,   src2  , dstStride, srcStride);
    cavs_put_filt8_hv_j(dst +8, src1+8,   src2+8  , dstStride, srcStride);
}



static void cavs_put_filt16_h_b(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{
    cavs_put_filt8_h_b(dst  , src1,   src2  , dstStride, srcStride);
    cavs_put_filt8_h_b(dst +8 , src1+8,   src2+8  , dstStride, srcStride);
    src1 += 8*srcStride;
    src2 += 8*srcStride;
    dst += 8*dstStride;
    cavs_put_filt8_h_b(dst  , src1,   src2  , dstStride, srcStride);
    cavs_put_filt8_h_b(dst +8, src1+8,   src2+8  , dstStride, srcStride);
}

static void cavs_put_filt16_v_h(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{
    cavs_put_filt8_v_h(dst  , src1,   src2  , dstStride, srcStride);
    cavs_put_filt8_v_h(dst +8 , src1+8,   src2+8  , dstStride, srcStride);
    src1 += 8*srcStride;
    src2 += 8*srcStride;
    dst += 8*dstStride;
    cavs_put_filt8_v_h(dst  , src1,   src2  , dstStride, srcStride);
    cavs_put_filt8_v_h(dst +8, src1+8,   src2+8  , dstStride, srcStride);
}

static void cavs_put_filt16_h_ac(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{
    cavs_put_filt8_h_ac(dst  , src1,   src2  , dstStride, srcStride);
    cavs_put_filt8_h_ac(dst +8 , src1+8,   src2+8  , dstStride, srcStride);
    src1 += 8*srcStride;
    src2 += 8*srcStride;
    dst += 8*dstStride;
    cavs_put_filt8_h_ac(dst  , src1,   src2  , dstStride, srcStride);
    cavs_put_filt8_h_ac(dst +8, src1+8,   src2+8  , dstStride, srcStride);
}

static void cavs_put_filt16_h_ca(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{
    cavs_put_filt8_h_ca(dst  , src1,   src2  , dstStride, srcStride);
    cavs_put_filt8_h_ca(dst +8 , src1+8,   src2+8  , dstStride, srcStride);
    src1 += 8*srcStride;
    src2 += 8*srcStride;
    dst += 8*dstStride;
    cavs_put_filt8_h_ca(dst  , src1,   src2  , dstStride, srcStride);
    cavs_put_filt8_h_ca(dst +8, src1+8,   src2+8  , dstStride, srcStride);
}

static void cavs_put_filt16_hv_ki(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{
    cavs_put_filt8_hv_ki(dst  , src1,   src2  , dstStride, srcStride);
    cavs_put_filt8_hv_ki(dst +8 , src1+8,   src2+8  , dstStride, srcStride);
    src1 += 8*srcStride;
    src2 += 8*srcStride;
    dst += 8*dstStride;
    cavs_put_filt8_hv_ki(dst  , src1,   src2  , dstStride, srcStride);
    cavs_put_filt8_hv_ki(dst +8, src1+8,   src2+8  , dstStride, srcStride);
}


static void cavs_put_filt16_hv_ik(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{
    cavs_put_filt8_hv_ik(dst  , src1,   src2  , dstStride, srcStride);
    cavs_put_filt8_hv_ik(dst +8 , src1+8,   src2+8  , dstStride, srcStride);
    src1 += 8*srcStride;
    src2 += 8*srcStride;
    dst += 8*dstStride;
    cavs_put_filt8_hv_ik(dst  , src1,   src2  , dstStride, srcStride);
    cavs_put_filt8_hv_ik(dst +8, src1+8,   src2+8  , dstStride, srcStride);
}

void cavs_qpel16_put_mc10_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_put_filt16_h_ac(dst,src,src+stride+1,stride,stride);

}
void cavs_qpel16_put_mc20_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_put_filt16_h_b(dst,src,src+stride+1,stride,stride);

}

void cavs_qpel16_put_mc30_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_put_filt16_h_ca(dst,src,src+stride+1,stride,stride);
}
void cavs_qpel16_put_mc01_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_put_filt16_v_dn(dst,src,src,stride,stride);

}

void cavs_qpel16_put_mc11_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_put_filt16_hv_egpr(dst,src,src,stride,stride);

}

void cavs_qpel16_put_mc21_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_put_filt16_hv_fq(dst,src,src,stride,stride);
}
void cavs_qpel16_put_mc31_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_put_filt16_hv_egpr(dst,src,src+1,stride,stride);
}

void cavs_qpel16_put_mc02_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_put_filt16_v_h(dst,src,src+1,stride,stride);
}

void cavs_qpel16_put_mc12_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_put_filt16_hv_ik(dst,src,src+stride+1,stride,stride);
}

void cavs_qpel16_put_mc22_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_put_filt16_hv_j(dst,src,src+1,stride,stride);
}

void cavs_qpel16_put_mc32_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_put_filt16_hv_ki(dst,src,src+1,stride,stride);
}

void cavs_qpel16_put_mc03_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_put_filt16_v_nd(dst,src,src+stride+1,stride,stride);
}

void cavs_qpel16_put_mc13_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_put_filt16_hv_egpr(dst,src,src+stride,stride,stride);
}

void cavs_qpel16_put_mc23_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_put_filt16_hv_qf(dst,src,src+stride+1,stride,stride);
}

void cavs_qpel16_put_mc33_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_put_filt16_hv_egpr(dst,src,src+stride+1,stride,stride);
}

void cavs_qpel8_put_mc10_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_put_filt8_h_ac(dst,src,src+stride+1,stride,stride);
}
void cavs_qpel8_put_mc20_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_put_filt8_h_b(dst,src,src+stride+1,stride,stride);
}

void cavs_qpel8_put_mc30_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_put_filt8_h_ca(dst,src,src+stride+1,stride,stride);
}
void cavs_qpel8_put_mc01_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_put_filt8_v_dn(dst,src,src,stride,stride);
}

void cavs_qpel8_put_mc11_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_put_filt8_hv_egpr(dst,src,src,stride,stride);
}

void cavs_qpel8_put_mc21_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_put_filt8_hv_fq(dst,src,src,stride,stride);
}
void cavs_qpel8_put_mc31_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_put_filt8_hv_egpr(dst,src,src+1,stride,stride);
}

void cavs_qpel8_put_mc02_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_put_filt8_v_h(dst,src,src+1,stride,stride);
}

void cavs_qpel8_put_mc12_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_put_filt8_hv_ik(dst,src,src+stride+1,stride,stride);
}

void cavs_qpel8_put_mc22_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_put_filt8_hv_j(dst,src,src+1,stride,stride);
}

void cavs_qpel8_put_mc32_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_put_filt8_hv_ki(dst,src,src+1,stride,stride);
}

void cavs_qpel8_put_mc03_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_put_filt8_v_nd(dst,src,src+stride+1,stride,stride);
}

void cavs_qpel8_put_mc13_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_put_filt8_hv_egpr(dst,src,src+stride,stride,stride);
}

void cavs_qpel8_put_mc23_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_put_filt8_hv_qf(dst,src,src+stride+1,stride,stride);
}

void cavs_qpel8_put_mc33_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_put_filt8_hv_egpr(dst,src,src+stride+1,stride,stride);
}

#define	BYTE_VEC32(c)	((c)*0x01010101UL)
static inline uint32_t rnd_avg32(uint32_t a, uint32_t b)
{
    return (a | b) - (((a ^ b) & ~BYTE_VEC32(0x01)) >> 1);
}

#define LD32(a) (*((uint32_t*)(a)))
#define PUT_PIX(OPNAME,OP) \
static void OPNAME ## _pixels8_c(uint8_t *block, const uint8_t *pixels, int line_size, int h){\
    int i;\
    for(i=0; i<h; i++){\
        OP(*((uint32_t*)(block  )), LD32(pixels  ));\
        OP(*((uint32_t*)(block+4)), LD32(pixels+4));\
        pixels+=line_size;\
        block +=line_size;\
    }\
}\
static void OPNAME ## _pixels16_c(uint8_t *block, const uint8_t *pixels, int line_size, int h){\
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

void cavs_qpel8_put_mc00_c(uint8_t *dst, uint8_t *src, int stride) 
 {
    put_pixels8_c(dst, src, stride, 8);
}
void cavs_qpel16_put_mc00_c(uint8_t *dst, uint8_t *src, int stride) 
{
    put_pixels16_c(dst, src, stride, 16);
}

void cavs_qpel8_avg_mc00_c(uint8_t *dst, uint8_t *src, int stride) 
{
    avg_pixels8_c(dst, src, stride, 8);
}

void cavs_qpel16_avg_mc00_c(uint8_t *dst, uint8_t *src, int stride) 
{
    avg_pixels16_c(dst, src, stride, 16);
}

static void cavs_avg_filt16_hv_fq(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{
    cavs_avg_filt8_hv_fq(dst  , src1,   src2  , dstStride, srcStride);
    cavs_avg_filt8_hv_fq(dst +8 , src1+8,   src2+8  , dstStride, srcStride);
    src1 += 8*srcStride;
    src2 += 8*srcStride;
    dst += 8*dstStride;
    cavs_avg_filt8_hv_fq(dst  , src1,   src2  , dstStride, srcStride);
    cavs_avg_filt8_hv_fq(dst +8, src1+8,   src2+8  , dstStride, srcStride);
}


static void cavs_avg_filt16_hv_qf(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{
    cavs_avg_filt8_hv_qf(dst  , src1,   src2  , dstStride, srcStride);
    cavs_avg_filt8_hv_qf(dst +8 , src1+8,   src2+8  , dstStride, srcStride);
    src1 += 8*srcStride;
    src2 += 8*srcStride;
    dst += 8*dstStride;
    cavs_avg_filt8_hv_qf(dst  , src1,   src2  , dstStride, srcStride);
    cavs_avg_filt8_hv_qf(dst +8, src1+8,   src2+8  , dstStride, srcStride);
}

static void cavs_avg_filt16_v_dn(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{
    cavs_avg_filt8_v_dn(dst  , src1,   src2  , dstStride, srcStride);
    cavs_avg_filt8_v_dn(dst +8 , src1+8,   src2+8  , dstStride, srcStride);
    src1 += 8*srcStride;
    src2 += 8*srcStride;
    dst += 8*dstStride;
    cavs_avg_filt8_v_dn(dst  , src1,   src2  , dstStride, srcStride);
    cavs_avg_filt8_v_dn(dst +8, src1+8,   src2+8  , dstStride, srcStride);
}
static void cavs_avg_filt16_v_nd(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{
    cavs_avg_filt8_v_nd(dst  , src1,   src2  , dstStride, srcStride);
    cavs_avg_filt8_v_nd(dst +8 , src1+8,   src2+8  , dstStride, srcStride);
    src1 += 8*srcStride;
    src2 += 8*srcStride;
    dst += 8*dstStride;
    cavs_avg_filt8_v_nd(dst  , src1,   src2  , dstStride, srcStride);
    cavs_avg_filt8_v_nd(dst +8, src1+8,   src2+8  , dstStride, srcStride);
}
static void cavs_avg_filt16_hv_egpr(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{
    cavs_avg_filt8_hv_egpr(dst  , src1,   src2  , dstStride, srcStride);
    cavs_avg_filt8_hv_egpr(dst +8 , src1+8,   src2+8  , dstStride, srcStride);
    src1 += 8*srcStride;
    src2 += 8*srcStride;
    dst += 8*dstStride;
    cavs_avg_filt8_hv_egpr(dst  , src1,   src2  , dstStride, srcStride);
    cavs_avg_filt8_hv_egpr(dst +8, src1+8,   src2+8  , dstStride, srcStride);
}
static void cavs_avg_filt16_hv_j(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{
    cavs_avg_filt8_hv_j(dst  , src1,   src2  , dstStride, srcStride);
    cavs_avg_filt8_hv_j(dst +8 , src1+8,   src2+8  , dstStride, srcStride);
    src1 += 8*srcStride;
    src2 += 8*srcStride;
    dst += 8*dstStride;
    cavs_avg_filt8_hv_j(dst  , src1,   src2  , dstStride, srcStride);
    cavs_avg_filt8_hv_j(dst +8, src1+8,   src2+8  , dstStride, srcStride);
}


static void cavs_avg_filt16_h_b(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{
    cavs_avg_filt8_h_b(dst  , src1,   src2  , dstStride, srcStride);
    cavs_avg_filt8_h_b(dst +8 , src1+8,   src2+8  , dstStride, srcStride);
    src1 += 8*srcStride;
    src2 += 8*srcStride;
    dst += 8*dstStride;
    cavs_avg_filt8_h_b(dst  , src1,   src2  , dstStride, srcStride);
    cavs_avg_filt8_h_b(dst +8, src1+8,   src2+8  , dstStride, srcStride);
}

static void cavs_avg_filt16_v_h(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{
    cavs_avg_filt8_v_h(dst  , src1,   src2  , dstStride, srcStride);
    cavs_avg_filt8_v_h(dst +8 , src1+8,   src2+8  , dstStride, srcStride);
    src1 += 8*srcStride;
    src2 += 8*srcStride;
    dst += 8*dstStride;
    cavs_avg_filt8_v_h(dst  , src1,   src2  , dstStride, srcStride);
    cavs_avg_filt8_v_h(dst +8, src1+8,   src2+8  , dstStride, srcStride);
}

static void cavs_avg_filt16_h_ac(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{
    cavs_avg_filt8_h_ac(dst  , src1,   src2  , dstStride, srcStride);
    cavs_avg_filt8_h_ac(dst +8 , src1+8,   src2+8  , dstStride, srcStride);
    src1 += 8*srcStride;
    src2 += 8*srcStride;
    dst += 8*dstStride;
    cavs_avg_filt8_h_ac(dst  , src1,   src2  , dstStride, srcStride);
    cavs_avg_filt8_h_ac(dst +8, src1+8,   src2+8  , dstStride, srcStride);
}

static void cavs_avg_filt16_h_ca(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{
    cavs_avg_filt8_h_ca(dst  , src1,   src2  , dstStride, srcStride);
    cavs_avg_filt8_h_ca(dst +8 , src1+8,   src2+8  , dstStride, srcStride);
    src1 += 8*srcStride;
    src2 += 8*srcStride;
    dst += 8*dstStride;
    cavs_avg_filt8_h_ca(dst  , src1,   src2  , dstStride, srcStride);
    cavs_avg_filt8_h_ca(dst +8, src1+8,   src2+8  , dstStride, srcStride);
}


static void cavs_avg_filt16_hv_ik(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{
    cavs_avg_filt8_hv_ik(dst  , src1,   src2  , dstStride, srcStride);
    cavs_avg_filt8_hv_ik(dst +8 , src1+8,   src2+8  , dstStride, srcStride);
    src1 += 8*srcStride;
    src2 += 8*srcStride;
    dst += 8*dstStride;
    cavs_avg_filt8_hv_ik(dst  , src1,   src2  , dstStride, srcStride);
    cavs_avg_filt8_hv_ik(dst +8, src1+8,   src2+8  , dstStride, srcStride);
}

static void cavs_avg_filt16_hv_ki(uint8_t *dst, uint8_t *src1, uint8_t *src2, int dstStride, int srcStride)
{
    cavs_avg_filt8_hv_ki(dst  , src1,   src2  , dstStride, srcStride);
    cavs_avg_filt8_hv_ki(dst +8 , src1+8,   src2+8  , dstStride, srcStride);
    src1 += 8*srcStride;
    src2 += 8*srcStride;
    dst += 8*dstStride;
    cavs_avg_filt8_hv_ki(dst  , src1,   src2  , dstStride, srcStride);
    cavs_avg_filt8_hv_ki(dst +8, src1+8,   src2+8  , dstStride, srcStride);
}

void cavs_qpel16_avg_mc10_c(uint8_t *dst, uint8_t *src, int stride)
{
    cavs_avg_filt16_h_ac(dst,src,src+stride+1,stride,stride);
}
void cavs_qpel16_avg_mc20_c(uint8_t *dst, uint8_t *src, int stride)
{
    cavs_avg_filt16_h_b(dst,src,src+stride+1,stride,stride);
}

void cavs_qpel16_avg_mc30_c(uint8_t *dst, uint8_t *src, int stride)
{
    cavs_avg_filt16_h_ca(dst,src,src+stride+1,stride,stride);
}
void cavs_qpel16_avg_mc01_c(uint8_t *dst, uint8_t *src, int stride)
{
    cavs_avg_filt16_v_dn(dst,src,src,stride,stride);
}

void cavs_qpel16_avg_mc11_c(uint8_t *dst, uint8_t *src, int stride)
{
    cavs_avg_filt16_hv_egpr(dst,src,src,stride,stride);
}

void cavs_qpel16_avg_mc21_c(uint8_t *dst, uint8_t *src, int stride)
{
    cavs_avg_filt16_hv_fq(dst,src,src,stride,stride);
}
void cavs_qpel16_avg_mc31_c(uint8_t *dst, uint8_t *src, int stride)
{
    cavs_avg_filt16_hv_egpr(dst,src,src+1,stride,stride);
}

void cavs_qpel16_avg_mc02_c(uint8_t *dst, uint8_t *src, int stride)
{
    cavs_avg_filt16_v_h(dst,src,src+1,stride,stride);
}

void cavs_qpel16_avg_mc12_c(uint8_t *dst, uint8_t *src, int stride)
{
    cavs_avg_filt16_hv_ik(dst,src,src+stride+1,stride,stride);
}

void cavs_qpel16_avg_mc22_c(uint8_t *dst, uint8_t *src, int stride)
{
    cavs_avg_filt16_hv_j(dst,src,src+1,stride,stride);
}

void cavs_qpel16_avg_mc32_c(uint8_t *dst, uint8_t *src, int stride)
{
    cavs_avg_filt16_hv_ki(dst,src,src+1,stride,stride);
}

void cavs_qpel16_avg_mc03_c(uint8_t *dst, uint8_t *src, int stride)
{
    cavs_avg_filt16_v_nd(dst,src,src+stride+1,stride,stride);
}

void cavs_qpel16_avg_mc13_c(uint8_t *dst, uint8_t *src, int stride)
{
    cavs_avg_filt16_hv_egpr(dst,src,src+stride,stride,stride);
}

void cavs_qpel16_avg_mc23_c(uint8_t *dst, uint8_t *src, int stride)
{
    cavs_avg_filt16_hv_qf(dst,src,src+stride+1,stride,stride);
}

void cavs_qpel16_avg_mc33_c(uint8_t *dst, uint8_t *src, int stride)
{
    cavs_avg_filt16_hv_egpr(dst,src,src+stride+1,stride,stride);
}

void cavs_qpel8_avg_mc10_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_avg_filt8_h_ac(dst,src,src+stride+1,stride,stride);
}
void cavs_qpel8_avg_mc20_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_avg_filt8_h_b(dst,src,src+stride+1,stride,stride);
}

void cavs_qpel8_avg_mc30_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_avg_filt8_h_ca(dst,src,src+stride+1,stride,stride);
}
void cavs_qpel8_avg_mc01_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_avg_filt8_v_dn(dst,src,src,stride,stride);
}

void cavs_qpel8_avg_mc11_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_avg_filt8_hv_egpr(dst,src,src,stride,stride);
}

void cavs_qpel8_avg_mc21_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_avg_filt8_hv_fq(dst,src,src,stride,stride);
}
void cavs_qpel8_avg_mc31_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_avg_filt8_hv_egpr(dst,src,src+1,stride,stride);
}

void cavs_qpel8_avg_mc02_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_avg_filt8_v_h(dst,src,src+1,stride,stride);
}

void cavs_qpel8_avg_mc12_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_avg_filt8_hv_ik(dst,src,src+stride+1,stride,stride);
}

void cavs_qpel8_avg_mc22_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_avg_filt8_hv_j(dst,src,src+1,stride,stride);
}

void cavs_qpel8_avg_mc32_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_avg_filt8_hv_ki(dst,src,src+1,stride,stride);
}

void cavs_qpel8_avg_mc03_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_avg_filt8_v_nd(dst,src,src+stride+1,stride,stride);
}

void cavs_qpel8_avg_mc13_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_avg_filt8_hv_egpr(dst,src,src+stride,stride,stride);
}

void cavs_qpel8_avg_mc23_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_avg_filt8_hv_qf(dst,src,src+stride+1,stride,stride);
}

void cavs_qpel8_avg_mc33_c(uint8_t *dst, uint8_t *src, int stride)
{
	cavs_avg_filt8_hv_egpr(dst,src,src+stride+1,stride,stride);
}
