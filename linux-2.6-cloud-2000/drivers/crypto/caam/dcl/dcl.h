/*
 * CAAM Descriptor Construction Library
 * Application level usage definitions and prototypes
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
 */

#ifndef DCL_H
#define DCL_H

#include "../desc.h"
#include "../pdb.h"


/*
 * Section 1 - Descriptor command construction definitions
 * Under development and NOT to be used at present, these are
 * almost guaranteed to change upon review.
 */

enum key_dest {
	KEYDST_KEYREG,
	KEYDST_PK_E,
	KEYDST_AF_SBOX,
	KEYDST_MD_SPLIT
};

enum key_cover {
	KEY_CLEAR,
	KEY_COVERED
};

enum item_inline {
	ITEM_REFERENCE,
	ITEM_INLINE
};

enum item_purpose {
	ITEM_CLASS1,
	ITEM_CLASS2
};

enum ref_type {
	PTR_DIRECT,
	PTR_SGLIST
};

enum ctxsave {
	CTX_SAVE,
	CTX_ERASE
};

enum shrnext {
	SHRNXT_SHARED,
	SHRNXT_LENGTH
};

enum execorder {
	ORDER_REVERSE,
	ORDER_FORWARD
};

enum shrst {
	SHR_NEVER,
	SHR_WAIT,
	SHR_SERIAL,
	SHR_ALWAYS,
	SHR_DEFER
};

enum protdir {
	DIR_ENCAP,
	DIR_DECAP
};

enum algdir {
	DIR_ENCRYPT,
	DIR_DECRYPT
};

enum mdstatesel {
	MDSTATE_UPDATE,
	MDSTATE_INIT,
	MDSTATE_FINAL,
	MDSTATE_COMPLETE	/* Full init+final in single operation */
};

enum icvsel {
	ICV_CHECK_OFF,
	ICV_CHECK_ON
};

enum mktrust {
	DESC_SIGN,
	DESC_STD
};

/*
 * Type selectors for cipher types in IPSec protocol OP instructions
 */
#define CIPHER_TYPE_IPSEC_DESCBC              2
#define CIPHER_TYPE_IPSEC_3DESCBC             3
#define CIPHER_TYPE_IPSEC_AESCBC              12
#define CIPHER_TYPE_IPSEC_AESCTR              13
#define CIPHER_TYPE_IPSEC_AES_CCM_ICV8        14
#define CIPHER_TYPE_IPSEC_AES_CCM_ICV12       15
#define CIPHER_TYPE_IPSEC_AES_CCM_ICV16       16
#define CIPHER_TYPE_IPSEC_AES_GCM_ICV8        18
#define CIPHER_TYPE_IPSEC_AES_GCM_ICV12       19
#define CIPHER_TYPE_IPSEC_AES_GCM_ICV16       20

/*
 * Type selectors for authentication in IPSec protocol OP instructions
 */
#define AUTH_TYPE_IPSEC_MD5HMAC_96            1
#define AUTH_TYPE_IPSEC_SHA1HMAC_96           2
#define AUTH_TYPE_IPSEC_AESXCBCMAC_96         6
#define AUTH_TYPE_IPSEC_SHA1HMAC_160          7
#define AUTH_TYPE_IPSEC_SHA2HMAC_256          12
#define AUTH_TYPE_IPSEC_SHA2HMAC_384          13
#define AUTH_TYPE_IPSEC_SHA2HMAC_512          14

/*
 * Command Generator Prototypes
 */
u_int32_t *cmd_insert_shared_hdr(u_int32_t *descwd, u_int8_t startidx,
				 u_int8_t desclen, enum ctxsave ctxsave,
				 enum shrst share);

u_int32_t *cmd_insert_hdr(u_int32_t *descwd, u_int8_t startidx,
			  u_int8_t desclen, enum shrst share,
			  enum shrnext sharenext, enum execorder reverse,
			  enum mktrust mktrusted);

u_int32_t *cmd_insert_key(u_int32_t *descwd, void *key, u_int32_t keylen,
			  enum ref_type sgref, enum key_dest dest,
			  enum key_cover cover, enum item_inline imm,
			  enum item_purpose purpose);

u_int32_t *cmd_insert_seq_key(u_int32_t *descwd, u_int32_t keylen,
			      enum ref_type sgref, enum key_dest dest,
			      enum key_cover cover, enum item_purpose purpose);

u_int32_t *cmd_insert_proto_op_ipsec(u_int32_t *descwd, u_int8_t cipheralg,
				     u_int8_t authalg, enum protdir dir);

