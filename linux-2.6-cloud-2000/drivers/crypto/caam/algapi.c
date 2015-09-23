/*
 * caam - Freescale Integrated Security Engine (SEC) device driver
 *
 * Copyright (c) 2008-2011 Freescale Semiconductor, Inc.
 *
 * Based on talitos Scatterlist Crypto API driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * relationship of job descriptors to shared descriptors (SteveC Dec 10 2008):
 *
 * ---------------                     ---------------
 * | JobDesc #1  |-------------------->|  ShareDesc  |
 * | *(packet 1) |                     |   (PDB)     |
 * ---------------      |------------->|  (hashKey)  |
 *       .              |              | (cipherKey) |
 *       .              |    |-------->| (operation) |
 * ---------------      |    |         ---------------
 * | JobDesc #2  |------|    |
 * | *(packet 2) |           |
 * ---------------           |
 *       .                   |
 *       .                   |
 * ---------------           |
 * | JobDesc #3  |------------
 * | *(packet 3) |
 * ---------------
 *
 * The SharedDesc never changes for a connection unless rekeyed, but
 * each packet will likely be in a different place. So all we need
 * to know to process the packet is where the input is, where the
 * output goes, and what context we want to process with. Context is
 * in the SharedDesc, packet references in the JobDesc.
 *
 * So, a job desc looks like:
 *
 * ---------------------
 * | Header            |
 * | ShareDesc Pointer |
 * | SEQ_OUT_PTR       |
 * | (output buffer)   |
 * | SEQ_IN_PTR        |
 * | (input buffer)    |
 * | LOAD (to DECO)    |
 * ---------------------
 */

#include "compat.h"
#include <linux/percpu.h>

#include "regs.h"
#include "intern.h"
#include "desc.h"
#include "pdb.h"
#include "jq.h"
#include "error.h"
#include "dcl/dcl.h"

/*
 * crypto alg
 */
#define CAAM_CRA_PRIORITY		3000
/* max key is sum of AES_MAX_KEY_SIZE, max split key size */
#define CAAM_MAX_KEY_SIZE		(AES_MAX_KEY_SIZE + \
					 SHA512_DIGEST_SIZE * 2)
/* max IV is max of AES_BLOCK_SIZE, DES3_EDE_BLOCK_SIZE */
#define CAAM_MAX_IV_LENGTH		16

#ifdef DEBUG
/* for print_hex_dumps with line references */
#define xstr(s) str(s)
#define str(s) #s
#define debug(format, arg...) printk(format, arg)
#else
#define debug(format, arg...)
#endif

/*
 * per-session context
 */
struct caam_ctx {
	struct device *dev;
	int class1_alg_type;
	int class2_alg_type;
	int alg_op;
	u8 *key;
	dma_addr_t key_phys;
	unsigned int keylen;
	unsigned int enckeylen;
	unsigned int authkeylen;
	unsigned int split_key_len;
	unsigned int split_key_pad_len;
	unsigned int authsize;
	union {
		struct ipsec_encap_pdb *shared_encap;
		struct ipsec_decap_pdb *shared_decap;
	};
	dma_addr_t shared_desc_phys;
	int shared_desc_len;
	spinlock_t first_lock;
};

/*
 * IPSec ESP Datapath Protocol Override Register (DPOVRD)
 */
struct ipsec_deco_dpovrd {
#define IPSEC_ENCAP_DECO_DPOVRD_USE 0x80
	u8 ovrd_ecn;
	u8 ip_hdr_len;
	u8 nh_offset;
	u8 next_header;	/* reserved if decap */
} __packed;

static DEFINE_PER_CPU(int, cpu_to_job_queue);

static struct ipsec_esp_edesc *crypto_edesc_alloc(int len, int flags,
					struct caam_drv_private *priv)
{
	u32 smp_processor_id = smp_processor_id();
	u32 current_edesc = priv->curr_edesc[smp_processor_id];
	if (unlikely(current_edesc == 0)) {
		return kmem_cache_alloc(priv->netcrypto_cache, flags);
	} else {
		 priv->curr_edesc[smp_processor_id] = current_edesc - 1;
		return priv->edesc_rec_queue[smp_processor_id]
					[current_edesc - 1];
	}
}

static void crypto_edesc_free(struct ipsec_esp_edesc *edesc,
			struct caam_drv_private *priv)
{
	u32 smp_processor_id = smp_processor_id();
	u32 current_edesc = priv->curr_edesc[smp_processor_id];
	if (unlikely(current_edesc == (MAX_RECYCLE_DESC - 1))) {
		kmem_cache_free(priv->netcrypto_cache, edesc);
	} else {
		priv->edesc_rec_queue[smp_processor_id][current_edesc] =
								edesc;
		priv->curr_edesc[smp_processor_id] = current_edesc + 1;
	}
}

static int aead_authenc_setauthsize(struct crypto_aead *authenc,
				    unsigned int authsize)
{
	struct caam_ctx *ctx = crypto_aead_ctx(authenc);
	struct device *dev = ctx->dev;

	debug("setauthsize: authsize %d\n", authsize);

	switch (authsize * 8) {
	case 96:
		if (ctx->alg_op != OP_ALG_ALGSEL_SHA1) {
			dev_err(dev, "h/w doesn't support %d-bit ICV trunc."
				" length with chosen authentication algorithm",
				authsize * 8);
			return -EOPNOTSUPP;
		}
		ctx->class2_alg_type = AUTH_TYPE_IPSEC_SHA1HMAC_96;
		break;
	case 128:
		ctx->class2_alg_type = AUTH_TYPE_IPSEC_SHA2HMAC_256;
		break;
	case 160:
		ctx->class2_alg_type = AUTH_TYPE_IPSEC_SHA1HMAC_160;
		break;
	case 192:
		ctx->class2_alg_type = AUTH_TYPE_IPSEC_SHA2HMAC_384;
		break;
	case 256:
		ctx->class2_alg_type = AUTH_TYPE_IPSEC_SHA2HMAC_256;
		break;
	default:
		dev_err(dev, "unknown auth digest size: %d\n", authsize);
		return -EINVAL;
	}

	ctx->authsize = authsize;

	return 0;
}

static int build_protocol_desc_ipsec_decap(struct caam_ctx *ctx,
					   struct aead_request *req)
{
	struct device *dev = ctx->dev;
	gfp_t flags = req->base.flags & CRYPTO_TFM_REQ_MAY_SLEEP ? GFP_KERNEL :
		      GFP_ATOMIC;
	struct ipsec_decap_pdb *sh_desc;
	int seq_no_offset = offsetof(struct ip_esp_hdr, seq_no);
	void *sh_desc_ptr, *enc_key;
	int endidx;

	/* build shared descriptor for this session */
	sh_desc = kzalloc(sizeof(struct ipsec_decap_pdb) + ctx->split_key_len +
			  ctx->enckeylen + sizeof(struct iphdr),
			  GFP_DMA | flags);
	if (!sh_desc) {
		dev_err(dev, "could not allocate shared descriptor\n");
		return -ENOMEM;
	}

	/* ip hdr len currently fixed */
	sh_desc->ip_hdr_len = sizeof(struct iphdr);

	/* we don't have a next hdr offset */
	sh_desc->ip_nh_offset = 0;

	/*
	 * options: ipv4, beneath crypto api, no real way of
	 * knowing tunnel vs. transport, so we treat tunnel mode
	 * as a special case of transport mode.
	 * linux doesn't support Extended Sequence Numbers
	 * as of time of writing: thus PDBOPTS_ESPCBC_ESN not set.
	 */
	sh_desc->options = 0;

