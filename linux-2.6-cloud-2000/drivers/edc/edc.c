/*
 * Copyright (C) 2005,2006,2009-2011 Freescale Semiconductor, Inc.
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

/** \file   edc.c
*** \brief  Event Data Collector function definitions.
**/

/* ???: This definitely need to be removed eventually */
#define MPC8548

#include "perfmon.h"
#include "edc.h"

/******************************************************************************
 Global Variables
******************************************************************************/

/** \brief The performance monitor register base address.
*** \note This address is set in the function Edc_linux_init in the file
*** edc_linux_harness.c
***
*** This is the base address from which all the performance monitor registers
*** may be accessed.
**/
uint32_t *G_pmon_regs = (void *)0;

/** \brief The eTSEC statistics registers
*** \note These addresses are set in the function Edc_linux_init in the file
*** edc_linux_harness.c
***
*** The base addresses for the eTSEC statistics registers, and
*** for the corresponding control registers.
**/
char	 *G_rmon_base            = (void *)0;
uint32_t *G_etsec1_rmon_ctrl_reg = (void *)0;
uint32_t *G_etsec2_rmon_ctrl_reg = (void *)0;
uint32_t *G_etsec3_rmon_ctrl_reg = (void *)0;
uint32_t *G_etsec4_rmon_ctrl_reg = (void *)0;

/** \brief Array which holds address offsets (in bytes) to rmon registers
*** \note  it has artificially imposed size by TOTAL_RMON define
*** \note  it is initialized to count RX and TX packets for the 4 TSECs
***
*** The offsets are calculated from the base address for all TSEC blocks
*** which is the same as the base address for the first TSEC block TSEC1
**/
uint32_t  G_rmon_offset[TOTAL_RMON] = {0x6A0, 0x6E4, 0x16A0, 0x16E4, 0x26A0,
	0x26E4, 0x36A0, 0x36E4};


/******************************************************************************
 Function Definitions
******************************************************************************/

uint32_t *edc_get_pmon_base_addr(void)
{
	return G_pmon_regs;
}


int32_t edc_start(uint32_t cmd)
{
	uint32_t start_cmd;
	DEBUG_PRINT("SWIM EDC: Function = %s, Line = %i, Cmd = %lx\n",
		__func__, __LINE__, cmd);

	/* Start core monitoring */
	if (cmd & EDC_START_CORE) {
		READ_CPMC(start_cmd, 400);
		start_cmd &= ~CPMON_PMGC0_FAC; /* CMME */
		SET_CPMC(start_cmd, 400);
	}

	/* Start System Monitoring */
	if (cmd & EDC_START_SYS)
		G_pmon_regs[SPMON_PMGC0] &= ~SPMON_PMGC0_FAC; /* CMME */

	/* Start TSEC RMON counters */
	if (cmd & EDC_START_RMON) {
		/* disable RMON */
		if (G_etsec1_rmon_ctrl_reg) *G_etsec1_rmon_ctrl_reg &= ~RMON_STEN;
		if (G_etsec2_rmon_ctrl_reg) *G_etsec2_rmon_ctrl_reg &= ~RMON_STEN;
		if (G_etsec3_rmon_ctrl_reg) *G_etsec3_rmon_ctrl_reg &= ~RMON_STEN;
		if (G_etsec4_rmon_ctrl_reg) *G_etsec4_rmon_ctrl_reg &= ~RMON_STEN;

		/* clear counters (self-resetting bit) */
		if (G_etsec1_rmon_ctrl_reg) *G_etsec1_rmon_ctrl_reg |= RMON_CLRCNT;
		if (G_etsec2_rmon_ctrl_reg) *G_etsec2_rmon_ctrl_reg |= RMON_CLRCNT;
		if (G_etsec3_rmon_ctrl_reg) *G_etsec3_rmon_ctrl_reg |= RMON_CLRCNT;
		if (G_etsec4_rmon_ctrl_reg) *G_etsec4_rmon_ctrl_reg |= RMON_CLRCNT;

		/* enable RMON */
		if (G_etsec1_rmon_ctrl_reg) *G_etsec1_rmon_ctrl_reg |= RMON_STEN;
		if (G_etsec2_rmon_ctrl_reg) *G_etsec2_rmon_ctrl_reg |= RMON_STEN;
		if (G_etsec3_rmon_ctrl_reg) *G_etsec3_rmon_ctrl_reg |= RMON_STEN;
		if (G_etsec4_rmon_ctrl_reg) *G_etsec4_rmon_ctrl_reg |= RMON_STEN;
	}

	if (!(cmd & (EDC_START_CORE | EDC_START_SYS | EDC_START_RMON |
					EDC_START_ALL))) {
		return EDC_ERROR_UNKNOWN_CMD;
	}

	asm volatile("msync");
	return 0;
}

