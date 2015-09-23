/*
 * CAAM/SEC 4.x transport/backend driver (prototype)
 * JobQ backend functionality
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
 *
 */

#include "compat.h"
#include "regs.h"
#include "jq.h"
#include "desc.h"
#include "intern.h"

/* Main per-queue interrupt handler */
irqreturn_t caam_jq_interrupt(int irq, void *st_dev)
{
	struct device *dev = st_dev;
	struct caam_drv_private_jq *jqp = dev_get_drvdata(dev);
	u32 irqstate;

	/*
	 * Check the output ring for ready responses, kick
	 * tasklet if jobs done.
	 */
	irqstate = rd_reg32(&jqp->qregs->jqintstatus);
	if (!irqstate)
		return IRQ_NONE;

	/*
	 * If JobQ error, we got more development work to do
	 * Flag a bug now, but we really need to shut down and
	 * restart the queue (and fix code).
	 */
	if (irqstate & JQINT_JQ_ERROR) {
		dev_err(dev, "job ring error: irqstate: %08x\n", irqstate);
		BUG();
	}

	/* mask valid interrupts */
	setbits32(&jqp->qregs->qconfig_lo, JQCFG_IMSK);

	/* Have valid interrupt at this point, just ACK and trigger */
	wr_reg32(&jqp->qregs->jqintstatus, irqstate);

	preempt_disable();
	if (napi_schedule_prep(per_cpu_ptr(jqp->irqtask, smp_processor_id())))
		__napi_schedule(per_cpu_ptr(jqp->irqtask, smp_processor_id()));
	preempt_enable();

	return IRQ_HANDLED;
}

/* Deferred service handler, run as interrupt-fired tasklet */
int caam_jq_dequeue(struct napi_struct *napi, int budget)
{
	int hw_idx, sw_idx, i, head, tail;
	struct device *dev = &napi->dev->dev;
	struct caam_drv_private_jq *jqp = dev_get_drvdata(dev);
	void (*usercall)(struct device *dev, u32 *desc, u32 status, void *arg);
	u32 *userdesc, userstatus;
	void *userarg;
	unsigned long flags;
	u8 count = 0, ret = 1;

	spin_lock_irqsave(&jqp->outlock, flags);

	head = ACCESS_ONCE(jqp->head);
	tail = jqp->tail;

	while (CIRC_CNT(head, tail, JOBQ_DEPTH) >= 1 &&
	       rd_reg32(&jqp->qregs->outring_used) && (count < budget)) {

		hw_idx = jqp->out_ring_read_index;
		for (i = 0; CIRC_CNT(head, tail + i, JOBQ_DEPTH) >= 1; i++) {
			sw_idx = (tail + i) & (JOBQ_DEPTH - 1);

			smp_read_barrier_depends();

			if (jqp->outring[hw_idx].desc ==
			    jqp->entinfo[sw_idx].desc_addr_dma)
				break; /* found */
		}
		/* we should never fail to find a matching descriptor */
		BUG_ON(CIRC_CNT(head, tail + i, JOBQ_DEPTH) <= 0);

		/* Unmap just-run descriptor so we can post-process */
		dma_unmap_single(dev, jqp->outring[hw_idx].desc,
				 jqp->entinfo[sw_idx].desc_size,
				 DMA_TO_DEVICE);

		/* mark completed, avoid matching on a recycled desc addr */
		jqp->entinfo[sw_idx].desc_addr_dma = 0;

		/* Stash callback params for use outside of lock */
		usercall = jqp->entinfo[sw_idx].callbk;
		userarg = jqp->entinfo[sw_idx].cbkarg;
		userdesc = jqp->entinfo[sw_idx].desc_addr_virt;
		userstatus = jqp->outring[hw_idx].jqstatus;

		smp_mb();

		jqp->out_ring_read_index = (jqp->out_ring_read_index + 1) &
					   (JOBQ_DEPTH - 1);

		/*
		 * if this job completed out-of-order, do not increment
		 * the tail.  Otherwise, increment tail by 1 plus the
		 * number of subsequent jobs already completed out-of-order
		 */
		if (sw_idx == tail) {
			do {
				tail = (tail + 1) & (JOBQ_DEPTH - 1);
				smp_read_barrier_depends();
			} while (CIRC_CNT(head, tail, JOBQ_DEPTH) >= 1 &&
				 jqp->entinfo[tail].desc_addr_dma == 0);

			jqp->tail = tail;
		}

		/* set done */
		wr_reg32(&jqp->qregs->outring_rmvd, 1);

		spin_unlock_irqrestore(&jqp->outlock, flags);

		/* Finally, execute user's callback */
		usercall(dev, userdesc, userstatus, userarg);

		spin_lock_irqsave(&jqp->outlock, flags);

		head = ACCESS_ONCE(jqp->head);
		tail = jqp->tail;
		count++;
	}

	if (CIRC_CNT(head, tail, JOBQ_DEPTH) >= 1)
		ret = 1;

	if (count < budget) {
		napi_complete(per_cpu_ptr(jqp->irqtask, smp_processor_id()));
		clrbits32(&jqp->qregs->qconfig_lo, JQCFG_IMSK);
		ret = 0;
	}

	spin_unlock_irqrestore(&jqp->outlock, flags);

	return ret;
}