	/* copy Sequence Number
	 * equivalent to:
	 * *spi = *(__be32*)(skb_transport_header(skb) + offset);
	 */
	sh_desc->seq_num = *(__be32 *)((char *)sg_virt(req->assoc) +
			    seq_no_offset);

	/* insert keys, leaving space here for the jump instruction */
	sh_desc_ptr = &sh_desc->end_index[1];

	/* process keys, starting with class 2/authentication */
	sh_desc_ptr = cmd_insert_key(sh_desc_ptr, (void *)&ctx->key_phys,
				     ctx->split_key_len * 8,
				     PTR_DIRECT, KEYDST_MD_SPLIT, KEY_COVERED,
				     ITEM_REFERENCE, ITEM_CLASS2);

	enc_key = ctx->key + ctx->split_key_pad_len;
	sh_desc_ptr = cmd_insert_key(sh_desc_ptr, &enc_key, ctx->enckeylen * 8,
				     PTR_DIRECT, KEYDST_KEYREG, KEY_CLEAR,
				     ITEM_INLINE, ITEM_CLASS1);

	/* insert jump instruction now that we are at the jump target */
	cmd_insert_jump((u32 *)&sh_desc->end_index[0], JUMP_TYPE_LOCAL, CLASS_2,
			JUMP_TEST_ALL, JUMP_COND_SHRD | JUMP_COND_SELF,
			(u32 *)sh_desc_ptr - (u32 *)(&sh_desc->end_index[0]),
			NULL);

	/* insert the operation command */
	sh_desc_ptr = cmd_insert_proto_op_ipsec(sh_desc_ptr,
						ctx->class1_alg_type,
						ctx->class2_alg_type,
						DIR_DECAP);

	/*
	 * update the header with size/offsets
	 * add 1 to include header
	 */
	endidx = (sh_desc_ptr - (void *)sh_desc) / sizeof(char *) + 1;
	cmd_insert_shared_hdr((u32 *)sh_desc, sizeof(struct ipsec_decap_pdb) /
			      sizeof(u32), endidx, CTX_SAVE, SHR_SERIAL);

#ifdef DEBUG
	print_hex_dump(KERN_ERR, "shrdesc@"xstr(__LINE__)": ",
		       DUMP_PREFIX_ADDRESS, 16, 4, sh_desc,
		       (sh_desc_ptr - (void *)sh_desc) * 4 + 1, 1);
	caam_desc_disasm((u_int32_t *)sh_desc, DISASM_SHOW_OFFSETS |
			 DISASM_SHOW_RAW);
#endif

	ctx->shared_desc_len = endidx * sizeof(u32);

	/* now we know the length, stop wasting preallocated sh_desc space */
	ctx->shared_decap = krealloc(sh_desc, ctx->shared_desc_len,
				    GFP_DMA | flags);

	ctx->shared_desc_phys = dma_map_single(dev, sh_desc, endidx *
					       sizeof(u32), DMA_BIDIRECTIONAL);
	if (dma_mapping_error(dev, ctx->shared_desc_phys)) {
		dev_err(dev, "unable to map shared descriptor\n");
		kfree(ctx->shared_decap);
		return -ENOMEM;
	}

	return 0;
}

static int build_protocol_desc_ipsec_encap(struct caam_ctx *ctx,
					   struct aead_request *areq)
{
	gfp_t flags = areq->base.flags & CRYPTO_TFM_REQ_MAY_SLEEP ? GFP_KERNEL :
		      GFP_ATOMIC;
	struct device *dev = ctx->dev;
	struct ipsec_encap_pdb *sh_desc;
	int endidx;
	void *sh_desc_ptr, *enc_key;

	/* build shared descriptor for this session */
	sh_desc = kzalloc(sizeof(struct ipsec_encap_pdb) +
			 ctx->split_key_len + ctx->enckeylen +
			 52 /*sizeof(struct iphdr)*/, GFP_DMA | flags);
	if (!sh_desc) {
		dev_err(dev, "could not allocate shared descriptor\n");
		return -ENOMEM;
	}

	/*
	 * options byte: IVsrc is RNG
	 * we do not Prepend IP header to output frame
	 */
#if !defined(DEBUG)
	sh_desc->options |= PDBOPTS_ESPCBC_IVSRC; /* IV src is RNG */
#endif

	/*
	 * need to pretend we have a full fledged pdb, otherwise get:
	 * [caam error] IPsec encapsulation: PDB is only 4 bytes, \
	 * expected at least 36 bytes
	 */

#ifdef DEBUG
	memcpy(&sh_desc->cbc.iv, "myivmyivmyivmyiv", sizeof(sh_desc->cbc.iv));
#endif

	/*
	 * indicate no IP header,
	 * rather a jump instruction and key specification follow
	 */
	sh_desc->ip_hdr_len = 0;

	/* insert keys, leaving space here for the jump instruction */
	sh_desc_ptr = &sh_desc->ip_hdr[1];

	/* process keys, starting with class 2/authentication */
	sh_desc_ptr = cmd_insert_key(sh_desc_ptr, (void *)&ctx->key_phys,
				     ctx->split_key_len * 8, PTR_DIRECT,
				     KEYDST_MD_SPLIT, KEY_COVERED,
				     ITEM_REFERENCE, ITEM_CLASS2);

	enc_key = ctx->key + ctx->split_key_pad_len;
	sh_desc_ptr = cmd_insert_key(sh_desc_ptr, &enc_key, ctx->enckeylen * 8,
				     PTR_DIRECT, KEYDST_KEYREG, KEY_CLEAR,
				     ITEM_INLINE, ITEM_CLASS1);

	/* insert jump instruction now that we are at the jump target */
	cmd_insert_jump((u32 *)&sh_desc->ip_hdr[0], JUMP_TYPE_LOCAL, CLASS_BOTH,
			JUMP_TEST_ALL, JUMP_COND_SHRD | JUMP_COND_SELF,
			(u32 *)sh_desc_ptr - (u32 *)(&sh_desc->ip_hdr[0]),
			NULL);

	/* insert the operation command */
	sh_desc_ptr = cmd_insert_proto_op_ipsec(sh_desc_ptr,
						ctx->class1_alg_type,
						ctx->class2_alg_type,
						DIR_ENCAP);

	/*
	 * update the header with size/offsets
	 * add 1 to include header
	 */
	endidx = (sh_desc_ptr - (void *)sh_desc) / sizeof(char *) + 1;
	cmd_insert_shared_hdr((u32 *)sh_desc, sizeof(struct ipsec_encap_pdb) /
			      sizeof(u32), endidx, CTX_SAVE, SHR_SERIAL);

#ifdef DEBUG
	print_hex_dump(KERN_ERR, "shrdesc@"xstr(__LINE__)": ",
		       DUMP_PREFIX_ADDRESS, 16, 4, sh_desc,
		       (sh_desc_ptr - (void *)sh_desc + 1 + 4) * 4, 1);
	caam_desc_disasm((u32 *)sh_desc, DISASM_SHOW_OFFSETS | DISASM_SHOW_RAW);
#endif

	ctx->shared_desc_len = endidx * sizeof(u32);

	/* now we know the length, stop wasting preallocated sh_desc space */
	ctx->shared_encap = krealloc(sh_desc, ctx->shared_desc_len,
				    GFP_DMA | flags);

	ctx->shared_desc_phys = dma_map_single(dev, sh_desc,
					       endidx * sizeof(u32),
					       DMA_BIDIRECTIONAL);
	if (dma_mapping_error(dev, ctx->shared_desc_phys)) {
		dev_err(ctx->dev, "unable to map shared descriptor\n");
		kfree(ctx->shared_encap);
		return -ENOMEM;
	}