int32_t edc_stop(uint32_t cmd)
{
	uint32_t value;
	DEBUG_PRINT("SWIM EDC: Function = %s, Line = %i, Cmd= %lx\n",
		__func__, __LINE__, cmd);

	/* Stop core monitoring */
	if (cmd & EDC_STOP_CORE) {
		/* Retrieve core PMGC0 */
		READ_CPMC(value, 400);
		value |= CPMON_PMGC0_FAC;
		SET_CPMC(value, 400);
	}

	/* Stop System Monitoring */
	if (cmd & EDC_STOP_SYS)
		G_pmon_regs[SPMON_PMGC0] |= SPMON_PMGC0_FAC;

	/* Stop TSEC RMON counters */
	if (cmd & EDC_STOP_RMON) {
		/* disable RMON */
		if (G_etsec1_rmon_ctrl_reg) *G_etsec1_rmon_ctrl_reg &= ~RMON_STEN;
		if (G_etsec2_rmon_ctrl_reg) *G_etsec2_rmon_ctrl_reg &= ~RMON_STEN;
		if (G_etsec3_rmon_ctrl_reg) *G_etsec3_rmon_ctrl_reg &= ~RMON_STEN;
		if (G_etsec4_rmon_ctrl_reg) *G_etsec4_rmon_ctrl_reg &= ~RMON_STEN;
	}

	if (!(cmd & (EDC_STOP_CORE | EDC_STOP_SYS | EDC_STOP_RMON |
					EDC_STOP_ALL))) {
		return EDC_ERROR_UNKNOWN_CMD;
	}

	asm volatile("msync");
	return 0;
}

int32_t edc_is_counting(void)
{
	/*
	   Check if either core or system FAC bits are clear, i.e.,
	   counters are incrementable.
	   Note that we return 0 if all are frozen, 1 if at least one
	   set of counters are not frozen.
	*/
	int32_t temp;

	READ_CPMC(temp, 400);
	/* Check if core is counting. */
	if ((temp & CPMON_PMGC0_FAC) == 0)
		return 1;

	/* Check if system is counting. */
	if ((G_pmon_regs[SPMON_PMGC0] & SPMON_PMGC0_FAC) == 0)
		return 1;

	return 0;
}

int32_t edc_reset(uint32_t cmd)
{
	uint32_t value = 0;

	DEBUG_PRINT("SWIM EDC: Function = %s, Line = %i, PMON Regs = %lx\n",
		__func__, __LINE__, (uint32_t)G_pmon_regs);

	#if defined(E500) || defined(E500V2)
	/* Reset Core counters */
	if (cmd & EDC_RESET_CORE) {
		/* Clear SPR16, 17, 18, 19 */
		SET_CPMC(value, 16);
		SET_CPMC(value, 17);
		SET_CPMC(value, 18);
		SET_CPMC(value, 19);
	}

	/* Reset System counters */
	if (cmd & EDC_RESET_SYS) {
		int32_t i;

		if (TOTAL_SPMCCB) {
			/* For CCB counter */
			G_pmon_regs[SPMON_PMC0u] = 0;
			G_pmon_regs[SPMON_PMC0l] = 0;
		}

		for (i = 0; i <= TOTAL_SPMC; i++)
			G_pmon_regs[SPMON_PMC(i)] = 0; /* clear counter */
	}

	/* Reset TSEC RMON counters */
	if (cmd & EDC_RESET_RMON) {
		/* clear counters (self-resetting bit) */
		if (G_etsec1_rmon_ctrl_reg) *G_etsec1_rmon_ctrl_reg |= RMON_CLRCNT;
		if (G_etsec2_rmon_ctrl_reg) *G_etsec2_rmon_ctrl_reg |= RMON_CLRCNT;
		if (G_etsec3_rmon_ctrl_reg) *G_etsec3_rmon_ctrl_reg |= RMON_CLRCNT;
		if (G_etsec4_rmon_ctrl_reg) *G_etsec4_rmon_ctrl_reg |= RMON_CLRCNT;
	}

	#else
	#error Architecure Unknown.
	return EDC_ERROR_UNKNOWN_ARCH;

	#endif /* defined(E500) || defined(E500V2) */

	if (!(cmd & (EDC_RESET_CORE | EDC_RESET_SYS | EDC_RESET_RMON |
					EDC_RESET_ALL))) {
		return EDC_ERROR_UNKNOWN_CMD;
	}

	asm volatile ("msync");
	return 0;
} /* int32_t edc_reset(uint32_t cmd) */