/**
 * caam_jq_register() - Alloc a queue for someone to use as needed. Returns
 * an ordinal of the queue allocated, else returns -ENODEV if no queues
 * are available.
 * @ctrldev: points to the controller level dev (parent) that
 *           owns queues available for use.
 * @dev:     points to where a pointer to the newly allocated queue's
 *           dev can be written to if successful.
 *
 * NOTE: this scheme needs re-evaluated in the 1.2+ timeframe
 **/
int caam_jq_register(struct device *ctrldev, struct device **qdev)
{
	struct caam_drv_private *ctrlpriv;
	struct caam_drv_private_jq *jqpriv;
	unsigned long flags;
	int q;

	jqpriv = NULL;
	ctrlpriv = dev_get_drvdata(ctrldev);


	/* Lock, if free queue - assign, unlock */
	spin_lock_irqsave(&ctrlpriv->jq_alloc_lock, flags);
	for (q = 0; q < ctrlpriv->total_jobqs; q++) {
		jqpriv = dev_get_drvdata(ctrlpriv->jqdev[q]);
		if (jqpriv->assign == JOBQ_UNASSIGNED) {
			jqpriv->assign = JOBQ_ASSIGNED;
			*qdev = ctrlpriv->jqdev[q];
			spin_unlock_irqrestore(&ctrlpriv->jq_alloc_lock, flags);
			return q;
		}
	}

	/* If assigned, write dev where caller needs it */
	spin_unlock_irqrestore(&ctrlpriv->jq_alloc_lock, flags);
	*qdev = NULL;
	return -ENODEV;
}
EXPORT_SYMBOL(caam_jq_register);

/**
 * caam_jq_deregister() - Deregister an API and release the queue.
 * Returns 0 if OK, -EBUSY if queue still contains pending entries
 * or unprocessed results at the time of the call
 * @dev     - points to the dev that identifies the queue to
 *            be released.
 **/
int caam_jq_deregister(struct device *qdev)
{
	struct caam_drv_private_jq *jqpriv = dev_get_drvdata(qdev);
	struct caam_drv_private *ctrlpriv;
	unsigned long flags;

	/* Get the owning controller's private space */
	ctrlpriv = dev_get_drvdata(jqpriv->parentdev);

	/*
	 * Make sure queue empty before release
	 */
	if ((jqpriv->qregs->outring_used) ||
	    (jqpriv->qregs->inpring_avail != JOBQ_DEPTH))
		return -EBUSY;

	/* Release queue */
	spin_lock_irqsave(&ctrlpriv->jq_alloc_lock, flags);
	jqpriv->assign = JOBQ_UNASSIGNED;
	spin_unlock_irqrestore(&ctrlpriv->jq_alloc_lock, flags);

	return 0;
}
EXPORT_SYMBOL(caam_jq_deregister);

