/*
 * Copyright (C) 2007-2009 Freescale Semiconductor, Inc. All rights reserved.
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
/*
 * Author: Roy Pledge, Roy.Pledge@freescale.com
 *
 * Description:
 * This file implements zero-copy mapping of freebuffer output into user-space
 * for the pattern-matcher driver.
 */

#include "user_private.h"

/* This file contains the implementation of methods used to map freebuffer
 * memory into user space. */

/* This is the structure that is attached to VMAs to
 * keep track of the usage count of a freebuff */
struct pme_fb_vma {
	/* The fbchain this node is mapping */
	struct pme_fbchain *buffers;
	atomic_t ref_count;
	enum fb_vma_type {
		fb_page_mapped, /* contigous, full page mapping */
		fb_phys_mapped /* physical address mapping */
	} type;
	/* The mapped size is used to perform
	 * the do_munmap */
	size_t mapped_size;
	/* Memory for the iovec, if needed */
	struct page *iovec_pages;
	size_t kmem_size;
	/* Pre faulted page pointers */
	struct page **page_array;
	int page_count;
};

/* Validate mmap output */
static inline int check_mmap_result(void *address)
{
	/* do_mmap returns either an address or an -errno.
	 * Null is returned if we try and mmap size 0
	 * Not likely to happen in production code */
	if (IS_ERR(address))
		return PTR_ERR(address);
	return 0;
}

/* Create a mem node */
static struct pme_fb_vma *fb_vma_create(struct pme_fbchain *buffers,
					enum fb_vma_type type,
					int inital_count, size_t mapped_size,
					size_t kmem_size,
					unsigned int page_array_size)
{
	struct page *p;
	int order, num_pages;
	/* Allocate enough space for the node structure, as well
	 * as any page* we need to keep track of */
	struct pme_fb_vma *result = vmalloc(sizeof(struct pme_fb_vma) +
			(page_array_size * sizeof(struct page *)));
	if (unlikely(!result))
		return NULL;

	atomic_set(&result->ref_count, inital_count);
	result->type = type;
	result->buffers = buffers;
	result->mapped_size = mapped_size;
	if (page_array_size) {
		result->page_array = (struct page **) (result + 1);
		result->page_count = page_array_size;
	} else {
		result->page_array = NULL;
		result->page_count = 0;
	}
	if (kmem_size) {
		order = get_order(kmem_size);
		result->iovec_pages = alloc_pages(GFP_KERNEL, order);
		if (!result->iovec_pages) {
			vfree(result);
			return NULL;
		}
		result->kmem_size = kmem_size;
		/* In the case of higher order allocations increase the page
		 * ref counts */
		if (order) {
			p = result->iovec_pages;
			num_pages = 1 << order;
			while (num_pages--)
				get_page(p++);
		}
	} else
		result->iovec_pages = NULL;
	return result;
}

/* Cleanup a mapping */
static void fb_vma_free(struct pme_fb_vma *node)
{
	int order, num_pages;
	struct page *p;
	if (atomic_dec_and_test(&node->ref_count)) {
		/* This buffer can be recycled
		 * (Buffers can be NULL in the case where
		 * the mapped area is an iovec structure) */
		if (node->buffers)
			pme_fbchain_recycle(node->buffers);

		if (node->iovec_pages) {
			order = get_order(node->kmem_size);
			if (order) {
				p = node->iovec_pages;
				num_pages = 1 << order;
				while (num_pages--) {
					put_page_testzero(p);
					p++;
				}
			}
			__free_pages(node->iovec_pages, order);
		}
		vfree(node);
	}
}

/* Map a freebuffer chain into a processes virtual address space
 * when the buffers are a multiple of PAGE_SIZE */