	return 0;
}

struct split_key_result {
	struct completion completion;
	int err;
};

static void split_key_done(struct device *dev, u32 *desc, u32 err,
			   void *context)
{
	struct split_key_result *res = context;

#ifdef DEBUG
	dev_err(dev, "%s %d: err 0x%x\n", __func__, __LINE__, err);
#endif
	if (err) {
		char tmp[256];

		dev_err(dev, "%s\n", caam_jq_strstatus(tmp, err));
	}

	res->err = err;

	complete(&res->completion);
}

/*
get a split ipad/opad key

Split key generation-----------------------------------------------

[00] 0xb0810008    jobdesc: stidx=1 share=never len=8
[01] 0x04000014        key: class2->keyreg len=20
			@0xffe01000
[03] 0x84410014  operation: cls2-op sha1 hmac init dec
[04] 0x24940000     fifold: class2 msgdata-last2 len=0 imm
[05] 0xa4000001       jump: class2 local all ->1 [06]
[06] 0x64260028    fifostr: class2 mdsplit-jdk len=40
			@0xffe04000
*/
static u32 gen_split_key(struct device *dev, struct caam_ctx *ctx,
			 const u8 *key_in, u32 authkeylen)
{
	struct caam_drv_private *priv = dev_get_drvdata(dev);
	u32 *desc, *desc_pos;
	struct split_key_result result;
	dma_addr_t dma_addr_in, dma_addr_out;
	struct device *tgt_jq_dev;
	int ret = 0;

	desc = kzalloc(MAX_CAAM_DESCSIZE, GFP_KERNEL | GFP_DMA);
	desc_pos = desc;

	/* skip header; done last */
	desc_pos++;

	dma_addr_in = dma_map_single(dev, (void *)key_in, authkeylen,
				     DMA_TO_DEVICE);
	if (dma_mapping_error(dev, dma_addr_in)) {
		dev_err(dev, "unable to map key input memory\n");
		kfree(desc);
		return -ENOMEM;
	}
	desc_pos = cmd_insert_key(desc_pos, (void *)&dma_addr_in,
				  authkeylen * 8, PTR_DIRECT, KEYDST_KEYREG,
				  KEY_CLEAR, ITEM_REFERENCE, ITEM_CLASS2);

	/* Sets MDHA up into an HMAC-INIT */
	desc_pos = cmd_insert_alg_op(desc_pos, OP_TYPE_CLASS2_ALG,
				     ctx->alg_op, OP_ALG_AAI_HMAC, MDSTATE_INIT,
				     ICV_CHECK_OFF, DIR_DECRYPT);

	/*
	 * do a FIFO_LOAD of zero, this will trigger the internal key expansion
	   into both pads inside MDHA
	 */
	desc_pos = cmd_insert_fifo_load(desc_pos, NULL, 0, LDST_CLASS_2_CCB,
					0, FIFOLD_IMM, 0,
					FIFOLD_TYPE_MSG | FIFOLD_TYPE_LAST2);

	/* jump to next insn only necessary due to erratum? */
	desc_pos = cmd_insert_jump(desc_pos, JUMP_TYPE_LOCAL, CLASS_2,
				  JUMP_TEST_ALL, 0, 1, NULL);

	/*
	 * FIFO_STORE with the explicit split-key content store
	 * (0x26 output type)
	 */
	dma_addr_out = dma_map_single(dev, ctx->key, ctx->split_key_pad_len,
				      DMA_FROM_DEVICE);
	if (dma_mapping_error(dev, dma_addr_out)) {
		dev_err(dev, "unable to map key output memory\n");
		kfree(ctx->key);
		kfree(desc);
		return -ENOMEM;
	}
	desc_pos = cmd_insert_fifo_store(desc_pos, (void *)&dma_addr_out,
					 ctx->split_key_len, LDST_CLASS_2_CCB,
					 0, 0, 0, FIFOST_TYPE_SPLIT_KEK);

	/* insert job descriptor header */
	cmd_insert_hdr(desc, 0, desc_pos - desc, SHR_NEVER, SHRNXT_LENGTH,
		       ORDER_FORWARD, DESC_STD);

#ifdef DEBUG
	print_hex_dump(KERN_ERR, "ctx.key@"xstr(__LINE__)": ",
		       DUMP_PREFIX_ADDRESS, 16, 4, ctx->key,
		       CAAM_MAX_KEY_SIZE, 1);
	print_hex_dump(KERN_ERR, "jobdesc@"xstr(__LINE__)": ",
		       DUMP_PREFIX_ADDRESS, 16, 4, desc,
		       (desc_pos - desc + 1) * 4, 1);
	caam_desc_disasm(desc, DISASM_SHOW_OFFSETS | DISASM_SHOW_RAW);
#endif

	result.err = 0;
	init_completion(&result.completion);

	tgt_jq_dev = priv->algapi_jq[per_cpu(cpu_to_job_queue,
				     raw_smp_processor_id())];
	ret = caam_jq_enqueue(tgt_jq_dev, desc, split_key_done, &result);
	if (!ret) {
		/* in progress */
		wait_for_completion_interruptible(&result.completion);
		ret = result.err;
#ifdef DEBUG
		print_hex_dump(KERN_ERR, "ctx.key@"xstr(__LINE__)": ",
			       DUMP_PREFIX_ADDRESS, 16, 4, ctx->key,
			       CAAM_MAX_KEY_SIZE, 1);
#endif
	}

	dma_unmap_single(dev, dma_addr_out, ctx->split_key_pad_len,
			 DMA_FROM_DEVICE);
	dma_unmap_single(dev, dma_addr_in, authkeylen, DMA_TO_DEVICE);

	kfree(desc);

	return ret;
}

static int aead_authenc_setkey(struct crypto_aead *aead,
			       const u8 *key, unsigned int keylen)
{
	/* Sizes for MDHA pads (*not* keys): MD5, SHA1, 224, 256, 384, 512 */
	static const u8 mdpadlen[] = { 16, 20, 32, 32, 64, 64 };
	struct caam_ctx *ctx = crypto_aead_ctx(aead);
	struct device *dev = ctx->dev;
	struct rtattr *rta = (void *)key;
	struct crypto_authenc_key_param *param;
	unsigned int authkeylen;
	unsigned int enckeylen;
	int ret = 0;

	if (!RTA_OK(rta, keylen))
		goto badkey;

	if (rta->rta_type != CRYPTO_AUTHENC_KEYA_PARAM)
		goto badkey;

	if (RTA_PAYLOAD(rta) < sizeof(*param))
		goto badkey;

	param = RTA_DATA(rta);
	enckeylen = be32_to_cpu(param->enckeylen);

	key += RTA_ALIGN(rta->rta_len);
	keylen -= RTA_ALIGN(rta->rta_len);

	if (keylen < enckeylen)
		goto badkey;

	authkeylen = keylen - enckeylen;

	if (keylen > CAAM_MAX_KEY_SIZE)
		goto badkey;

	/* Pick class 2 key length from algorithm submask */
	ctx->split_key_len = mdpadlen[(ctx->alg_op & OP_ALG_ALGSEL_SUBMASK) >>
				      OP_ALG_ALGSEL_SHIFT] * 2;
	ctx->split_key_pad_len = ALIGN(ctx->split_key_len, 16);

#ifdef DEBUG
	printk(KERN_ERR "keylen %d enckeylen %d authkeylen %d\n",
	       keylen, enckeylen, authkeylen);
	printk(KERN_ERR "split_key_len %d split_key_pad_len %d\n",
	       ctx->split_key_len, ctx->split_key_pad_len);
	print_hex_dump(KERN_ERR, "key in @"xstr(__LINE__)": ",
		       DUMP_PREFIX_ADDRESS, 16, 4, key,
		       CAAM_MAX_KEY_SIZE, 1);
#endif
	ctx->key = kzalloc(ctx->split_key_pad_len + enckeylen,
			   GFP_KERNEL | GFP_DMA);
	if (!ctx->key) {
		dev_err(ctx->dev, "could not allocate key output memory\n");
		return -ENOMEM;
	}

	ret = gen_split_key(dev, ctx, key, authkeylen);
	if (ret)
		goto badkey;

	/* postpend encryption key to auth split key */
	memcpy(ctx->key + ctx->split_key_pad_len, key + authkeylen, enckeylen);

	ctx->key_phys = dma_map_single(dev, ctx->key, ctx->split_key_pad_len +
				       enckeylen, DMA_TO_DEVICE);
	if (dma_mapping_error(dev, ctx->key_phys)) {
		dev_err(dev, "unable to map key i/o memory\n");
		return -ENOMEM;
	}
#ifdef DEBUG
	print_hex_dump(KERN_ERR, "ctx.key@"xstr(__LINE__)": ",
		       DUMP_PREFIX_ADDRESS, 16, 4, ctx->key,
		       CAAM_MAX_KEY_SIZE, 1);
#endif

	ctx->keylen = keylen;
	ctx->enckeylen = enckeylen;
	ctx->authkeylen = authkeylen;

	return ret;
badkey:
	crypto_aead_set_flags(aead, CRYPTO_TFM_RES_BAD_KEY_LEN);
	return -EINVAL;
}

