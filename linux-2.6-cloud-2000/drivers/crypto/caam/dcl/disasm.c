/*
 * CAAM Descriptor Construction Library
 * Descriptor Disassembler
 *
 * This is EXPERIMENTAL and incomplete code. It assumes BE32 for the
 * moment, and much functionality remains to be filled in
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

#include "../compat.h"
#include "dcl.h"

#define MAX_LEADER_LEN 31 /* offset + raw + instruction-name-length */

/* Descriptor header/shrheader share enums */
static const char *deschdr_share[] = {
	"never", "wait", "serial", "always", "defer",
};

/* KEY/SEQ_KEY instruction-specific class enums */
static const char *key_class[] = {
	"<rsvd>", "class1", "class2", "<rsvd>",
};

/* LOAD/STORE instruction-specific class enums */
static const char *ldst_class[] = {
	"class-ind-ccb", "class-1-ccb", "class-2-ccb", "deco",
};

/* FIFO_LOAD/FIFO_STORE instruction-specific class enums */
static const char *fifoldst_class[] = {
	"skip", "class1", "class2", "both",
};

/* KEY/SEQ_KEY instruction destination enums */
static const char *key_dest[] = {
	"keyreg", "pk-e", "af-sbox", "md-split",
};

/* FIFO_STORE/SEQ_FIFO_STORE output data type enums */
static const char *fifo_output_data_type[] = {
	"pk-a0", "pk-a1", "pk-a2", "pk-a3",
	"pk-b0", "pk-b1", "pk-b2", "pk-b3",
	"pk-n", "<rsvd>", "<rsvd>", "<rsvd>",
	"pk-a", "pk-b", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"afha-s-jdk", "afha-s-tdk", "pkha-e-jdk", "pkha-e-tdk",
	"keyreg-jdk", "keyreg-tdk", "mdsplit-jdk", "mdsplit-tdk",
	"outfifo-jdk", "outfifo-tdk", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"msgdata", "<rsvd>", "<rsvd>", "<rsvd>",
	"rng-ref", "rng-outfifo", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "seqfifo-skip",
};

/* LOAD/STORE instruction source/destination by class */
static const char *ldstr_srcdst[4][0x80] = {
{
	/* Class-independent CCB destination set */
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "cha-ctrl", "irq-ctrl",
	"clrw", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "info-fifo", "<rsvd>",
	"indata-fifo", "<rsvd>", "output-fifo", "<rsvd>",
},
{
	/* Class1 CCB destination set */
	"class1-mode", "class1-keysz", "class1-datasz", "class1-icvsz",
	"<rsvd>", "<rsvd>", "<rsvd>",  "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "aadsz",
	"class1-ivsz", "<rsvd>", "<rsvd>", "class1-altdsz",
	"pk-a-sz", "pk-b-sz", "pk-n-sz", "pk-e-sz",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"class1-ctx", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"class1-key", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
},
{
	/* Class2 CCB destination set */
	"class2-mode", "class2-keysz", "class2-datasz", "class2-ivsz",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"class2-ctx", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"class2-key", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>",  "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
},
{
	/* DECO destination set */
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "deco-ctrl", "deco-povrd",
	"deco-math0", "deco-math1", "deco-math2", "deco-math3",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"descbuf", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
} };

/* JUMP instruction destination type enums */
static const char *jump_types[] = {
	"local", "nonlocal", "halt", "halt-user",
};

/* JUMP instruction test enums */
static const char *jump_tests[] = {
	"all", "!all", "any", "!any",
};

/* LOAD_FIFO/SEQ_LOAD_FIFO instruction PK input type enums */
static const char *load_pkha_inp_types[] = {
	"a0", "a1", "a2", "a3",
	"b0", "b1", "b2", "b3",
	"n", "<rsvd>", "<rsvd>", "<rsvd>",
	"a", "b", "<rsvd>", "<rsvd>",
};

/* LOAD_FIFO/SEQ_LOAD_FIFO instruction non-PK input type enums */
static const char *load_inp_types[] = {
	"<rsvd>", "<rsvd>", "msgdata", "msgdata1->2",
	"iv", "bitlendata",
};

/* MOVE instruction source enums */
static const char *move_src[] = {
	"class1-ctx", "class2-ctx", "out-fifo", "descbuf",
	"math0", "math1", "math2", "math3",
	"inp-fifo",
};

/* MOVE instruction destination enums */
static const char *move_dst[] = {
	"class1-ctx", "class2-ctx", "output-fifo", "descbuf",
	"math0", "math1", "math2", "math3",
	"class1-inp-fifo", "class2-inp-fifo", "<rsvd>", "<rsvd>",
	"pk-a", "class1-key", "class2-key", "<rsvd>",
};

/* MATH instruction source 0 enumerations */
static const char *math_src0[] = {
	"math0", "math1", "math2", "math3",
	"imm", "<rsvd>", "<rsvd>", "<rsvd>",
	"seqin", "seqout", "vseqin", "vseqout",
	"0", "<rsvd>", "<rsvd>", "<rsvd>",
};

/* MATH instruction source1 enumerations (not same as src0) */
static const char *math_src1[] = {
	"math0", "math1", "math2", "math3",
	"imm", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "inp-fifo", "out-fifo",
	"1", "<rsvd>", "<rsvd>", "<rsvd>",
};

/* MATH instruction destination enumerations */
static const char *math_dest[] = {
	"math0", "math1", "math2", "math3",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"seqin", "seqout", "vseqin", "vseqout",
	"<rsvd>", "<rsvd>", "<rsvd>", "<none>",
};

/* MATH instruction function enumerations */
static const char *math_fun[] = {
	"add", "addc", "sub", "subb",
	"or", "and", "xor", "lsh",
	"rsh", "lshd", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
};

/* SIGNATURE instruction type enumerations */
static const char *sig_type[] = {
	"final", "final-restore", "final-nonzero", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "imm-2", "imm-3",
	"imm-4", "<rsvd>", "<rsvd>", "<rsvd>",
};

/* OPERATION instruction unidirectional protocol enums */
static const char *unidir_pcl[] = {
	"<rsvd> ", "ikev1-prf ", "ikev2-prf ", "<rsvd> ",
	"<rsvd> ", "<rsvd> ", "<rsvd> ", "<rsvd> ",
	"ssl3.0-prf ", "tls1.0-prf ", "tls1.1-prf ", "<rsvd> ",
	"dtls1.0-prf ", "blob ", "<rsvd> ", "<rsvd> ",
	"<rsvd> ", "<rsvd> ", "<rsvd> ", "<rsvd> ",
	"pk-pargen ", "dsa-sign ", "dsa-verify ", "<rsvd> ",
	"<rsvd> ", "<rsvd> ", "<rsvd> ", "<rsvd> ",
	"<rsvd> ", "<rsvd> ", "<rsvd> ", "<rsvd> ",
};

