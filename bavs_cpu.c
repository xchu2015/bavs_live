/*****************************************************************************
 * cpu.c: bavs decoder library
 *****************************************************************************
*/

#include "bavs.h"
#include "bavs_cpu.h"

#ifndef	_STRING_H
# include <string.h>
#endif	/* string.h  */

#if HAVE_POSIXTHREAD && SYS_LINUX
#define _GNU_SOURCE
#include <sched.h>
#endif
#if SYS_BEOS
#include <kernel/OS.h>
#endif
#if SYS_MACOSX || SYS_FREEBSD
#include <sys/types.h>
#include <sys/sysctl.h>
#endif
#if SYS_OPENBSD
#include <sys/param.h>
#include <sys/sysctl.h>
#include <machine/cpu.h>
#endif

const xavs_cpu_name_t xavs_cpu_names[] = {
  {"Altivec", CAVS_CPU_ALTIVEC},
  {"MMX2", CAVS_CPU_MMX | CAVS_CPU_MMXEXT},
  {"MMXEXT", CAVS_CPU_MMX | CAVS_CPU_MMXEXT},
  {"SSE2Slow", CAVS_CPU_MMX | CAVS_CPU_MMXEXT | CAVS_CPU_SSE | CAVS_CPU_SSE2 | CAVS_CPU_SSE2_IS_SLOW},
  {"SSE2", CAVS_CPU_MMX | CAVS_CPU_MMXEXT | CAVS_CPU_SSE | CAVS_CPU_SSE2},
  {"SSE2Fast", CAVS_CPU_MMX | CAVS_CPU_MMXEXT | CAVS_CPU_SSE | CAVS_CPU_SSE2 | CAVS_CPU_SSE2_IS_FAST},
  {"SSE3", CAVS_CPU_MMX | CAVS_CPU_MMXEXT | CAVS_CPU_SSE | CAVS_CPU_SSE2 | CAVS_CPU_SSE3},
  {"SSSE3", CAVS_CPU_MMX | CAVS_CPU_MMXEXT | CAVS_CPU_SSE | CAVS_CPU_SSE2 | CAVS_CPU_SSE3 | CAVS_CPU_SSSE3},
  {"PHADD",   CAVS_CPU_MMX| CAVS_CPU_MMXEXT | CAVS_CPU_SSE | CAVS_CPU_SSE2 | CAVS_CPU_SSE3 | CAVS_CPU_SSSE3 | CAVS_CPU_PHADD_IS_FAST},
  {"SSE4.1", CAVS_CPU_MMX | CAVS_CPU_MMXEXT | CAVS_CPU_SSE | CAVS_CPU_SSE2 | CAVS_CPU_SSE3 | CAVS_CPU_SSSE3 | CAVS_CPU_SSE4},
  {"SSE4.2", CAVS_CPU_MMX | CAVS_CPU_MMXEXT | CAVS_CPU_SSE | CAVS_CPU_SSE2 | CAVS_CPU_SSE3 | CAVS_CPU_SSSE3 | CAVS_CPU_SSE4 | CAVS_CPU_SSE42},
  {"Cache32", CAVS_CPU_CACHELINE_32},
  {"Cache64", CAVS_CPU_CACHELINE_64},
  {"SSEMisalign", CAVS_CPU_SSE_MISALIGN},
  {"LZCNT", CAVS_CPU_LZCNT},
  {"Slow_mod4_stack", CAVS_CPU_STACK_MOD4},
  {"", 0},
};


#if HAVE_MMX
extern int cavs_cpu_cpuid_test (void);
extern unsigned int cavs_cpu_cpuid (unsigned int op, unsigned int * eax, unsigned int * ebx, unsigned int * ecx, unsigned int * edx);
void cavs_cpu_xgetbv( unsigned int op, unsigned int *eax, unsigned int *edx );