struct link_tbl_entry {
	__be64 ptr;
	__be32 len;
	u8 reserved;
	u8 buf_pool_id;
	__be16 offset;
};

/*
 * ipsec_esp_edesc - s/w-extended ipsec_esp descriptor
 * @hw_desc: the h/w job descriptor
 * @src_nents: number of segments in input scatterlist
 * @dst_nents: number of segments in output scatterlist
 * @assoc_nents: number of segments in associated data (SPI+Seq) scatterlist
 * @desc: h/w descriptor (variable length; must not exceed MAX_CAAM_DESCSIZE)
 * @dma_len: length of dma mapped link_tbl space
 * @link_tbl_phys: bus physical mapped address of h/w link table
 * @link_tbl: space for flattened i/o data (if {src,dst}_nents > 1)
 *           (until s-g support added)
 */
struct ipsec_esp_edesc {
	u32 hw_desc[MAX_CAAM_DESCSIZE];
	int src_nents;
	int dst_nents;
	int assoc_nents;
	int dma_len;
	dma_addr_t link_tbl_phys;
	struct link_tbl_entry link_tbl[0];
};

static void ipsec_esp_unmap(struct device *dev,
			    struct ipsec_esp_edesc *edesc,
			    struct aead_request *areq)
{
	dma_unmap_sg(dev, areq->assoc, edesc->assoc_nents, DMA_TO_DEVICE);

	dma_unmap_sg(dev, areq->src, edesc->src_nents ? : 1,
		     DMA_BIDIRECTIONAL);

	if (edesc->dma_len)
		dma_unmap_single(dev, edesc->link_tbl_phys, edesc->dma_len,
				 DMA_BIDIRECTIONAL);
}

/*
 * ipsec_esp descriptor callbacks
 */
static void ipsec_esp_encrypt_done(struct device *dev, u32 *desc, u32 err,
				   void *context)
{
	struct aead_request *areq = context;
	struct ipsec_esp_edesc *edesc = (struct ipsec_esp_edesc *)desc;
	struct crypto_aead *aead = crypto_aead_reqtfm(areq);
	struct caam_ctx *ctx = crypto_aead_ctx(aead);

#ifdef DEBUG
	dev_err(dev, "%s %d: err 0x%x\n", __func__, __LINE__, err);
#endif

	if (err) {
		char tmp[256];

		dev_err(dev, "%s\n", caam_jq_strstatus(tmp, err));
	}

	ipsec_esp_unmap(dev, edesc, areq);

#ifdef DEBUG
	print_hex_dump(KERN_ERR, "iphdrout@"xstr(__LINE__)": ",
		       DUMP_PREFIX_ADDRESS, 16, 4,
		       ((char *)sg_virt(areq->assoc) - sizeof(struct iphdr)),
		       sizeof(struct iphdr) + areq->assoclen + areq->cryptlen +
		       ctx->authsize + 36, 1);
	if (!err && edesc->dma_len) {
		struct scatterlist *sg = sg_last(areq->src, edesc->src_nents);
		print_hex_dump(KERN_ERR, "sglastout"xstr(__LINE__)": ",
			       DUMP_PREFIX_ADDRESS, 16, 4, sg_virt(sg),
				sg->length + ctx->authsize + 16, 1);
	}
#endif
	crypto_edesc_free(edesc, dev_get_drvdata(ctx->dev));

	aead_request_complete(areq, err);
}

static void ipsec_esp_decrypt_done(struct device *dev, u32 *desc, u32 err,
				   void *context)
{
	struct aead_request *areq = context;
	struct ipsec_esp_edesc *edesc = (struct ipsec_esp_edesc *)desc;
	struct crypto_aead *aead = crypto_aead_reqtfm(areq);
	struct caam_ctx *ctx = crypto_aead_ctx(aead);

#ifdef DEBUG
	struct crypto_aead *aead = crypto_aead_reqtfm(areq);
	dev_err(dev, "%s %d: err 0x%x\n", __func__, __LINE__, err);
#endif
	if (err) {
		char tmp[256];

		dev_err(dev, "%s\n", caam_jq_strstatus(tmp, err));
	}

	ipsec_esp_unmap(dev, edesc, areq);

	/*
	 * verify hw auth check passed else return -EBADMSG
	 */
	debug("err 0x%08x\n", err);
	if ((err & JQSTA_CCBERR_ERRID_MASK) == JQSTA_CCBERR_ERRID_ICVCHK)
		err = -EBADMSG;

#ifdef DEBUG
	print_hex_dump(KERN_ERR, "iphdrout@"xstr(__LINE__)": ",
		       DUMP_PREFIX_ADDRESS, 16, 4,
		       ((char *)sg_virt(areq->assoc) - sizeof(struct iphdr)),
		       sizeof(struct iphdr) + areq->assoclen +
		       ((areq->cryptlen > 1500) ? 1500 : areq->cryptlen) +
		       ctx->authsize + 36, 1);
	if (!err && edesc->dma_len) {
		struct scatterlist *sg = sg_last(areq->src, edesc->src_nents);
		print_hex_dump(KERN_ERR, "sglastout@"xstr(__LINE__)": ",
			       DUMP_PREFIX_ADDRESS, 16, 4, sg_virt(sg),
			sg->length + ctx->authsize + 16, 1);
	}
#endif
	crypto_edesc_free(edesc, dev_get_drvdata(ctx->dev));

	aead_request_complete(areq, err);
}

/*
 * convert scatterlist to h/w link table format
 * scatterlist must have been previously dma mapped
 */
static void sg_to_link_tbl(struct scatterlist *sg, int sg_count,
			   struct link_tbl_entry *link_tbl_ptr, int offset)
{
	while (sg_count) {
		link_tbl_ptr->ptr = sg_dma_address(sg);
		link_tbl_ptr->len = sg_dma_len(sg);
		link_tbl_ptr->reserved = 0;
		link_tbl_ptr->buf_pool_id = 0;
		link_tbl_ptr->offset = offset;
		link_tbl_ptr++;
		sg = sg_next(sg);
		sg_count--;
	}

