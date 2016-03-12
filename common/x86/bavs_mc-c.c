/*****************************************************************************
* bavs_mc.c.c: mc
*****************************************************************************
*/

#if HAVE_MMX
#include "cavsdsp.h"
#endif

//#include "config.h"
#include "../../bavs_globe.h"

#ifdef _MSC_VER
#define DECLARE_ALIGNED( var, n ) __declspec(align(n)) var
//#define DECLARE_ALIGNED( type, var, n ) __declspec(align(n)) type var
#else
//#define DECLARE_ALIGNED( var, n ) var __attribute__((aligned(n)))
//#define DECLARE_ALIGNED( type, var, n ) type var __attribute__((aligned(n)))
#define DECLARE_ALIGNED( var, n ) var __attribute__((aligned(n)))
#endif

#define DECLARE_ALIGNED_16( var ) DECLARE_ALIGNED( var, 16 )
//#define DECLARE_ALIGNED_8( var )  DECLARE_ALIGNED( var, 8 )
//#define DECLARE_ALIGNED_4( var )  DECLARE_ALIGNED( var, 4 )

/* asm declaration */
extern void cavs_put_qpel8_h_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
extern void cavs_avg_qpel8_h_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
extern void cavs_put_qpel8or16_v1_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
extern void cavs_put_qpel8or16_v1_h16_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
extern void cavs_put_qpel8or16_v2_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
extern void cavs_put_qpel8or16_v2_h16_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
extern void cavs_put_qpel8or16_v3_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
extern void cavs_put_qpel8or16_v3_h16_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
extern void cavs_avg_qpel8or16_v1_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
extern void cavs_avg_qpel8or16_v1_h16_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
extern void cavs_avg_qpel8or16_v2_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
extern void cavs_avg_qpel8or16_v2_h16_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
extern void cavs_avg_qpel8or16_v3_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
extern void cavs_avg_qpel8or16_v3_h16_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
extern void cavs_put_qpel8or16_h1_mmxext( uint8_t *dst, uint8_t *src, int dstStride, int srcStride );
extern void cavs_avg_qpel8or16_h1_mmxext( uint8_t *dst, uint8_t *src, int dstStride, int srcStride );
extern void cavs_put_qpel8or16_h2_mmxext( uint8_t *dst, uint8_t *src, int dstStride, int srcStride );
extern void cavs_avg_qpel8or16_h2_mmxext( uint8_t *dst, uint8_t *src, int dstStride, int srcStride );
extern void cavs_put_qpel8or16_j_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
extern void cavs_avg_qpel8or16_j_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
extern void cavs_get_qpel8or16_c1_mmxext(uint16_t *mid_dst, uint8_t *src, int dstStride, int srcStride);
extern void cavs_put_qpel8or16_c1_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride, uint16_t *mid_dst);
extern void cavs_avg_qpel8or16_c1_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride, uint16_t *mid_dst);
extern void cavs_get_qpel8or16_hh_mmxext(uint16_t *mid_dst, uint8_t *src, int dstStride, int srcStride);
extern void cavs_get_qpel8or16_hv_mmxext(uint16_t *mid_dst, uint8_t *src, int dstStride, int srcStride);
extern void cavs_get_qpel8or16_hc_mmxext(uint16_t *mid_dst, uint8_t *src, int dstStride, int srcStride);
extern void cavs_put_qpel8or16_i_mmxext(uint8_t *dst, int dstStride, uint16_t *mid_hv, uint16_t *mid_c );
extern void cavs_avg_qpel8or16_i_mmxext(uint8_t *dst, int dstStride, uint16_t *mid_hv, uint16_t *mid_c );
extern void cavs_put_qpel8or16_k_mmxext(uint8_t *dst, int dstStride, uint16_t *mid_hv, uint16_t *mid_c );
extern void cavs_avg_qpel8or16_k_mmxext(uint8_t *dst, int dstStride, uint16_t *mid_hv, uint16_t *mid_c );
extern void cavs_put_qpel8or16_f_mmxext(uint8_t *dst, int dstStride, uint16_t *mid_hv, uint16_t *mid_c );
extern void cavs_avg_qpel8or16_f_mmxext(uint8_t *dst, int dstStride, uint16_t *mid_hv, uint16_t *mid_c );
extern void cavs_put_qpel8or16_q_mmxext(uint8_t *dst, int dstStride, uint16_t *mid_hv, uint16_t *mid_c );
extern void cavs_avg_qpel8or16_q_mmxext(uint8_t *dst, int dstStride, uint16_t *mid_hv, uint16_t *mid_c );
extern void cavs_put_qpel8or16_d_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride, uint16_t *mid_v);
extern void cavs_avg_qpel8or16_d_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride, uint16_t *mid_v);
extern void cavs_put_qpel8or16_n_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride, uint16_t *mid_v);
extern void cavs_avg_qpel8or16_n_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride, uint16_t *mid_v);
extern void cavs_put_qpel8or16_a_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride, uint16_t *mid_h);
extern void cavs_avg_qpel8or16_a_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride, uint16_t *mid_h);
extern void cavs_put_qpel8or16_c_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride, uint16_t *mid_h);
extern void cavs_avg_qpel8or16_c_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride, uint16_t *mid_h);
#if (1/*HAVE_MMXEXT_INLINE*/ || HAVE_AMD3DNOW_INLINE)

/*****************************************************************************
 *
 * motion compensation
 *
 ****************************************************************************/
