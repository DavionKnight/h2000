
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <fcntl.h> 
#include <sys/ioctl.h>
#include <unistd.h>

#include "global_dec.h"
#include "deal_fpga.h"

/* notes : ------------------------------------------------------------------------------ */
/*
	lc		:	local
	rm		:	remote
	ic		:	fpga
	rt		:	realtime
	sp		:	space
	sgl		:	single
	reg		:	register
*/

/* global variables */

/*
	NOTE: 
		In fact, the IC(fpga) is accessed ONLY with 32-bits width !!!
		So, addresses, data and lengths used here are all based on 32 bits !!!
*/

const uint 				ic_size		= 0x00004000;			// 0x00010000 bytes
static int				fpga_dev	= -1;


/* loop && nrt read parameters */
#define IC_RD_BUF_WIDTH					6		// bits
#define IC_RD_BLK_WIDTH					5		// bits
#define IC_RD_BUF_NUM_MAX				0x40
#define IC_NL_RD_BUF_NUM				0x40	// no real read
#define IC_L1_RD_BUF_NUM				0x40	// loop 1 read
#define IC_L2_RD_BUF_NUM				0x40	// loop 2 read
#define IC_L3_RD_BUF_NUM				0x20	// loop 3 read
#define IC_RT_RD_BUF_NUM				0x10	// real read
#define IC_RT_WR_BUF_NUM				0x40	// real write

typedef union
{
	struct
	{
		uint		type	: 3;
		uint		off		: 11;
		uint		addr	: 18;
	}	sgl;

	struct
	{
		uint		type	: 3;
		uint		buf		: IC_RD_BUF_WIDTH;
		uint		len		: IC_RD_BLK_WIDTH;
		uint		addr	: 18;
	}	blk;

	uint		whole;
}	ic_rd_op;

typedef struct
{
	ushort		addr;
	ushort		data;
}	fpga_reg;
#define	FPGA_REG_WR		_IOW('F', 1, unsigned long)
#define	FPGA_REG_RD		_IOR('F', 2, unsigned long)


/* fpga read instruction cache */
/* The order MUST be same with the enum ic_op_type !!! */
ic_rd_op			ic_rd_icache[ic_op_end][IC_RD_BUF_NUM_MAX];

/* The order MUST be same with the enum ic_op_type !!! */
const ic_seg_sp ic_op_sp[ic_op_end]	= 	{
	/* non-realtime read operation space */
	{
		{ 0x00000400, IC_NL_RD_BUF_NUM  },
		{ 0x00000400, 0x800  },					// size bases on 16-bits
	},

	/* loop read 1 operation space */
	{
		{ 0x00000800, IC_L1_RD_BUF_NUM  },
		{ 0x00000800, 0x800  },					// size bases on 16-bits
	},
	
	/* loop read 2 operation space */
	{
		{ 0x00000C00, IC_L2_RD_BUF_NUM  },
		{ 0x00000C00, 0x800  },					// size bases on 16-bits
	},

	/* realtime read operation space */	
	{
		{ 0x00001000, IC_RT_RD_BUF_NUM  },
		{ 0x00001000, 0x100  },					// size bases on 16-bits
	},

	/* realtime write operation space */
	{
		{ 0x00001400, IC_RT_WR_BUF_NUM  },		// size bases on 16-bits
		{ 0x00001400, 0  },
	},

	/* loop read 3 operation space */
	{
		{ 0x00001800, IC_L3_RD_BUF_NUM  },		// size bases on 16-bits
		{ 0x00001800, 0x400  },
	}
};


/* ---------------------------------------------------------------------------------- */
/*
	fpga_init
	must be called in first !!!
	return:
		0,		success
		-1,		fail to open the driver
*/
int fpga_init(void)
{
#ifdef SIMU_FPGA
	fpga_dev	= 200;
#else
	fpga_dev	= open( "/dev/fpga_rw_drv", O_RDWR );
	if( fpga_dev == -1 )
		return -1;
#endif

	return 0;
}