u_int32_t *cmd_insert_proto_op_wimax(u_int32_t *descwd, u_int8_t mode,
				     enum protdir dir);

u_int32_t *cmd_insert_proto_op_wifi(u_int32_t *descwd, enum protdir dir);

u_int32_t *cmd_insert_proto_op_macsec(u_int32_t *descwd, enum protdir dir);

u_int32_t *cmd_insert_proto_op_unidir(u_int32_t *descwd, u_int32_t protid,
				      u_int32_t protinfo);

u_int32_t *cmd_insert_alg_op(u_int32_t *descwd, u_int32_t optype,
			     u_int32_t algtype, u_int32_t algmode,
			     enum mdstatesel mdstate, enum icvsel icv,
			     enum algdir dir);

u_int32_t *cmd_insert_pkha_op(u_int32_t *descwd, u_int32_t pkmode);

u_int32_t *cmd_insert_seq_in_ptr(u_int32_t *descwd, dma_addr_t ptr,
				 u_int32_t len, enum ref_type sgref);

u_int32_t *cmd_insert_seq_out_ptr(u_int32_t *descwd, dma_addr_t ptr,
				  u_int32_t len, enum ref_type sgref);

u_int32_t *cmd_insert_load(u_int32_t *descwd, void *data,
			   u_int32_t class_access, u_int32_t sgflag,
			   u_int32_t dest, u_int8_t offset,
			   u_int8_t len, enum item_inline imm);

u_int32_t *cmd_insert_fifo_load(u_int32_t *descwd, void *data, u_int32_t len,
				u_int32_t class_access, u_int32_t sgflag,
				u_int32_t imm, u_int32_t ext, u_int32_t type);

u_int32_t *cmd_insert_seq_load(u_int32_t *descwd, u_int32_t class_access,
			       u_int32_t variable_len_flag, u_int32_t dest,
			       u_int8_t offset, u_int8_t len);

u_int32_t *cmd_insert_seq_fifo_load(u_int32_t *descwd, u_int32_t class_access,
				    u_int32_t variable_len_flag,
				    u_int32_t data_type, u_int32_t len);

u_int32_t *cmd_insert_store(u_int32_t *descwd, void *data,
			    u_int32_t class_access, u_int32_t sg_flag,
			    u_int32_t src, u_int8_t offset,
			    u_int8_t len, enum item_inline imm);

u_int32_t *cmd_insert_seq_store(u_int32_t *descwd, u_int32_t class_access,
				u_int32_t variable_len_flag, u_int32_t src,
				u_int8_t offset, u_int8_t len);

u_int32_t *cmd_insert_fifo_store(u_int32_t *descwd, void **data, u_int32_t len,
				 u_int32_t class_access, u_int32_t sgflag,
				 u_int32_t imm, u_int32_t ext, u_int32_t type);

u_int32_t *cmd_insert_seq_fifo_store(u_int32_t *descwd, u_int32_t class_access,
				     u_int32_t variable_len_flag,
				     u_int32_t out_type, u_int32_t len);

u_int32_t *cmd_insert_jump(u_int32_t *descwd, u_int32_t jtype,
			   u_int32_t class, u_int32_t test, u_int32_t cond,
			   int8_t offset, u_int32_t *jmpdesc);

u_int32_t *cmd_insert_move(u_int32_t *descwd, u_int32_t waitcomp,
			   u_int32_t src, u_int32_t dst, u_int8_t offset,
			   u_int8_t length);

u_int32_t *cmd_insert_math(u_int32_t *descwd, u_int32_t func,
			    u_int32_t src0, u_int32_t src1,
			    u_int32_t dest, u_int32_t len,
			    u_int32_t flagupd, u_int32_t stall,
			    u_int32_t immediate, u_int32_t *data);

/*
 * Section 2 - Simple descriptor construction definitions
 */

struct pk_in_params {
	u_int8_t *e;
	u_int32_t e_siz;
	u_int8_t *n;
	u_int32_t n_siz;
	u_int8_t *a;
	u_int32_t a_siz;
	u_int8_t *b;
	u_int32_t b_siz;
};

int cnstr_seq_jobdesc(u_int32_t *jobdesc, u_int16_t *jobdescsz,
		      u_int32_t *shrdesc, u_int16_t shrdescsz,
		      void *inbuf, u_int32_t insize,
		      void *outbuf, u_int32_t outsize);

int cnstr_jobdesc_blkcipher_cbc(u_int32_t *descbuf, u_int16_t *bufsz,
				u_int8_t *data_in, u_int8_t *data_out,
				u_int32_t datasz,
				u_int8_t *key, u_int32_t keylen,
				u_int8_t *iv, u_int32_t ivlen,
				enum algdir dir, u_int32_t cipher,
				u_int8_t clear);