#define QPEL_CAVSVNUM(VOP,OP,ADD,MUL1,MUL2)\
    int w= 2;\
    src -= (srcStride<<1);\
    \
    while(w--){\
            cavs_## OP ##_qpel8or16_## VOP ##_mmxext(dst, src, dstStride, srcStride); \
     if(h==16){\
	        src += 13*srcStride; \
			dst += (dstStride<<3); \
            cavs_## OP ##_qpel8or16_## VOP ##_h16_mmxext(dst, src, dstStride, srcStride); \
			src -= 13*srcStride;\
			dst -= (dstStride<<3); \
     }\
     src += 4;\
     dst += 4;\
   }


#define QPEL_CAVS(OPNAME, OP, MMX)\
static void OPNAME ## cavs_qpel8_h_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    cavs_## OPNAME ## qpel8_h_ ## MMX(dst, src, dstStride, srcStride );\
}\
\
static inline UNUSED void OPNAME ## cavs_qpel8or16_v1_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride, int h){\
  QPEL_CAVSVNUM(v1,OP,ff_pw_64,ff_pw_96,ff_pw_42)      \
}\
\
static inline void OPNAME ## cavs_qpel8or16_v2_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride, int h){\
  QPEL_CAVSVNUM(v2,OP,ff_pw_4,ff_pw_5,ff_pw_5)         \
}\
\
static inline UNUSED void OPNAME ## cavs_qpel8or16_v3_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride, int h){\
  QPEL_CAVSVNUM(v3,OP,ff_pw_64,ff_pw_96,ff_pw_42)      \
}\
\
static UNUSED void OPNAME ## cavs_qpel8_v1_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    OPNAME ## cavs_qpel8or16_v1_ ## MMX(dst  , src  , dstStride, srcStride, 8);\
}\
static UNUSED void OPNAME ## cavs_qpel16_v1_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    OPNAME ## cavs_qpel8or16_v1_ ## MMX(dst  , src  , dstStride, srcStride, 16);\
    OPNAME ## cavs_qpel8or16_v1_ ## MMX(dst+8, src+8, dstStride, srcStride, 16);\
}\
\
static void OPNAME ## cavs_qpel8_v2_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    OPNAME ## cavs_qpel8or16_v2_ ## MMX(dst  , src  , dstStride, srcStride, 8);\
}\
static void OPNAME ## cavs_qpel16_v2_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    OPNAME ## cavs_qpel8or16_v2_ ## MMX(dst  , src  , dstStride, srcStride, 16);\
    OPNAME ## cavs_qpel8or16_v2_ ## MMX(dst+8, src+8, dstStride, srcStride, 16);\
}\
\
static UNUSED void OPNAME ## cavs_qpel8_v3_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    OPNAME ## cavs_qpel8or16_v3_ ## MMX(dst  , src  , dstStride, srcStride, 8);\
}\
static UNUSED void OPNAME ## cavs_qpel16_v3_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    OPNAME ## cavs_qpel8or16_v3_ ## MMX(dst  , src  , dstStride, srcStride, 16);\
    OPNAME ## cavs_qpel8or16_v3_ ## MMX(dst+8, src+8, dstStride, srcStride, 16);\
}\
\
static void OPNAME ## cavs_qpel16_h_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    OPNAME ## cavs_qpel8_h_ ## MMX(dst  , src  , dstStride, srcStride);\
    OPNAME ## cavs_qpel8_h_ ## MMX(dst+8, src+8, dstStride, srcStride);\
    src += (srcStride<<3);\
    dst += (dstStride<<3);\
    OPNAME ## cavs_qpel8_h_ ## MMX(dst  , src  , dstStride, srcStride);\
    OPNAME ## cavs_qpel8_h_ ## MMX(dst+8, src+8, dstStride, srcStride);\
}\
static UNUSED void OPNAME ## cavs_qpel8_h1_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    cavs_ ## OPNAME ## qpel8or16_h1_ ## MMX(dst  , src  , dstStride, srcStride );\
}\
static UNUSED void OPNAME ## cavs_qpel16_h1_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    cavs_## OPNAME ## qpel8or16_h1_ ## MMX(dst  , src  , dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_h1_ ## MMX(dst+8, src+8, dstStride, srcStride);\
    src += (srcStride<<3);\
    dst += (dstStride<<3);\
    cavs_## OPNAME ## qpel8or16_h1_ ## MMX(dst  , src  , dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_h1_ ## MMX(dst+8, src+8, dstStride, srcStride);\
}\
static UNUSED void OPNAME ## cavs_qpel8_h2_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    cavs_ ## OPNAME ## qpel8or16_h2_ ## MMX(dst  , src  , dstStride, srcStride );\
}\
static UNUSED void OPNAME ## cavs_qpel16_h2_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    cavs_## OPNAME ## qpel8or16_h2_ ## MMX(dst  , src  , dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_h2_ ## MMX(dst+8, src+8, dstStride, srcStride);\
    src += (srcStride<<3);\
    dst += (dstStride<<3);\
    cavs_## OPNAME ## qpel8or16_h2_ ## MMX(dst  , src  , dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_h2_ ## MMX(dst+8, src+8, dstStride, srcStride);\
}\
static void OPNAME ## cavs_qpel8_j_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    cavs_ ## OPNAME ## qpel8or16_j_ ## MMX(dst  , src  , dstStride, srcStride );\
}\
static void OPNAME ## cavs_qpel16_j_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    cavs_## OPNAME ## qpel8or16_j_ ## MMX(dst  , src  , dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_j_ ## MMX(dst+8, src+8, dstStride, srcStride);\
    src += (srcStride<<3);\
    dst += (dstStride<<3);\
    cavs_## OPNAME ## qpel8or16_j_ ## MMX(dst  , src  , dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_j_ ## MMX(dst+8, src+8, dstStride, srcStride);\
}\
static void OPNAME ## cavs_qpel8_e_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    DECLARE_ALIGNED_16(uint16_t temp[16][16]);\
    uint16_t *mid_dst = &temp[0][0];\
    cavs_get_qpel8or16_c1_## MMX( mid_dst, src, dstStride, srcStride);\
    cavs_ ## OPNAME ## qpel8or16_c1_ ## MMX(dst  , src  , dstStride, srcStride, mid_dst );\
}\
static void OPNAME ## cavs_qpel16_e_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    DECLARE_ALIGNED_16(uint16_t temp[16][16]);\
    uint16_t *mid_dst = &temp[0][0];\
    cavs_get_qpel8or16_c1_## MMX( mid_dst, src, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_c1_ ## MMX(dst  , src  , dstStride, srcStride, mid_dst);\
    cavs_get_qpel8or16_c1_## MMX( mid_dst+8, src+8, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_c1_ ## MMX(dst+8, src+8, dstStride, srcStride, mid_dst+8);\
    src += (srcStride<<3);\
    dst += (dstStride<<3);\
    cavs_get_qpel8or16_c1_## MMX( mid_dst + /*8*16*/128, src, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_c1_ ## MMX(dst  , src  , dstStride, srcStride, mid_dst + /*8*16*/128);\
    cavs_get_qpel8or16_c1_## MMX( mid_dst + /*8*16 +8*/136, src + 8, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_c1_ ## MMX(dst+8, src+8, dstStride, srcStride, mid_dst + /*8*16 +8*/136);\
}\
static void OPNAME ## cavs_qpel8_g_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    DECLARE_ALIGNED_16(uint16_t temp[16][16]);\
    uint16_t *mid_dst = &temp[0][0];\
    cavs_get_qpel8or16_c1_## MMX( mid_dst, src, dstStride, srcStride);\
    cavs_ ## OPNAME ## qpel8or16_c1_ ## MMX(dst  , src+1  , dstStride, srcStride, mid_dst );\
}\
static void OPNAME ## cavs_qpel16_g_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    DECLARE_ALIGNED_16(uint16_t temp[16][16]);\
    uint16_t *mid_dst = &temp[0][0];\
    cavs_get_qpel8or16_c1_## MMX( mid_dst, src, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_c1_ ## MMX(dst  , src+1  , dstStride, srcStride, mid_dst);\
    cavs_get_qpel8or16_c1_## MMX( mid_dst+8, src+8, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_c1_ ## MMX(dst+8, src+8+1, dstStride, srcStride, mid_dst+8);\
    src += (srcStride<<3);\
    dst += (dstStride<<3);\
    cavs_get_qpel8or16_c1_## MMX( mid_dst + /*8*16*/128, src, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_c1_ ## MMX(dst  , src +1 , dstStride, srcStride, mid_dst + /*8*16*/128);\
    cavs_get_qpel8or16_c1_## MMX( mid_dst + /*8*16 +8*/136, src + 8, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_c1_ ## MMX(dst+8, src+8+1, dstStride, srcStride, mid_dst + /*8*16 +8*/136);\
}\
static void OPNAME ## cavs_qpel8_p_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    DECLARE_ALIGNED_16(uint16_t temp[16][16]);\
    uint16_t *mid_dst = &temp[0][0];\
    cavs_get_qpel8or16_c1_## MMX( mid_dst, src, dstStride, srcStride);\
    cavs_ ## OPNAME ## qpel8or16_c1_ ## MMX(dst  , src+ srcStride , dstStride, srcStride, mid_dst );\
}\
static void OPNAME ## cavs_qpel16_p_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    DECLARE_ALIGNED_16(uint16_t temp[16][16]);\
    uint16_t *mid_dst = &temp[0][0];\
    cavs_get_qpel8or16_c1_## MMX( mid_dst, src, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_c1_ ## MMX(dst  , src+srcStride  , dstStride, srcStride, mid_dst);\
    cavs_get_qpel8or16_c1_## MMX( mid_dst+8, src+8, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_c1_ ## MMX(dst+8, src+8+srcStride, dstStride, srcStride, mid_dst+8);\
    src += (srcStride<<3);\
    dst += (dstStride<<3);\
    cavs_get_qpel8or16_c1_## MMX( mid_dst + /*8*16*/128, src, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_c1_ ## MMX(dst  , src +srcStride , dstStride, srcStride, mid_dst + /*8*16*/128);\
    cavs_get_qpel8or16_c1_## MMX( mid_dst + /*8*16 +8*/136, src + 8, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_c1_ ## MMX(dst+8, src+8+srcStride, dstStride, srcStride, mid_dst + /*8*16 +8*/136);\
}\
static void OPNAME ## cavs_qpel8_r_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    DECLARE_ALIGNED_16(uint16_t temp[16][16]);\
    uint16_t *mid_dst = &temp[0][0];\
    cavs_get_qpel8or16_c1_## MMX( mid_dst, src, dstStride, srcStride);\
    cavs_ ## OPNAME ## qpel8or16_c1_ ## MMX(dst  , src+ srcStride + 1, dstStride, srcStride, mid_dst );\
}\
static void OPNAME ## cavs_qpel16_r_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    DECLARE_ALIGNED_16(uint16_t temp[16][16]);\
    uint16_t *mid_dst = &temp[0][0];\
    cavs_get_qpel8or16_c1_## MMX( mid_dst, src, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_c1_ ## MMX(dst  , src+srcStride+1  , dstStride, srcStride, mid_dst);\
    cavs_get_qpel8or16_c1_## MMX( mid_dst+8, src+8, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_c1_ ## MMX(dst+8, src+8+srcStride+1, dstStride, srcStride, mid_dst+8);\
    src += (srcStride<<3);\
    dst += (dstStride<<3);\
    cavs_get_qpel8or16_c1_## MMX( mid_dst + /*8*16*/128, src, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_c1_ ## MMX(dst  , src +srcStride +1 , dstStride, srcStride, mid_dst + /*8*16*/128);\
    cavs_get_qpel8or16_c1_## MMX( mid_dst + /*8*16 +8*/136, src + 8, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_c1_ ## MMX(dst+8, src+8+srcStride+1, dstStride, srcStride, mid_dst + /*8*16 +8*/136);\
}\
static void OPNAME ## cavs_qpel8_i_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    DECLARE_ALIGNED_16(uint16_t temp_v[24][24]);\
    DECLARE_ALIGNED_16(uint16_t temp_c[24][24]);\
    uint16_t *mid_dst_v = &temp_v[0][0];\
    uint16_t *mid_dst_c = &temp_c[0][0];\
    int src_offset = 4+(srcStride<<2); \
    int mid_offset = /*4+4*24*/100; \
    cavs_get_qpel8or16_hv_## MMX( mid_dst_v, src - src_offset, dstStride, srcStride);\
    cavs_get_qpel8or16_hc_## MMX( mid_dst_c, src - src_offset, dstStride, srcStride);\
    cavs_ ## OPNAME ## qpel8or16_i_ ## MMX(dst  , dstStride,  mid_dst_v + mid_offset, mid_dst_c + mid_offset );\
}\
static void OPNAME ## cavs_qpel16_i_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    DECLARE_ALIGNED_16(uint16_t temp_v[24][24]);\
    DECLARE_ALIGNED_16(uint16_t temp_c[24][24]);\
    uint16_t *mid_dst_v = &temp_v[0][0];\
    uint16_t *mid_dst_c = &temp_c[0][0];\
    int src_offset = 4+(srcStride<<2);\
    int mid_offset = /*4+4*24*/100;\
    cavs_get_qpel8or16_hv_## MMX( mid_dst_v, src - src_offset, dstStride, srcStride);\
    cavs_get_qpel8or16_hc_## MMX( mid_dst_c, src - src_offset, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_i_ ## MMX(dst  , dstStride, mid_dst_v + mid_offset, mid_dst_c + mid_offset);\
    cavs_get_qpel8or16_hv_## MMX( mid_dst_v + 8, src-src_offset + 8, dstStride, srcStride);\
    cavs_get_qpel8or16_hc_## MMX( mid_dst_c + 8, src -src_offset + 8, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_i_ ## MMX(dst + 8 , dstStride, mid_dst_v+ mid_offset + 8, mid_dst_c + mid_offset + 8);\
    src += (srcStride<<3);\
    dst += (dstStride<<3);\
    cavs_get_qpel8or16_hv_## MMX( mid_dst_v + /*8*24*/192, src- src_offset, dstStride, srcStride);\
    cavs_get_qpel8or16_hc_## MMX( mid_dst_c + /*8*24*/192, src- src_offset, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_i_ ## MMX(dst  , dstStride, mid_dst_v+/*8*24*/192 +mid_offset, mid_dst_c+/*8*24*/192+mid_offset);\
    cavs_get_qpel8or16_hv_## MMX( mid_dst_v + /*8*24 +8*/200, src-src_offset + 8, dstStride, srcStride);\
    cavs_get_qpel8or16_hc_## MMX( mid_dst_c + /*8*24 +8*/200, src-src_offset + 8, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_i_ ## MMX(dst + 8  , dstStride, mid_dst_v+/*8*24+8*/200+mid_offset, mid_dst_c+/*8*24+8*/200+mid_offset);\
}\
static void OPNAME ## cavs_qpel8_k_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    DECLARE_ALIGNED_16(uint16_t temp_v[24][24]);\
    DECLARE_ALIGNED_16(uint16_t temp_c[24][24]);\
    uint16_t *mid_dst_v = &temp_v[0][0];\
    uint16_t *mid_dst_c = &temp_c[0][0];\
    int src_offset = 4+(srcStride<<2); \
    int mid_offset = /*4+4*24*/100; \
    cavs_get_qpel8or16_hv_## MMX( mid_dst_v, src - src_offset, dstStride, srcStride);\
    cavs_get_qpel8or16_hc_## MMX( mid_dst_c, src - src_offset, dstStride, srcStride);\
    cavs_ ## OPNAME ## qpel8or16_k_ ## MMX(dst  , dstStride,  mid_dst_v + mid_offset, mid_dst_c + mid_offset );\
}\
static void OPNAME ## cavs_qpel16_k_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    DECLARE_ALIGNED_16(uint16_t temp_v[24][24]);\
    DECLARE_ALIGNED_16(uint16_t temp_c[24][24]);\
    uint16_t *mid_dst_v = &temp_v[0][0];\
    uint16_t *mid_dst_c = &temp_c[0][0];\
    int src_offset = 4+(srcStride<<2);\
    int mid_offset = /*4+4*24*/100;\
    cavs_get_qpel8or16_hv_## MMX( mid_dst_v, src - src_offset, dstStride, srcStride);\
    cavs_get_qpel8or16_hc_## MMX( mid_dst_c, src - src_offset, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_k_ ## MMX(dst  , dstStride, mid_dst_v + mid_offset, mid_dst_c + mid_offset);\
    cavs_get_qpel8or16_hv_## MMX( mid_dst_v + 8, src-src_offset + 8, dstStride, srcStride);\
    cavs_get_qpel8or16_hc_## MMX( mid_dst_c + 8, src -src_offset + 8, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_k_ ## MMX(dst + 8 , dstStride, mid_dst_v+ mid_offset + 8, mid_dst_c + mid_offset + 8);\
    src += (srcStride<<3);\
    dst += (dstStride<<3);\
    cavs_get_qpel8or16_hv_## MMX( mid_dst_v + /*8*24*/192, src- src_offset, dstStride, srcStride);\
    cavs_get_qpel8or16_hc_## MMX( mid_dst_c + /*8*24*/192, src- src_offset, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_k_ ## MMX(dst  , dstStride, mid_dst_v+/*8*24*/192 +mid_offset, mid_dst_c+/*8*24*/192+mid_offset);\
    cavs_get_qpel8or16_hv_## MMX( mid_dst_v + /*8*24 +8*/200, src-src_offset + 8, dstStride, srcStride);\
    cavs_get_qpel8or16_hc_## MMX( mid_dst_c + /*8*24 + 8*/200, src-src_offset + 8, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_k_ ## MMX(dst + 8  , dstStride, mid_dst_v+/*8*24+8*/200+mid_offset, mid_dst_c+/*8*24+8*/200+mid_offset);\
}\
static void OPNAME ## cavs_qpel8_f_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    DECLARE_ALIGNED_16(uint16_t temp_h[24][24]);\
    DECLARE_ALIGNED_16(uint16_t temp_c[24][24]);\
    uint16_t *mid_dst_h = &temp_h[0][0];\
    uint16_t *mid_dst_c = &temp_c[0][0];\
    int src_offset = 4+(srcStride<<2); \
    int mid_offset = /*4+4*24*/100; \
    cavs_get_qpel8or16_hh_## MMX( mid_dst_h, src - src_offset, dstStride, srcStride);\
    cavs_get_qpel8or16_hc_## MMX( mid_dst_c, src - src_offset, dstStride, srcStride);\
    cavs_ ## OPNAME ## qpel8or16_f_ ## MMX(dst  , dstStride,  mid_dst_h + mid_offset, mid_dst_c + mid_offset );\
}\
static void OPNAME ## cavs_qpel16_f_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    DECLARE_ALIGNED_16(uint16_t temp_h[24][24]);\
    DECLARE_ALIGNED_16(uint16_t temp_c[24][24]);\
    uint16_t *mid_dst_h = &temp_h[0][0];\
    uint16_t *mid_dst_c = &temp_c[0][0];\
    int src_offset = 4+(srcStride<<2);\
    int mid_offset = /*4+4*24*/100;\
    cavs_get_qpel8or16_hh_## MMX( mid_dst_h, src - src_offset, dstStride, srcStride);\
    cavs_get_qpel8or16_hc_## MMX( mid_dst_c, src - src_offset, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_f_ ## MMX(dst  , dstStride, mid_dst_h + mid_offset, mid_dst_c + mid_offset);\
    cavs_get_qpel8or16_hh_## MMX( mid_dst_h + 8, src-src_offset + 8, dstStride, srcStride);\
    cavs_get_qpel8or16_hc_## MMX( mid_dst_c + 8, src -src_offset + 8, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_f_ ## MMX(dst + 8 , dstStride, mid_dst_h+ mid_offset + 8, mid_dst_c + mid_offset + 8);\
    src += (srcStride<<3);\
    dst += (dstStride<<3);\
    cavs_get_qpel8or16_hh_## MMX( mid_dst_h + /*8*24*/192, src- src_offset, dstStride, srcStride);\
    cavs_get_qpel8or16_hc_## MMX( mid_dst_c + /*8*24*/192, src- src_offset, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_f_ ## MMX(dst  , dstStride, mid_dst_h+/*8*24*/192 +mid_offset, mid_dst_c+8*24+mid_offset);\
    cavs_get_qpel8or16_hh_## MMX( mid_dst_h + /*8*24 +8*/200, src-src_offset + 8, dstStride, srcStride);\
    cavs_get_qpel8or16_hc_## MMX( mid_dst_c + /*8*24 + 8*/200, src-src_offset + 8, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_f_ ## MMX(dst + 8  , dstStride, mid_dst_h+/*8*24+8*/200+mid_offset, mid_dst_c+/*8*24+8*/200+mid_offset);\
}\
static void OPNAME ## cavs_qpel8_q_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    DECLARE_ALIGNED_16(uint16_t temp_h[24][24]);\
    DECLARE_ALIGNED_16(uint16_t temp_c[24][24]);\
    uint16_t *mid_dst_h = &temp_h[0][0];\
    uint16_t *mid_dst_c = &temp_c[0][0];\
    int src_offset = 4+(srcStride<<2); \
    int mid_offset = /*4+4*24*/100; \
    cavs_get_qpel8or16_hh_## MMX( mid_dst_h, src - src_offset, dstStride, srcStride);\
    cavs_get_qpel8or16_hc_## MMX( mid_dst_c, src - src_offset, dstStride, srcStride);\
    cavs_ ## OPNAME ## qpel8or16_q_ ## MMX(dst  , dstStride,  mid_dst_h + mid_offset, mid_dst_c + mid_offset );\
}\
static void OPNAME ## cavs_qpel16_q_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    DECLARE_ALIGNED_16(uint16_t temp_h[24][24]);\
    DECLARE_ALIGNED_16(uint16_t temp_c[24][24]);\
    uint16_t *mid_dst_h = &temp_h[0][0];\
    uint16_t *mid_dst_c = &temp_c[0][0];\
    int src_offset = 4+(srcStride<<2);\
    int mid_offset = /*4+4*24*/100;\
    cavs_get_qpel8or16_hh_## MMX( mid_dst_h, src - src_offset, dstStride, srcStride);\
    cavs_get_qpel8or16_hc_## MMX( mid_dst_c, src - src_offset, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_q_ ## MMX(dst  , dstStride, mid_dst_h + mid_offset, mid_dst_c + mid_offset);\
    cavs_get_qpel8or16_hh_## MMX( mid_dst_h + 8, src-src_offset + 8, dstStride, srcStride);\
    cavs_get_qpel8or16_hc_## MMX( mid_dst_c + 8, src -src_offset + 8, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_q_ ## MMX(dst + 8 , dstStride, mid_dst_h+ mid_offset + 8, mid_dst_c + mid_offset + 8);\
    src += (srcStride<<3);\
    dst += (dstStride<<3);\
    cavs_get_qpel8or16_hh_## MMX( mid_dst_h + /*8*24*/192, src- src_offset, dstStride, srcStride);\
    cavs_get_qpel8or16_hc_## MMX( mid_dst_c + /*8*24*/192, src- src_offset, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_q_ ## MMX(dst  , dstStride, mid_dst_h+/*8*24*/192 +mid_offset, mid_dst_c+/*8*24*/192+mid_offset);\
    cavs_get_qpel8or16_hh_## MMX( mid_dst_h + /*8*24 +8*/200, src-src_offset + 8, dstStride, srcStride);\
    cavs_get_qpel8or16_hc_## MMX( mid_dst_c + /*8*24 + 8*/200, src-src_offset + 8, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_q_ ## MMX(dst + 8  , dstStride, mid_dst_h+/*8*24+8*/200+mid_offset, mid_dst_c+/*8*24+8*/200+mid_offset);\
}\
static void OPNAME ## cavs_qpel8_d_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    DECLARE_ALIGNED_16(uint16_t temp_v[24][24]);\
    uint16_t *mid_dst_v = &temp_v[0][0];\
    int src_offset = 4+(srcStride<<2); \
    int mid_offset = /*4+4*24*/100; \
    cavs_get_qpel8or16_hv_## MMX( mid_dst_v, src - src_offset, dstStride, srcStride);\
    cavs_ ## OPNAME ## qpel8or16_d_ ## MMX(dst  , src, dstStride,  srcStride, mid_dst_v + mid_offset );\
}\
static void OPNAME ## cavs_qpel16_d_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    DECLARE_ALIGNED_16(uint16_t temp_v[24][24]);\
    uint16_t *mid_dst_v = &temp_v[0][0];\
    int src_offset = 4+(srcStride<<2);\
    int mid_offset = /*4+4*24*/100;\
    cavs_get_qpel8or16_hv_## MMX( mid_dst_v, src - src_offset, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_d_ ## MMX(dst  , src, dstStride, srcStride, mid_dst_v + mid_offset);\
    cavs_get_qpel8or16_hv_## MMX( mid_dst_v + 8, src-src_offset + 8, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_d_ ## MMX(dst + 8 , src + 8, dstStride, srcStride, mid_dst_v+ mid_offset + 8);\
    src += (srcStride<<3);\
    dst += (dstStride<<3);\
    cavs_get_qpel8or16_hv_## MMX( mid_dst_v + /*8*24*/192, src- src_offset, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_d_ ## MMX(dst  , src, dstStride, srcStride, mid_dst_v+/*8*24*/192 +mid_offset);\
    cavs_get_qpel8or16_hv_## MMX( mid_dst_v + /*8*24 +8*/200, src-src_offset + 8, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_d_ ## MMX(dst + 8 , src + 8, dstStride, srcStride, mid_dst_v+/*8*24+8*/200+mid_offset);\
}\
static void OPNAME ## cavs_qpel8_n_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    DECLARE_ALIGNED_16(uint16_t temp_v[24][24]);\
    uint16_t *mid_dst_v = &temp_v[0][0];\
    int src_offset = 4+(srcStride<<2); \
    int mid_offset = /*4+4*24*/100; \
    cavs_get_qpel8or16_hv_## MMX( mid_dst_v, src - src_offset, dstStride, srcStride);\
    cavs_ ## OPNAME ## qpel8or16_n_ ## MMX(dst  , src, dstStride,  srcStride, mid_dst_v + mid_offset );\
}\
static void OPNAME ## cavs_qpel16_n_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    DECLARE_ALIGNED_16(uint16_t temp_v[24][24]);\
    uint16_t *mid_dst_v = &temp_v[0][0];\
    int src_offset = 4+(srcStride<<2);\
    int mid_offset = /*4+4*24*/100;\
    cavs_get_qpel8or16_hv_## MMX( mid_dst_v, src - src_offset, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_n_ ## MMX(dst  , src, dstStride, srcStride, mid_dst_v + mid_offset);\
    cavs_get_qpel8or16_hv_## MMX( mid_dst_v + 8, src-src_offset + 8, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_n_ ## MMX(dst + 8 , src + 8, dstStride, srcStride, mid_dst_v+ mid_offset + 8);\
    src += (srcStride<<3);\
    dst += (dstStride<<3);\
    cavs_get_qpel8or16_hv_## MMX( mid_dst_v + /*8*24*/192, src- src_offset, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_n_ ## MMX(dst  , src, dstStride, srcStride, mid_dst_v+/*8*24*/192 +mid_offset);\
    cavs_get_qpel8or16_hv_## MMX( mid_dst_v + /*8*24 +8*/200, src-src_offset + 8, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_n_ ## MMX(dst + 8 , src + 8, dstStride, srcStride, mid_dst_v+/*8*24+8*/200+mid_offset);\
}\
static void OPNAME ## cavs_qpel8_a_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    DECLARE_ALIGNED_16(uint16_t temp_h[24][24]);\
    uint16_t *mid_dst_h = &temp_h[0][0];\
    int src_offset = 4+(srcStride<<2); \
    int mid_offset = /*4+4*24*/100; \
    cavs_get_qpel8or16_hh_## MMX( mid_dst_h, src - src_offset, dstStride, srcStride);\
    cavs_ ## OPNAME ## qpel8or16_a_ ## MMX(dst  , src, dstStride,  srcStride, mid_dst_h + mid_offset );\
}\
static void OPNAME ## cavs_qpel16_a_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    DECLARE_ALIGNED_16(uint16_t temp_h[24][24]);\
    uint16_t *mid_dst_h = &temp_h[0][0];\
    int src_offset = 4+(srcStride<<2);\
    int mid_offset = /*4+4*24*/100;\
    cavs_get_qpel8or16_hh_## MMX( mid_dst_h, src - src_offset, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_a_ ## MMX(dst  , src, dstStride, srcStride, mid_dst_h + mid_offset);\
    cavs_get_qpel8or16_hh_## MMX( mid_dst_h + 8, src-src_offset + 8, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_a_ ## MMX(dst + 8 , src + 8, dstStride, srcStride, mid_dst_h+ mid_offset + 8);\
    src += (srcStride<<3);\
    dst += (dstStride<<3);\
    cavs_get_qpel8or16_hh_## MMX( mid_dst_h + /*8*24*/192, src- src_offset, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_a_ ## MMX(dst  , src, dstStride, srcStride, mid_dst_h+/*8*24*/192 +mid_offset);\
    cavs_get_qpel8or16_hh_## MMX( mid_dst_h + /*8*24 +8*/200, src-src_offset + 8, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_a_ ## MMX(dst + 8 , src + 8, dstStride, srcStride, mid_dst_h+/*8*24+8*/200+mid_offset);\
}\
static void OPNAME ## cavs_qpel8_c_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    DECLARE_ALIGNED_16(uint16_t temp_h[24][24]);\
    uint16_t *mid_dst_h = &temp_h[0][0];\
    int src_offset = 4+(srcStride<<2); \
    int mid_offset = /*4+4*24*/100; \
    cavs_get_qpel8or16_hh_## MMX( mid_dst_h, src - src_offset, dstStride, srcStride);\
    cavs_ ## OPNAME ## qpel8or16_c_ ## MMX(dst  , src, dstStride,  srcStride, mid_dst_h + mid_offset );\
}\
static void OPNAME ## cavs_qpel16_c_ ## MMX(uint8_t *dst, uint8_t *src, int dstStride, int srcStride){\
    DECLARE_ALIGNED_16(uint16_t temp_h[24][24]);\
    uint16_t *mid_dst_h = &temp_h[0][0];\
    int src_offset = 4+(srcStride<<2);\
    int mid_offset = /*4+4*24*/100;\
    cavs_get_qpel8or16_hh_## MMX( mid_dst_h, src - src_offset, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_c_ ## MMX(dst  , src, dstStride, srcStride, mid_dst_h + mid_offset);\
    cavs_get_qpel8or16_hh_## MMX( mid_dst_h + 8, src-src_offset + 8, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_c_ ## MMX(dst + 8 , src + 8, dstStride, srcStride, mid_dst_h+ mid_offset + 8);\
    src += (srcStride<<3);\
    dst += (dstStride<<3);\
    cavs_get_qpel8or16_hh_## MMX( mid_dst_h + /*8*24*/192, src- src_offset, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_c_ ## MMX(dst  , src, dstStride, srcStride, mid_dst_h+/*8*24*/192 +mid_offset);\
    cavs_get_qpel8or16_hh_## MMX( mid_dst_h + /*8*24 +8*/200, src-src_offset + 8, dstStride, srcStride);\
    cavs_## OPNAME ## qpel8or16_c_ ## MMX(dst + 8 , src + 8, dstStride, srcStride, mid_dst_h+/*8*24+8*/200+mid_offset);\
}\