/* OPERATION instruction protocol info cipher types - IPSec/SRTP */
static const char *ipsec_pclinfo_cipher[] = {
	"<rsvd>", "des", "des", "3des",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"aes-cbc", "aes-ctr", "aes-ccm8", "aes-ccm12",
	"aes-ccm16", "<rsvd>", "aes-gcm8", "aes-gcm12",
	"aes-gcm16", "<rsvd>", "aes-xts", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
};

/* OPERATION instruction protocol info authentication types - IPSec/SRTP */
static const char *ipsec_pclinfo_auth[] = {
	"<none>", "hmac-md5-96", "hmac-sha1-96", "<rsvd>",
	"<rsvd>", "aes-xcbcmac-96", "hmac-md5-128", "hmac-sha1-160",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"hmac-sha2-256-128", "hmac-sha2-384-192",
	"hmac-sha2-512-256", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
};

/* OPERATION instruction PKHA algorithmic functions (PKHA_MODE_LS) */
static const char *pk_function[] = {
	"<rsvd>", "clrmem", "a+b%n", "a-b%n",
	"b-a%n", "a*b%n", "a^e%n", "a%n",
	"a^-1%n", "ecc-p1+p2", "ecc-p1+p1", "ecc-e*p1",
	"monty-const", "crt-const", "gcd(a,n)", "miller-rabin",
	"cpymem-n-sz", "cpymem-src-sz", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
	"<rsvd>", "<rsvd>", "<rsvd>", "<rsvd>",
};

static const char *pk_srcdst[] = {
	"a",
	"b",
	"e", /* technically not legal for a source, legal as dest */
	"n",
};

/*
 * Simple hexdumper for use by the disassembler. Displays 32-bit
 * words on a line-by-line bases with an offset shown,. and an
 * optional indentation/description string to prefix each line with.
 *
 * descdata     - data to dump
 * size         - size of buffer in words
 * wordsperline - number of words to display per line, minimum 1.
 *                4 is a practical maximum using an 80-character line
 * indentstr    - points to a string to ident or identify each line
 */
void desc_hexdump(u_int32_t *descdata,
		  u_int32_t  size,
		  u_int32_t  wordsperline,
		  int8_t    *leader)
{
	int i, idx, rem, line;

	idx = 0;
	rem = size;

	while (rem) {
		PRINT("%s[%02d] ", leader, idx);
		if (rem <= wordsperline)
			line = rem;
		else
			line = wordsperline;

		for (i = 0; i < line; i++) {
			PRINT("0x%08x ", descdata[idx]);
			rem--; idx++;
		}
		PRINT("\n");
	};
}
EXPORT_SYMBOL(desc_hexdump);

static void show_shrhdr(u_int32_t *hdr)
{
	PRINT("   shrdesc: stidx=%d share=%s ",
	      (*hdr >> HDR_START_IDX_SHIFT) & HDR_START_IDX_MASK,
	      deschdr_share[(*hdr >> HDR_SD_SHARE_SHIFT) & HDR_SD_SHARE_MASK]);

	if (*hdr & HDR_DNR)
		PRINT("noreplay ");

	if (*hdr & HDR_SAVECTX)
		PRINT("savectx ");

	if (*hdr & HDR_PROP_DNR)
		PRINT("propdnr ");

	PRINT("len=%d\n", *hdr & HDR_DESCLEN_SHR_MASK);
}

static void show_hdr(u_int32_t *hdr)
{
	if (*hdr & HDR_SHARED) {
		PRINT("   jobdesc: shrsz=%d ",
		      (*hdr >> HDR_START_IDX_SHIFT) & HDR_START_IDX_MASK);
	} else {
		PRINT("   jobdesc: stidx=%d ",
		      (*hdr >> HDR_START_IDX_SHIFT) & HDR_START_IDX_MASK);
	}
	PRINT("share=%s ",
	      deschdr_share[(*hdr >> HDR_SD_SHARE_SHIFT) & HDR_SD_SHARE_MASK]);

	if (*hdr & HDR_DNR)
		PRINT("noreplay ");

	if (*hdr & HDR_TRUSTED)
		PRINT("trusted ");

	if (*hdr & HDR_MAKE_TRUSTED)
		PRINT("mktrusted ");

	if (*hdr & HDR_SHARED)
		PRINT("getshared ");

	if (*hdr & HDR_REVERSE)
		PRINT("reversed ");

	PRINT("len=%d\n", *hdr & HDR_DESCLEN_MASK);
}

static void show_key(u_int32_t *cmd, u_int8_t *idx, int8_t *leader)
{
	u_int32_t keylen, *keydata;

	keylen = *cmd & KEY_LENGTH_MASK;
	keydata = cmd + 1; /* point to key or pointer */

	PRINT("       key: %s->%s len=%d ",
	      key_class[(*cmd & CLASS_MASK) >> CLASS_SHIFT],
	      key_dest[(*cmd & KEY_DEST_MASK) >> KEY_DEST_SHIFT],
	      keylen);

	if (*cmd & KEY_SGF)
		PRINT("s/g ");

	if (*cmd & KEY_ENC)
		PRINT("enc ");

	if (*cmd & KEY_IMM)
		PRINT("imm ");

	PRINT("\n");
	if (*cmd & KEY_IMM) {
		desc_hexdump(keydata, keylen >> 2, 4, leader);
		(*idx) += keylen >> 2;
	} else {
		PRINT("%s@0x%08x", leader, *keydata);
		if (sizeof(dma_addr_t) > sizeof(u32))
			PRINT("_%08x\n", *(keydata + 1));
		else
			PRINT("\n");
		/* key pointer follows instruction */
		*idx += sizeof(dma_addr_t) / sizeof(u32);
	}
	(*idx)++;
}

static void show_seq_key(u_int32_t *cmd, u_int8_t *idx, int8_t *leader)
{
	u_int32_t keylen, *keydata;

	keylen  = *cmd & KEY_LENGTH_MASK;
	keydata = cmd + 1;

	PRINT("    seqkey: %s->%s len=%d ",
	      key_class[(*cmd & CLASS_MASK) >> CLASS_SHIFT],
	      key_dest[(*cmd & KEY_DEST_MASK) >> KEY_DEST_SHIFT],
	      keylen);

	if (*cmd & KEY_VLF)
		PRINT("vlf ");

	if (*cmd & KEY_ENC)
		PRINT("enc ");

	if (*cmd & KEY_IMM)
		PRINT("imm ");

	PRINT("\n");
	if (*cmd & KEY_IMM) {
		desc_hexdump(keydata, keylen >> 2, 4, leader);
		(*idx) += keylen >> 2;
	} else {
		PRINT("%s@0x%08x\n", leader, *keydata);
		(*idx)++;
	}
	(*idx)++;
}