/**
 * caam_jq_enqueue() - Enqueue a job descriptor head. Returns 0 if OK,
 * -EBUSY if the queue is full, -EIO if it cannot map the caller's
 * descriptor.
 * @dev:  device of the job queue to be used. This device should have
 *        been assigned prior by caam_jq_register().
 * @desc: points to a job descriptor that execute our request. All
 *        descriptors (and all referenced data) must be in a DMAable
 *        region, and all data references must be physical addresses
 *        accessible to CAAM (i.e. within a PAMU window granted
 *        to it).
 * @cbk:  pointer to a callback function to be invoked upon completion
 *        of this request. This has the form:
 *        callback(struct device *dev, u32 *desc, u32 stat, void *arg)
 *        where:
 *        @dev:    contains the job queue device that processed this
 *                 response.
 *        @desc:   descriptor that initiated the request, same as
 *                 "desc" being argued to caam_jq_enqueue().
 *        @status: untranslated status received from CAAM. See the
 *                 reference manual for a detailed description of
 *                 error meaning, or see the JQSTA definitions in the
 *                 register header file
 *        @areq:   optional pointer to an argument passed with the
 *                 original request
 * @areq: optional pointer to a user argument for use at callback
 *        time.
 **/
 int caam_jq_enqueue(struct device *dev, u32 *desc,
		    void (*cbk)(struct device *dev, u32 *desc,
				u32 status, void *areq),
		    void *areq)
{
	struct caam_drv_private_jq *jqp = dev_get_drvdata(dev);
	struct caam_jqentry_info *head_entry;
	unsigned long flags;
	int head, tail, desc_size;
	dma_addr_t desc_dma;

	desc_size = (*desc & HDR_JD_LENGTH_MASK) * sizeof(u32);
	desc_dma = dma_map_single(dev, desc, desc_size, DMA_TO_DEVICE);
	if (dma_mapping_error(dev, desc_dma)) {
		dev_err(dev, "caam_jq_enqueue(): can't map jobdesc\n");
		return -EIO;
	}

	spin_lock_irqsave(&jqp->inplock, flags);

	head = jqp->head;
	tail = ACCESS_ONCE(jqp->tail);

	if (!rd_reg32(&jqp->qregs->inpring_avail) ||
	    CIRC_SPACE(head, tail, JOBQ_DEPTH) <= 0) {
		spin_unlock_irqrestore(&jqp->inplock, flags);
		dma_unmap_single(dev, desc_dma, desc_size, DMA_TO_DEVICE);
		return -EBUSY;
	}

	head_entry = &jqp->entinfo[head];
	head_entry->desc_addr_virt = desc;
	head_entry->desc_size = desc_size;
	head_entry->callbk = (void *)cbk;
	head_entry->cbkarg = areq;
	head_entry->desc_addr_dma = desc_dma;

	jqp->inpring[jqp->inp_ring_write_index] = desc_dma;

	smp_wmb();

	jqp->inp_ring_write_index = (jqp->inp_ring_write_index + 1) &
				    (JOBQ_DEPTH - 1);
	jqp->head = (head + 1) & (JOBQ_DEPTH - 1);

	wmb();

	wr_reg32(&jqp->qregs->inpring_jobadd, 1);

	spin_unlock_irqrestore(&jqp->inplock, flags);

	return 0;
}
EXPORT_SYMBOL(caam_jq_enqueue);

static int caam_reset_hw_jq(struct device *dev)
{
	struct caam_drv_private_jq *jqp = dev_get_drvdata(dev);
	unsigned int timeout = 100000;

	/* initiate flush (required prior to reset) */
	wr_reg32(&jqp->qregs->jqcommand, JQCR_RESET);
	while (((rd_reg32(&jqp->qregs->jqintstatus) & JQINT_ERR_HALT_MASK) ==
		JQINT_ERR_HALT_INPROGRESS) && --timeout)
		cpu_relax();

	if ((rd_reg32(&jqp->qregs->jqintstatus) & JQINT_ERR_HALT_MASK) !=
	    JQINT_ERR_HALT_COMPLETE || timeout == 0) {
		dev_err(dev, "failed to flush job queue %d\n", jqp->qidx);
		return -EIO;
	}

	/* initiate reset */
	timeout = 100000;
	wr_reg32(&jqp->qregs->jqcommand, JQCR_RESET);
	while ((rd_reg32(&jqp->qregs->jqcommand) & JQCR_RESET) && --timeout)
		cpu_relax();

	if (timeout == 0) {
		dev_err(dev, "failed to reset job queue %d\n", jqp->qidx);
		return -EIO;
	}

	return 0;
}

/*
 * Init JobQ independent of platform property detection
 */