#define CAVS_MC(OPNAME, SIZE, MMX) \
void ff_ ## OPNAME ## cavs_qpel ## SIZE ## _mc20_ ## MMX(uint8_t *dst, uint8_t *src, int stride)\
{\
    OPNAME ## cavs_qpel ## SIZE ## _h_ ## MMX(dst, src, stride, stride);\
}\
\
void ff_ ## OPNAME ## cavs_qpel ## SIZE ## _mc02_ ## MMX(uint8_t *dst, uint8_t *src, int stride)\
{\
    OPNAME ## cavs_qpel ## SIZE ## _v2_ ## MMX(dst, src, stride, stride);\
}\
void ff_ ## OPNAME ## cavs_qpel ## SIZE ## _mc22_ ## MMX(uint8_t *dst, uint8_t *src, int stride)\
{\
    OPNAME ## cavs_qpel ## SIZE ## _j_ ## MMX(dst, src, stride, stride);\
}\
void ff_ ## OPNAME ## cavs_qpel ## SIZE ## _mc11_ ## MMX(uint8_t *dst, uint8_t *src, int stride)\
{\
    OPNAME ## cavs_qpel ## SIZE ## _e_ ## MMX(dst, src, stride, stride);\
}\
void ff_ ## OPNAME ## cavs_qpel ## SIZE ## _mc31_ ## MMX(uint8_t *dst, uint8_t *src, int stride)\
{\
    OPNAME ## cavs_qpel ## SIZE ## _g_ ## MMX(dst, src, stride, stride);\
}\
void ff_ ## OPNAME ## cavs_qpel ## SIZE ## _mc13_ ## MMX(uint8_t *dst, uint8_t *src, int stride)\
{\
    OPNAME ## cavs_qpel ## SIZE ## _p_ ## MMX(dst, src, stride, stride);\
}\
void ff_ ## OPNAME ## cavs_qpel ## SIZE ## _mc33_ ## MMX(uint8_t *dst, uint8_t *src, int stride)\
{\
    OPNAME ## cavs_qpel ## SIZE ## _r_ ## MMX(dst, src, stride, stride);\
}\
void ff_ ## OPNAME ## cavs_qpel ## SIZE ## _mc12_ ## MMX(uint8_t *dst, uint8_t *src, int stride)\
{\
    OPNAME ## cavs_qpel ## SIZE ## _i_ ## MMX(dst, src, stride, stride);\
}\
void ff_ ## OPNAME ## cavs_qpel ## SIZE ## _mc32_ ## MMX(uint8_t *dst, uint8_t *src, int stride)\
{\
    OPNAME ## cavs_qpel ## SIZE ## _k_ ## MMX(dst, src, stride, stride);\
}\
void ff_ ## OPNAME ## cavs_qpel ## SIZE ## _mc21_ ## MMX(uint8_t *dst, uint8_t *src, int stride)\
{\
    OPNAME ## cavs_qpel ## SIZE ## _f_ ## MMX(dst, src, stride, stride);\
}\
void ff_ ## OPNAME ## cavs_qpel ## SIZE ## _mc23_ ## MMX(uint8_t *dst, uint8_t *src, int stride)\
{\
    OPNAME ## cavs_qpel ## SIZE ## _q_ ## MMX(dst, src, stride, stride);\
}\
void ff_ ## OPNAME ## cavs_qpel ## SIZE ## _mc01_ ## MMX(uint8_t *dst, uint8_t *src, int stride)\
{\
    OPNAME ## cavs_qpel ## SIZE ## _d_ ## MMX(dst, src, stride, stride);\
}\
void ff_ ## OPNAME ## cavs_qpel ## SIZE ## _mc03_ ## MMX(uint8_t *dst, uint8_t *src, int stride)\
{\
    OPNAME ## cavs_qpel ## SIZE ## _n_ ## MMX(dst, src, stride, stride);\
}\
void ff_ ## OPNAME ## cavs_qpel ## SIZE ## _mc10_ ## MMX(uint8_t *dst, uint8_t *src, int stride)\
{\
    OPNAME ## cavs_qpel ## SIZE ## _a_ ## MMX(dst, src, stride, stride);\
}\
void ff_ ## OPNAME ## cavs_qpel ## SIZE ## _mc30_ ## MMX(uint8_t *dst, uint8_t *src, int stride)\
{\
    OPNAME ## cavs_qpel ## SIZE ## _c_ ## MMX(dst, src, stride, stride);\
}\