static void show_load(u_int32_t *cmd, u_int8_t *idx, int8_t *leader)
{
	u_int32_t ldlen, *lddata;
	u_int8_t class;

	ldlen  = *cmd & LDST_LEN_MASK;
	lddata = cmd + 1; /* point to key or pointer */

	class = (*cmd & CLASS_MASK) >> CLASS_SHIFT;
	PRINT("        ld: %s->%s len=%d offs=%d",
	      ldst_class[class],
	      ldstr_srcdst[class][(*cmd & LDST_SRCDST_MASK) >>
				  LDST_SRCDST_SHIFT],
	      (*cmd & LDST_LEN_MASK),
	      (*cmd & LDST_OFFSET_MASK) >> LDST_OFFSET_SHIFT);

	if (*cmd & LDST_SGF)
		PRINT(" s/g");

	if (*cmd & LDST_IMM)
		PRINT(" imm");

	PRINT("\n");

	/*
	 * Special case for immediate load to DECO control. In this case
	 * only, the immediate value is the bits in offset/length, NOT
	 * the data following the instruction, so, skip the trailing
	 * data processing step.
	 */

	if (((*cmd & LDST_CLASS_MASK) ==  LDST_CLASS_DECO) &&
	    ((*cmd & LDST_SRCDST_MASK) == LDST_SRCDST_WORD_DECOCTRL)) {
		(*idx)++;
		return;
	}

	if (*cmd & LDST_IMM) {
		desc_hexdump(lddata, ldlen >> 2, 4, leader);
		(*idx) += ldlen >> 2;
	} else {
		PRINT("%s@0x%08x\n", leader, *lddata);
		(*idx)++;
	}
	(*idx)++;
}

static void show_seq_load(u_int32_t *cmd, u_int8_t *idx, int8_t *leader)
{
	u_int8_t class;

	class = (*cmd & CLASS_MASK) >> CLASS_SHIFT;
	PRINT("     seqld: %s->%s len=%d offs=%d",
	      ldst_class[class],
	      ldstr_srcdst[class][(*cmd & LDST_SRCDST_MASK) >>
				  LDST_SRCDST_SHIFT],
	      (*cmd & LDST_LEN_MASK),
	      (*cmd & LDST_OFFSET_MASK) >> LDST_OFFSET_SHIFT);

	if (*cmd & LDST_VLF)
		PRINT(" vlf");

	PRINT("\n");
	(*idx)++;
}

static void show_fifo_load(u_int32_t *cmd, u_int8_t *idx, int8_t *leader)
{
	u_int32_t *trdata, len;

	len  = *cmd & FIFOLDST_LEN_MASK;
	trdata = cmd + 1;

	PRINT("    fifold: %s",
	      fifoldst_class[(*cmd & CLASS_MASK) >> CLASS_SHIFT]);

	if ((*cmd & FIFOLD_TYPE_PK_MASK) == FIFOLD_TYPE_PK)
		PRINT(" pk-%s",
		      load_pkha_inp_types[(*cmd & FIFOLD_TYPE_PK_TYPEMASK) >>
					  FIFOLD_TYPE_SHIFT]);
	else {
		PRINT(" %s",
		      load_inp_types[(*cmd & FIFOLD_TYPE_MSG_MASK) >>
				     FIFOLD_CONT_TYPE_SHIFT]);

		if (*cmd & FIFOLD_TYPE_LAST2)
			PRINT("-last2");

		if (*cmd & FIFOLD_TYPE_LAST1)
			PRINT("-last1");

		if (*cmd & FIFOLD_TYPE_FLUSH1)
			PRINT("-flush1");
	}

	PRINT(" len=%d", len);

	if (*cmd & FIFOLDST_SGF_MASK)
		PRINT(" s/g");

	if (*cmd & FIFOLD_IMM_MASK)
		PRINT(" imm");

	if (*cmd & FIFOLDST_EXT_MASK)
		PRINT(" ext");

	(*idx)++; /* Bump index either to extension or next instruction */

	PRINT("\n");
	if (*cmd & FIFOLD_IMM) {
		desc_hexdump(trdata, len >> 2, 4, leader);
		(*idx) += len >> 2;
	} else { /* is just trailing pointer */
		PRINT("%s@0x%08x\n", leader, *trdata);
		(*idx)++;
	}

	if (*cmd & FIFOLDST_EXT) {
		PRINT("%sextlen=%d\n", leader, *(++trdata));
		(*idx)++;
	}
}

static void show_seq_fifo_load(u_int32_t *cmd, u_int8_t *idx, int8_t *leader)
{
	u_int32_t *trdata, len;

	len  = *cmd & FIFOLDST_LEN_MASK;
	trdata = cmd + 1;

	PRINT(" seqfifold: %s",
	      fifoldst_class[(*cmd & CLASS_MASK) >> CLASS_SHIFT]);

	if ((*cmd & FIFOLD_TYPE_PK_MASK) == FIFOLD_TYPE_PK)
		PRINT(" pk-%s",
		      load_pkha_inp_types[(*cmd * FIFOLD_TYPE_PK_TYPEMASK) >>
					  FIFOLD_TYPE_SHIFT]);
	else {
		PRINT(" %s",
		      load_inp_types[(*cmd & FIFOLD_TYPE_MSG_MASK) >>
				     FIFOLD_CONT_TYPE_SHIFT]);

		if (*cmd & FIFOLD_TYPE_LAST2)
			PRINT("-last2");

		if (*cmd & FIFOLD_TYPE_LAST1)
			PRINT("-last1");

		if (*cmd & FIFOLD_TYPE_FLUSH1)
			PRINT("-flush1");
	}

	PRINT(" len=%d", len);

	if (*cmd & FIFOLDST_VLF_MASK)
		PRINT(" vlf");

	if (*cmd & FIFOLD_IMM_MASK)
		PRINT(" imm");

	if (*cmd & FIFOLDST_EXT_MASK)
		PRINT(" ext");

	PRINT("\n");

	*idx += sizeof(dma_addr_t) / sizeof(u32);

	if (*cmd & FIFOLD_IMM) {
		desc_hexdump(trdata, len >> 2, 4, leader);
		(*idx) += len >> 2;
	}

	if (*cmd & FIFOLDST_EXT) {
		PRINT("%sextlen=%d\n", leader, *(++trdata));
		(*idx)++;
	}
}

