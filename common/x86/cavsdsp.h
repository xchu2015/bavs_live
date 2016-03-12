
#ifndef CAVS_MC_H
#define CAVS_MC_H

#include <stdint.h>

/* put */
void ff_put_bavs_qpel8_mc00_mmxext(uint8_t *dst, uint8_t *src, int stride); /*D*/
void ff_put_bavs_qpel16_mc00_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_put_cavs_qpel8_mc20_mmxext(uint8_t *dst, uint8_t *src, int stride);/*b*/
void ff_put_cavs_qpel16_mc20_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_put_cavs_qpel8_mc01_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_put_cavs_qpel16_mc01_mmxext(uint8_t *dst, uint8_t *src, int stride);/*d*/
void ff_put_cavs_qpel8_mc02_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_put_cavs_qpel16_mc02_mmxext(uint8_t *dst, uint8_t *src, int stride);/*h*/
void ff_put_cavs_qpel8_mc03_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_put_cavs_qpel16_mc03_mmxext(uint8_t *dst, uint8_t *src, int stride);/*n*/
/*a*/
void ff_put_cavs_qpel8_mc10_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_put_cavs_qpel16_mc10_mmxext(uint8_t *dst, uint8_t *src, int stride);
/*c*/
void ff_put_cavs_qpel8_mc30_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_put_cavs_qpel16_mc30_mmxext(uint8_t *dst, uint8_t *src, int stride);
/*j*/
void ff_put_cavs_qpel8_mc22_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_put_cavs_qpel16_mc22_mmxext(uint8_t *dst, uint8_t *src, int stride);
/*e*/
void ff_put_cavs_qpel8_mc11_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_put_cavs_qpel16_mc11_mmxext(uint8_t *dst, uint8_t *src, int stride);
/*g*/
void ff_put_cavs_qpel8_mc31_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_put_cavs_qpel16_mc31_mmxext(uint8_t *dst, uint8_t *src, int stride);
/*p*/
void ff_put_cavs_qpel8_mc13_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_put_cavs_qpel16_mc13_mmxext(uint8_t *dst, uint8_t *src, int stride);
/*r*/
void ff_put_cavs_qpel8_mc33_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_put_cavs_qpel16_mc33_mmxext(uint8_t *dst, uint8_t *src, int stride);

/*i*/
void ff_put_cavs_qpel8_mc12_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_put_cavs_qpel16_mc12_mmxext(uint8_t *dst, uint8_t *src, int stride);

/*k*/
void ff_put_cavs_qpel8_mc32_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_put_cavs_qpel16_mc32_mmxext(uint8_t *dst, uint8_t *src, int stride);

/*f*/
void ff_put_cavs_qpel8_mc21_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_put_cavs_qpel16_mc21_mmxext(uint8_t *dst, uint8_t *src, int stride);

/*q*/
void ff_put_cavs_qpel8_mc23_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_put_cavs_qpel16_mc23_mmxext(uint8_t *dst, uint8_t *src, int stride);

/* avg */
void ff_avg_bavs_qpel8_mc00_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_avg_bavs_qpel16_mc00_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_avg_cavs_qpel8_mc20_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_avg_cavs_qpel16_mc20_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_avg_cavs_qpel8_mc01_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_avg_cavs_qpel16_mc01_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_avg_cavs_qpel8_mc02_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_avg_cavs_qpel16_mc02_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_avg_cavs_qpel8_mc03_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_avg_cavs_qpel16_mc03_mmxext(uint8_t *dst, uint8_t *src, int stride);
/*a*/
void ff_avg_cavs_qpel8_mc10_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_avg_cavs_qpel16_mc10_mmxext(uint8_t *dst, uint8_t *src, int stride);
/*c*/
void ff_avg_cavs_qpel8_mc30_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_avg_cavs_qpel16_mc30_mmxext(uint8_t *dst, uint8_t *src, int stride);
/*j*/
void ff_avg_cavs_qpel8_mc22_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_avg_cavs_qpel16_mc22_mmxext(uint8_t *dst, uint8_t *src, int stride);
/*e*/
void ff_avg_cavs_qpel8_mc11_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_avg_cavs_qpel16_mc11_mmxext(uint8_t *dst, uint8_t *src, int stride);
/*g*/
void ff_avg_cavs_qpel8_mc31_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_avg_cavs_qpel16_mc31_mmxext(uint8_t *dst, uint8_t *src, int stride);
/*p*/
void ff_avg_cavs_qpel8_mc13_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_avg_cavs_qpel16_mc13_mmxext(uint8_t *dst, uint8_t *src, int stride);
/*r*/
void ff_avg_cavs_qpel8_mc33_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_avg_cavs_qpel16_mc33_mmxext(uint8_t *dst, uint8_t *src, int stride);
/*i*/
void ff_avg_cavs_qpel8_mc12_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_avg_cavs_qpel16_mc12_mmxext(uint8_t *dst, uint8_t *src, int stride);

/*k*/
void ff_avg_cavs_qpel8_mc32_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_avg_cavs_qpel16_mc32_mmxext(uint8_t *dst, uint8_t *src, int stride);

/*f*/
void ff_avg_cavs_qpel8_mc21_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_avg_cavs_qpel16_mc21_mmxext(uint8_t *dst, uint8_t *src, int stride);

/*q*/
void ff_avg_cavs_qpel8_mc23_mmxext(uint8_t *dst, uint8_t *src, int stride);
void ff_avg_cavs_qpel16_mc23_mmxext(uint8_t *dst, uint8_t *src, int stride);

#endif /* AVCODEC_CAVSDSP_H */
