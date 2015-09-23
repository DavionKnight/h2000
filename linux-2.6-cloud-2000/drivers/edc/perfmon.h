/*
 * Copyright (C) 2005,2006,2009,2010 Freescale Semiconductor, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** \file   perfmon.h
*** \brief  Performance monitor register definitions.
**/

#ifndef __PERFMON_H__
#define __PERFMON_H__

/** \brief Creates a number with a specified bit set
*** \arg i An integer that represents a valid bit index (0-31)
*** \returns An integer with only bit position i set (big-endian notation),
***          or zero for an invalid argument
***
**/
/* FIX: Modified macro so that it returns 0 if i is not [0-31] */
/* ???: Do we need to also account for 64-bit architectures here? */
#define ATBIT(i) ((i > 31 || i < 0) ? 0 : (0x1<<(31-i)))

/** \name  Core Performance Monitor Registers
*** \brief FILLME
***
*** Register Definitions:
***   -# CPMON_PMGC0_FAC   = FILLME
***   -# CPMON_PMGC0_PMIE  = FILLME
***   -# CPMON_PMGC0_FCECE = FILLME
***   -# CPMON_PMGC0_TBSEL = FILLME
***   -# CPMON_PMGC0_TBEE  = FILLME
**/
/*@{*/
#define CPMON_PMGC0_FAC   ATBIT(0)
#define CPMON_PMGC0_PMIE  ATBIT(1)
#define CPMON_PMGC0_FCECE ATBIT(2)
#define CPMON_PMGC0_TBSEL (ATBIT(19)|ATBIT(20))
#define CPMON_PMGC0_TBEE  ATBIT(22)
/*@}*/

/**  \name  System Performance Monitor Registers
***  \brief FILLME
***
*** Register Definitions:
***   -# SPMON_PMGC0    = FILLME
***   -# SPMON_PMLCA(i) = FILLME
***   -# SPMON_PMLCB(i) = FILLME
***   -# SPMON_PMC0l    = FILLME
***   -# SPMON_PMC0u    = FILLME
***   -# SPMON_PMC(i)   = FILLME
**/
/*@{*/
#define SPMON_PMGC0    0
/* ???: The following function definitions using (i) check that (i) is a valid
   argument, or otherwise return zero or some kind of error. (See the ATBIT(i)
   fn for an example). */
/*! Offset of 0x10 bytes, every 0x10 bytes */
#define SPMON_PMLCA(i) (i*4 + 4)
/*! Offset of 0x14 bytes, every 0x10 bytes */
#define SPMON_PMLCB(i) (i*4 + 5)
/*! Offset of 0x18 bytes */
#define SPMON_PMC0l    6
#define SPMON_PMC0u    7
/*! Offset of 0x18, every 0x10 bytes */
/* ???: Should this be i*sizeof(int) instead of i * 4?? */
#define SPMON_PMC(i)   (i*4 + 6)
/*@}*/

/** \name  System Performance Monitor Registers,
*** \brief Bit Definitions for the System PerfMon Registers.
*** \return Zero on an error, non-zero
***
*** Register Definitions:
***   -# SPMON_PMGC0_FAC      = FILLME
***   -# SPMON_PMGC0_PMIE     = FILLME
***   -# SPMON_PMGC0_FCECE    = FILLME
***   -# SPMON_PMLCA_FC       = FILLME
***   -# SPMON_PMLCA_CE       = FILLME
***   -# SPMON_PMLCA_EVENT(i) = FILLME
***   -# SPMON_PMLCA_BSIZE(i) = FILLME
***   -# SPMON_PMLCA_BGRAN(i) = FILLME
***   -# SPMON_PMLCA_BDIST(i) = FILLME
***   -# SPMON_REF(i)         = FILLME
***   -# SPMON_CSPEC(i)       = FILLME
**/
/* ???: Most of these macros are not used anywhere else in the code. If they
   are not needed for near future use (SWIM 1.0), please either remove them or
   comment them out. */