static void show_store(u_int32_t *cmd, u_int8_t *idx, int8_t *leader)
{
	u_int32_t stlen, *stdata;
	u_int8_t class;

	class = (*cmd & CLASS_MASK) >> CLASS_SHIFT;
	stlen  = *cmd & LDST_LEN_MASK;
	stdata = cmd + 1;

	PRINT("       str: %s %s len=%d offs=%d\n",
	      ldst_class[class],
	      ldstr_srcdst[class]
			  [(*cmd & LDST_SRCDST_MASK) >> LDST_SRCDST_SHIFT],
	      (*cmd & LDST_LEN_MASK) >> LDST_LEN_SHIFT,
	      (*cmd & LDST_OFFSET_MASK) >> LDST_OFFSET_SHIFT);

	if (*cmd & LDST_SGF)
		PRINT(" s/g");

	if (*cmd & LDST_IMM)
		PRINT(" imm");

	(*idx)++;

	if (*cmd & LDST_IMM) {
		desc_hexdump(stdata, stlen >> 2, 4, leader);
		(*idx) += stlen >> 2;
	} else {
		PRINT("%s@0x%08x\n", leader, *stdata);
		(*idx)++;
	}
}

static void show_seq_store(u_int32_t *cmd, u_int8_t *idx, int8_t *leader)
{
	u_int8_t class;

	class = (*cmd & CLASS_MASK) >> CLASS_SHIFT;

	PRINT("    seqstr: %s %s len=%d offs=%d\n",
	      ldst_class[class],
	      ldstr_srcdst[class]
			  [(*cmd & LDST_SRCDST_MASK) >> LDST_SRCDST_SHIFT],
	      (*cmd & LDST_LEN_MASK) >> LDST_LEN_SHIFT,
	      (*cmd & LDST_OFFSET_MASK) >> LDST_OFFSET_SHIFT);

	if (*cmd & LDST_VLF)
		PRINT(" vlf");

	if (*cmd & LDST_IMM)
		PRINT(" imm");

	(*idx)++;
}

static void show_fifo_store(u_int32_t *cmd, u_int8_t *idx, int8_t *leader)
{
	u_int32_t *trdata, len;

	len  = *cmd & FIFOLDST_LEN_MASK;
	trdata = cmd + 1;

	PRINT("   fifostr: %s %s len=%d",
	      fifoldst_class[(*cmd & CLASS_MASK) >> CLASS_SHIFT],
	      fifo_output_data_type[(*cmd & FIFOST_TYPE_MASK) >>
				    FIFOST_TYPE_SHIFT], len);

	if (*cmd & FIFOLDST_SGF_MASK)
		PRINT(" s/g");

	if (*cmd & FIFOST_CONT_MASK)
		PRINT(" cont");

	if (*cmd & FIFOLDST_EXT_MASK)
		PRINT(" ext");

	PRINT("\n");
	(*idx)++;

	if (*cmd & FIFOST_IMM) {
		desc_hexdump(trdata, len >> 2, 4, leader);
		(*idx) += len >> 2;
	} else {
		PRINT("%s@0x%08x", leader, *trdata);
		if (sizeof(dma_addr_t) > sizeof(u32))
			PRINT("_%08x\n", *(trdata + 1));
		else
			PRINT("\n");
		*idx += sizeof(dma_addr_t) / sizeof(u32);
	}

	if (*cmd & FIFOLDST_EXT) {
		PRINT("%sextlen=%d\n", leader, *(++trdata));
		(*idx)++;
	}
}

static void show_seq_fifo_store(u_int32_t *cmd, u_int8_t *idx, int8_t *leader)
{
	u_int32_t pcmd, *trdata, len;

	len  = *cmd & FIFOLDST_LEN_MASK;
	trdata = cmd + 1;

	PRINT("seqfifostr: %s %s len=%d",
	      fifoldst_class[(*cmd & CLASS_MASK) >> CLASS_SHIFT],
	      fifo_output_data_type[(*cmd & FIFOST_TYPE_MASK) >>
				    FIFOST_TYPE_SHIFT], len);

	if (*cmd & FIFOLDST_VLF_MASK)
		PRINT(" vlf");

	if (*cmd & FIFOST_CONT_MASK)
		PRINT(" cont");

	if (*cmd & FIFOLDST_EXT_MASK)
		PRINT(" ext");

	PRINT("\n");
	pcmd = *cmd;
	(*idx)++; /* Bump index either to extension or next instruction */

	if (pcmd & FIFOST_IMM) {
		desc_hexdump(trdata, len >> 2, 4, leader);
		(*idx) += len >> 2;
	}

	if (pcmd & FIFOLDST_EXT) {
		PRINT("%sextlen=%d\n", leader, *(++trdata));
		(*idx)++;
	}
}

static void show_move(u_int32_t *cmd, u_int8_t *idx, int8_t *leader)
{
	PRINT("      move: %s->%s len=%d offs=%d",
	      move_src[(*cmd & MOVE_SRC_MASK) >> MOVE_SRC_SHIFT],
	      move_dst[(*cmd & MOVE_DEST_MASK) >> MOVE_DEST_SHIFT],
	      (*cmd & MOVE_LEN_MASK) >> MOVE_LEN_SHIFT,
	      (*cmd & MOVE_OFFSET_MASK) >> MOVE_OFFSET_SHIFT);

	if (*cmd & MOVE_WAITCOMP)
		PRINT("wait ");

	PRINT("\n");
	(*idx)++;
}