int caam_jq_init(struct device *dev)
{
	struct caam_drv_private_jq *jqp;
	dma_addr_t inpbusaddr, outbusaddr;
	int i, error;

	jqp = dev_get_drvdata(dev);

	setbits32(&jqp->qregs->qconfig_lo, JQCFG_IMSK);
	error = caam_reset_hw_jq(dev);
	if (error)
		return error;

	clrbits32(&jqp->qregs->qconfig_lo, JQCFG_IMSK);

	jqp->inpring = kzalloc(sizeof(dma_addr_t) * JOBQ_DEPTH,
			       GFP_KERNEL | GFP_DMA);
	jqp->outring = kzalloc(sizeof(struct jq_outentry) *
			       JOBQ_DEPTH, GFP_KERNEL | GFP_DMA);

	jqp->entinfo = kzalloc(sizeof(struct caam_jqentry_info) * JOBQ_DEPTH,
			       GFP_KERNEL);

	if ((jqp->inpring == NULL) || (jqp->outring == NULL) ||
	    (jqp->entinfo == NULL)) {
		dev_err(dev, "can't allocate job rings for %d\n",
			jqp->qidx);
		return -ENOMEM;
	}

	for (i = 0; i < JOBQ_DEPTH; i++)
		jqp->entinfo[i].desc_addr_dma = !0;

	/* Setup rings */
	inpbusaddr = dma_map_single(dev, jqp->inpring,
				    sizeof(u32 *) * JOBQ_DEPTH,
				    DMA_BIDIRECTIONAL);
	if (dma_mapping_error(dev, inpbusaddr)) {
		dev_err(dev, "caam_jq_init(): can't map input ring\n");
		kfree(jqp->inpring);
		kfree(jqp->outring);
		kfree(jqp->entinfo);
		return -EIO;
	}

	outbusaddr = dma_map_single(dev, jqp->outring,
				    sizeof(struct jq_outentry) * JOBQ_DEPTH,
				    DMA_BIDIRECTIONAL);
	if (dma_mapping_error(dev, outbusaddr)) {
		dev_err(dev, "caam_jq_init(): can't map output ring\n");
			dma_unmap_single(dev, inpbusaddr,
					 sizeof(u32 *) * JOBQ_DEPTH,
					 DMA_BIDIRECTIONAL);
		kfree(jqp->inpring);
		kfree(jqp->outring);
		kfree(jqp->entinfo);
		return -EIO;
	}

	jqp->inp_ring_write_index = 0;
	jqp->out_ring_read_index = 0;
	jqp->head = 0;
	jqp->tail = 0;

	wr_reg64(&jqp->qregs->inpring_base, inpbusaddr);
	wr_reg64(&jqp->qregs->outring_base, outbusaddr);
	wr_reg32(&jqp->qregs->inpring_size, JOBQ_DEPTH);
	wr_reg32(&jqp->qregs->outring_size, JOBQ_DEPTH);

	jqp->ringsize = JOBQ_DEPTH;

	spin_lock_init(&jqp->inplock);
	spin_lock_init(&jqp->outlock);

	/* Select interrupt coalescing parameters */
	setbits32(&jqp->qregs->qconfig_lo, JOBQ_INTC |
		  (JOBQ_INTC_COUNT_THLD << JQCFG_ICDCT_SHIFT) |
		  (JOBQ_INTC_TIME_THLD << JQCFG_ICTT_SHIFT));

	/* Connect job queue interrupt handler. */
	jqp->irqtask = alloc_percpu(struct napi_struct);
	if (jqp->irqtask == NULL) {
		dev_err(dev, "can't allocate memory while connecting job"
			" queue interrupt handler\n");
		kfree(jqp->inpring);
		kfree(jqp->outring);
		kfree(jqp->entinfo);

		return -ENOMEM;
	}

	jqp->net_dev = alloc_percpu(struct net_device);
	if (jqp->net_dev == NULL) {
		dev_err(dev, "can't allocate memory while connecting job"
			" queue interrupt handler\n");
		kfree(jqp->inpring);
		kfree(jqp->outring);
		kfree(jqp->entinfo);
		free_percpu(jqp->irqtask);

		return -ENOMEM;
	}

	for_each_possible_cpu(i) {
		(per_cpu_ptr(jqp->net_dev, i))->dev = *dev;
		INIT_LIST_HEAD(&per_cpu_ptr(jqp->net_dev, i)->napi_list);
		netif_napi_add(per_cpu_ptr(jqp->net_dev, i),
				per_cpu_ptr(jqp->irqtask, i),
				caam_jq_dequeue, CAAM_NAPI_WEIGHT);
		napi_enable(per_cpu_ptr(jqp->irqtask, i));
	}

	error = request_irq(jqp->irq, caam_jq_interrupt, IRQF_SHARED,
			    "caam-jobq", dev);
	if (error) {
		dev_err(dev, "can't connect JobQ %d interrupt (%d)\n",
			jqp->qidx, jqp->irq);
		irq_dispose_mapping(jqp->irq);
		jqp->irq = 0;
		dma_unmap_single(dev, inpbusaddr, sizeof(u32 *) * JOBQ_DEPTH,
				 DMA_BIDIRECTIONAL);
		dma_unmap_single(dev, outbusaddr, sizeof(u32 *) * JOBQ_DEPTH,
				 DMA_BIDIRECTIONAL);
		kfree(jqp->inpring);
		kfree(jqp->outring);
		kfree(jqp->entinfo);
		return -EINVAL;
	}

	jqp->assign = JOBQ_UNASSIGNED;
	return 0;
}

