/*
 * SHA-256 internal definitions
 * Copyright (c) 2003-2011, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef SHA256_I_H
#define SHA256_I_H

#define SHA256_BLOCK_SIZE 64

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0))
struct _sha256_state {
	u64 length;
	u32 state[8], curlen;
	u8 buf[SHA256_BLOCK_SIZE];
};

void _sha256_init(struct _sha256_state *md);
int sha256_process(struct _sha256_state *md, const unsigned char *in,
		   unsigned long inlen);
int sha256_done(struct _sha256_state *md, unsigned char *out);
#endif
#endif /* SHA256_I_H */
