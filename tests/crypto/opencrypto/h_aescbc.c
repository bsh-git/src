/* $NetBSD: h_aescbc.c,v 1.1 2017/04/17 03:59:37 knakahara Exp $ */

/*-
 * Copyright (c) 2017 Internet Initiative Japan Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/time.h>

#include <crypto/cryptodev.h>

/*
 * Test vectors from RFC 3602
 */

const struct {
	size_t len;
	size_t key_len;
	unsigned char key[16];
	unsigned char iv[16];
	unsigned char plaintx[64];
	unsigned char ciphertx[64];
} tests[] = {
	/* Case #1: Encrypting 16 bytes (1 block) using AES-CBC with 128-bit key */
	{ 16, 16,
	  { 0x06, 0xa9, 0x21, 0x40, 0x36, 0xb8, 0xa1, 0x5b,
	    0x51, 0x2e, 0x03, 0xd5, 0x34, 0x12, 0x00, 0x06, },
	  { 0x3d, 0xaf, 0xba, 0x42, 0x9d, 0x9e, 0xb4, 0x30,
	    0xb4, 0x22, 0xda, 0x80, 0x2c, 0x9f, 0xac, 0x41, },
	  "Single block msg",
	  { 0xe3, 0x53, 0x77, 0x9c, 0x10, 0x79, 0xae, 0xb8,
	    0x27, 0x08, 0x94, 0x2d, 0xbe, 0x77, 0x18, 0x1a, },
	},

	/* Case #2: Encrypting 32 bytes (2 blocks) using AES-CBC with 128-bit key */
	{ 32, 16,
	  { 0xc2, 0x86, 0x69, 0x6d, 0x88, 0x7c, 0x9a, 0xa0,
	    0x61, 0x1b, 0xbb, 0x3e, 0x20, 0x25, 0xa4, 0x5a, },
	  { 0x56, 0x2e, 0x17, 0x99, 0x6d, 0x09, 0x3d, 0x28,
	    0xdd, 0xb3, 0xba, 0x69, 0x5a, 0x2e, 0x6f, 0x58, },
	  { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
            0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, },
	  { 0xd2, 0x96, 0xcd, 0x94, 0xc2, 0xcc, 0xcf, 0x8a,
	    0x3a, 0x86, 0x30, 0x28, 0xb5, 0xe1, 0xdc, 0x0a,
	    0x75, 0x86, 0x60, 0x2d, 0x25, 0x3c, 0xff, 0xf9,
	    0x1b, 0x82, 0x66, 0xbe, 0xa6, 0xd6, 0x1a, 0xb1, },
	},

	/* Case #3: Encrypting 48 bytes (3 blocks) using AES-CBC with 128-bit key */
	{ 48, 16,
	  { 0x6c, 0x3e, 0xa0, 0x47, 0x76, 0x30, 0xce, 0x21,
	    0xa2, 0xce, 0x33, 0x4a, 0xa7, 0x46, 0xc2, 0xcd, },
	  { 0xc7, 0x82, 0xdc, 0x4c, 0x09, 0x8c, 0x66, 0xcb,
	    0xd9, 0xcd, 0x27, 0xd8, 0x25, 0x68, 0x2c, 0x81, },
	  "This is a 48-byte message (exactly 3 AES blocks)",
	  { 0xd0, 0xa0, 0x2b, 0x38, 0x36, 0x45, 0x17, 0x53,
	    0xd4, 0x93, 0x66, 0x5d, 0x33, 0xf0, 0xe8, 0x86,
	    0x2d, 0xea, 0x54, 0xcd, 0xb2, 0x93, 0xab, 0xc7,
	    0x50, 0x69, 0x39, 0x27, 0x67, 0x72, 0xf8, 0xd5,
	    0x02, 0x1c, 0x19, 0x21, 0x6b, 0xad, 0x52, 0x5c,
	    0x85, 0x79, 0x69, 0x5d, 0x83, 0xba, 0x26, 0x84, },
	},

	/* Case #4: Encrypting 64 bytes (4 blocks) using AES-CBC with 128-bit key */
	{ 64, 16,
	  { 0x56, 0xe4, 0x7a, 0x38, 0xc5, 0x59, 0x89, 0x74,
	    0xbc, 0x46, 0x90, 0x3d, 0xba, 0x29, 0x03, 0x49, },
	  { 0x8c, 0xe8, 0x2e, 0xef, 0xbe, 0xa0, 0xda, 0x3c,
	    0x44, 0x69, 0x9e, 0xd7, 0xdb, 0x51, 0xb7, 0xd9, },
	  { 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
	    0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	    0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
	    0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	    0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
	    0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	    0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
	    0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, },
	  { 0xc3, 0x0e, 0x32, 0xff, 0xed, 0xc0, 0x77, 0x4e,
	    0x6a, 0xff, 0x6a, 0xf0, 0x86, 0x9f, 0x71, 0xaa,
	    0x0f, 0x3a, 0xf0, 0x7a, 0x9a, 0x31, 0xa9, 0xc6,
	    0x84, 0xdb, 0x20, 0x7e, 0xb0, 0xef, 0x8e, 0x4e,
	    0x35, 0x90, 0x7a, 0xa6, 0x32, 0xc3, 0xff, 0xdf,
	    0x86, 0x8b, 0xb7, 0xb2, 0x9d, 0x3d, 0x46, 0xad,
	    0x83, 0xce, 0x9f, 0x9a, 0x10, 0x2e, 0xe9, 0x9d,
	    0x49, 0xa5, 0x3e, 0x87, 0xf4, 0xc3, 0xda, 0x55,
	  },
	},
};

int
main(void)
{
	int fd, res;
	size_t i;
	struct session_op cs;
	struct crypt_op co;
	unsigned char buf[64];

	for (i = 0; i < __arraycount(tests); i++) {
		fd = open("/dev/crypto", O_RDWR, 0);
		if (fd < 0)
			err(1, "open %zu", i);
		memset(&cs, 0, sizeof(cs));
		cs.cipher = CRYPTO_AES_CBC;
		cs.keylen = tests[i].key_len;
		cs.key = __UNCONST(&tests[i].key);
		res = ioctl(fd, CIOCGSESSION, &cs);
		if (res < 0)
			err(1, "CIOCGSESSION %zu", i);

		memset(&co, 0, sizeof(co));
		co.ses = cs.ses;
		co.op = COP_ENCRYPT;
		co.len = tests[i].len;
		co.src = __UNCONST(&tests[i].plaintx);
		co.dst = buf;
		co.dst_len = sizeof(buf);
		co.iv = __UNCONST(&tests[i].iv);
		res = ioctl(fd, CIOCCRYPT, &co);
		if (res < 0)
			err(1, "CIOCCRYPT %zu", i);

		if (memcmp(co.dst, tests[i].ciphertx, tests[i].len)) {
			size_t j;
			for (j = 0; j < tests[i].len; j++)
				printf("0x%2zu:  0x%2x    0x%2x\n", j,
					buf[j], tests[i].ciphertx[j]);
			errx(1, "verification failed %zu", i);
		}
		close(fd);
	}
	return 0;
}