static int fb_to_user_page_size(struct file *filep,
				struct pme_fbchain *buffers,
				unsigned long *user_addr,
				size_t *size)
{
	struct vm_area_struct *vma;
	int index, ret;
	void *data;
	size_t data_size;
	struct pme_fb_vma *mem_node;

	int list_count = pme_fbchain_num(buffers);
	/* These buffers are page aligned and occupy
	 * complete pages.  This means we can mmap it all at once */
	*size = list_count * pme_fbchain_max(buffers);

	/* We need to lock the mmap_sem because other threads
	 * could be modifying the address space layout */
	down_write(&current->mm->mmap_sem);

	*user_addr = do_mmap(filep, 0, *size,
			     PROT_READ | PROT_WRITE, MAP_PRIVATE, 0);
	ret = check_mmap_result((void *) *user_addr);
	if (ret)
		goto err;

	/* Lookup the new VMA and stuff the fbchain into
	 * it so when a page fault occurs, we can find the
	 * proper page and return it */
	vma = find_vma(current->mm, (unsigned long) *user_addr);

	mem_node = vma->vm_private_data = fb_vma_create(buffers,
			fb_page_mapped, 1, *size, 0,
			(*size + PAGE_SIZE - 1) / PAGE_SIZE);
	if (!mem_node) {
		ret = -ENOMEM;
		/* Make sure we clean the mapped area out of
		 * the users process space */
		 do_munmap(current->mm, (*user_addr) & PAGE_MASK,
			 *size + offset_in_page(*user_addr));
		goto err;
	}
	/* Pre compute the page* for each page in the buffer.  This step makes
	 * the nopage implementation easy as we have already determined
	 * which page* to return */
	index = 0;
	data = pme_fbchain_current(buffers);
	data_size = pme_fbchain_current_bufflen(buffers);
	while (data_size) {
		while (data_size) {
			mem_node->page_array[index] = virt_to_page(data);
			index++;
			if (data_size > PAGE_SIZE) {
				data_size -= PAGE_SIZE;
				data += PAGE_SIZE;
			} else
				data_size = 0;
		}
		data = pme_fbchain_next(buffers);
		data_size = pme_fbchain_current_bufflen(buffers);
	}
	up_write(&current->mm->mmap_sem);
	/* Re-adjust the size to be the actual data length of the buffer */
	*size = pme_fbchain_length(buffers);
	return PME_MEM_CONTIG;
err:
	up_write(&current->mm->mmap_sem);
	return ret;
}

/* Maps an FBChain of length 1 into user space */
static int map_fb_to_user_contig(struct file *filep,
				struct pme_fbchain *buffers,
				unsigned long *user_addr, size_t *size)
{
	void *data;
	size_t data_size;
	int ret;
	struct vm_area_struct *vma;
	struct pme_fb_vma *mem_node;

	/* The resulting FB Chain is a single, non multiple of PAGE_SIZE buffer.
	 * Map it into RAM */
	data = pme_fbchain_current(buffers);
	data_size = pme_fbchain_current_bufflen(buffers);

	mem_node = fb_vma_create(buffers, fb_phys_mapped, 1, data_size, 0, 0);
	if (!mem_node)
		return -ENOMEM;

	down_write(&current->mm->mmap_sem);

	*user_addr = (unsigned long) do_mmap(filep, 0,
					     data_size + offset_in_page(data),
					     PROT_READ | PROT_WRITE,
					     MAP_PRIVATE,
					     virt_to_phys(data) & PAGE_MASK);

	ret = check_mmap_result((void *) *user_addr);
	if (ret)
		goto err;

	vma = find_vma(current->mm, (unsigned long) *user_addr);
	vma->vm_private_data = mem_node;
	up_write(&current->mm->mmap_sem);

	*user_addr += offset_in_page(data);
	*size = data_size;

	return PME_MEM_CONTIG;
err:
	up_write(&current->mm->mmap_sem);
	fb_vma_free(mem_node);
	return -EINVAL;
}

/* Map a non page aligned fbchain into user space.  This
 * requires creating a iovec and populating it correctly */
