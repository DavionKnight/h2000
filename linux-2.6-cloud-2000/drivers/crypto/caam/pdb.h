/*
 * CAAM Protocol Data Block (PDB) definition header file
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

#ifndef CAAM_PDB_H
#define CAAM_PDB_H

/*
 * General IPSec encap/decap PDB definitions
 */
struct ipsec_encap_cbc {
	__be32 iv[4];
} __packed;

struct ipsec_encap_ctr {
	__be32 ctr_nonce;
	__be32 ctr_initial;
	__be32 iv[2];
} __packed;

struct ipsec_encap_ccm {
	__be32 salt; /* lower 24 bits */
	u8 b0_flags;
	u8 ctr_flags;
	__be16 ctr_initial;
	__be32 iv[2];
} __packed;

struct ipsec_encap_gcm {
	__be32 salt; /* lower 24 bits */
	__be32 rsvd1;
	__be32 iv[2];
} __packed;

struct ipsec_encap_pdb {
	__be32 desc_hdr;
	u8 rsvd;
	u8 ip_nh;
	u8 ip_nh_offset;
	u8 options;
	__be32 seq_num_ext_hi;
	__be32 seq_num;
	union {
		struct ipsec_encap_cbc cbc;
		struct ipsec_encap_ctr ctr;
		struct ipsec_encap_ccm ccm;
		struct ipsec_encap_gcm gcm;
	};
	__be32 spi;
	__be16 rsvd1;
	__be16 ip_hdr_len;
	__be32 ip_hdr[0]; /* optional IP Header content */
} __packed;

struct ipsec_decap_cbc {
	__be32 rsvd[2];
} __packed;

struct ipsec_decap_ctr {
	__be32 salt;
	__be32 ctr_initial;
} __packed;

struct ipsec_decap_ccm {
	__be32 salt;
	u8 iv_flags;
	u8 ctr_flags;
	__be16 ctr_initial;
} __packed;

struct ipsec_decap_gcm {
	__be32 salt;
	__be32 resvd;
} __packed;

struct ipsec_decap_pdb {
	__be32 desc_hdr;
	__be16 ip_hdr_len;
	u8 ip_nh_offset;
	u8 options;
	union {
		struct ipsec_decap_cbc cbc;
		struct ipsec_decap_ctr ctr;
		struct ipsec_decap_ccm ccm;
		struct ipsec_decap_gcm gcm;
	};
	__be32 seq_num_ext_hi;
	__be32 seq_num;
	__be32 anti_replay[2];
	__be32 end_index[0];
} __packed;

/*
 * IEEE 802.11i WiFi Protocol Data Block
 */
#define WIFI_PDBOPTS_FCS	0x01
#define WIFI_PDBOPTS_AR		0x40

struct wifi_encap_pdb {
	__be32 desc_hdr;
	__be16 mac_hdr_len;
	u8 rsvd;
	u8 options;
	u8 iv_flags;
	u8 pri;
	__be16 pn1;
	__be32 pn2;
	__be16 frm_ctrl_mask;
	__be16 seq_ctrl_mask;
	u8 rsvd1[2];
	u8 cnst;
	u8 key_id;
	u8 ctr_flags;
	u8 rsvd2;
	__be16 ctr_init;
} __packed;

struct wifi_decap_pdb {
	__be32 desc_hdr;
	__be16 mac_hdr_len;
	u8 rsvd;
	u8 options;
	u8 iv_flags;
	u8 pri;
	__be16 pn1;
	__be32 pn2;
	__be16 frm_ctrl_mask;
	__be16 seq_ctrl_mask;
	u8 rsvd1[4];
	u8 ctr_flags;
	u8 rsvd2;
	__be16 ctr_init;
} __packed;

/*
 * IEEE 802.16 WiMAX Protocol Data Block
 */
#define WIMAX_PDBOPTS_FCS	0x01
#define WIMAX_PDBOPTS_AR	0x40 /* decap only */

struct wimax_encap_pdb {
	__be32 desc_hdr;
	u8 rsvd[3];
	u8 options;
	__be32 nonce;
	u8 b0_flags;
	u8 ctr_flags;
	__be16 ctr_init;
	/* begin DECO writeback region */
	__be32 pn;
	/* end DECO writeback region */
} __packed;

struct wimax_decap_pdb {
	__be32 desc_hdr;
	u8 rsvd[3];
	u8 options;
	__be32 nonce;
	u8 iv_flags;
	u8 ctr_flags;
	__be16 ctr_init;
	/* begin DECO writeback region */
	__be32 pn;
	u8 rsvd1[2];
	__be16 antireplay_len;
	__be64 antireplay_scorecard;
	/* end DECO writeback region */
} __packed;

/*
 * IEEE 801.AE MacSEC Protocol Data Block
 */
#define MACSEC_PDBOPTS_FCS	0x01
#define MACSEC_PDBOPTS_AR	0x40 /* used in decap only */

struct macsec_encap_pdb {
	__be32 desc_hdr;
	__be16 aad_len;
	u8 rsvd;
	u8 options;
	__be64 sci;
	__be16 ethertype;
	u8 tci_an;
	u8 rsvd1;
	/* begin DECO writeback region */
	__be32 pn;
	/* end DECO writeback region */
} __packed;

struct macsec_decap_pdb {
	__be32 desc_hdr;
	__be16 aad_len;
	u8 rsvd;
	u8 options;
	__be64 sci;
	u8 rsvd1[3];
	/* begin DECO writeback region */
	u8 antireplay_len;
	__be32 pn;
	__be64 antireplay_scorecard;
	/* end DECO writeback region */
} __packed;