	/* set Final bit (marks end of link table) */
	link_tbl_ptr--;
	link_tbl_ptr->len |= cpu_to_be32(0x40000000);
}

/*
 * fill in and submit ipsec_esp job descriptor
 */
static int ipsec_esp(struct ipsec_esp_edesc *edesc, struct aead_request *areq,
		     u8 *giv, enum protdir direction,
		     void (*callback) (struct device *dev, u32 *desc, u32 err,
				       void *context))
{
	struct crypto_aead *aead = crypto_aead_reqtfm(areq);
	struct caam_ctx *ctx = crypto_aead_ctx(aead);
	struct device *dev = ctx->dev;
	struct caam_drv_private *priv = dev_get_drvdata(dev);
	struct device *tgt_jq_dev;
	struct scatterlist *sg;
	u32 *desc = edesc->hw_desc;
	u32 *descptr = desc;
	struct link_tbl_entry *link_tbl_ptr = &edesc->link_tbl[0];
	int startidx, endidx, ret, sg_count, assoc_sg_count, len, padlen;
	int ivsize = crypto_aead_ivsize(aead);
	dma_addr_t ptr;
	struct ipsec_deco_dpovrd dpovrd = {0, 0, 0, 0};

#ifdef DEBUG
	debug("assoclen %d cryptlen %d authsize %d\n",
	      areq->assoclen, areq->cryptlen, ctx->authsize);
	print_hex_dump(KERN_ERR, "iphdrin@"xstr(__LINE__)": ",
		       DUMP_PREFIX_ADDRESS, 16, 4,
		       ((char *)sg_virt(areq->assoc) - sizeof(struct iphdr)),
		       sizeof(struct iphdr) + areq->assoclen + ivsize +
		       areq->cryptlen + ctx->authsize + 16, 1);
#endif

	/* skip job header (filled in last) */
	descptr++;

	/* insert shared descriptor pointer */
	*(dma_addr_t *)descptr = ctx->shared_desc_phys;
	descptr += sizeof(dma_addr_t) / sizeof(u32);

	/* Save current location for computing start index later */
	startidx = descptr - desc;

	/*
	 * insert the SEQ IN (data in) command
	 * assoc is bidirectional because we're using the protocol descriptor
	 * and encap takes SPI + seq.num from PDB.
	 */
	assoc_sg_count = dma_map_sg(dev, areq->assoc, edesc->assoc_nents ? : 1,
				    DMA_BIDIRECTIONAL);
	if (areq->src == areq->dst)
		sg_count = dma_map_sg(dev, areq->src, edesc->src_nents ? : 1,
				      DMA_BIDIRECTIONAL);
	else
		sg_count = dma_map_sg(dev, areq->src, edesc->src_nents ? : 1,
				      DMA_TO_DEVICE);
	if (direction == DIR_ENCAP) {
		if (!edesc->dma_len) {
			ptr = sg_dma_address(areq->src);
			padlen = *(u8 *)((u8 *)sg_virt(areq->src)
					 + areq->cryptlen - 2);
			/* cryptlen includes padlen / is blocksize aligned */
			len = areq->cryptlen - padlen - 2;
			dpovrd.next_header = *(u8 *)((u8 *)sg_virt(areq->src) +
						     areq->cryptlen - 1);
		} else {
			sg_to_link_tbl(areq->src, sg_count, link_tbl_ptr, 0);
#ifdef DEBUG
			print_hex_dump(KERN_ERR, "link_tbl@"xstr(__LINE__)": ",
			       DUMP_PREFIX_ADDRESS, 16, 4, &edesc->link_tbl[0],
			       edesc->dma_len, 1);
#endif
			ptr = dma_map_single(dev, link_tbl_ptr, edesc->dma_len,
						   DMA_BIDIRECTIONAL);
			edesc->link_tbl_phys = ptr;
			sg = sg_last(areq->src, edesc->src_nents);
			padlen = *(u8 *)(((u8 *)sg_virt(sg)) + sg->length -
				 ctx->authsize - 2);
			/* cryptlen includes padlen / is blocksize aligned */
			len = areq->cryptlen - padlen - 2;
			dpovrd.next_header = *(u8 *)((u8 *)sg_virt(sg) +
						     sg->length -
						     ctx->authsize - 1);
#ifdef DEBUG
			print_hex_dump(KERN_ERR, "sglastin@"xstr(__LINE__)": ",
				       DUMP_PREFIX_ADDRESS, 16, 4, sg_virt(sg),
				sg->length + ctx->authsize + 16, 1);
#endif
		}

		debug("pad length is %d\n", padlen);
		debug("next header is %d\n", dpovrd.next_header);
	} else { /* DECAP */
		debug("seq.num %d\n",
		      *(u32 *)((u32 *)sg_virt(areq->assoc) + 1));
/* #define INJECT_ICV_CHECK_FAILURE to verify correctness of ICV check */
#ifdef INJECT_ICV_CHECK_FAILURE
		{
			/*
			 * intentionally tamper with packet's data
			 * to verify proper ICV check result propagation
			 */
			u32 *foil_ptr;
			struct scatterlist *foil_sg;

			foil_sg = sg_last(areq->src, edesc->src_nents ? : 1);
			foil_ptr = sg_virt(foil_sg) +
				   (areq->src->length - 26) / 4;

			dev_warn(dev, "BEFORE FOILING PACKET DATA: addr 0x%p"
				 "  data 0x%x\n", foil_ptr, *foil_ptr);
			(*foil_ptr)++;
			dev_warn(dev, " AFTER FOILING PACKET DATA: addr 0x%p"
				 "  data 0x%x\n", foil_ptr, *foil_ptr);
		}
#endif
		/* h/w wants ip hdr + assoc + iv data in input */
		if (!edesc->dma_len) {
			ptr = sg_dma_address(areq->src) - ivsize -
			      areq->assoclen - sizeof(struct iphdr);
		} else {
			sg_to_link_tbl(areq->src, sg_count, link_tbl_ptr, 0);
			link_tbl_ptr->ptr = cpu_to_be64(link_tbl_ptr->ptr -
							sizeof(struct iphdr) -
							areq->assoclen -
							ivsize);
			link_tbl_ptr->len += cpu_to_be32(sizeof(struct iphdr) +
							 areq->assoclen +
							 ivsize);
#ifdef DEBUG
			print_hex_dump(KERN_ERR, "link_tbl@"xstr(__LINE__)": ",
			       DUMP_PREFIX_ADDRESS, 16, 4, &edesc->link_tbl[0],
			       edesc->dma_len, 1);
#endif
			ptr = dma_map_single(dev, link_tbl_ptr, edesc->dma_len,
						   DMA_BIDIRECTIONAL);
			edesc->link_tbl_phys = ptr;
		}
		len = sizeof(struct iphdr) + areq->assoclen + ivsize +
		      areq->cryptlen + ctx->authsize;
	}
	descptr = cmd_insert_seq_in_ptr(descptr, ptr, len, edesc->dma_len ?
					PTR_SGLIST : PTR_DIRECT);

	/* insert the SEQ OUT (data out) command */
	sg_count = dma_map_sg(dev, areq->dst, edesc->dst_nents ? : 1,
			      DMA_BIDIRECTIONAL);

	if (edesc->dma_len) {
		/*
		 * write output sg list after input sg list
		 * just without assigning offsets
		 */
		link_tbl_ptr = &edesc->link_tbl[sg_count];
		memcpy(link_tbl_ptr, &edesc->link_tbl[0],
		       sg_count * sizeof(struct link_tbl_entry));
	}

	if (direction == DIR_ENCAP) {
		/* h/w writes assoc + iv data to output */
		len = areq->assoclen + ivsize + areq->cryptlen + ctx->authsize;
		if (!edesc->dma_len) {
			ptr = sg_dma_address(areq->assoc);
		} else {
			link_tbl_ptr->ptr -= areq->assoclen + ivsize;
			link_tbl_ptr->len += areq->assoclen + ivsize;
#ifdef DEBUG
			print_hex_dump(KERN_ERR, "link_tbl@"xstr(__LINE__)": ",
			       DUMP_PREFIX_ADDRESS, 16, 4, &edesc->link_tbl[0],
			       edesc->dma_len, 1);
#endif
			ptr += sg_count * sizeof(struct link_tbl_entry);
		}
	} else { /* DECAP */
		len = areq->cryptlen + ctx->authsize;
		if (!edesc->dma_len) {
			ptr = sg_dma_address(areq->dst) - sizeof(struct iphdr) -
			      areq->assoclen - ivsize;
			len += sizeof(struct iphdr) + areq->assoclen + ivsize;
		} else {
			len += sizeof(struct iphdr) + areq->assoclen + ivsize +
			       ctx->authsize;
			ptr += sg_count * sizeof(struct link_tbl_entry);
			/* FIXME: need dma_sync since post-map adjustments? */
#ifdef DEBUG
			print_hex_dump(KERN_ERR, "link_tbl@"xstr(__LINE__)": ",
			       DUMP_PREFIX_ADDRESS, 16, 4, &edesc->link_tbl[0],
			       edesc->dma_len, 1);
#endif
		}
	}

	descptr = cmd_insert_seq_out_ptr(descptr, ptr, len, edesc->dma_len ?
					 PTR_SGLIST : PTR_DIRECT);

	if (direction == DIR_ENCAP) {
		/* insert the LOAD command */
		dpovrd.ovrd_ecn |= IPSEC_ENCAP_DECO_DPOVRD_USE;
		/* DECO class, no s-g, 7 == DPROVRD, 0 offset */
		descptr = cmd_insert_load(descptr, &dpovrd, LDST_CLASS_DECO,
					  0, 0x07 << 16, 0, sizeof(dpovrd),
					  ITEM_INLINE);
	}

	/*
	 * write the job descriptor header with shared descriptor length,
	 * reverse order execution, and size/offsets.
	 */
	endidx = descptr - desc;

	cmd_insert_hdr(desc, ctx->shared_desc_len / sizeof(u32),
		       endidx, SHR_SERIAL, SHRNXT_SHARED /* has_shared */,
		       ORDER_REVERSE, DESC_STD /*don't make trusted*/);
#ifdef DEBUG
	print_hex_dump(KERN_ERR, "jobdesc@"xstr(__LINE__)": ",
		       DUMP_PREFIX_ADDRESS, 16, 4, desc,
		       (descptr - desc + 1) * 4, 1);
	caam_desc_disasm(desc, DISASM_SHOW_OFFSETS | DISASM_SHOW_RAW);
#endif

	tgt_jq_dev = priv->algapi_jq[per_cpu(cpu_to_job_queue,
					     raw_smp_processor_id())];
	ret = caam_jq_enqueue(tgt_jq_dev, desc, callback, areq);
	if (!ret)
		ret = -EINPROGRESS;
	else {
		ipsec_esp_unmap(dev, edesc, areq);
		crypto_edesc_free(edesc, priv);
	}

	return ret;
}

