
#ifndef _PXM_DRV_FPGA_H_
#define _PXM_DRV_FPGA_H_

/* notes : ----------------------------------------------------*/
/*
	lc :	local
	rm :	remote
	ic :	fpga
	rt :	realtime read
	cr :	circle read
*/
extern int			pxm_fpga_init	(void);
extern int			pxm_fpga_close	(void);

/* for local(pxm) ---------------------------------------------*/
extern int			pxm_fpga_lc_rd	(unsigned short addr, unsigned int *data);
extern int 			pxm_fpga_lc_rd_len(unsigned short addr, unsigned int *data, unsigned short len);

extern int			pxm_fpga_lc_wr	(unsigned short addr, unsigned int data);

/* for trib slot ----------------------------------------------*/

extern int			pxm_fpga_rm_cr_rd_set	(int clause, unsigned char slot, unsigned short addr, unsigned int size);
extern int			pxm_fpga_rm_cr_rd_inf	(int clause, unsigned char *slot, unsigned short *addr, unsigned int *size);
extern int			pxm_fpga_rm_cr_en		(int clause);
extern int			pxm_fpga_rm_cr_en_blk	(unsigned short *enbuf, unsigned int size);

/*
 * cir rd data
 *   mode: 0, check status, defaul
 *         1, no check
 */
extern int			pxm_fpga_rm_cr_rd		(int clause, unsigned char slot, unsigned short addr, unsigned short *pbuf, unsigned int size);
extern int			pxm_fpga_rm_rt_rd		(int clause, unsigned char slot, unsigned short addr, unsigned short *pbuf, unsigned int size, int mode);
extern int			pxm_fpga_rm_wr			(unsigned char slot, unsigned short addr, unsigned short *pbuf, unsigned int size);
#endif