int32_t edc_config(uint32_t cmd, uint32_t *write_buffer)
{
	int32_t err_msg;
	DEBUG_PRINT("SWIM EDC: Function = %s, Line = %i, Cmd = %lx\n",
		__func__, __LINE__, cmd);

	/* FIX: I added support for EDC_CONFIG_SET _CORE and _SYSTEM here and
	   put the core/sys config code in two new functions. This function
	   also will now return an error value if something is wrong. Please
	   double check over the new implementation to make sure it is correct
	*/
	switch (cmd) {
	case EDC_CONFIG_SET_ALL:
		err_msg = edc_config_core(&write_buffer);
		if (err_msg != EDC_NO_ERROR) {
			asm volatile ("msync");
			return err_msg;
		}

		err_msg = edc_config_system(&write_buffer);
		if (err_msg == EDC_NO_ERROR)
			asm volatile ("msync");

		err_msg = edc_config_rmon(&write_buffer);
		if (err_msg == EDC_NO_ERROR)
			asm volatile ("msync");
		break;

	case EDC_CONFIG_SET_CORE:
		err_msg = edc_config_core(&write_buffer);
		if (err_msg == EDC_NO_ERROR)
			asm volatile ("msync");
		break;

	case EDC_CONFIG_SET_SYS:
		err_msg = edc_config_system(&write_buffer);
		if (err_msg == EDC_NO_ERROR)
			asm volatile ("msync");
		break;

	case EDC_CONFIG_SET_RMON:
		err_msg = edc_config_rmon(&write_buffer);
		if (err_msg == EDC_NO_ERROR)
			asm volatile ("msync");
		break;

	/* FIX: Returned an error code for the default case. May want to */
	/* print an error message as well. */
	default:
		err_msg = EDC_ERROR_UNKNOWN_CMD;
		break;
	} /* switch (cmd) */

	return err_msg;
} /* int32_t edc_config(uint32_t cmd, uint32_t* write_buffer) */

/* Core PerfMon configuration
**   - setup PMGC0
**   - setup PMLCa0-a3 and PMLCb0-b3
*/
/* FIX: New function, please make sure it is implemented correctly. Note that
   asm volatile ("msync") is performed in the parent edc_config funciton. */
int32_t edc_config_core(uint32_t **ptr_write_buffer)
{
	uint32_t *write_buffer;
	#if defined(E500) || defined(E500V2)

	write_buffer = *ptr_write_buffer;

	SET_CPMC(*write_buffer++, 400); /* Core PMGC0 */
	SET_CPMC(*write_buffer++, 144); /* Set reg144; PMLCa0 */
	SET_CPMC(*write_buffer++, 272); /* Set reg272; PMLCb0 */
	SET_CPMC(*write_buffer++, 145); /* Set reg145; PMLCa1 */
	SET_CPMC(*write_buffer++, 273); /* Set reg273; PMLCb1 */
	SET_CPMC(*write_buffer++, 146); /* Set reg146; PMLCa2 */
	SET_CPMC(*write_buffer++, 274); /* Set reg274; PMLCb2 */
	SET_CPMC(*write_buffer++, 147); /* Set reg147; PMLCa3 */
	SET_CPMC(*write_buffer++, 275); /* Set reg275; PMLCb3 */

	*ptr_write_buffer = write_buffer;
	return EDC_NO_ERROR;

	#else
	#error Architecture unknown.
	return EDC_ERROR_UNKNOWN_ARCH;

	#endif /* defined(E500) || defined(E500V2) */
}

