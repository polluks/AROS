/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

#ifndef lint
static char rcsid[] = "$FreeBSD: src/lib/msun/src/e_atan2l.c,v 1.3 2008/08/02 19:17:00 das Exp $";
#endif

#include <float.h>
#include "math.h"

#include "invtrig.h"
#include "math_private.h"

static const volatile long double
tiny  __attribute__ ((__section__(".rodata,\"a\" " SECTIONCOMMENT))) = 1.0e-300;
static const long double
zero  = 0.0;

#ifdef __i386__
/* XXX Work around the fact that gcc truncates long double constants on i386 */
static const volatile double
pi1 __attribute__ ((__section__(".rodata,\"a\" " SECTIONCOMMENT))) =  3.14159265358979311600e+00,	/*  0x1.921fb54442d18p+1  */
pi2 __attribute__ ((__section__(".rodata,\"a\" " SECTIONCOMMENT))) =  1.22514845490862001043e-16;	/*  0x1.1a80000000000p-53 */
#define	pi	((long double)pi1 + pi2)
#else
static const long double
pi =  3.14159265358979323846264338327950280e+00L;
#endif

long double
__ieee754_atan2l(long double y, long double x)
{
	union IEEEl2bits ux, uy;
	long double z;
	int32_t k,m;
	int16_t exptx, expsignx, expty, expsigny;

	uy.e = y;
	expsigny = uy.xbits.expsign;
	expty = expsigny & 0x7fff;
	ux.e = x;
	expsignx = ux.xbits.expsign;
	exptx = expsignx & 0x7fff;

	if ((exptx==BIAS+LDBL_MAX_EXP &&
	     ((ux.bits.manh&~LDBL_NBIT)|ux.bits.manl)!=0) ||	/* x is NaN */
	    (expty==BIAS+LDBL_MAX_EXP &&
	     ((uy.bits.manh&~LDBL_NBIT)|uy.bits.manl)!=0))	/* y is NaN */
	    return x+y;
	if (expsignx==BIAS && ((ux.bits.manh&~LDBL_NBIT)|ux.bits.manl)==0)
	    return atanl(y);					/* x=1.0 */
	m = ((expsigny>>15)&1)|((expsignx>>14)&2);	/* 2*sign(x)+sign(y) */

    /* when y = 0 */
	if(expty==0 && ((uy.bits.manh&~LDBL_NBIT)|uy.bits.manl)==0) {
	    switch(m) {
		case 0: 
		case 1: return y; 	/* atan(+-0,+anything)=+-0 */
		case 2: return  pi+tiny;/* atan(+0,-anything) = pi */
		case 3: return -pi-tiny;/* atan(-0,-anything) =-pi */
	    }
	}
    /* when x = 0 */
	if(exptx==0 && ((ux.bits.manh&~LDBL_NBIT)|ux.bits.manl)==0)
	    return (expsigny<0)?  -pio2_hi-tiny: pio2_hi+tiny;

    /* when x is INF */
	if(exptx==BIAS+LDBL_MAX_EXP) {
	    if(expty==BIAS+LDBL_MAX_EXP) {
		switch(m) {
		    case 0: return  pio2_hi*0.5+tiny;/* atan(+INF,+INF) */
		    case 1: return -pio2_hi*0.5-tiny;/* atan(-INF,+INF) */
		    case 2: return  1.5*pio2_hi+tiny;/*atan(+INF,-INF)*/
		    case 3: return -1.5*pio2_hi-tiny;/*atan(-INF,-INF)*/
		}
	    } else {
		switch(m) {
		    case 0: return  zero  ;	/* atan(+...,+INF) */
		    case 1: return -zero  ;	/* atan(-...,+INF) */
		    case 2: return  pi+tiny  ;	/* atan(+...,-INF) */
		    case 3: return -pi-tiny  ;	/* atan(-...,-INF) */
		}
	    }
	}
    /* when y is INF */
	if(expty==BIAS+LDBL_MAX_EXP)
	    return (expsigny<0)? -pio2_hi-tiny: pio2_hi+tiny;

    /* compute y/x */
	k = expty-exptx;
	if(k > LDBL_MANT_DIG+2) {			/* |y/x| huge */
	    z=pio2_hi+pio2_lo;
	    m&=1;
	}
	else if(expsignx<0&&k<-LDBL_MANT_DIG-2) z=0.0; 	/* |y/x| tiny, x<0 */
	else z=atanl(fabsl(y/x));		/* safe to do y/x */
	switch (m) {
	    case 0: return       z  ;	/* atan(+,+) */
	    case 1: return      -z  ;	/* atan(-,+) */
	    case 2: return  pi-(z-pi_lo);/* atan(+,-) */
	    default: /* case 3 */
	    	    return  (z-pi_lo)-pi;/* atan(-,-) */
	}
}
