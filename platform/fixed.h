/**
 *  @file fixed.h
 *
 *  Copyright (C) 2006 V2_lab, Simon de Bakker <simon@v2.nl>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

/* based on libmad fixed implementation */

#ifndef	__SIOS_FIXED_H__
#define __SIOS_FIXED_H__

/* 
 * fixed point format: 16.16 
 * whole part: sign + 15 bits
 * fractional part: 16 bits
 */

typedef signed int mm_fixed_t;
typedef signed int mm_fixed64hi_t;
typedef unsigned int mm_fixed64lo_t;

#define mm_fixed64_t signed long long

#define MM_FIXED_RADIX	    16
#define MM_FIXED_MIN	    ((mm_fixed_t) -0x80000000L)
#define MM_FIXED_MAX	    ((mm_fixed_t) +0x7fffffffL)
#define MM_FIXED_ONE	    ((mm_fixed_t) +0x10000L)

#define ftofix(x)	    ((mm_fixed_t) \
			    ((x) * (double) (1L << MM_FIXED_RADIX) + 0.5))
#define itofix(x)	    ((x) << MM_FIXED_RADIX)
#define fixtoi(x)	    fintpart(x)
#define fintpart(x)	    ((x) >> MM_FIXED_RADIX)
#define ffracpart(x)	    ((x) & ((1L << MM_FIXED_RADIX) - 1))

#ifdef ARM
# define fmulff(x, y)					    \
  ({ mm_fixed64hi_t __hi;					    \
     mm_fixed64lo_t __lo;					    \
     mm_fixed_t __result;					    \
     __asm__ __volatile__				    \
         ("smull    %0, %1, %3, %4\n\t"			    \
          "movs     %0, %0, lsr %5\n\t"			    \
          "adc      %2, %0, %1, lsl %6"			    \
          : "=&r" (__lo), "=&r" (__hi), "=r" (__result)	    \
          : "%r" (x), "r" (y),				    \
            "M" (MM_FIXED_RADIX), "M" (32 - MM_FIXED_RADIX) \
          : "cc");					    \
     __result;						    \
  })

# define add(x, y)					    \
  ({							    \
     int __result;					    \
     __asm__ __volatile__				    \
       ("qadd    %0, %1, %2"				    \
        : "=r" (__result)				    \
	: "r" (x), "r" (y));				    \
    (__result);						    \
  })
	
# define sub(x, y)					    \
  ({							    \
     int __result;					    \
     __asm__ __volatile__				    \
       ("qsub    %0, %1, %2"				    \
        : "=r" (__result)				    \
	: "r" (x), "r" (y));				    \
    (__result);						    \
  })

    
#else
# define fmulff(x, y)	    (int)(((long long)(x) * (long long)(y)) >> MM_FIXED_RADIX)
# define add(x, y)	    ((x) + (y))
# define sub(x, y)	    ((x) - (y))
#endif

//#define fadd(x, y)	    ((x) + (y))
//#define fsub(x, y)	    ((x) - (y))
#define fadd(x, y)	    add((x), (y))
#define fsub(x, y)	    sub((x), (y))
#define fdivff(x1, x2)	    (int)((((long long)(x1) << 32) / (x2)) >> 16)

#endif