/*
 * derive number of elements in scatterlist
 */
static int sg_count(struct scatterlist *sg_list, int nbytes, int *chained)
{
	struct scatterlist *sg = sg_list;
	int sg_nents = 0;

	*chained = 0;
	while (nbytes > 0) {
		sg_nents++;
		nbytes -= sg->length;
		if (!sg_is_last(sg) && (sg + 1)->length == 0)
			*chained = 1;
		sg = scatterwalk_sg_next(sg);
	}

	return sg_nents;
}

/*
 * allocate and map the ipsec_esp extended descriptor
 */
static struct ipsec_esp_edesc *ipsec_esp_edesc_alloc(struct aead_request *areq,
						     enum protdir direction)
{
	struct crypto_aead *aead = crypto_aead_reqtfm(areq);
	struct caam_ctx *ctx = crypto_aead_ctx(aead);
	struct caam_drv_private *priv = dev_get_drvdata(ctx->dev);
	gfp_t flags = areq->base.flags & CRYPTO_TFM_REQ_MAY_SLEEP ? GFP_KERNEL :
		      GFP_ATOMIC;
	int assoc_nents, src_nents, dst_nents, chained, dma_len = 0;
	struct ipsec_esp_edesc *edesc;

	BUG_ON(areq->dst != areq->src);

	assoc_nents = sg_count(areq->assoc, areq->assoclen, &chained);
	BUG_ON(chained);
	assoc_nents = (assoc_nents == 1) ? 0 : assoc_nents;

	src_nents = sg_count(areq->src, areq->cryptlen + ctx->authsize,
			     &chained);
	BUG_ON(chained);
	src_nents = (src_nents == 1) ? 0 : src_nents;

	/* + 1 for the IV, which is not included in assoc data */
	if (assoc_nents || src_nents)
		dma_len = ((assoc_nents ? : 1) + 1 + (src_nents ? : 1)) *
			  sizeof(struct link_tbl_entry);

	dst_nents = src_nents;
	dma_len *= 2; /* because we assume src == dst */

	/*
	 * allocate space for base edesc plus the two link tables
	 */
	edesc = crypto_edesc_alloc(sizeof(struct ipsec_esp_edesc) + dma_len,
					GFP_DMA | flags, priv);
	if (!edesc) {
		dev_err(ctx->dev, "could not allocate extended descriptor\n");
		return ERR_PTR(-ENOMEM);
	}

	edesc->assoc_nents = assoc_nents;
	edesc->src_nents = src_nents;
	edesc->dst_nents = dst_nents;
	edesc->dma_len = dma_len;

	return edesc;
}

#ifdef CONFIG_AS_FASTPATH
int secfp_caam_submit(struct device *dev, u32 *desc,
	void (*callback) (struct device *dev, u32 *desc,
	u32 error, void *context), void *context)
{
	struct caam_drv_private *priv = dev_get_drvdata(dev);
	struct device *tgt_jq_dev;
	int ret;

	tgt_jq_dev = priv->algapi_jq[per_cpu(cpu_to_job_queue,
					     raw_smp_processor_id())];

	ret = caam_jq_enqueue(tgt_jq_dev, desc, callback, context);

	return ret;
}
EXPORT_SYMBOL(secfp_caam_submit);
#endif

static int aead_authenc_encrypt(struct aead_request *areq)
{
	struct aead_givcrypt_request *req =
		 container_of(areq, struct aead_givcrypt_request, areq);
	struct ipsec_esp_edesc *edesc;

	/* allocate extended descriptor */
	edesc = ipsec_esp_edesc_alloc(areq, DIR_ENCAP);
	if (IS_ERR(edesc))
		return PTR_ERR(edesc);

	return ipsec_esp(edesc, areq, req->giv, DIR_ENCAP,
			 ipsec_esp_encrypt_done);
}