/* need a BUNCH of these decoded... */
static void decode_bidir_pcl_op(u_int32_t *cmd)
{
	switch (*cmd & OP_PCLID_MASK) {
	case OP_PCLID_IPSEC:
		PRINT("ipsec %s %s ",
		      ipsec_pclinfo_cipher[(*cmd & OP_PCL_IPSEC_CIPHER_MASK) >>
					   8],
		      ipsec_pclinfo_auth[(*cmd & OP_PCL_IPSEC_AUTH_MASK)]);
		break;

	case OP_PCLID_SRTP:
		PRINT("srtp %s %s ",
		      ipsec_pclinfo_cipher[(*cmd & OP_PCL_IPSEC_CIPHER_MASK) >>
		      8],
		      ipsec_pclinfo_auth[(*cmd & OP_PCL_IPSEC_AUTH_MASK)]);
		break;

	case OP_PCLID_MACSEC:
		PRINT("macsec ");
		if ((*cmd & OP_PCLINFO_MASK) == OP_PCL_MACSEC)
			PRINT("aes-ccm-8 ");
		else
			PRINT("<rsvd 0x%04x> ", *cmd & OP_PCLINFO_MASK);
		break;

	case OP_PCLID_WIFI:
		PRINT("wifi ");
		if ((*cmd & OP_PCLINFO_MASK) == OP_PCL_WIFI)
			PRINT("aes-gcm-16 ");
		else
			PRINT("<rsvd 0x%04x> ", *cmd & OP_PCLINFO_MASK);
		break;

	case OP_PCLID_WIMAX:
		PRINT("wimax ");
		switch (*cmd & OP_PCLINFO_MASK) {
		case OP_PCL_WIMAX_OFDM:
			PRINT("ofdm ");
			break;

		case OP_PCL_WIMAX_OFDMA:
			PRINT("ofdma ");
			break;

		default:
			PRINT("<rsvd 0x%04x> ", *cmd & OP_PCLINFO_MASK);
		}
		break;

	case OP_PCLID_SSL30:
		PRINT("ssl3.0 ");
		PRINT("pclinfo=0x%04x ", *cmd & OP_PCLINFO_MASK);
		break;

	case OP_PCLID_TLS10:
		PRINT("tls1.0 ");
		PRINT("pclinfo=0x%04x ", *cmd & OP_PCLINFO_MASK);
		break;

	case OP_PCLID_TLS11:
		PRINT("tls1.1 ");
		PRINT("pclinfo=0x%04x ", *cmd & OP_PCLINFO_MASK);
		break;

	case OP_PCLID_TLS12:
		PRINT("tls1.2 ");
		PRINT("pclinfo=0x%04x ", *cmd & OP_PCLINFO_MASK);
		break;

	case OP_PCLID_DTLS:
		PRINT("dtls ");
		PRINT("pclinfo=0x%04x ", *cmd & OP_PCLINFO_MASK);
		break;
	}
}

static void decode_class12_op(u_int32_t *cmd)
{
	/* Algorithm type */
	switch (*cmd & OP_ALG_ALGSEL_MASK) {
	case OP_ALG_ALGSEL_AES:
		PRINT("aes ");
		break;

	case OP_ALG_ALGSEL_DES:
		PRINT("des ");
		break;

	case OP_ALG_ALGSEL_3DES:
		PRINT("3des ");
		break;

	case OP_ALG_ALGSEL_ARC4:
		PRINT("arc4 ");
		break;

	case OP_ALG_ALGSEL_MD5:
		PRINT("md5 ");
		break;

	case OP_ALG_ALGSEL_SHA1:
		PRINT("sha1 ");
		break;

	case OP_ALG_ALGSEL_SHA224:
		PRINT("sha224 ");
		break;

	case OP_ALG_ALGSEL_SHA256:
		PRINT("sha256 ");
		break;

	case OP_ALG_ALGSEL_SHA384:
		PRINT("sha384 ");
		break;

	case OP_ALG_ALGSEL_SHA512:
		PRINT("sha512 ");
		break;

	case OP_ALG_ALGSEL_RNG:
		PRINT("rng ");
		break;

	case OP_ALG_ALGSEL_SNOW:
		PRINT("snow ");
		break;

	case OP_ALG_ALGSEL_KASUMI:
		PRINT("kasumi ");
		break;

	case OP_ALG_ALGSEL_CRC:
		PRINT("crc ");
		break;

	default:
		PRINT("<rsvd> ");
	}

	/* Additional info */
	switch (*cmd & OP_ALG_ALGSEL_MASK) {
	case OP_ALG_ALGSEL_AES:
		switch (*cmd & OP_ALG_AAI_MASK) {
		case OP_ALG_AAI_CTR_MOD128:
			PRINT("ctr128 ");
			break;

		case OP_ALG_AAI_CTR_MOD8:
			PRINT("ctr8 ");
			break;

		case OP_ALG_AAI_CTR_MOD16:
			PRINT("ctr16 ");
			break;

		case OP_ALG_AAI_CTR_MOD24:
			PRINT("ctr24 ");
			break;

		case OP_ALG_AAI_CTR_MOD32:
			PRINT("ctr32 ");
			break;

		case OP_ALG_AAI_CTR_MOD40:
			PRINT("ctr40 ");
			break;

		case OP_ALG_AAI_CTR_MOD48:
			PRINT("ctr48 ");
			break;

		case OP_ALG_AAI_CTR_MOD56:
			PRINT("ctr56 ");
			break;

		case OP_ALG_AAI_CTR_MOD64:
			PRINT("ctr64 ");
			break;

		case OP_ALG_AAI_CTR_MOD72:
			PRINT("ctr72 ");
			break;

		case OP_ALG_AAI_CTR_MOD80:
			PRINT("ctr80 ");
			break;

		case OP_ALG_AAI_CTR_MOD88:
			PRINT("ctr88 ");
			break;

		case OP_ALG_AAI_CTR_MOD96:
			PRINT("ctr96 ");
			break;

		case OP_ALG_AAI_CTR_MOD104:
			PRINT("ctr104 ");
			break;

		case OP_ALG_AAI_CTR_MOD112:
			PRINT("ctr112 ");
			break;

		case OP_ALG_AAI_CTR_MOD120:
			PRINT("ctr120 ");
			break;

		case OP_ALG_AAI_CBC:
			PRINT("cbc ");
			break;

		case OP_ALG_AAI_ECB:
			PRINT("ecb ");
			break;

		case OP_ALG_AAI_CFB:
			PRINT("cfb ");
			break;

		case OP_ALG_AAI_OFB:
			PRINT("ofb ");
			break;

		case OP_ALG_AAI_XTS:
			PRINT("xts ");
			break;

		case OP_ALG_AAI_CMAC:
			PRINT("cmac ");
			break;

		case OP_ALG_AAI_XCBC_MAC:
			PRINT("xcbc-mac ");
			break;

		case OP_ALG_AAI_CCM:
			PRINT("ccm ");
			break;

		case OP_ALG_AAI_GCM:
			PRINT("gcm ");
			break;

		case OP_ALG_AAI_CBC_XCBCMAC:
			PRINT("cbc-xcbc-mac ");
			break;

		case OP_ALG_AAI_CTR_XCBCMAC:
			PRINT("ctr-xcbc-mac ");
			break;

		case OP_ALG_AAI_DK:
			PRINT("dk ");
			break;
		}
		break;

	case OP_ALG_ALGSEL_DES:
	case OP_ALG_ALGSEL_3DES:
		switch (*cmd & OP_ALG_AAI_MASK) {
		case OP_ALG_AAI_CBC:
			PRINT("cbc ");
			break;

		case OP_ALG_AAI_ECB:
			PRINT("ecb ");
			break;

		case OP_ALG_AAI_CFB:
			PRINT("cfb ");
			break;

		case OP_ALG_AAI_OFB:
			PRINT("ofb ");
			break;

		case OP_ALG_AAI_CHECKODD:
			PRINT("chkodd ");
			break;
		}
		break;

	case OP_ALG_ALGSEL_RNG:
		switch (*cmd & OP_ALG_AAI_MASK) {
		case OP_ALG_AAI_RNG:
			PRINT("rng ");
			break;

		case OP_ALG_AAI_RNG_NOZERO:
			PRINT("rng-no0 ");
			break;

		case OP_ALG_AAI_RNG_ODD:
			PRINT("rngodd ");
			break;
		}
		break;


	case OP_ALG_ALGSEL_SNOW:
	case OP_ALG_ALGSEL_KASUMI:
		switch (*cmd & OP_ALG_AAI_MASK) {
		case OP_ALG_AAI_F8:
			PRINT("f8 ");
			break;

		case OP_ALG_AAI_F9:
			PRINT("f9 ");
			break;

		case OP_ALG_AAI_GSM:
			PRINT("gsm ");
			break;

		case OP_ALG_AAI_EDGE:
			PRINT("edge ");
			break;
		}
		break;

	case OP_ALG_ALGSEL_CRC:
		switch (*cmd & OP_ALG_AAI_MASK) {
		case OP_ALG_AAI_802:
			PRINT("802 ");
			break;

		case OP_ALG_AAI_3385:
			PRINT("3385 ");
			break;

		case OP_ALG_AAI_CUST_POLY:
			PRINT("custom-poly ");
			break;

		case OP_ALG_AAI_DIS:
			PRINT("dis ");
			break;

		case OP_ALG_AAI_DOS:
			PRINT("dos ");
			break;

		case OP_ALG_AAI_DOC:
			PRINT("doc ");
			break;
		}
		break;

	case OP_ALG_ALGSEL_MD5:
	case OP_ALG_ALGSEL_SHA1:
	case OP_ALG_ALGSEL_SHA224:
	case OP_ALG_ALGSEL_SHA256:
	case OP_ALG_ALGSEL_SHA384:
	case OP_ALG_ALGSEL_SHA512:
		switch (*cmd & OP_ALG_AAI_MASK) {
		case OP_ALG_AAI_HMAC:
			PRINT("hmac ");
			break;

		case OP_ALG_AAI_SMAC:
			PRINT("smac ");
			break;

		case OP_ALG_AAI_HMAC_PRECOMP:
			PRINT("hmac-pre ");
			break;
		}
		break;

	default:
		PRINT("unknown-aai ");
	}

	if (*cmd & OP_ALG_TYPE_MASK) {
		switch (*cmd & OP_ALG_AS_MASK) {
		case OP_ALG_AS_UPDATE:
			PRINT("update ");
			break;

		case OP_ALG_AS_INIT:
			PRINT("init ");
			break;

		case OP_ALG_AS_FINALIZE:
			PRINT("final ");
			break;

		case OP_ALG_AS_INITFINAL:
			PRINT("init-final ");
			break;
		}
	}

	if (*cmd & OP_ALG_ICV_MASK)
		PRINT("icv ");

	if (*cmd & OP_ALG_DIR_MASK)
		PRINT("enc ");
	else
		PRINT("dec ");

}

