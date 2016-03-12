/*****************************************************************************
 * cpu.h: cavs decoder library
 *****************************************************************************
*/

#ifndef _CAVS_CPU_H_
#define _CAVS_CPU_H_

#ifndef _MSC_VER
#include "config.h"
#endif

unsigned int cavs_cpu_detect (void);
int cavs_cpu_num_processors (void);
void cavs_cpu_emms (void);
void cavs_cpu_mask_misalign_sse (void);
void cavs_cpu_sfence( void );

#if HAVE_MMX
#define cavs_emms() cavs_cpu_emms()
#else
#define cavs_emms()
#endif

#define cavs_sfence cavs_cpu_sfence

typedef struct
{
  const char name[16];
  int flags;
} xavs_cpu_name_t;
extern const xavs_cpu_name_t xavs_cpu_names[];

void cavs_cpu_restore (unsigned int cpu);

#endif