unsigned int
cavs_cpu_detect (void)
{
  unsigned int cpu = 0;
  unsigned int eax, ebx, ecx, edx;
  unsigned int vendor[4] = { 0 };
  int max_extended_cap;
  int cache;

#if !ARCH_X86_64
  if (!cavs_cpu_cpuid_test ())
    return 0;
#endif

  cavs_cpu_cpuid (0, &eax, vendor + 0, vendor + 2, vendor + 1);
  if (eax == 0)
    return 0;

  cavs_cpu_cpuid (1, &eax, &ebx, &ecx, &edx);
  if (edx & 0x00800000)
    cpu |= CAVS_CPU_MMX;
  else
    return 0;
  if (edx & 0x02000000)
    cpu |= CAVS_CPU_MMXEXT | CAVS_CPU_SSE;
  if (edx & 0x04000000)
    cpu |= CAVS_CPU_SSE2;
  if (ecx & 0x00000001)
    cpu |= CAVS_CPU_SSE3;
  if (ecx & 0x00000200)
    cpu |= CAVS_CPU_SSSE3;
  if (ecx & 0x00080000)
    cpu |= CAVS_CPU_SSE4;
  if (ecx & 0x00100000)
    cpu |= CAVS_CPU_SSE42;
  /* Check OXSAVE and AVX bits */
  if( (ecx&0x18000000) == 0x18000000 )
  {
	  /* Check for OS support */
	  cavs_cpu_xgetbv( 0, &eax, &edx );
	  if( (eax&0x6) == 0x6 )
		  cpu |= CAVS_CPU_AVX;
  }

  if (cpu & CAVS_CPU_SSSE3)
    cpu |= CAVS_CPU_SSE2_IS_FAST;
  if (cpu & CAVS_CPU_SSE4)
    //cpu |= CAVS_CPU_SHUFFLE_IS_FAST;
	cpu |= CAVS_CPU_PHADD_IS_FAST;

  cavs_cpu_cpuid (0x80000000, &eax, &ebx, &ecx, &edx);
  max_extended_cap = eax;

  if (!strcmp ((char *) vendor, "AuthenticAMD") && max_extended_cap >= 0x80000001)
  {
    cavs_cpu_cpuid (0x80000001, &eax, &ebx, &ecx, &edx);
    if (edx & 0x00400000)
      cpu |= CAVS_CPU_MMXEXT;
    if (cpu & CAVS_CPU_SSE2)
    {
      if (ecx & 0x00000040)     /* SSE4a */
      {
        cpu |= CAVS_CPU_SSE2_IS_FAST;
        cpu |= CAVS_CPU_SSE_MISALIGN;
        cpu |= CAVS_CPU_LZCNT;
        cpu |= CAVS_CPU_SHUFFLE_IS_FAST;
        cavs_cpu_mask_misalign_sse ();
      }
      else
        cpu |= CAVS_CPU_SSE2_IS_SLOW;
    }
  }

  if (!strcmp ((char *) vendor, "GenuineIntel"))
  {
    int family, model, stepping;
    cavs_cpu_cpuid (1, &eax, &ebx, &ecx, &edx);
    family = ((eax >> 8) & 0xf) + ((eax >> 20) & 0xff);
    model = ((eax >> 4) & 0xf) + ((eax >> 12) & 0xf0);
    stepping = eax & 0xf;
    /* 6/9 (pentium-m "banias"), 6/13 (pentium-m "dothan"), and 6/14 (core1 "yonah")
     * theoretically support sse2, but it's significantly slower than mmx for
     * almost all of bavs's functions, so let's just pretend they don't. */
    if (family == 6 && (model == 9 || model == 13 || model == 14))
    {
      cpu &= ~(CAVS_CPU_SSE2 | CAVS_CPU_SSE3);
//      assert (!(cpu & (CAVS_CPU_SSSE3 | CAVS_CPU_SSE4)));
    }
  }

  if ((!strcmp ((char *) vendor, "GenuineIntel") || !strcmp ((char *) vendor, "CyrixInstead")) && !(cpu & CAVS_CPU_SSE42))
  {
    /* cacheline size is specified in 3 places, any of which may be missing */
    cavs_cpu_cpuid (1, &eax, &ebx, &ecx, &edx);
    cache = (ebx & 0xff00) >> 5;        // cflush size
    if (!cache && max_extended_cap >= 0x80000006)
    {
      cavs_cpu_cpuid (0x80000006, &eax, &ebx, &ecx, &edx);
      cache = ecx & 0xff;       // cacheline size
    }
    if (!cache)
    {
      // Cache and TLB Information
      static const char cache32_ids[] = { 0x0a, 0x0c, 0x41, 0x42, 0x43, 0x44, 0x45, 0x82, 0x83, 0x84, 0x85, 0 };
      static const char cache64_ids[] = { 0x22, 0x23, 0x25, 0x29, 0x2c, 0x46, 0x47, 0x49, 0x60, 0x66, 0x67, 0x68, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7c, 0x7f, 0x86, 0x87, 0 };
      unsigned int buf[4];
      int max, i = 0, j;
      do
      {
        cavs_cpu_cpuid (2, buf + 0, buf + 1, buf + 2, buf + 3);
        max = buf[0] & 0xff;
        buf[0] &= ~0xff;
        for (j = 0; j < 4; j++)
          if (!(buf[j] >> 31))
            while (buf[j])
            {
              if (strchr (cache32_ids, buf[j] & 0xff))
                cache = 32;
              if (strchr (cache64_ids, buf[j] & 0xff))
                cache = 64;
              buf[j] >>= 8;
            }
      }
      while (++i < max);
    }

    if (cache == 32)
      cpu |= CAVS_CPU_CACHELINE_32;
    else if (cache == 64)
      cpu |= CAVS_CPU_CACHELINE_64;
    else
      fprintf (stderr, "bavs [warning]: unable to determine cacheline size\n");
  }

#ifdef BROKEN_STACK_ALIGNMENT
  cpu |= XAVS_CPU_STACK_MOD4;
#endif

  return cpu;
}