static void show_op_pk_clrmem_args(u_int32_t inst)
{
	if (inst & OP_ALG_PKMODE_A_RAM)
		PRINT("a ");

	if (inst & OP_ALG_PKMODE_B_RAM)
		PRINT("b ");

	if (inst & OP_ALG_PKMODE_E_RAM)
		PRINT("e ");

	if (inst & OP_ALG_PKMODE_N_RAM)
		PRINT("n ");
}

static void show_op_pk_modmath_args(u_int32_t inst)
{
	if (inst & OP_ALG_PKMODE_MOD_IN_MONTY)
		PRINT("inmont ");

	if (inst & OP_ALG_PKMODE_MOD_OUT_MONTY)
		PRINT("outmont ");

	if (inst & OP_ALG_PKMODE_MOD_F2M)
		PRINT("poly ");

	if (inst & OP_ALG_PKMODE_MOD_R2_IN)
		PRINT("r2%%n-inp ");

	if (inst & OP_ALG_PKMODE_PRJECTV)
		PRINT("prj ");

	if (inst & OP_ALG_PKMODE_TIME_EQ)
		PRINT("teq ");

	if (inst & OP_ALG_PKMODE_OUT_A)
		PRINT("->a ");
	else
		PRINT("->b ");
}

static void show_op_pk_cpymem_args(u_int32_t inst)
{
	u_int8_t srcregix, dstregix, srcsegix, dstsegix;

	srcregix = (inst & OP_ALG_PKMODE_SRC_REG_MASK) >>
		   OP_ALG_PKMODE_SRC_REG_SHIFT;
	dstregix = (inst & OP_ALG_PKMODE_DST_REG_MASK) >>
		   OP_ALG_PKMODE_DST_REG_SHIFT;
	srcsegix = (inst & OP_ALG_PKMODE_SRC_SEG_MASK) >>
		   OP_ALG_PKMODE_SRC_SEG_SHIFT;
	dstsegix = (inst & OP_ALG_PKMODE_DST_SEG_MASK) >>
		   OP_ALG_PKMODE_DST_SEG_SHIFT;

	PRINT("%s[%d]->%s[%d] ", pk_srcdst[srcregix], srcsegix,
	      pk_srcdst[dstregix], dstsegix);
}

