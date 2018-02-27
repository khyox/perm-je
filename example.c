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

/*
 * Example program showing usage of persistent memory functions.
 */

#include <stdio.h>
#include <string.h>

/* use JEMALLOC_MANGLE if: configure --with-jemalloc-prefix=je */
/* #define JEMALLOC_MANGLE */
#include "jemalloc/jemalloc.h"

#define BACK_FILE "/tmp/app.back" /* Note: different backup and mmap files */
#define MMAP_FILE "/tmp/app.mmap"
#define MMAP_SIZE ((size_t)1 << 30)

typedef struct {
	/* ... */
} home_st;

PERM home_st *home; /* use PERM to mark home as persistent */

int main(int argc, char *argv[])
{
	int do_restore = argc > 1 && strcmp("-r", argv[1]) == 0;
	const char *mode = (do_restore) ? "r+" : "w+";

	/* call perm() and mopen() before malloc() */
	perm(PERM_START, PERM_SIZE);
	mopen(MMAP_FILE, mode, MMAP_SIZE);
	bopen(BACK_FILE, mode);
	if (do_restore) {
		restore();
	} else {
		home = (home_st *)malloc(sizeof(home_st));
		/* initialize home struct... */
		mflush(); backup();
	}

	for (;/* each step */;) {
		/* Application_Step(); */
		backup();
	}

	free(home);
	mclose();
	bclose();
	return(0);
}