#elif defined( ARCH_PPC )

#if defined(SYS_MACOSX) || defined(SYS_OPENBSD)
#include <sys/sysctl.h>
unsigned int
cavs_cpu_detect (void)
{
  /* Thank you VLC */
  unsigned int cpu = 0;
#if SYS_OPENBSD
  int selectors[2] = { CTL_MACHDEP, CPU_ALTIVEC };
#else
  int selectors[2] = { CTL_HW, HW_VECTORUNIT };
#endif
  int has_altivec = 0;
  size_t length = sizeof (has_altivec);
  int error = sysctl (selectors, 2, &has_altivec, &length, NULL, 0);

  if (error == 0 && has_altivec != 0)
  {
    cpu |= CAVS_CPU_ALTIVEC;
  }

  return cpu;
}

#elif defined( SYS_LINUX )
#include <signal.h>
#include <setjmp.h>
static sigjmp_buf jmpbuf;
static volatile sig_atomic_t canjump = 0;

static void
sigill_handler (int sig)
{
  if (!canjump)
  {
    signal (sig, SIG_DFL);
    raise (sig);
  }

  canjump = 0;
  siglongjmp (jmpbuf, 1);
}

unsigned int
cavs_cpu_detect (void)
{
  static void (*oldsig) (int);

  oldsig = signal (SIGILL, sigill_handler);
  if (sigsetjmp (jmpbuf, 1))
  {
    signal (SIGILL, oldsig);
    return 0;
  }

  canjump = 1;
  asm volatile ("mtspr 256, %0\n\t" "vand 0, 0, 0\n\t"::"r" (-1));
  canjump = 0;

  signal (SIGILL, oldsig);

  return CAVS_CPU_ALTIVEC;
}
#endif

#else

unsigned int
cavs_cpu_detect (void)
{
  return 0;
}

void
xavs_cpu_restore (unsigned int cpu)
{
  return;
}

//void
//xavs_emms (void)
//{
//  return;
//}

#endif

extern int pthread_num_processors_np(void);

int
xavs_cpu_num_processors (void)
{
#if !HAVE_THREAD
  return 1;

#elif SYS_WINDOWS
  return pthread_num_processors_np ();

#elif SYS_LINUX
  unsigned int bit;
  int np;
  cpu_set_t p_aff;
  memset (&p_aff, 0, sizeof (p_aff));
  sched_getaffinity (0, sizeof (p_aff), &p_aff);
  for (np = 0, bit = 0; bit < sizeof (p_aff); bit++)
    np += (((unsigned char *) & p_aff)[bit / 8] >> (bit % 8)) & 1;
  return np;

#elif SYS_BEOS
  system_info info;
  get_system_info (&info);
  return info.cpu_count;

#elif SYS_MACOSX || SYS_FREEBSD || SYS_OPENBSD
  int numberOfCPUs;
  size_t length = sizeof (numberOfCPUs);
#if SYS_OPENBSD
  int mib[2] = { CTL_HW, HW_NCPU };
  if (sysctl (mib, 2, &numberOfCPUs, &length, NULL, 0))
#else
  if (sysctlbyname ("hw.ncpu", &numberOfCPUs, &length, NULL, 0))
#endif
  {
    numberOfCPUs = 1;
  }
  return numberOfCPUs;

#else
  return 1;
#endif
}