static void show_op(u_int32_t *cmd, u_int8_t *idx, int8_t *leader)
{
	PRINT(" operation: ");

	switch (*cmd & OP_TYPE_MASK) {
	case OP_TYPE_UNI_PROTOCOL:
		PRINT("uni-pcl ");
		PRINT("%s ",
		      unidir_pcl[(*cmd & OP_PCLID_MASK) >> OP_PCLID_SHIFT]);
		break;

	case OP_TYPE_PK:
		PRINT("pk %s ",
		      pk_function[*cmd & OP_ALG_PK_FUN_MASK]);
		switch (*cmd & OP_ALG_PK_FUN_MASK) {
		case OP_ALG_PKMODE_CLEARMEM:
			show_op_pk_clrmem_args(*cmd);
			break;

		case OP_ALG_PKMODE_MOD_ADD:
		case OP_ALG_PKMODE_MOD_SUB_AB:
		case OP_ALG_PKMODE_MOD_SUB_BA:
		case OP_ALG_PKMODE_MOD_MULT:
		case OP_ALG_PKMODE_MOD_EXPO:
		case OP_ALG_PKMODE_MOD_REDUCT:
		case OP_ALG_PKMODE_MOD_INV:
		case OP_ALG_PKMODE_MOD_ECC_ADD:
		case OP_ALG_PKMODE_MOD_ECC_DBL:
		case OP_ALG_PKMODE_MOD_ECC_MULT:
		case OP_ALG_PKMODE_MOD_MONT_CNST:
		case OP_ALG_PKMODE_MOD_CRT_CNST:
		case OP_ALG_PKMODE_MOD_GCD:
		case OP_ALG_PKMODE_MOD_PRIMALITY:
			show_op_pk_modmath_args(*cmd);
			break;

		case OP_ALG_PKMODE_CPYMEM_N_SZ:
		case OP_ALG_PKMODE_CPYMEM_SRC_SZ:
			show_op_pk_cpymem_args(*cmd);
			break;
		}
		break;

	case OP_TYPE_CLASS1_ALG:
		PRINT("cls1-op ");
		decode_class12_op(cmd);
		break;

	case OP_TYPE_CLASS2_ALG:
		PRINT("cls2-op ");
		decode_class12_op(cmd);
		break;

	case OP_TYPE_DECAP_PROTOCOL:
		PRINT("decap ");
		decode_bidir_pcl_op(cmd);
		break;

	case OP_TYPE_ENCAP_PROTOCOL:
		PRINT("encap ");
		decode_bidir_pcl_op(cmd);
		break;
	}
	PRINT("\n");
	(*idx)++;
}

static void show_signature(u_int32_t *cmd, u_int8_t *idx, int8_t *leader)
{
	PRINT(" signature: %s\n",
	      sig_type[(*cmd & SIGN_TYPE_MASK) >> SIGN_TYPE_SHIFT]);
	(*idx)++;

	/* Process 8 word signature */
	desc_hexdump(cmd + 1, 8, 4, leader);
	idx += 8;
}

static void show_jump(u_int32_t *cmd, u_int8_t *idx, int8_t *leader)
{
	u_int32_t cond;
	int8_t relidx, offset;

	PRINT("      jump: %s %s %s",
	      fifoldst_class[(*cmd & CLASS_MASK) >> CLASS_SHIFT],
	      jump_types[(*cmd & JUMP_TYPE_MASK) >> JUMP_TYPE_SHIFT],
	      jump_tests[(*cmd & JUMP_TEST_MASK) >> JUMP_TEST_SHIFT]);

	cond = (*cmd & (JUMP_COND_MASK & ~JUMP_JSL));
	if (!(*cmd & JUMP_JSL)) {
		if (cond & JUMP_COND_PK_0)
			PRINT(" pk-0");

		if (cond & JUMP_COND_PK_GCD_1)
			PRINT(" pk-gcd=1");

		if (cond & JUMP_COND_PK_PRIME)
			PRINT(" pk-prime");

		if (cond & JUMP_COND_MATH_N)
			PRINT(" math-n");

		if (cond & JUMP_COND_MATH_Z)
			PRINT(" math-z");

		if (cond & JUMP_COND_MATH_C)
			PRINT(" math-c");

		if (cond & JUMP_COND_MATH_NV)
			PRINT(" math-nv");
	} else {
		if (cond & JUMP_COND_JQP)
			PRINT(" jq-pend");

		if (cond & JUMP_COND_SHRD)
			PRINT(" share-skip");

		if (cond & JUMP_COND_SELF)
			PRINT(" share-ctx");

		if (cond & JUMP_COND_CALM)
			PRINT(" complete");

		if (cond & JUMP_COND_NIP)
			PRINT(" no-input");

		if (cond & JUMP_COND_NIFP)
			PRINT(" no-infifo");

		if (cond & JUMP_COND_NOP)
			PRINT(" no-output");

		if (cond & JUMP_COND_NCP)
			PRINT(" no-ctxld");
	}

	relidx = *idx; /* sign extend index to compute relative instruction */
	offset = *cmd & JUMP_OFFSET_MASK;
	if ((*cmd & JUMP_TYPE_MASK) == JUMP_TYPE_LOCAL) {
		PRINT(" ->%d [%02d]\n", offset, relidx + offset);
		(*idx)++;
	} else {
		PRINT(" ->@0x%08x\n", (*idx + 1));
		*idx += 2;
	}
}

static void show_math(u_int32_t *cmd, u_int8_t *idx, int8_t *leader)
{
	u_int32_t mathlen, *mathdata;

	mathlen  = *cmd & MATH_LEN_MASK;
	mathdata = cmd + 1;

	PRINT("      math: %s.%s.%s->%s len=%d ",
	      math_src0[(*cmd & MATH_SRC0_MASK) >> MATH_SRC0_SHIFT],
	      math_fun[(*cmd & MATH_FUN_MASK) >> MATH_FUN_SHIFT],
	      math_src1[(*cmd & MATH_SRC1_MASK) >> MATH_SRC1_SHIFT],
	      math_dest[(*cmd & MATH_DEST_MASK) >> MATH_DEST_SHIFT],
	      mathlen);

	if (*cmd & MATH_IFB)
		PRINT("imm4 ");
	if (*cmd & MATH_NFU)
		PRINT("noflag ");
	if (*cmd & MATH_STL)
		PRINT("stall ");

	PRINT("\n");
	(*idx)++;

	if  (((*cmd & MATH_SRC0_MASK) == MATH_SRC0_IMM) ||
	     ((*cmd & MATH_SRC1_MASK) == MATH_SRC1_IMM)) {
		desc_hexdump(cmd + 1, 1, 4, leader);
		(*idx)++;
	};
};

static void show_seq_in_ptr(u_int32_t *cmd, u_int8_t *idx, int8_t *leader)
{
	PRINT("  seqinptr:");
	if (*cmd & SQIN_RBS)
		PRINT(" rls-buf");
	if (*cmd & SQIN_INL)
		PRINT(" imm");
	if (*cmd & SQIN_SGF)
		PRINT(" s/g");
	if (*cmd & SQIN_PRE) {
		PRINT(" PRE");
	} else {
		PRINT(" @0x%08x", *(cmd + 1));
		if (sizeof(dma_addr_t) > sizeof(u32))
			PRINT("_%08x", *(cmd + 2));
		*idx += sizeof(dma_addr_t) / sizeof(u32);
	}
	if (*cmd & SQIN_EXT)
		PRINT(" EXT");
	else
		PRINT(" %d", *cmd & 0xffff);
	if (*cmd & SQIN_RTO)
		PRINT(" RTO");
	PRINT("\n");
	(*idx)++;
}