/* ---------------------------------------------------------------------------------- */
/*
	fpga_close
	must be called in first !!!
	return:
		0,		success
		-1,		the driver was not opened
*/
int fpga_close(void)
{
#ifndef SIMU_FPGA
	if( fpga_dev == -1 )
		return -1;
	close( fpga_dev );
#endif
	fpga_dev	= -1;
	return 0;
}



/* ---------------------------------------------------------------------------------- */
/*
	read any registers of the fpga, directly
	return:
		>=0,	success
		-1,		fail to open the driver
		-2,		some parameters are out of the valid range
 */
int fpga_rd( uint * pdata, uint addr, int len )
{
	int			i;
	ushort		base_addr;
	fpga_reg	reg[2];

#ifdef SIMU_FPGA
	return len;
#endif

	if( fpga_dev == -1 )
		return -1;
	if( addr >= ic_size )
		return -2;

	for( i = 0; i < len; i++ )
	{
		base_addr	= addr << 1;
		reg[0].addr	= (unsigned short)(base_addr);
		reg[1].addr	= (unsigned short)(base_addr + 1);
		ioctl( fpga_dev, FPGA_REG_RD, (unsigned long)(&reg[0]) );
		ioctl( fpga_dev, FPGA_REG_RD, (unsigned long)(&reg[1]) );
		pdata[i]	= ((unsigned int)reg[1].data << 16) | reg[0].data;

		addr++;
	}

	return len;
}


/* ---------------------------------------------------------------------------------- */
/*
	write any registers of the fpga, directly
	return:
		0,		success
		-1,		fail to open the driver
		-2,		some parameters are out of the valid range
*/
int fpga_wr( uint addr, uint * pdata, int len )
{
	int			i;
	ushort		base_addr;
	fpga_reg	reg[2];

#ifdef SIMU_FPGA
	return 0;
#endif

	if( fpga_dev == -1 )
		return -1;
	if( addr >= ic_size )
		return -2;

	for( i = 0; i < len; i++ )
	{
		base_addr		= addr << 1;
		reg[0].addr		= (unsigned short)(base_addr);
		reg[0].data		= (unsigned short)(pdata[i]);
		reg[1].addr		= (unsigned short)(base_addr + 1);
		reg[1].data		= (unsigned short)(pdata[i] >> 16);
		ioctl( fpga_dev, FPGA_REG_WR, (unsigned long)(&reg[0]) );
		ioctl( fpga_dev, FPGA_REG_WR, (unsigned long)(&reg[1]) );

		addr++;
	}

	return 0;
}

/* ---------------------------------------------------------------------------------- */
/*
	realtime write, for remote registers
	input:
		addr	: base on 16-bits
		data	: bases on 16-bits
	return:
		0,		sucess
		-1,		fail to open the driver
		-2,		some parameters are out of the valid range
*/
int fpga_rm_wr( uint addr, ushort data )
{
	uint			cardslot;
	uint			rmdata;

	if( fpga_dev == -1 )
		return -1;

	/* the address is 18-bits, and the highest 4-bits is sent on the MPC address bus A5~A2
		the lowest 14-bits is sent with the data : 
		the MPC data bus = | RS-2bits | ADDR - 14bits | DATA - 16bits | */

	cardslot	= (addr >> 14) & 0x0F;
	rmdata		= ((addr & 0x3FFF) << 16) | data;
	fpga_wr( ic_op_sp[ic_rt_wr].opr.base + cardslot, &rmdata, 1 );

	// must delay 5 usec for the FPGA dealing with the instruction
	delay_usec( 5 );

	return 0;
}


/* ---------------------------------------------------------------------------------- */
/*
	return the maxisize of a data block in the data buffer.
*/
uint fpga_blk_size( ic_op_type opt )
{
	return (ic_op_sp[opt].buf.size / ic_op_sp[opt].opr.size);	
}

