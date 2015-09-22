
#ifndef _FPGA_DEAL_H_
#define _FPGA_DEAL_H_

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

/* type defines ----------------------------------------------------------------------- */
#undef uchar
#define uchar                       unsigned char

#undef ushort
#define ushort                      unsigned short

#undef uint
#define uint                        unsigned int


/* type defines ----------------------------------------------------------------------- */
/* fpga instruction type */
typedef enum { ic_rd_nrt = 0, ic_rd_lp1, ic_rd_lp2, ic_rd_rt
						, ic_rt_wr, ic_rd_lp3, ic_op_end } ic_op_type;
						
/* ic space */
typedef struct
{
    uint           base;			// base on 32-bits !!!
    uint           size;			// size of 32-bits !!!
}	addr_space;

/* fpga operation type */
typedef struct
{
	addr_space	opr;
	addr_space	buf;
}	ic_seg_sp;

extern const ic_seg_sp ic_op_sp[ic_op_end];
/* ---------------------------------------------------------------------------------- */
/*
	fpga_init
	must be called in first !!!
	return:
		0,		success
		-1,		fail to open the driver
*/
int fpga_init(void);

/* ---------------------------------------------------------------------------------- */
/*
	fpga_close
	must be called in first !!!
	return:
		0,		success
		-1,		the driver was not opened
*/
int fpga_close(void);

/* ---------------------------------------------------------------------------------- */
/*
    read any registers of the fpga, directly
	return:
		>=0,	success
		-1,		fail to open the driver
		-2,		some parameters are out of the valid range
 */
int fpga_rd( uint * pdata, uint addr, int len );

/* ---------------------------------------------------------------------------------- */
/*
	write any registers of the fpga, directly
	return:
		0,		success
		-1,		fail to open the driver
		-2,		some parameters are out of the valid range
*/
int fpga_wr( uint addr, uint * pdata, int len );

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
int fpga_rm_wr( uint addr, ushort data );

/* ---------------------------------------------------------------------------------- */
/*
	return the maxisize of a data block in the data buffer.
*/
uint fpga_blk_size( ic_op_type opt );

/* ---------------------------------------------------------------------------------- */
/*
	read setting, for remote registers
		op		: ic_op_type 		< ic_rt_wr //read type over !
		cache	: the instruction register's position, and it is which read buffer segment( 0 ~ IC_RD_BUF_NUM-1 )
		len		: the length of the block (based on 16bits !!!) ( maxisize = sp->buf.size / sp->opr.size )
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
int fpga_rm_rd_set( ic_op_type opt, uint cache, uint len, uint off, uint addr );

/* ---------------------------------------------------------------------------------- */
/*
	read instruction state
		op		: ic_op_type 		< ic_rt_wr //read type over !
		cache	: the instruction register's position, and it is which read buffer segment( 0 ~ IC_RD_BUF_NUM-1 )
		* plen	: return the length of the block (based on 16bits !!!) ( maxisize = sp->buf.size / sp->opr.size )
					0,		disable
					1,		single
					>1,		block
		* poff	: NO use
		* paddr	: return the base of the registers
		* pwhole: return the whole uint
	return:
		>0,		success
		-1,		fail to open the driver
		-2,		some parameters are out of the valid range
*/
int fpga_rm_rd_state( ic_op_type opt, uint cache, uint * plen, uint * poff, uint * paddr, uint * pwhole );

/* ---------------------------------------------------------------------------------- */
/*
	read a block of bytes, for remote registers
		op		: ic_op_type 		< ic_rt_wr //read type over !
		cache	: the instruction register's position, and it is which read buffer segment( 0 ~ IC_RD_BUF_NUM-1 )
		pdata	: data
	return:
		>0,		read len, base on 16-bits
		-1,		fail to open the driver
		-2,		some parameters are out of the valid range
*/
int fpga_rm_rd( ic_op_type opt, uint cache, ushort * pdata );
int fpga_rm_rd_byte( ic_op_type opt, uint cache, uchar * pdata );

#endif