#if 0
#define PUT_OP(a,b,temp, size) "mov" #size " " #a ", " #b "    \n\t"
#define AVG_3DNOW_OP(a,b,temp, size) \
"mov" #size " " #b ", " #temp "   \n\t"\
"pavgusb " #temp ", " #a "        \n\t"\
"mov" #size " " #a ", " #b "      \n\t"
#define AVG_MMXEXT_OP(a, b, temp, size) \
"mov" #size " " #b ", " #temp "   \n\t"\
"pavgb " #temp ", " #a "          \n\t"\
"mov" #size " " #a ", " #b "      \n\t"
#endif

#endif /* (HAVE_MMXEXT_INLINE || HAVE_AMD3DNOW_INLINE) */

#if 1//HAVE_MMXEXT_INLINE
QPEL_CAVS(put_, put, mmxext)
QPEL_CAVS(avg_, avg, mmxext)

CAVS_MC(put_,  8, mmxext)
CAVS_MC(put_, 16, mmxext)
CAVS_MC(avg_,  8, mmxext)
CAVS_MC(avg_, 16, mmxext)


/* CAVS-specific */
extern void cavs_avg_pixels8_mmxext(uint8_t *block, const uint8_t *pixels, int line_size, int h);
extern void cavs_avg_pixels16_sse2(uint8_t *block, const uint8_t *pixels, int line_size, int h);
extern void cavs_put_pixels8_mmxext(uint8_t *block, const uint8_t *pixels, int line_size, int h);
extern void cavs_put_pixels16_mmxext(uint8_t *block, const uint8_t *pixels, int line_size, int h);

void ff_put_bavs_qpel8_mc00_mmxext(uint8_t *dst, uint8_t *src, int stride)
{
    cavs_put_pixels8_mmxext(dst, src , stride, 8);
}

void ff_avg_bavs_qpel8_mc00_mmxext(uint8_t *dst, uint8_t *src, int stride)
{
    cavs_avg_pixels8_mmxext(dst, src, stride, 8);
}

void ff_put_bavs_qpel16_mc00_mmxext(uint8_t *dst, uint8_t *src, int stride)
{
    cavs_put_pixels16_mmxext(dst, src, stride, 16);
}

void ff_avg_bavs_qpel16_mc00_mmxext(uint8_t *dst, uint8_t *src, int stride)
{
#if 1
    cavs_avg_pixels16_sse2(dst, src, stride, 16);
#else
    cavs_avg_pixels8_mmxext(dst, src, stride, 16);
    cavs_avg_pixels8_mmxext(dst+8, src+8, stride, 16);
#endif     
}

#endif

#undef DECLARE_ALIGNED_16
