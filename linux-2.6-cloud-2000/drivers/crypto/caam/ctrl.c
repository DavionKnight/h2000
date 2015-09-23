/*
 * CAAM control-plane driver backend
 * Controller-level driver, kernel property detection, initialization
 *
 * Copyright (c) 2008-2011 Freescale Semiconductor, Inc.
 * All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Freescale Semiconductor nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Freescale Semiconductor BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "compat.h"
#include "regs.h"
#include "intern.h"
#include "jq.h"
#include "caamxor.h"

/*
 * Use of CONFIG_PPC throughout is used to switch in code
 * native to Power, which is all that's implemented or tested at
 * this time. Of particular note, this turns on large proportion
 * of Power-specific fdt probe and detection code.
 *
 * Future variants of this driver will also be ported to ARM
 * architectures, which will lack the fdt-dependent constructs.
 * Therefore, they will have their own probe/initialization code
 * inserted at that time. For now, CONFIG_PPC will be used to
 * switch in sections that are known fdt-dependent, although
 * these may need to transition to CONFIG_OF if ARM architectures
 * were to take up use of an fdt.
 */

#ifdef CONFIG_PPC

int caam_jq_shutdown(struct device *dev);

#if 0
static int caam_reset_qi(struct device *ctrldev)
{
	struct caam_drv_private *ctrlpriv = dev_get_drvdata(ctrldev);
	unsigned int timeout = 100000;

	/* initiate flush (required prior to reset) */
	wr_reg32(&ctrlpriv->qi->qi_control_lo, QICTL_STOP);
	while ((rd_reg32(&ctrlpriv->qi->qi_status) & QISTA_STOPD) == 0 &&
	       --timeout)
		cpu_relax();

	if ((rd_reg32(&ctrlpriv->qi->qi_status) & QISTA_STOPD) != QISTA_STOPD ||
	    timeout == 0) {
		dev_err(ctrldev, "failed to flush queue interface\n");
		return -EIO;
	}

	/* initiate reset */
	timeout = 100000;
	wr_reg32(&ctrlpriv->qi->qi_control_lo, QICTL_STOP);
	while ((rd_reg32(&ctrlpriv->qi->qi_control_lo) & QICTL_STOP) != 0 &&
	       --timeout)
		cpu_relax();

	if (timeout == 0) {
		dev_err(ctrldev, "failed to reset queue interface\n");
		return -EIO;
	}

	return 0;
}
#endif

static int caam_remove(struct of_device *ofdev)
{
	struct device *ctrldev;
	struct caam_drv_private *ctrlpriv;
	struct caam_drv_private_jq *jqpriv;
	int q, ret = 0;

	ctrldev = &ofdev->dev;
	ctrlpriv = dev_get_drvdata(ctrldev);

	/* shut down JobQs */
	for (q = 0; q < ctrlpriv->total_jobqs; q++) {
		ret |= caam_jq_shutdown(ctrlpriv->jqdev[q]);
		jqpriv = dev_get_drvdata(ctrlpriv->jqdev[q]);
		irq_dispose_mapping(jqpriv->irq);
	}

	/* Shut down debug views */
#ifdef CONFIG_DEBUG_FS
	debugfs_remove_recursive(ctrlpriv->dfs_root);
#endif

	/* Unmap controller region */
	iounmap(&ctrlpriv->ctrl);

	kfree(ctrlpriv->jqdev);
	if (ctrlpriv->netcrypto_cache != NULL)
		kmem_cache_destroy(ctrlpriv->netcrypto_cache);

	kfree(ctrlpriv);

	return ret;
}

#ifdef CONFIG_AS_FASTPATH
struct device *asf_caam_dev;

struct device *asf_caam_device(void)
{
	return asf_caam_dev;
}
EXPORT_SYMBOL(asf_caam_device);
#endif

int caam_jq_probe(struct of_device *ofdev,
		  struct device_node *np,
		  int q);

