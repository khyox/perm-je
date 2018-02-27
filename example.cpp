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

#include <iostream>
#include <cstring>
#include <list>
//#define JEMALLOC_MANGLE // use if configured --with-jemalloc-prefix=je
//#define PERM_OVERRIDE_NEW // use to override new and delete operators
#include "jemalloc/pallocator.h"
using namespace std;

#define BACK_FILE "/tmp/app.back" /* Note: different backup and mmap files */
#define MMAP_FILE "/tmp/app.mmap"
#define MMAP_SIZE ((size_t)1 << 23)

#define CRASH_STEP 3

class globals {
public:
	int step;
	list<int, PERM_NS::allocator<int> > plist;
	/* ... */
	globals() {step = 1; cout << "globals construct:" << this << endl;}
	~globals() {cout << "globals destruct:" << this << endl;}
};

PERM globals *home; /* use PERM to mark home as persistent */

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
		if (home->plist.size() != CRASH_STEP-1) cerr << "error: restore\n";
	} else {
		home = PERM_NEW(globals);
		/* more initialization within home... */
		mflush(); backup();
	}

	while (home->step <= 5) {
		/* application step */
		home->plist.push_back(home->step);
		if (!do_restore && home->step == CRASH_STEP) {
			cout << "simulate crash!\n"; return(1);
		}
		/* step complete */
		cout << "step:" << home->step << endl;
		home->step++;
		backup();
	}

	PERM_DELETE(home, globals);
	mclose();
	bclose();
	return(0);
}