/*
 * SSL/TLS/DTLS Protocol Data Blocks
 */

#define TLS_PDBOPTS_ARS32	0x40
#define TLS_PDBOPTS_ARS64	0xc0
#define TLS_PDBOPTS_OUTFMT	0x08
#define TLS_PDBOPTS_IV_WRTBK	0x02 /* 1.1/1.2/DTLS only */
#define TLS_PDBOPTS_EXP_RND_IV	0x01 /* 1.1/1.2/DTLS only */

struct tls_block_encap_pdb {
	__be32 desc_hdr;
	u8 type;
	u8 version[2];
	u8 options;
	__be64 seq_num;
	__be32 iv[4];
} __packed;

struct tls_stream_encap_pdb {
	__be32 desc_hdr;
	u8 type;
	u8 version[2];
	u8 options;
	__be64 seq_num;
	u8 i;
	u8 j;
	u8 rsvd1[2];
} __packed;

struct dtls_block_encap_pdb {
	__be32 desc_hdr;
	u8 type;
	u8 version[2];
	u8 options;
	__be16 epoch;
	__be16 seq_num[3];
	__be32 iv[4];
} __packed;

struct tls_block_decap_pdb {
	__be32 desc_hdr;
	u8 rsvd[3];
	u8 options;
	__be64 seq_num;
	__be32 iv[4];
} __packed;

struct tls_stream_decap_pdb {
	__be32 desc_hdr;
	u8 rsvd[3];
	u8 options;
	__be64 seq_num;
	u8 i;
	u8 j;
	u8 rsvd1[2];
} __packed;

struct dtls_block_decap_pdb {
	__be32 desc_hdr;
	u8 rsvd[3];
	u8 options;
	__be16 epoch;
	__be16 seq_num[3];
	__be32 iv[4];
	__be64 antireplay_scorecard;
} __packed;

/*
 * SRTP Protocol Data Blocks
 */
#define SRTP_PDBOPTS_MKI	0x08
#define SRTP_PDBOPTS_AR		0x40

struct srtp_encap_pdb {
	__be32 desc_hdr;
	u8 x_len;
	u8 mki_len;
	u8 n_tag;
	u8 options;
	__be32 cnst0;
	u8 rsvd[2];
	__be16 cnst1;
	__be16 salt[7];
	__be16 cnst2;
	__be32 rsvd1;
	__be32 roc;
	__be32 opt_mki;
} __packed;

struct srtp_decap_pdb {
	__be32 desc_hdr;
	u8 x_len;
	u8 mki_len;
	u8 n_tag;
	u8 options;
	__be32 cnst0;
	u8 rsvd[2];
	__be16 cnst1;
	__be16 salt[7];
	__be16 cnst2;
	__be16 rsvd1;
	__be16 seq_num;
	__be32 roc;
	__be64 antireplay_scorecard;
} __packed;

/*
 * DSA/ECDSA Protocol Data Blocks
 * Two of these exist: DSA-SIGN, and DSA-VERIFY. They are similar
 * except for the treatment of "w" for verify, "s" for sign,
 * and the placement of "a,b".
 */
#define DSA_PDB_SGF_SHIFT	24
#define DSA_PDB_SGF_MASK	(0xff << DSA_PDB_SGF_SHIFT)
#define DSA_PDB_SGF_Q		(0x80 << DSA_PDB_SGF_SHIFT)
#define DSA_PDB_SGF_R		(0x40 << DSA_PDB_SGF_SHIFT)
#define DSA_PDB_SGF_G		(0x20 << DSA_PDB_SGF_SHIFT)
#define DSA_PDB_SGF_W		(0x10 << DSA_PDB_SGF_SHIFT)
#define DSA_PDB_SGF_S		(0x10 << DSA_PDB_SGF_SHIFT)
#define DSA_PDB_SGF_F		(0x08 << DSA_PDB_SGF_SHIFT)
#define DSA_PDB_SGF_C		(0x04 << DSA_PDB_SGF_SHIFT)
#define DSA_PDB_SGF_D		(0x02 << DSA_PDB_SGF_SHIFT)
#define DSA_PDB_SGF_AB_SIGN	(0x02 << DSA_PDB_SGF_SHIFT)
#define DSA_PDB_SGF_AB_VERIFY	(0x01 << DSA_PDB_SGF_SHIFT)

#define DSA_PDB_L_SHIFT		7
#define DSA_PDB_L_MASK		(0x3ff << DSA_PDB_L_SHIFT)

#define DSA_PDB_N_MASK		0x7f

struct dsa_sign_pdb {
	__be32 desc_hdr;
	__be32 sgf_ln; /* Use DSA_PDB_ defintions per above */
	u8 *q;
	u8 *r;
	u8 *g;	/* or Gx,y */
	u8 *s;
	u8 *f;
	u8 *c;
	u8 *d;
	u8 *ab;  /* ECC only */
	u8 *u;
} __packed;

struct dsa_verify_pdb {
	__be32 desc_hdr;
	__be32 sgf_ln;
	u8 *q;
	u8 *r;
	u8 *g;	/* or Gx,y */
	u8 *w;  /* or Wx,y */
	u8 *f;
	u8 *c;
	u8 *d;
	u8 *tmp; /* temporary data block */
	u8 *ab;  /* only used if ECC processing */
} __packed;

#endif