/* Probe routine for CAAM top (controller) level */
static int caam_probe(struct of_device *ofdev,
		      const struct of_device_id *devmatch)
{
	int d, q, qspec;
	struct device *dev;
	struct device_node *nprop, *np;
	struct caam_full *topregs;
	struct caam_drv_private *ctrlpriv;
	u32 caam_id;
	u32 no_of_deco;

	ctrlpriv = kzalloc(sizeof(struct caam_drv_private), GFP_KERNEL);
	if (!ctrlpriv)
		return -ENOMEM;

	dev = &ofdev->dev;
	dev_set_drvdata(dev, ctrlpriv);
	ctrlpriv->ofdev = ofdev;
	nprop = ofdev->dev.of_node;
#ifdef CONFIG_AS_FASTPATH
	asf_caam_dev = dev;
#endif

	/* Get configuration properties from device tree */
	/* First, get register page */
	ctrlpriv->ctrl = of_iomap(nprop, 0);
	if (ctrlpriv->ctrl == NULL) {
		dev_err(dev, "caam: of_iomap() failed\n");
		return -ENOMEM;
	}

	/* topregs used to derive pointers to CAAM sub-blocks only */
	topregs = (struct caam_full *)ctrlpriv->ctrl;

	/* Get the IRQ of the controller (for security violations only) */
	ctrlpriv->secvio_irq = of_irq_to_resource(nprop, 0, NULL);

	/*
	 * Enable DECO watchdogs and, if this is a PHYS_ADDR_T_64BIT kernel,
	 * 36-bit pointers in master configuration register
	 */
	setbits32(&ctrlpriv->ctrl->mcr, MCFGR_WDENABLE |
		  (sizeof(dma_addr_t) == sizeof(u64) ? MCFGR_LONG_PTR : 0));

	if (sizeof(dma_addr_t) == sizeof(u64))
		dma_set_mask(dev, DMA_BIT_MASK(36));

	ctrlpriv->netcrypto_cache = kmem_cache_create("netcrypto_cache",
						MAX_DESC_LEN, 0,
					SLAB_HWCACHE_ALIGN, NULL);
	if (!ctrlpriv->netcrypto_cache) {
		dev_err(dev, "%s: failed to create block cache\n", __func__);
		iounmap(&ctrlpriv->ctrl);
		kfree(ctrlpriv);

		return -ENOMEM;
	}

	caam_id = (rd_reg64(&ctrlpriv->ctrl->perfmon.caam_id)) >> 32;

	/*
	 * Device tree provides no information on the actual number
	 * of DECOs instantiated in the device. Total available will
	 * have to be version-register-derived.
	 *
	 * In practice, their only practical use is as a debug tool.
	 * Since the controller iomaps all possible CAAM registers,
	 * this just calculates handy pointers to deco registers in
	 * pre-mapped space.
	 */
	if (caam_id == P1010_CAAM_BLOCK)
		no_of_deco = 1;

	if (caam_id == P4080_CAAM_BLOCK)
		no_of_deco = 4;

	topregs->deco = kmalloc(no_of_deco * sizeof(struct caam_deco),
				GFP_KERNEL);
	ctrlpriv->deco = kmalloc(no_of_deco * sizeof(struct caam_deco *),
				GFP_KERNEL);

	for (d = 0; d < no_of_deco; d++)
		ctrlpriv->deco[d] = &topregs->deco[d];

	/*
	 * Detect and enable JobQs
	 * First, find out how many queues spec'ed, allocate references
	 * for all, then go probe each one.
	 */
	qspec = 0;
	for_each_compatible_node(np, NULL, "fsl,sec4.0-job-ring")
		qspec++;
	ctrlpriv->jqdev = kzalloc(sizeof(struct device *) * qspec, GFP_KERNEL);
	if (ctrlpriv->jqdev == NULL) {
		iounmap(&ctrlpriv->ctrl);
		return -ENOMEM;
	}

	q = 0;
	ctrlpriv->total_jobqs = 0;
	for_each_compatible_node(np, NULL, "fsl,sec4.0-job-ring") {
		caam_jq_probe(ofdev, np, q);
		ctrlpriv->total_jobqs++;
		q++;
	}

	if (caam_id == P4080_CAAM_BLOCK) {
		const unsigned int *pty;

		/* Check to see if QI present. If so, enable */
		pty = of_get_property(nprop, "fsl,qi-spids", NULL);
		if (pty) {
			ctrlpriv->qi_present = 1;
			ctrlpriv->qi_spids = *pty;
			ctrlpriv->qi = &topregs->qi;
			/* This is all that's reqired to physically enable QI */
			wr_reg32(&ctrlpriv->qi->qi_control_lo, QICTL_DQEN);
		} else {
			ctrlpriv->qi_present = 0;
			ctrlpriv->qi_spids = 0;
		}

		/* If no QI and no queues specified, quit and go home */
		if ((!ctrlpriv->qi_present) && (!ctrlpriv->total_jobqs)) {
			dev_err(dev, "no queues configured, terminating\n");
			caam_remove(ofdev);
			return -ENOMEM;
		}
	}

	/* If no queues specified, quit and go home */
	if (!ctrlpriv->total_jobqs) {
		dev_err(dev, "no queues configured, terminating\n");
		caam_remove(ofdev);
		return -ENOMEM;
	}

	/* NOTE: RTIC detection ought to go here, around Si time */

	/* Initialize queue allocator lock */
	spin_lock_init(&ctrlpriv->jq_alloc_lock);

	/* Report "alive" for developer to see */
	dev_info(dev, "device ID = 0x%016llx\n",
		 rd_reg64(&ctrlpriv->ctrl->perfmon.caam_id));
	dev_info(dev, "job queues = %d\n", ctrlpriv->total_jobqs);
	if (caam_id == P4080_CAAM_BLOCK)
		dev_info(dev, "qi = %d\n", ctrlpriv->qi_present);

#ifdef CONFIG_CRYPTO_DEV_FSL_CAAM_CRYPTO_API
	/* register algorithms with scatterlist crypto API */
	caam_jq_algapi_init(dev);
#endif

#ifdef CONFIG_CRYPTO_DEV_FSL_CAAM_DMAXOR_API
	caam_jq_dma_init(dev);
#endif

#ifdef CONFIG_DEBUG_FS
	/*
	 * FIXME: needs better naming distinction, as some amalgamation of
	 * "caam" and nprop->full_name. The OF name isn't distinctive,
	 * but does separate instances
	 */
	ctrlpriv->dfs_root = debugfs_create_dir("caam", NULL);
	ctrlpriv->ctl = debugfs_create_dir("ctl", ctrlpriv->dfs_root);

	/* Controller-level - performance monitor counters */
	ctrlpriv->ctl_rq_dequeued =
		debugfs_create_u64("rq_dequeued",
				   S_IFCHR | S_IRUSR | S_IRGRP | S_IROTH,
				   ctrlpriv->ctl,
				   &ctrlpriv->ctrl->perfmon.req_dequeued);
	ctrlpriv->ctl_ob_enc_req =
		debugfs_create_u64("ob_rq_encrypted",
				   S_IFCHR | S_IRUSR | S_IRGRP | S_IROTH,
				   ctrlpriv->ctl,
				   &ctrlpriv->ctrl->perfmon.ob_enc_req);
	ctrlpriv->ctl_ib_dec_req =
		debugfs_create_u64("ib_rq_decrypted",
				   S_IFCHR | S_IRUSR | S_IRGRP | S_IROTH,
				   ctrlpriv->ctl,
				   &ctrlpriv->ctrl->perfmon.ib_dec_req);
	ctrlpriv->ctl_ob_enc_bytes =
		debugfs_create_u64("ob_bytes_encrypted",
				   S_IFCHR | S_IRUSR | S_IRGRP | S_IROTH,
				   ctrlpriv->ctl,
				   &ctrlpriv->ctrl->perfmon.ob_enc_bytes);
	ctrlpriv->ctl_ob_prot_bytes =
		debugfs_create_u64("ob_bytes_protected",
				   S_IFCHR | S_IRUSR | S_IRGRP | S_IROTH,
				   ctrlpriv->ctl,
				   &ctrlpriv->ctrl->perfmon.ob_prot_bytes);
	ctrlpriv->ctl_ib_dec_bytes =
		debugfs_create_u64("ib_bytes_decrypted",
				   S_IFCHR | S_IRUSR | S_IRGRP | S_IROTH,
				   ctrlpriv->ctl,
				   &ctrlpriv->ctrl->perfmon.ib_dec_bytes);
	ctrlpriv->ctl_ib_valid_bytes =
		debugfs_create_u64("ib_bytes_validated",
				   S_IFCHR | S_IRUSR | S_IRGRP | S_IROTH,
				   ctrlpriv->ctl,
				   &ctrlpriv->ctrl->perfmon.ib_valid_bytes);

	/* Controller level - global status values */
	ctrlpriv->ctl_faultaddr =
		debugfs_create_u64("fault_addr",
				   S_IFCHR | S_IRUSR | S_IRGRP | S_IROTH,
				   ctrlpriv->ctl,
				   &ctrlpriv->ctrl->perfmon.faultaddr);
	ctrlpriv->ctl_faultdetail =
		debugfs_create_u32("fault_detail",
				   S_IFCHR | S_IRUSR | S_IRGRP | S_IROTH,
				   ctrlpriv->ctl,
				   &ctrlpriv->ctrl->perfmon.faultdetail);
	ctrlpriv->ctl_faultstatus =
		debugfs_create_u32("fault_status",
				   S_IFCHR | S_IRUSR | S_IRGRP | S_IROTH,
				   ctrlpriv->ctl,
				   &ctrlpriv->ctrl->perfmon.status);

	/* Internal covering keys (useful in non-secure mode only) */
	ctrlpriv->ctl_kek_wrap.data = &ctrlpriv->ctrl->kek[0];
	ctrlpriv->ctl_kek_wrap.size = KEK_KEY_SIZE * sizeof(__be32);
	ctrlpriv->ctl_kek = debugfs_create_blob("kek",
						S_IFCHR | S_IRUSR |
						S_IRGRP | S_IROTH,
						ctrlpriv->ctl,
						&ctrlpriv->ctl_kek_wrap);

	ctrlpriv->ctl_tkek_wrap.data = &ctrlpriv->ctrl->tkek[0];
	ctrlpriv->ctl_tkek_wrap.size = KEK_KEY_SIZE * sizeof(__be32);
	ctrlpriv->ctl_tkek = debugfs_create_blob("tkek",
						 S_IFCHR | S_IRUSR |
						 S_IRGRP | S_IROTH,
						 ctrlpriv->ctl,
						 &ctrlpriv->ctl_tkek_wrap);

	ctrlpriv->ctl_tdsk_wrap.data = &ctrlpriv->ctrl->tdsk[0];
	ctrlpriv->ctl_tdsk_wrap.size = KEK_KEY_SIZE * sizeof(__be32);
	ctrlpriv->ctl_tdsk = debugfs_create_blob("tdsk",
						 S_IFCHR | S_IRUSR |
						 S_IRGRP | S_IROTH,
						 ctrlpriv->ctl,
						 &ctrlpriv->ctl_tdsk_wrap);
#endif
	return 0;
}

static struct of_device_id caam_match[] = {
	{
		.compatible = "fsl,sec4.0",
	},
	{},
};
MODULE_DEVICE_TABLE(of, caam_match);

static struct of_platform_driver caam_driver = {
	.driver = {
		.name		= "caam",
		.of_match_table = caam_match,
	},
	.probe       = caam_probe,
	.remove      = __devexit_p(caam_remove),
};

static int __init caam_base_init(void)
{
	return of_register_platform_driver(&caam_driver);
}

static void __exit caam_base_exit(void)
{
	return of_unregister_platform_driver(&caam_driver);
}
#else /* not CONFIG_PPC */
      /* need ARM-specific probe/detect/map/initialize/shutdown */
#endif

module_init(caam_base_init);
module_exit(caam_base_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("FSL CAAM request backend");
MODULE_AUTHOR("Freescale Semiconductor - NMG/STC");