/* ---------------------------------------------------------------------------------- */
/*
	read setting, for remote registers
		op		: ic_op_type 		< ic_rt_wr //read type over !
		cache	: the instruction register's position, and it is which read buffer segment( 0 ~ IC_RD_BUF_NUM-1 )
		len		: the length of the block (based on 16bits !!!) ( maxisize = fpga_blk_size( opt ) )
					0,		disable
					1,		single
					>1,		block
		off		: NO use
		addr	: the base of the registers
	return:
		>0,		success
		-1,		fail to open the driver
		-2,		some parameters are out of the valid range
*/
int fpga_rm_rd_set( ic_op_type opt, uint cache, uint len, uint off, uint addr )
{
	ic_rd_op			* oper;
	const ic_seg_sp		* sp;
	uint		        seg_size;

	if( fpga_dev == -1 )
		return -1;
	if( (opt >= ic_rt_wr && opt != ic_rd_lp3) || cache >= ic_op_sp[opt].opr.size )
		return -2;

	/* get pointers */
	oper		= &ic_rd_icache[opt][cache];
	sp			= &ic_op_sp[opt];
	seg_size	= fpga_blk_size( opt );		/* base on 16-bits */

	/* organize the operator */
	if( len == 0 )
	{
		oper->sgl.type	= 0;
		oper->sgl.off	= off;
		oper->sgl.addr	= addr;
	}
	else if( len == 1 )
	{
		oper->sgl.type	= 1;
		oper->sgl.off	= seg_size*cache;
		oper->sgl.addr	= addr;
	}
	else
	{
		if( len > seg_size )
			return -2;
		oper->blk.type	= 2;
		oper->blk.buf	= cache;
		oper->blk.len	= len - 1;
		oper->blk.addr	= addr;
	}
	/* set */
	fpga_wr(sp->opr.base + cache, &oper->whole, 1 );

	return len;
}


/* ---------------------------------------------------------------------------------- */
/*
	read instruction state
		op		: ic_op_type 		< ic_rt_wr //read type over !
		cache	: the instruction register's position, and it is which read buffer segment( 0 ~ IC_RD_BUF_NUM-1 )
		* plen	: return the length of the block (based on 16bits !!!) ( maxisize = fpga_blk_size( opt ) )
					0,		disable
					1,		single
					>1,		block
		* poff	: NO use
		* paddr	: return the base of the registers
		* pwhole: return the whole uint
	return:
		>0,		read len, base on 16-bits
		== 0, 	disable
		-1,		fail to open the driver
		-2,		some parameters are out of the valid range
*/
int fpga_rm_rd_state( ic_op_type opt, uint cache, uint * plen, uint * poff, uint * paddr, uint * pwhole )
{
	ic_rd_op			* oper;
	const ic_seg_sp		* sp;

	if( fpga_dev == -1 )
		return -1;
	if( (opt >= ic_rt_wr && opt != ic_rd_lp3) || cache >= ic_op_sp[opt].opr.size )
		return -2;
	if( plen == NULL )
		return -3;

	/* get pointers */
	oper		= &ic_rd_icache[opt][cache];
	sp			= &ic_op_sp[opt];
	if( pwhole != NULL )
	{
		*pwhole	= oper->whole;
	}

	/* organize the operator && get */
	if( poff != NULL )
	{
		*poff	= oper->sgl.off;
	}
	if( paddr != NULL )
	{
		*paddr	= oper->sgl.addr;
	}

	if( oper->sgl.type == 1 )
	{
		*plen	= 1;
		return 1;
	}
	else if( oper->blk.type == 2 )
	{
		*plen	= oper->blk.len + 1;
		return *plen;
	}
	else
	{
		*plen	= 0;
	}

	return 0;
}