static void show_seq_out_ptr(u_int32_t *cmd, u_int8_t *idx, int8_t *leader)
{
	PRINT(" seqoutptr:");
	if (*cmd & SQOUT_SGF)
		PRINT(" s/g");
	if (*cmd & SQOUT_PRE) {
		PRINT(" PRE");
	} else {
		PRINT(" @0x%08x", *(cmd + 1));
		if (sizeof(dma_addr_t) > sizeof(u32))
			PRINT("_%08x", *(cmd + 2));
		*idx += sizeof(dma_addr_t) / sizeof(u32);
	}
	if (*cmd & SQOUT_EXT)
		PRINT(" EXT");
	else
		PRINT(" %d", *cmd & 0xffff);
	PRINT("\n");
	(*idx)++;
}

static void show_illegal_inst(u_int32_t *cmd, u_int8_t *idx, int8_t *leader)
{
	PRINT("<illegal-instruction>\n");
	(*idx)++;
}

/* Handlers for each instruction based on CTYPE as an enumeration */
static void (*inst_disasm_handler[])(u_int32_t *, u_int8_t *, int8_t *) = {
	show_key,
	show_seq_key,
	show_load,
	show_seq_load,
	show_fifo_load,
	show_seq_fifo_load,
	show_illegal_inst,
	show_illegal_inst,
	show_illegal_inst,
	show_illegal_inst,
	show_store,
	show_seq_store,
	show_fifo_store,
	show_seq_fifo_store,
	show_illegal_inst,
	show_move,
	show_op,
	show_illegal_inst,
	show_signature,
	show_illegal_inst,
	show_jump,
	show_math,
	show_illegal_inst, /* header */
	show_illegal_inst, /* shared header */
	show_illegal_inst,
	show_illegal_inst,
	show_illegal_inst,
	show_illegal_inst,
	show_illegal_inst,
	show_illegal_inst,
	show_seq_in_ptr,
	show_seq_out_ptr,
};

/**
 * caam_desc_disasm() - Top-level descriptor disassembler
 * @desc - points to the descriptor to disassemble. First command
 *	   must be a header, or shared header, and the overall size
 *	   is determined by this. Does not handle a QI preheader as
 *	   it's first command, and cannot yet follow links in a list
 *	   of descriptors
 * @opts - selects options for output:
 *	   DISASM_SHOW_OFFSETS - displays the index/offset of each
 *				 instruction in the descriptor. Helpful
 *				 for visualizing flow control changes
 *         DISASM_SHOW_RAW     - displays value of each instruction
 **/
void caam_desc_disasm(u_int32_t *desc, u_int32_t opts)
{
	u_int8_t len, idx, stidx;
	int8_t emptyleader[MAX_LEADER_LEN], pdbleader[MAX_LEADER_LEN];

	stidx  = 0;

	/*
	 * Build up padded leader strings for non-instruction content
	 * These get used for pointer and PDB content dumps
	 */
	emptyleader[0] = 0;
	pdbleader[0] = 0;

	/* Offset leader is a 5-char string, e.g. "[xx] " */
	if (opts & DISASM_SHOW_OFFSETS) {
		strcat((char *)emptyleader, "     ");
		strcat((char *)pdbleader, "     ");
	}

	/* Raw instruction leader is an 11-char string, e.g. "0xnnnnnnnn " */
	if (opts & DISASM_SHOW_RAW) {
		strcat((char *)emptyleader, "           ");
		strcat((char *)pdbleader, "           ");
	}

	/* Finish out leaders. Instruction names use a 12-char space */
	strcat((char *)emptyleader, "            ");
	strcat((char *)pdbleader, "     (pdb): ");

	/*
	 * Now examine our descriptor, starting with it's header.
	 * First word must be header or shared header, or we quit
	 * under the assumption that a bad desc pointer was passed.
	 * If we have a valid header, save off indices and size for
	 * determining descriptor area boundaries
	 */
	switch (*desc & CMD_MASK) {
	case CMD_SHARED_DESC_HDR:
		if (opts & DISASM_SHOW_OFFSETS)
			PRINT("[%02d] ", 0);
		if (opts & DISASM_SHOW_RAW)
			PRINT("0x%08x ", desc[0]);
		show_shrhdr(desc);
		len   = *desc & HDR_DESCLEN_SHR_MASK;
		stidx = (*desc >> HDR_START_IDX_SHIFT) &
			HDR_START_IDX_MASK;

		if (stidx == 0)
			stidx++;

		/*
		 * Show PDB area (that between header and startindex)
		 * Improve PDB content dumps later...
		 */
		if (stidx > 1) /* >1 means real PDB data exists */
			desc_hexdump(&desc[1], stidx - 1, 4,
				     (int8_t *)pdbleader);

		idx = stidx;
		break;

	case CMD_DESC_HDR:
		if (opts & DISASM_SHOW_OFFSETS)
			PRINT("[%02d] ", 0);
		if (opts & DISASM_SHOW_RAW)
			PRINT("0x%08x ", desc[0]);
		show_hdr(desc);
		len   = *desc & HDR_DESCLEN_MASK;
		stidx = (*desc >> HDR_START_IDX_SHIFT) &
			HDR_START_IDX_MASK;

		/* Start index of 0 really just means 1, so fix */
		if (stidx == 0)
			stidx++;

		/* Skip sharedesc pointer if SHARED, else display PDB */
		if (*desc & HDR_SHARED) {
			/* just skip past sharedesc ptr */
			stidx = 1 + sizeof(dma_addr_t) / sizeof(u32);
			PRINT("%s sharedesc->0x%08x", emptyleader, desc[1]);
			if (sizeof(dma_addr_t) > sizeof(u32))
				PRINT("_%08x\n", desc[2]);
			else
				PRINT("\n");
		} else
			if (stidx > 1) /* >1 means real PDB data exists */
				desc_hexdump(&desc[1], stidx - 1, 4,
					     (int8_t *)pdbleader);

		idx = stidx;
		break;

	default:
		PRINT("caam_desc_disasm(): no header: 0x%08x\n",
		      *desc);
		return;
	}

	/* Header verified, now process sequential instructions */
	while (idx < len) {
		if (opts & DISASM_SHOW_OFFSETS)
			PRINT("[%02d] ", idx);
		if (opts & DISASM_SHOW_RAW)
			PRINT("0x%08x ", desc[idx]);
		inst_disasm_handler[(desc[idx] & CMD_MASK) >> CMD_SHIFT]
				    (&desc[idx], &idx, emptyleader);
	}
}
EXPORT_SYMBOL(caam_desc_disasm);
