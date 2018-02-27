/*
 * Copyright (c) 2013, Lawrence Livermore National Security, LLC. 
 * Produced at the Lawrence Livermore National Laboratory. Written by
 * G. Scott Lloyd, lloyd23@llnl.gov. LLNL-CODE-613632. All rights reserved.
 * 
 * This file is part of PERM. For details, see
 * http://computation.llnl.gov/casc/perm/ 
 * 
 * Please also read COPYING.LLNL – Our Notice and GNU Lesser General Public 
 * License. 
 * 
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License (as published by the 
 * Free Software Foundation) version 2.1 dated February 1999. 
 * 
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY OF 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and 
 * conditions of the GNU General Public License for more details. 
 * 
 * You should have received a copy of the GNU Lesser General Public License 
 * along with this program; if not, write to the Free Software Foundation, 
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA 
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#define	JEMALLOC_MANGLE
#include "jemalloc_test.h"
#ifndef USE_PERM
#undef PERM
#define PERM
#endif

#define MAX_BLKS 256

#define MMAP_FILE "test/persist.mmap"
#define MMAP_SIZE ((size_t)1 << 26)

PERM void *addr[MAX_BLKS];
PERM size_t size[MAX_BLKS];
PERM size_t total;
PERM int idx;

int main(void)
{
	int i, ret;

	fprintf(stderr, "Test begin\n");

#ifdef USE_PERM
	#pragma message("Using PERM attribute")
	perm(PERM_START, PERM_SIZE);
#else
	perm(addr, sizeof(addr));
	perm(size, sizeof(size));
	perm(&total, sizeof(total));
	perm(&idx, sizeof(idx));
#endif
	ret = mopen(MMAP_FILE, "r", MMAP_SIZE);
	if (ret) {
		fprintf(stderr, "%s(): Error in mopen()\n", __func__);
		goto RETURN;
	}

	for (i = 0; i < MAX_BLKS; i++) {
		unsigned char *taddr, *eaddr;
		size_t tsize;
		unsigned c;

		if (addr[i] == NULL) {
			fprintf(stderr, "%s(): addr[%d] == NULL\n", __func__, i);
			ret = 1;
			goto RETURN;
		}
		tsize = i < MAX_BLKS/2 ? (i+1) * 7 : (i+1) * 2039;
		if (size[i] != tsize) {
			fprintf(stderr, "%s(): size[%d] found:%zu expect:%zu\n",
				__func__, i, size[i], tsize);
			ret = 1;
			goto RETURN;
		}
		c = i & 0xFF;
		for (taddr = addr[i], eaddr = taddr+tsize; taddr < eaddr; taddr++) {
			if (*taddr != c) {
				fprintf(stderr,
					"%s(): data corrupted found:%u expect:%u at:%p in block(%d):%p size:%zu\n",
					__func__, *taddr, c, taddr, i, addr[i], size[i]);
				break;
			}
		}
	}

	ret = mclose();
	if (ret) {
		fprintf(stderr, "%s(): Error in mclose()\n", __func__);
		goto RETURN;
	}

RETURN:
	fprintf(stderr, "Test end\n");
	return (ret);
}