static int aead_authenc_encrypt_first(struct aead_request *areq)
{
	struct crypto_aead *aead = crypto_aead_reqtfm(areq);
	struct caam_ctx *ctx = crypto_aead_ctx(aead);
	struct aead_givcrypt_request *req =
		 container_of(areq, struct aead_givcrypt_request, areq);
	int err;

	spin_lock_bh(&ctx->first_lock);
	if (crypto_aead_crt(aead)->encrypt != aead_authenc_encrypt_first)
		goto unlock;

	err = build_protocol_desc_ipsec_encap(ctx, areq);
	if (err) {
		spin_unlock_bh(&ctx->first_lock);
		return err;
	}

	/* copy sequence number to PDB */
	ctx->shared_encap->seq_num = req->seq;

	/* and the SPI */
	ctx->shared_encap->spi = *((u32 *)sg_virt(areq->assoc));

	crypto_aead_crt(aead)->encrypt = aead_authenc_encrypt;
unlock:
	spin_unlock_bh(&ctx->first_lock);

	return aead_authenc_encrypt(areq);
}

static int aead_authenc_decrypt(struct aead_request *req)
{
	struct crypto_aead *aead = crypto_aead_reqtfm(req);
	struct caam_ctx *ctx = crypto_aead_ctx(aead);
	struct ipsec_esp_edesc *edesc;

	req->cryptlen -= ctx->authsize;

	/* allocate extended descriptor */
	edesc = ipsec_esp_edesc_alloc(req, DIR_DECAP);
	if (IS_ERR(edesc))
		return PTR_ERR(edesc);

	return ipsec_esp(edesc, req, NULL, DIR_DECAP, ipsec_esp_decrypt_done);
}

static int aead_authenc_decrypt_first(struct aead_request *req)
{
	struct crypto_aead *aead = crypto_aead_reqtfm(req);
	struct caam_ctx *ctx = crypto_aead_ctx(aead);
	int err;

	spin_lock_bh(&ctx->first_lock);
	if (crypto_aead_crt(aead)->decrypt != aead_authenc_decrypt_first)
		goto unlock;

	err = build_protocol_desc_ipsec_decap(ctx, req);
	if (err) {
		spin_unlock_bh(&ctx->first_lock);
		return err;
	}

	/* copy sequence number to PDB */
	ctx->shared_decap->seq_num = cpu_to_be32(*(u32 *)((u32 *)
						 sg_virt(req->assoc) + 1));

	crypto_aead_crt(aead)->decrypt = aead_authenc_decrypt;
unlock:
	spin_unlock_bh(&ctx->first_lock);

	return aead_authenc_decrypt(req);
}

static int aead_authenc_givencrypt(struct aead_givcrypt_request *req)
{
	struct aead_request *areq = &req->areq;
	struct ipsec_esp_edesc *edesc;

	/* allocate extended descriptor */
	edesc = ipsec_esp_edesc_alloc(areq, DIR_ENCAP);
	if (IS_ERR(edesc))
		return PTR_ERR(edesc);

	return ipsec_esp(edesc, areq, req->giv, DIR_ENCAP,
			 ipsec_esp_encrypt_done);
}

static int aead_authenc_givencrypt_first(struct aead_givcrypt_request *req)
{
	struct aead_request *areq = &req->areq;
	struct crypto_aead *aead = crypto_aead_reqtfm(areq);
	struct caam_ctx *ctx = crypto_aead_ctx(aead);
	int err;

	spin_lock_bh(&ctx->first_lock);
	if (crypto_aead_crt(aead)->givencrypt != aead_authenc_givencrypt_first)
		goto unlock;

	err = build_protocol_desc_ipsec_encap(ctx, areq);
	if (err) {
		spin_unlock_bh(&ctx->first_lock);
		return err;
	}

	/* copy sequence number to PDB */
	ctx->shared_encap->seq_num = cpu_to_be32(req->seq);

	/* and the SPI */
	ctx->shared_encap->spi = cpu_to_be32(*((u32 *)sg_virt(areq->assoc)));

	crypto_aead_crt(aead)->givencrypt = aead_authenc_givencrypt;
unlock:
	spin_unlock_bh(&ctx->first_lock);

	return aead_authenc_givencrypt(req);
}

struct caam_alg_template {
	char name[CRYPTO_MAX_ALG_NAME];
	char driver_name[CRYPTO_MAX_ALG_NAME];
	unsigned int blocksize;
	struct aead_alg aead;
	struct device *dev;
	int class1_alg_type;
	int class2_alg_type;
	int alg_op;
};

static struct caam_alg_template driver_algs[] = {
	/* single-pass ipsec_esp descriptor */
	{
		.name = "authenc(hmac(sha1),cbc(aes))",
		.driver_name = "authenc-hmac-sha1-cbc-aes-caam",
		.blocksize = AES_BLOCK_SIZE,
		.aead = {
			.setkey = aead_authenc_setkey,
			.setauthsize = aead_authenc_setauthsize,
			.encrypt = aead_authenc_encrypt_first,
			.decrypt = aead_authenc_decrypt_first,
			.givencrypt = aead_authenc_givencrypt_first,
			.geniv = "<built-in>",
			.ivsize = AES_BLOCK_SIZE,
			.maxauthsize = SHA1_DIGEST_SIZE,
			},
		.class1_alg_type = CIPHER_TYPE_IPSEC_AESCBC,
		.class2_alg_type = AUTH_TYPE_IPSEC_SHA1HMAC_96,
		.alg_op = OP_ALG_ALGSEL_SHA1,
	},
	{
		.name = "authenc(hmac(sha256),cbc(aes))",
		.driver_name = "authenc-hmac-sha256-cbc-aes-caam",
		.blocksize = AES_BLOCK_SIZE,
		.aead = {
			.setkey = aead_authenc_setkey,
			.setauthsize = aead_authenc_setauthsize,
			.encrypt = aead_authenc_encrypt_first,
			.decrypt = aead_authenc_decrypt_first,
			.givencrypt = aead_authenc_givencrypt_first,
			.geniv = "<built-in>",
			.ivsize = AES_BLOCK_SIZE,
			.maxauthsize = SHA256_DIGEST_SIZE,
			},
		.class1_alg_type = CIPHER_TYPE_IPSEC_AESCBC,
		.class2_alg_type = AUTH_TYPE_IPSEC_SHA2HMAC_256,
		.alg_op = OP_ALG_ALGSEL_SHA256,
	},
	{
		.name = "authenc(hmac(sha1),cbc(des3_ede))",
		.driver_name = "authenc-hmac-sha1-cbc-des3_ede-caam",
		.blocksize = DES3_EDE_BLOCK_SIZE,
		.aead = {
			.setkey = aead_authenc_setkey,
			.setauthsize = aead_authenc_setauthsize,
			.encrypt = aead_authenc_encrypt_first,
			.decrypt = aead_authenc_decrypt_first,
			.givencrypt = aead_authenc_givencrypt_first,
			.geniv = "<built-in>",
			.ivsize = DES3_EDE_BLOCK_SIZE,
			.maxauthsize = SHA1_DIGEST_SIZE,
			},
		.class1_alg_type = CIPHER_TYPE_IPSEC_3DESCBC,
		.class2_alg_type = AUTH_TYPE_IPSEC_SHA1HMAC_96,
		.alg_op = OP_ALG_ALGSEL_SHA1,
	},
	{
		.name = "authenc(hmac(sha256),cbc(des3_ede))",
		.driver_name = "authenc-hmac-sha256-cbc-des3_ede-caam",
		.blocksize = DES3_EDE_BLOCK_SIZE,
		.aead = {
			.setkey = aead_authenc_setkey,
			.setauthsize = aead_authenc_setauthsize,
			.encrypt = aead_authenc_encrypt_first,
			.decrypt = aead_authenc_decrypt_first,
			.givencrypt = aead_authenc_givencrypt_first,
			.geniv = "<built-in>",
			.ivsize = DES3_EDE_BLOCK_SIZE,
			.maxauthsize = SHA256_DIGEST_SIZE,
			},
		.class1_alg_type = CIPHER_TYPE_IPSEC_3DESCBC,
		.class2_alg_type = AUTH_TYPE_IPSEC_SHA2HMAC_256,
		.alg_op = OP_ALG_ALGSEL_SHA256,
	},
	{
		.name = "authenc(hmac(sha1),cbc(des))",
		.driver_name = "authenc-hmac-sha1-cbc-des-caam",
		.blocksize = DES_BLOCK_SIZE,
		.aead = {
			.setkey = aead_authenc_setkey,
			.setauthsize = aead_authenc_setauthsize,
			.encrypt = aead_authenc_encrypt_first,
			.decrypt = aead_authenc_decrypt_first,
			.givencrypt = aead_authenc_givencrypt_first,
			.geniv = "<built-in>",
			.ivsize = DES_BLOCK_SIZE,
			.maxauthsize = SHA1_DIGEST_SIZE,
			},
		.class1_alg_type = CIPHER_TYPE_IPSEC_DESCBC,
		.class2_alg_type = AUTH_TYPE_IPSEC_SHA1HMAC_96,
		.alg_op = OP_ALG_ALGSEL_SHA1,
	},
	{
		.name = "authenc(hmac(sha256),cbc(des))",
		.driver_name = "authenc-hmac-sha256-cbc-des-caam",
		.blocksize = DES_BLOCK_SIZE,
		.aead = {
			.setkey = aead_authenc_setkey,
			.setauthsize = aead_authenc_setauthsize,
			.encrypt = aead_authenc_encrypt_first,
			.decrypt = aead_authenc_decrypt_first,
			.givencrypt = aead_authenc_givencrypt_first,
			.geniv = "<built-in>",
			.ivsize = DES_BLOCK_SIZE,
			.maxauthsize = SHA256_DIGEST_SIZE,
			},
		.class1_alg_type = CIPHER_TYPE_IPSEC_DESCBC,
		.class2_alg_type = AUTH_TYPE_IPSEC_SHA2HMAC_256,
		.alg_op = OP_ALG_ALGSEL_SHA256,
	},
};