/* ---------------------------------------------------------------------------------- */
/*
	read a block of bytes, for remote registers
		op		: ic_op_type 		< ic_rt_wr //read type over !
		cache	: the instruction register's position, and it is which read buffer segment( 0 ~ IC_RD_BUF_NUM-1 )
		pdata	: data
	return:
		>0,		read len, base on 16-bits
		== 0, 	disable
		-1,		fail to open the driver
		-2,		some parameters are out of the valid range
*/
int fpga_rm_rd( ic_op_type opt, uint cache, ushort * pdata )
{
	ic_rd_op			* oper;
	const ic_seg_sp		* sp;
	int					seg_size;
	uint		        i, tmp, len, reg_num;
	uint				pic;

	if( fpga_dev == -1 )
		return -1;
	if( (opt >= ic_rt_wr && opt != ic_rd_lp3) || cache >= ic_op_sp[opt].opr.size || pdata == NULL )
		return -2;

	/* get pointers */
	oper		= &ic_rd_icache[opt][cache];
	sp			= &ic_op_sp[opt];
	seg_size	= fpga_blk_size( opt );		/* base on 16-bits */
	tmp			= 0;

	/* organize the operator && get */
	if( oper->sgl.type == 1 )
	{
		/* the off bases on 16-bits */
		reg_num		= oper->sgl.off >> 1;
		fpga_rd( &tmp, sp->buf.base + reg_num, 1 );
		pdata[0]	= (oper->sgl.off & 1) ? ((ushort)(tmp >> 16)) : ((ushort)tmp);

		return 1;
	}
	else if( oper->blk.type == 2 )
	{
	    /* the off and the len base on 16-bits */
	    len			= oper->blk.len + 1;
	    reg_num		= len >> 1;
	    pic			= sp->buf.base + ((cache*seg_size) >> 1);

		for( i = 0; i < reg_num; i++ )
		{
			fpga_rd( &tmp, pic, 1 );
			*pdata++	= (ushort)(tmp);
			*pdata++	= (ushort)(tmp >> 16);
			pic++;
		}
	    if( len & 1 )
	    {
	    	fpga_rd( &tmp, pic, 1 );
			*pdata++	= (ushort)(tmp);
			pic++;
	    }

		return len;
	}

	return 0;
}


int fpga_rm_rd_byte( ic_op_type opt, uint cache, uchar * pdata )
{
	ic_rd_op			* oper;
	const ic_seg_sp		* sp;
	int					seg_size;
	uint		        i, tmp, len, reg_num;
	uint				pic;

	if( fpga_dev == -1 )
		return -1;
	if( (opt >= ic_rt_wr && opt != ic_rd_lp3) || cache >= ic_op_sp[opt].opr.size || pdata == NULL )
		return -2;

	/* get pointers */
	oper		= &ic_rd_icache[opt][cache];
	sp			= &ic_op_sp[opt];
	seg_size	= fpga_blk_size( opt );		/* base on 16-bits */
	tmp			= 0;

	/* organize the operator && get */
	if( oper->sgl.type == 1 )
	{
		/* the off bases on 16-bits */
		reg_num		= oper->sgl.off >> 1;
		fpga_rd( &tmp, sp->buf.base + reg_num, 1 );
		if( oper->sgl.off & 1 )
		{
			*pdata++	= (uchar)(tmp >> 16 );
			*pdata++	= (uchar)(tmp >> 24 );
		}
		else
		{
			*pdata++	= (uchar)(tmp );
			*pdata++	= (uchar)(tmp >>  8 );
		}

		return 2;
	}
	else if( oper->blk.type == 2 )
	{
	    /* the off and the len base on 16-bits */
	    len			= oper->blk.len + 1;
	    reg_num		= len >> 1;
	    pic			= sp->buf.base + ((cache*seg_size) >> 1);
		for( i = 0; i < reg_num; i++ )
		{
			fpga_rd( &tmp, pic, 1 );
			*pdata++	= (uchar)(tmp );
			*pdata++	= (uchar)(tmp >>  8);
			*pdata++	= (uchar)(tmp >> 16);
			*pdata++	= (uchar)(tmp >> 24);
			pic++;
		}
	    if( len & 2 )
	    {
	    	fpga_rd( &tmp, pic, 1 );
			*pdata++	= (uchar)(tmp );
			*pdata++	= (uchar)(tmp >>  8);
			pic++;
	    }

		return (len << 1);
	}

	return 0;
}