/*@{*/
#define SPMON_PMGC0_FAC    ATBIT(0)
#define SPMON_PMGC0_PMIE   ATBIT(1)
#define SPMON_PMGC0_FCECE  ATBIT(2)

#define SPMON_PMLCA_FC     ATBIT(0)
#define SPMON_PMLCA_CE     ATBIT(5)

/* FIX: Changed so that if i < 0 it returns 0. Please confirm that this is
   correct/good behavior, and if necessary add an additional condition if i
   is too large */
#define SPMON_PMLCA_EVENT(i) ((i < 0) ? 0 : (i<<(31-15)))
#define SPMON_PMLCA_BSIZE(i) ((i < 0) ? 0 : (i<<(31-20)))
#define SPMON_PMLCA_BGRAN(i) ((i < 0) ? 0 : (i<<(31-25)))
#define SPMON_PMLCA_BDIST(i) ((i < 0) ? 0 : (i<<(31-31)))

/* ???: What is the puprose of SPMON_REF if it just returns the argument?
   I suggest removing this. */
/* ???: Add error checking for the following two functions as appropriate
   please. */
#define SPMON_REF(i)         (i)
#define SPMON_CSPEC(i)       (i+64)
/*@}*/

/** \name  FILLME
*** \brief FILLME
*** \arg   var FILLME
*** \arg   num FILLME
*** \note  If your compiler does not support the following inline assembly
*** assembly syntax, please modify it.
**/
/* ???: Should we attempt to detect which compilers support it this and add
   support for one other compiler that does not? */
/*@{*/
#define READ_CPMC(var, num) do { asm volatile \
	("mfpmr %0,"#num : "=r"(var) : ); } while (0)
#define SET_CPMC(var, num) do { asm volatile \
	("mtpmr " #num ",%0" : : "r"(var)); } while (0)
/*@}*/

/** \name  TSEC RMON Control Register Bit Assignment,
*** \brief Bit Definitions for the RMON Control Register.
**/

/*@{*/
#define RMON_STEN ATBIT(19)
#define RMON_CLRCNT ATBIT(17)
/*@}*/

/*! \name MPC8548 Architecture Definitions*/
#if defined(MPC8548)
	#define E500V2
	#define TOTAL_CPMC           4
	#define TOTAL_SPMCCB         2
	#define TOTAL_SPMC           9
	/*note: TOTAL_RMON is artificially imposed*/
	#define TOTAL_RMON           8
	#define TOTAL_PM_GLOBAL_CTL  2
	#define TOTAL_CPMC_CTL       8
	#define TOTAL_SPMCCB_CTL     2
	#define TOTAL_SPMC_CTL       18
	/*offset to a particular TSEC block from TSEC base */
	#define RMON_TSECn_OFFSET    0x1000

/*! \name MPC85XX Architecture Definitions*/
#elif defined(MPC85XX)
	#define E500
	#define TOTAL_CPMC           4
	#define TOTAL_SPMCCB         2
	#define TOTAL_SPMC           8
	/*note: TOTAL_RMON is artificially imposed*/
	#define TOTAL_RMON           8
	#define TOTAL_PM_GLOBAL_CTL  2
	#define TOTAL_CPMC_CTL       8
	#define TOTAL_SPMCCB_CTL     2
	#define TOTAL_SPMC_CTL       16
	/*offset to a particular TSEC block from TSEC base */
	#define RMON_TSECn_OFFSET    0x1000
#else
	#error Unknown or undefined architecture
#endif

/** \name  Number of Control Registers
*** \brief FILLME
**/
#define TOTAL_CTL_REGS (TOTAL_PM_GLOBAL_CTL + TOTAL_CPMC_CTL + \
		TOTAL_SPMCCB_CTL + TOTAL_SPMC_CTL)

/** \name  Total number of counter registers
*** \brief
**/
#define TOTAL_CNT_REGS (TOTAL_CPMC + TOTAL_SPMCCB + TOTAL_SPMC + TOTAL_RMON)

#endif /* __PERFMON_H__ */