static int map_fb_to_user_sg(struct file *filep, struct pme_fbchain *buffers,
				unsigned long *user_addr, size_t *size)
{
	void *data;
	size_t data_size;
	struct iovec *vect;
	int vector_size, ret, list_count, index = 0;
	unsigned long paddr;
	struct vm_area_struct *vma;
	struct pme_fb_vma *mem_node, *iovec_mem_node;
	list_count = pme_fbchain_num(buffers);

	vector_size = sizeof(struct iovec) * list_count;
	iovec_mem_node = fb_vma_create(NULL, fb_phys_mapped, 1, list_count,
			vector_size, 0);
	if (!iovec_mem_node)
		return -ENOMEM;

	/* The space for the iovec is allocate as whole pages and
	 * a kernel mapping needs to be created in case they were
	 * allocated from high mem */
	vect = kmap(iovec_mem_node->iovec_pages);
	/* Create a mem node to keep track of the fbchain
	 * Otherwise, we won't know when to release the freebuff list */
	mem_node = fb_vma_create(buffers, fb_phys_mapped, 0, 0, 0, 0);
	if (!mem_node) {
		fb_vma_free(iovec_mem_node);
		kunmap(iovec_mem_node->iovec_pages);
		return -ENOMEM;
	}
	/* For each freebuff, map it to user space, storing the
	 * userspace data in the iovec */
	data = pme_fbchain_current(buffers);

	down_write(&current->mm->mmap_sem);

	while (data) {
		data_size = pme_fbchain_current_bufflen(buffers);
		vect[index].iov_base = (void *) do_mmap(filep, 0,
							data_size +
							offset_in_page(data),
							PROT_READ | PROT_WRITE,
							MAP_PRIVATE,
							virt_to_phys(data) &
							PAGE_MASK);
		ret = check_mmap_result(vect[index].iov_base);
		if (ret)
			/*  Need to unmap any previous sucesses */
			goto err;

		vma = find_vma(current->mm,
				(unsigned long) vect[index].iov_base);

		vma->vm_private_data = mem_node;
		atomic_inc(&mem_node->ref_count);

		vect[index].iov_base += offset_in_page(data);
		vect[index].iov_len = data_size;
		++index;
		data = pme_fbchain_next(buffers);
	}

	/* Now map the iovec into user spcae */
	paddr = page_to_pfn(iovec_mem_node->iovec_pages) << PAGE_SHIFT;
	*user_addr = (unsigned long) do_mmap(filep, 0,
					     vector_size +
					     offset_in_page(paddr),
					     PROT_READ |
					     PROT_WRITE, MAP_PRIVATE,
					     paddr & PAGE_MASK);

	ret = check_mmap_result((void *) *user_addr);
	if (ret)
		goto err;

	vma = find_vma(current->mm, (unsigned long) *user_addr);

	vma->vm_private_data = iovec_mem_node;

	up_write(&current->mm->mmap_sem);
	*user_addr += offset_in_page(paddr);
	*size = list_count;
	kunmap(iovec_mem_node->iovec_pages);
	return PME_MEM_SG;
err:
	while (index--)
		do_munmap(current->mm,
			((unsigned long)vect[index].iov_base) & PAGE_MASK,
			 vect[index].iov_len +
			 offset_in_page(vect[index].iov_base));

	up_write(&current->mm->mmap_sem);
	kunmap(iovec_mem_node->iovec_pages);
	return -EINVAL;
}

/* Map an fbchain into user space
 * This function merely determines the type of fbchain we are
 * dealing with and calls one of the 3 varients above */
int pme_mem_fb_map(struct file *filep, struct pme_fbchain *buffers,
			unsigned long *user_addr, size_t *size)
{
	int page_sized = !(pme_fbchain_max(buffers) % PAGE_SIZE);

	int list_count = pme_fbchain_num(buffers);
	if (!list_count)
		return -EINVAL;

	/* We can't allow the mapping of buffers that are not page sized
	 * but are greater than one page in size to user space, because
	 * the page counts are only managed for the first page in a
	 * high order allocation. */

	if (!page_sized && pme_fbchain_max(buffers) > PAGE_SIZE)
		return -EINVAL;
	if (page_sized)
		return fb_to_user_page_size(filep, buffers, user_addr, size);
	else if (list_count > 1)
		/* This will expose additional memory area to user space */
		return map_fb_to_user_sg(filep, buffers, user_addr, size);
	else
		return map_fb_to_user_contig(filep, buffers, user_addr, size);
}

/* Releases a SG list from user space */
static int unmap_sglist_from_user(struct iovec *vector,
				  unsigned long user_vec, int size)
{
	int i, ret;
	for (i = 0; i < size; i++) {
		ret = do_munmap(current->mm,
			((unsigned long)vector[i].iov_base) & PAGE_MASK,
			vector[i].iov_len + offset_in_page(vector[i].iov_base));
		if (ret)
			return ret;
	}
	return do_munmap(current->mm, user_vec & PAGE_MASK,
			 size + offset_in_page(user_vec));
}

/* Unmaps an fbchain from user space */
int pme_mem_fb_unmap(struct file *filep, unsigned long user_addr)
{
	int ret;
	struct pme_fb_vma *mem_node;
	struct vm_area_struct *vma;
	void *iovec_mem;

	down_write(&current->mm->mmap_sem);
	vma = find_vma(current->mm, user_addr);
	if (!vma) {
		ret = -EINVAL;
		goto done;
	}
	/* Get the type from the node */
	mem_node = vma->vm_private_data;
	if (mem_node) {
		if (!mem_node->iovec_pages)
			ret = do_munmap(current->mm, user_addr & PAGE_MASK,
				mem_node->mapped_size +
				offset_in_page(user_addr));
		else {
			iovec_mem = kmap(mem_node->iovec_pages);
			ret = unmap_sglist_from_user(iovec_mem,
					user_addr, mem_node->mapped_size);
			kunmap(mem_node->iovec_pages);
		}
	} else
		ret = -EINVAL;
done:
	up_write(&current->mm->mmap_sem);
	return ret;
}