/*
 * Shutdown JobQ independent of platform property code
 */
int caam_jq_shutdown(struct device *dev)
{
	struct caam_drv_private_jq *jqp = dev_get_drvdata(dev);
	int ret, i;

	ret = caam_reset_hw_jq(dev);

	for_each_possible_cpu(i) {
		napi_disable(per_cpu_ptr(jqp->irqtask, i));
		netif_napi_del(per_cpu_ptr(jqp->irqtask, i));
	}

	free_percpu(jqp->irqtask);
	free_percpu(jqp->net_dev);

	/* Release interrupt */
	free_irq(jqp->irq, dev);

	/* Free rings */
	dma_unmap_single(dev, (u32)jqp->outring,
			 sizeof(struct jq_outentry) * JOBQ_DEPTH,
			 DMA_BIDIRECTIONAL);
	dma_unmap_single(dev, (u32)jqp->inpring, sizeof(u32 *) * JOBQ_DEPTH,
			 DMA_BIDIRECTIONAL);
	kfree(jqp->outring);
	kfree(jqp->inpring);
	kfree(jqp->entinfo);

	return ret;
}

/*
 * Probe routine for each detected JobQ subsystem. It assumes that
 * property detection was picked up externally.
 */
int caam_jq_probe(struct of_device *ofdev,
		  struct device_node *np,
		  int q)
{
	struct device *ctrldev, *jqdev;
	struct of_device *jq_ofdev;
	struct caam_drv_private *ctrlpriv;
	struct caam_drv_private_jq *jqpriv;
	u32 *jqoffset;
	int error;

	ctrldev = &ofdev->dev;
	ctrlpriv = dev_get_drvdata(ctrldev);

	jqpriv = kmalloc(sizeof(struct caam_drv_private_jq),
			 GFP_KERNEL);
	if (jqpriv == NULL) {
		dev_err(ctrldev, "can't alloc private mem for job queue %d\n",
			q);
		return -ENOMEM;
	}
	jqpriv->parentdev = ctrldev; /* point back to parent */
	jqpriv->qidx = q; /* save queue identity relative to detection */

	/*
	 * Derive a pointer to the detected JobQs regs
	 * Driver has already iomapped the entire space, we just
	 * need to add in the offset to this JobQ. Don't know if I
	 * like this long-term, but it'll run
	 */
	jqoffset = (u32 *)of_get_property(np, "reg", NULL);
	jqpriv->qregs = (struct caam_job_queue *)((u32)ctrlpriv->ctrl +
						  *jqoffset);

	/* Build a local dev for each detected queue */
	jq_ofdev = of_platform_device_create(np, NULL, ctrldev);
	if (jq_ofdev == NULL) {
		kfree(jqpriv);
		return -EINVAL;
	}
	jqdev = &jq_ofdev->dev;
	dev_set_drvdata(jqdev, jqpriv);
	ctrlpriv->jqdev[q] = jqdev;

	/* Identify the interrupt */
	jqpriv->irq = of_irq_to_resource(np, 0, NULL);

	/* Now do the platform independent part */
	error = caam_jq_init(jqdev); /* now turn on hardware */
	if (error) {
		kfree(jqpriv);
		return error;
	}

	return error;
}