/* System PerfMon configuration
**   - setup PMGC0
**   - setup PMLCa0-a9 and PMLCb0-b9 (MPC8548)
*/
/* FIX: New function, please make sure it is implemented correctly. Note that
   asm volatile ("msync") is performed in the parent edc_config funciton. */
int32_t edc_config_system(uint32_t **ptr_write_buffer)
{
	uint32_t i;
	uint32_t *write_buffer;

	write_buffer = *ptr_write_buffer;

	#if defined(E500) || defined(E500V2)
	G_pmon_regs[SPMON_PMGC0] = *write_buffer++;
	/* CLARRIFY: Sys Counter ~ freeze, no overflow */
	for (i = 0; i < TOTAL_SPMC; i++) {
		G_pmon_regs[SPMON_PMLCA(i)] = *write_buffer++;
		G_pmon_regs[SPMON_PMLCB(i)] = *write_buffer++;
	}

	*ptr_write_buffer = write_buffer;
	return EDC_NO_ERROR;

	#else
	#error Architecture unknown.
	return EDC_ERROR_UNKNOWN_ARCH;

	#endif /* defined(E500) || defined(E500V2) */
}

/* RMON offset configuration
**   - fill in the array of offsets to RMON counters for the read function
*/
int32_t edc_config_rmon(uint32_t **ptr_write_buffer)
{
	uint32_t i, block_offset, block_id, rmon_offset;
	uint32_t *write_buffer;

	write_buffer = *ptr_write_buffer;

	for (i = 0; i < TOTAL_RMON; i++) {
		block_id    = *write_buffer++;
		rmon_offset = *write_buffer++;
		if (block_id < 1 || block_id > 4 || rmon_offset < 0x600 ||
			rmon_offset > 0x7FF) {
			/* error Invalid TSEC block ID and/or rmon offset */
			return EDC_ERROR_UNKNOWN_ARCH;
		} else {
			block_offset = (block_id - 1) * RMON_TSECn_OFFSET;
			G_rmon_offset[i] = block_offset + rmon_offset;
		}
	}

	*ptr_write_buffer = write_buffer;
	return EDC_NO_ERROR;
}

extern int num_etsec;
int32_t edc_read(uint32_t count, uint32_t *read_buffer)
{
	/* CLARIFY: Explain the comment below. If count is not going to be used
	   in the near future (before SWIM 1.0 release), it should be removed */
	/* ignore count in this version */
	uint32_t temp;
	uint32_t i, j = 0;

	DEBUG_PRINT("SWIM EDC: Function = %s, Line = %i\n",
		__func__, __LINE__);

	#if defined(E500) || defined(E500V2)
	READ_CPMC(temp, 16);
	*(read_buffer+j++) = temp;
	READ_CPMC(temp, 17);
	*(read_buffer+j++) = temp;
	READ_CPMC(temp, 18);
	*(read_buffer+j++) = temp;
	READ_CPMC(temp, 19);
	*(read_buffer+j++) = temp;

	/* SPMC0 upper and lower parts of CCB counter */
	if (TOTAL_SPMCCB) {
		*(read_buffer+j++) = G_pmon_regs[SPMON_PMC0u];
		*(read_buffer+j++) = G_pmon_regs[SPMON_PMC0l];
	}
	/* SPMC1-SPMCn */
	for (i = 1; i <= TOTAL_SPMC; i++)
		*(read_buffer+j++) = G_pmon_regs[SPMON_PMC(i)];

	/* RMON registers */
	if (num_etsec > 0) *(read_buffer+j++) = *((uint32_t *)(G_rmon_base+G_rmon_offset[0]));
	DEBUG_PRINT("G_rmon_base+G_rmon_offset[0] = 0x%x\n",
			G_rmon_base+G_rmon_offset[0]);
	DEBUG_PRINT("TSEC_ID (for MPC8548 expecting 0x1240000) = 0x%x\n",
			*((uint32_t *)G_rmon_base));

	if (num_etsec > 1) *(read_buffer+j++) = *((uint32_t *)(G_rmon_base+G_rmon_offset[1]));
	if (num_etsec > 2) *(read_buffer+j++) = *((uint32_t *)(G_rmon_base+G_rmon_offset[2]));
	if (num_etsec > 3) *(read_buffer+j++) = *((uint32_t *)(G_rmon_base+G_rmon_offset[3]));
	if (num_etsec > 4) *(read_buffer+j++) = *((uint32_t *)(G_rmon_base+G_rmon_offset[4]));
	if (num_etsec > 5) *(read_buffer+j++) = *((uint32_t *)(G_rmon_base+G_rmon_offset[5]));
	if (num_etsec > 6) *(read_buffer+j++) = *((uint32_t *)(G_rmon_base+G_rmon_offset[6]));
	if (num_etsec > 7) *(read_buffer+j++) = *((uint32_t *)(G_rmon_base+G_rmon_offset[7]));

	/* return number of bytes read out */
	/* ???: Should this be sizeof(uint64_t) for 64-bit architectures? */
	j *= sizeof(uint32_t);
	return j;

	#else
		#error Architecture unknown.
		return EDC_ERROR_UNKNOWN_ARCH;
	#endif /* defined(E500) || defined(E500V2) */
} /* int32_t edc_read(uint32_t count, uint32_t* read_buffer) */