struct caam_crypto_alg {
	struct list_head entry;
	struct device *dev;
	int class1_alg_type;
	int class2_alg_type;
	int alg_op;
	struct crypto_alg crypto_alg;
};

static int caam_cra_init(struct crypto_tfm *tfm)
{
	struct crypto_alg *alg = tfm->__crt_alg;
	struct caam_crypto_alg *caam_alg =
		 container_of(alg, struct caam_crypto_alg, crypto_alg);
	struct caam_ctx *ctx = crypto_tfm_ctx(tfm);

	/* update context with ptr to dev */
	ctx->dev = caam_alg->dev;

	/* copy descriptor header template value */
	ctx->class1_alg_type = caam_alg->class1_alg_type;
	ctx->class2_alg_type = caam_alg->class2_alg_type;
	ctx->alg_op = caam_alg->alg_op;

	spin_lock_init(&ctx->first_lock);

	return 0;
}

static void caam_cra_exit(struct crypto_tfm *tfm)
{
	struct caam_ctx *ctx = crypto_tfm_ctx(tfm);

	if (!dma_mapping_error(ctx->dev, ctx->shared_desc_phys))
		dma_unmap_single(ctx->dev, ctx->shared_desc_phys,
				 ctx->shared_desc_len, DMA_BIDIRECTIONAL);
	kfree(ctx->shared_encap);
}

void caam_algapi_remove(struct device *dev)
{
	struct caam_drv_private *priv = dev_get_drvdata(dev);
	struct caam_crypto_alg *t_alg, *n;

	if (!priv->alg_list.next)
		return;

	list_for_each_entry_safe(t_alg, n, &priv->alg_list, entry) {
		crypto_unregister_alg(&t_alg->crypto_alg);
		list_del(&t_alg->entry);
		kfree(t_alg);
	}
}

static struct caam_crypto_alg *caam_alg_alloc(struct device *dev,
					      struct caam_alg_template
					      *template)
{
	struct caam_crypto_alg *t_alg;
	struct crypto_alg *alg;

	t_alg = kzalloc(sizeof(struct caam_crypto_alg), GFP_KERNEL);
	if (!t_alg) {
		dev_err(dev, "failed to allocate t_alg\n");
		return ERR_PTR(-ENOMEM);
	}

	alg = &t_alg->crypto_alg;

	snprintf(alg->cra_name, CRYPTO_MAX_ALG_NAME, "%s", template->name);
	snprintf(alg->cra_driver_name, CRYPTO_MAX_ALG_NAME, "%s",
		 template->driver_name);
	alg->cra_module = THIS_MODULE;
	alg->cra_init = caam_cra_init;
	alg->cra_exit = caam_cra_exit;
	alg->cra_priority = CAAM_CRA_PRIORITY;
	alg->cra_flags = CRYPTO_ALG_TYPE_AEAD | CRYPTO_ALG_ASYNC;
	alg->cra_blocksize = template->blocksize;
	alg->cra_alignmask = 0;
	alg->cra_type = &crypto_aead_type;
	alg->cra_ctxsize = sizeof(struct caam_ctx);
	alg->cra_u.aead = template->aead;

	t_alg->class1_alg_type = template->class1_alg_type;
	t_alg->class2_alg_type = template->class2_alg_type;
	t_alg->alg_op = template->alg_op;
	t_alg->dev = dev;

	return t_alg;
}

void caam_jq_algapi_init(struct device *ctrldev)
{
	struct caam_drv_private *priv = dev_get_drvdata(ctrldev);
	struct device **dev;
	int i = 0, err = 0, cpu;

	INIT_LIST_HEAD(&priv->alg_list);

	dev = kmalloc(sizeof(*dev) * priv->total_jobqs, GFP_KERNEL);
	for (i = 0; i < priv->total_jobqs; i++) {
		err = caam_jq_register(ctrldev, &dev[i]);
		if (err < 0)
			break;
	}
	if (err < 0 && i == 0) {
		dev_err(ctrldev, "algapi error in job queue registration: %d\n",
			err);
		return;
	}
	priv->num_jqs_for_algapi = i;

	/* register crypto algorithms the device supports */
	for (i = 0; i < ARRAY_SIZE(driver_algs); i++) {
		/* TODO: check if h/w supports alg */
		struct caam_crypto_alg *t_alg;

		t_alg = caam_alg_alloc(ctrldev, &driver_algs[i]);
		if (IS_ERR(t_alg)) {
			err = PTR_ERR(t_alg);
			dev_warn(ctrldev, "%s alg registration failed\n",
				t_alg->crypto_alg.cra_driver_name);
			continue;
		}

		err = crypto_register_alg(&t_alg->crypto_alg);
		if (err) {
			dev_warn(ctrldev, "%s alg registration failed\n",
				t_alg->crypto_alg.cra_driver_name);
			kfree(t_alg);
		} else {
			list_add_tail(&t_alg->entry, &priv->alg_list);
			dev_info(ctrldev, "%s\n",
				 t_alg->crypto_alg.cra_driver_name);
		}
	}

	priv->algapi_jq = dev;
	for_each_online_cpu(cpu)
		per_cpu(cpu_to_job_queue, cpu) = cpu % priv->num_jqs_for_algapi;
}
