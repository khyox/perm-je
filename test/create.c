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

#define BACK_FILE "test/persist.back"
#define MMAP_FILE "test/persist.mmap"
#define MMAP_SIZE ((size_t)1 << 26)

PERM void *addr[MAX_BLKS];
PERM size_t size[MAX_BLKS];
PERM size_t total;
PERM int idx;

int check_overlap(int nidx, void *naddr, size_t nsize)
{
	int i;

	for (i = 0; i < nidx; i++) {
		if ((naddr >= addr[i] && naddr < addr[i]+size[i]) ||
			(naddr+nsize > addr[i] && naddr+nsize <= addr[i]+size[i])) {
			fprintf(stderr,
				"%s(): \n"
				"New block (idx:%d addr:%p size:%zu)\n"
				"overlaps with (idx:%d addr:%p size:%zu)\n",
				__func__, nidx, naddr, nsize, i, addr[i], size[i]);
			return(-1);
		}
	}
	return(0);
}

int main(void)
{
	int ret;
	int flag = 0;

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
	ret = mopen(MMAP_FILE, "w+", MMAP_SIZE);
	if (ret) {
		fprintf(stderr, "%s(): Error in mopen()\n", __func__);
		goto RETURN;
	}
	ret = bopen(BACK_FILE, "w+");
	if (ret) {
		fprintf(stderr, "%s(): Error in bopen()\n", __func__);
		goto RETURN;
	}

	for (idx = 0; idx < MAX_BLKS; idx++) {
		void *taddr;
		size_t tsize;

		tsize = idx < MAX_BLKS/2 ? (idx+1) * 7 : (idx+1) * 2039;
		taddr = JEMALLOC_P(malloc)(tsize);
		//fprintf(stderr, "idx:%d addr:%p size:%zu\n", idx, taddr, tsize);
		if (taddr == NULL) {
			fprintf(stderr, "%s(): Error in malloc()\n", __func__);
			ret = 1;
			goto RETURN;
		}
		if (check_overlap(idx, taddr, tsize)) {
			goto RETURN;
		}
		memset(taddr, idx & 0xFF, tsize);
		addr[idx] = taddr;
		size[idx] = tsize;
		total += tsize;
		if ((idx & 0x7F) == 0x40) { /* every 64 */
			fprintf(stderr, "idx:%d - before mflush();\n", idx);
			mflush();
		}
		if ((idx & 0x3F) == 0x20) { /* every 32 */
			fprintf(stderr, "idx:%d - before backup();\n", idx);
			backup();
		}
		if (idx == 0x8F && !flag) {
			flag = 1;
			fprintf(stderr, "idx:%d - before restore();\n", idx);
			restore();
			fprintf(stderr, "idx:%d - after restore();\n", idx);
		}
	}
	ret = mclose();
	if (ret) {
		fprintf(stderr, "%s(): Error in mclose()\n", __func__);
		goto RETURN;
	}
	ret = bclose();
	if (ret) {
		fprintf(stderr, "%s(): Error in bclose()\n", __func__);
		goto RETURN;
	}

RETURN:
	//fprintf(stderr, "total memory allocated: %zu\n", total);
	fprintf(stderr, "Test end\n");
	return (ret);
}