/* New fault method instead of nopage */
static int pme_mem_fops_fault(struct vm_area_struct *vma,
				       struct vm_fault *vmf)
{
	struct page *pageptr;
	unsigned long offset, physaddr, pageframe;
	struct pme_fb_vma *mem_node = vma->vm_private_data;
	int index = 0;
	if (!mem_node)
		return -1;
	if (mem_node->type == fb_phys_mapped) {
		/* Memory is mapped using the physical address method*/
		offset = vma->vm_pgoff << PAGE_SHIFT;
		physaddr = (unsigned long)vmf->virtual_address - vma->vm_start +
					offset;
		pageframe = physaddr >> PAGE_SHIFT;
		pageptr = pfn_to_page(pageframe);
	} else {
		/* There is a buffer list attached to this
		 * VMA, meaning every entry of the buffer
		 * list is page aligned.  We need to look
		 * up the proper page and return it */
		offset = (unsigned long)vmf->virtual_address - vma->vm_start;
		index = offset / PAGE_SIZE;
		if (index >= mem_node->page_count)
			return -1;
		pageptr = mem_node->page_array[index];
	}
	get_page(pageptr);
	vmf->page = pageptr;
	vmf->flags |= VM_FAULT_MINOR;
	return 0;
}

/* Called when a process forks and a vma struct is copied
 * We need to add a reference to the area so that we know
 * when the fbchain can be recycled */
void pme_mem_fops_open(struct vm_area_struct *vma)
{
	struct pme_fb_vma *fb_vma = vma->vm_private_data;
	if (fb_vma)
		atomic_inc(&fb_vma->ref_count);
}

/* Called when an area is unmapped from used space
 * Decrease the reference count and recycle if needed */
void pme_mem_fops_close(struct vm_area_struct *vma)
{
	if (vma->vm_private_data)
		fb_vma_free(vma->vm_private_data);
}

/* Operation set for vm_areas that are mapping
 * pm memory */
struct vm_operations_struct pme_mem_vm_ops = {
	/* PB: .nopage = pme_mem_fops_nopage,*/
	.fault = pme_mem_fops_fault,
	.open = pme_mem_fops_open,
	.close = pme_mem_fops_close
};

int pme_mem_fops_mmap(struct file *filep, struct vm_area_struct *vma)
{
	/* Nothing to do except adjust the operations */
	/* User space mmaps will succeed, but since the
	 * private_data field of any vm_area mapped
	 * by the user will be NULL when no_page is called,
	 * after this, any derefernce will return a Bus Error */
	vma->vm_flags |= VM_RESERVED;
	vma->vm_ops = &pme_mem_vm_ops;
	return 0;
}

/* Helpers to copy fbchains to user space
 * These basically implement the copy_to_user
 * function for freebuffers */
size_t pme_mem_fb_copy_to_user(void *user_data,
			    size_t user_data_len,
			    struct pme_fbchain *fbchain, size_t *offset)
{
	size_t result = 0, buffer_size, amount;
	void *source_data = pme_fbchain_current(fbchain);
	while (source_data) {
		buffer_size = pme_fbchain_current_bufflen(fbchain);
		if ((buffer_size - *offset) < (user_data_len - result)) {
			/* We can copy all of this buffer */
			if (copy_to_user(user_data + result,
					 source_data + *offset,
					 buffer_size - *offset))
				return -EFAULT;
			result += buffer_size - *offset;
		} else if ((buffer_size - *offset) > (user_data_len - result)) {
			/* We need to copy what we can and adjust the offset of
			 * the chain */
			amount = user_data_len - result;
			if (copy_to_user(user_data + result,
					 source_data + *offset, amount))
				return -EFAULT;
			result += amount;
			*offset += amount;
			return result;
		} else {
			/* The buffer fits the remaining user space
			 * perfectly and the buffer in the chain */
			if (copy_to_user(user_data + result,
					 source_data + *offset,
					 user_data_len - result))
				return -EFAULT;
			result += user_data_len - result;
			pme_fbchain_next(fbchain);
			*offset = 0;
			return result;
		}
		source_data = pme_fbchain_next(fbchain);
		*offset = 0;
	}
	/* All out of data! */
	return result;
}

/* Copy a freebuffer to a user supplied iovec */
size_t pme_mem_fb_copy_to_user_iovec(const struct iovec *user_data,
			     size_t user_data_len,
			     struct pme_fbchain *fbchain, size_t *offset)
{
	/* Copy into an iovec */
	size_t result = 0, ret;
	int i = 0;
	/* While we haven't exhausted the iovec or the chain */
	while (i < user_data_len && pme_fbchain_current(fbchain)) {
		ret = pme_mem_fb_copy_to_user(user_data[i].iov_base,
				user_data[i].iov_len, fbchain, offset);
		if (ret < 0)
			return -EFAULT;
		result += ret;
		i++;
	}
	return result;
}