int32_t cnstr_jobdesc_hmac(u_int32_t *descbuf, u_int16_t *bufsize,
			   u_int8_t *msg, u_int32_t msgsz, u_int8_t *digest,
			   u_int8_t *key, u_int32_t cipher, u_int8_t *icv,
			   u_int8_t clear);

int cnstr_jobdesc_pkha_rsaexp(u_int32_t *descbuf, u_int16_t *bufsz,
			      struct pk_in_params *pkin, u_int8_t *out,
			      u_int32_t out_siz, u_int8_t clear);

int cnstr_jobdesc_mdsplitkey(u_int32_t *descbuf, u_int16_t *bufsize,
			     u_int8_t *key, u_int32_t cipher,
			     u_int8_t *padbuf);

int cnstr_jobdesc_aes_gcm(u_int32_t *descbuf, u_int16_t *bufsize,
			  u_int8_t *key, u_int32_t keylen, u_int8_t *ctx,
			  enum mdstatesel mdstate, enum icvsel icv,
			  enum algdir dir, u_int8_t *in, u_int8_t *out,
			  u_int16_t size, u_int8_t *mac);

int cnstr_jobdesc_kasumi_f8(u_int32_t *descbuf, u_int16_t *bufsize,
			    u_int8_t *key, u_int32_t keylen,
			    enum algdir dir, u_int32_t *ctx,
			    u_int8_t *in, u_int8_t *out, u_int16_t size);

int cnstr_jobdesc_kasumi_f9(u_int32_t *descbuf, u_int16_t *bufsize,
			    u_int8_t *key, u_int32_t keylen,
			    enum algdir dir, u_int32_t *ctx,
			    u_int8_t *in, u_int16_t size, u_int8_t *mac);

/*
 * Section 3 - Single-pass descriptor construction definitions
 */

/*
 * Section 4 - Protocol descriptor construction definitions
 */

int cnstr_jobdesc_dsaverify(u_int32_t *descbuf, u_int16_t *bufsz,
			    struct dsa_verify_pdb *dsadata, u_int8_t *msg,
			    u_int32_t msg_sz, u_int8_t clear);

/* If protocol descriptor, IPV4 or 6? */
enum protocolvers {
	PDB_IPV4,
	PDB_IPV6
};

/* If antireplay in PDB, how big? */
enum antirply_winsiz {
	PDB_ANTIRPLY_NONE,
	PDB_ANTIRPLY_32,
	PDB_ANTIRPLY_64
};

/* Tunnel or Transport (for next-header byte) ? */
enum connect_type {
	PDB_TUNNEL,
	PDB_TRANSPORT
};

/* Extended sequence number support? */
enum esn {
	PDB_NO_ESN,
	PDB_INCLUDE_ESN
};

/* Decapsulation output format */
enum decap_out {
	PDB_OUTPUT_COPYALL,
	PDB_OUTPUT_DECAP_PDU
};

/* IV source */
enum ivsrc {
	PDB_IV_FROM_PDB,
	PDB_IV_FROM_RNG
};

/*
 * Request parameters for specifying authentication data
 * for a single-pass or protocol descriptor
 */
struct authparams {
	u_int8_t   algtype;  /* Select algorithm */
	u_int8_t  *key;      /* Key as an array of bytes */
	u_int32_t  keylen;   /* Length of key in bits */
};

/*
 * Request parameters for specifying blockcipher data
 * for a single-pass or protocol descriptor
 */
struct cipherparams {
	u_int8_t   algtype;
	u_int8_t  *key;
	u_int32_t  keylen;
};


/* Generic IPSec - to be deprecated */
struct seqnum {
	enum esn              esn;
	enum antirply_winsiz  antirplysz;
};

struct pdbcont {
	u_int16_t             opthdrlen;
	u_int8_t             *opthdr;
	enum connect_type     transmode;
	enum protocolvers     pclvers;
	enum decap_out        outfmt;
	enum ivsrc            ivsrc;
	struct seqnum         seq;
};

int32_t cnstr_pcl_shdsc_ipsec_cbc_decap(u_int32_t *descbuf,
					u_int16_t *bufsize,
					struct pdbcont *pdb,
					struct cipherparams *cipherdata,
					struct authparams *authdata,
					u_int8_t clear);

int32_t cnstr_pcl_shdsc_ipsec_cbc_encap(u_int32_t *descbuf,
					u_int16_t *bufsize,
					struct pdbcont *pdb,
					struct cipherparams *cipherdata,
					struct authparams *authdata,
					u_int8_t clear);