int32_t edc_read_config(uint32_t count, uint32_t *read_buffer)
{
	/* ???: Explain the comment below. If count is not going to be used in,
	   the near future (before SWIM 1.0 release), it should be removed */
	/* ignore count in this version */
	uint32_t temp;
	uint32_t i;
	int32_t j = 0;

	DEBUG_PRINT("SWIM EDC: Function = %s, Line = %i\n",
		__func__, __LINE__);

	/* CMME: State what the following block does */
	READ_CPMC(temp, 400);
	*(read_buffer+j++);
	READ_CPMC(temp, 144);
	*(read_buffer+j++);
	READ_CPMC(temp, 145);
	*(read_buffer+j++);
	READ_CPMC(temp, 146);
	*(read_buffer+j++);
	READ_CPMC(temp, 147);
	*(read_buffer+j++);

	/* CMME: State what the following block does */
	*(read_buffer+j++) = G_pmon_regs[SPMON_PMGC0];
	for (i = 1; i <= TOTAL_SPMC; i++) {
		*(read_buffer+j++) = G_pmon_regs[SPMON_PMLCA(i)];
		*(read_buffer+j++) = G_pmon_regs[SPMON_PMLCB(i)];
	}

	return j;
} /* int32_t edc_read_config(uint32_t count, uint32_t* read_buffer) */

/* 8548 System PerfMon ctrl register:
**   Core   PMGC0, PMLCA0-3, PMLCB0-3 = 9
**   System PMGC0, PMLCA0-9, PMLCB0-9 = 21
**   Total = 30
** 85xx System PerfMon ctrl register:
** CLARIFY why this is stated twice: 8548 System PerfMon ctrl register:
**   Core   PMGC0, PMLCA0-3, PMLCB0-3 = 9
**   System PMGC0, PMLCA0-8, PMLCB0-8 = 19
**   Total 28
*/
int32_t edc_reg_count(void)
{
	DEBUG_PRINT("SWIM EDC: Function = %s, Line = %i\n",
		__func__, __LINE__);

	return TOTAL_CTL_REGS + 2*TOTAL_RMON;
} /* int32_t edc_reg_count(void) */

uint32_t edc_get_pvr(void)
{
	uint32_t pvr_val = 0;
	/* 287 = Number for PVR SPR */
	asm volatile ("mfspr  %0, 287;" : "=r"(pvr_val));
	return pvr_val;
}

uint32_t edc_get_svr(void)
{
	uint32_t svr_val = 0;
	/* 1023 = Number for SVR SPR */
	asm volatile ("mfspr  %0, 1023;" : "=r"(svr_val));
	return svr_val;
}