int32_t cnstr_shdsc_ipsec_encap(u_int32_t *descbuf, u_int16_t *bufsize,
				    struct ipsec_encap_pdb *pdb,
				    u_int8_t *opthdr,
				    struct cipherparams *cipherdata,
				    struct authparams *authdata);

int32_t cnstr_shdsc_ipsec_decap(u_int32_t *descbuf, u_int16_t *bufsize,
				struct ipsec_decap_pdb *pdb,
				struct cipherparams *cipherdata,
				struct authparams *authdata);


int32_t cnstr_shdsc_wimax_encap(u_int32_t *descbuf, u_int16_t *bufsize,
				struct wimax_encap_pdb *pdb,
				struct cipherparams *cipherdata,
				u_int8_t mode);

int32_t cnstr_shdsc_wimax_decap(u_int32_t *descbuf, u_int16_t *bufsize,
				struct wimax_decap_pdb *pdb,
				struct cipherparams *cipherdata,
				u_int8_t mode);

int32_t cnstr_shdsc_macsec_encap(u_int32_t *descbuf, u_int16_t *bufsize,
				 struct macsec_encap_pdb *pdb,
				 struct cipherparams *cipherdata);

int32_t cnstr_shdsc_macsec_decap(u_int32_t *descbuf, u_int16_t *bufsize,
				 struct macsec_decap_pdb *pdb,
				 struct cipherparams *cipherdata);

/*
 * Non protocol sharedesc constructors
 */
int32_t cnstr_shdsc_snow_f8(u_int32_t *descbuf, u_int16_t *bufsize,
			    u_int8_t *key, u_int32_t keylen,
			    enum algdir dir, u_int32_t count,
			    u_int8_t bearer, u_int8_t direction);

int32_t cnstr_shdsc_snow_f9(u_int32_t *descbuf, u_int16_t *bufsize,
			    u_int8_t *key, u_int32_t keylen,
			    enum algdir dir, u_int32_t count,
			    u_int32_t fresh, u_int8_t direction);

int32_t cnstr_shdsc_kasumi_f8(u_int32_t *descbuf, u_int16_t *bufsize,
			      u_int8_t *key, u_int32_t keylen,
			      enum algdir dir, u_int32_t count,
			      u_int8_t bearer, u_int8_t direction);

int32_t cnstr_shdsc_kasumi_f9(u_int32_t *descbuf, u_int16_t *bufsize,
			      u_int8_t *key, u_int32_t keylen,
			      enum algdir dir, u_int32_t count,
			      u_int32_t fresh, u_int8_t direction);

int32_t cnstr_shdsc_cbc_blkcipher(u_int32_t *descbuf, u_int16_t *bufsize,
				  u_int8_t *key, u_int32_t keylen,
				  u_int8_t *iv, u_int32_t ivlen,
				  enum algdir dir, u_int32_t cipher,
				  u_int8_t clear);

int32_t cnstr_shdsc_aes_gcm(u_int32_t *descbuf, u_int16_t *bufsize,
			    u_int8_t *key, u_int32_t keylen, u_int8_t *ctx,
			    enum mdstatesel mdstate, enum icvsel icv,
			    enum algdir dir);

int32_t cnstr_shdsc_hmac(u_int32_t *descbuf, u_int16_t *bufsize,
			 u_int8_t *key, u_int32_t cipher, u_int8_t *icv,
			 u_int8_t clear);

int32_t cnstr_pcl_shdsc_3gpp_rlc_decap(u_int32_t *descbuf, u_int16_t *bufsize,
				       u_int8_t *key, u_int32_t keysz,
				       u_int32_t count, u_int32_t bearer,
				       u_int32_t direction,
				       u_int16_t payload_sz);

int32_t cnstr_pcl_shdsc_3gpp_rlc_encap(u_int32_t *descbuf, u_int16_t *bufsize,
				       u_int8_t *key, u_int32_t keysz,
				       u_int32_t count, u_int32_t bearer,
				       u_int32_t direction,
				       u_int16_t payload_sz);

/*
 * Section 5 - disassembler definitions
 */

/* Disassembler options */
#define DISASM_SHOW_OFFSETS	0x01 /* display instruction indices */
#define DISASM_SHOW_RAW		0x02 /* display each raw instruction */

void desc_hexdump(u_int32_t *descdata, u_int32_t  size, u_int32_t wordsperline,
		  int8_t *indentstr);

void caam_desc_disasm(u_int32_t *desc, u_int32_t opts);

#endif /* DCL_H */
