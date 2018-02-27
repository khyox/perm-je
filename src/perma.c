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
 * $Id: $
 *
 * Description: Persistent memory library used in conjunction with a
 * dynamic-memory manager.
 *
 * $Log: $
 */

#include "jemalloc/internal/jemalloc_internal.h"

#define MAX_IO_BLKS 50
#if IOV_MAX < MAX_IO_BLKS
#error the number of I/O blocks exceeds the system limit
#endif

#define PERM_KEY 0x20130411

static malloc_mutex_t perm_mtx =
#ifdef JEMALLOC_OSSPIN
    0
#else
    MALLOC_MUTEX_INITIALIZER
#endif
    ;

static int mfd = -1; /* mmap file descriptor */
static int bfd = -1; /* backup file descriptor */

static size_t perm_size; /* total size of persistent globals */
static int nperm; /* number of perm I/O blocks */
static struct iovec permv[MAX_IO_BLKS]; /* vector of perm I/O blocks */

static int check_header(int fd, size_t *heap_sz);

#define PRINT_VARS \
printf("narenas:%u ncpus:%u plib:%p\n", narenas, ncpus, plib); \
printf("swap_base:%p swap_end:%p diff:%zu swap_max:%p\n", swap_base, swap_end, swap_end-swap_base, swap_max); \
printf("base_pages:%p base_next_addr:%p diff:%zu base_past_addr:%p\n", base_pages, base_next_addr, base_next_addr-base_pages, base_past_addr); \
printf("key:0x%x globals:%p gsize:%zu\n", plib->version_key, plib->globals, plib->gsize); \
{int i; for (i = 0; i < nperm; i++) printf("i:%d buf:%p end:%p len:%zu\n", i, permv[i].iov_base, permv[i].iov_base+permv[i].iov_len, permv[i].iov_len);}


static ssize_t readvb(void *buf, size_t size, const struct iovec *iov, int iovcnt)
{
	int i = 0;
	ssize_t osize = size;

	for (i = 0; i < iovcnt && size; i++) {
		size_t n = iov[i].iov_len;
		if (n > size) n = size;
		memcpy(iov[i].iov_base, buf, n);
		buf += n;
		size -= n;
	}
	return(osize - (ssize_t)size);
}

static ssize_t writevb(void *buf, size_t size, const struct iovec *iov, int iovcnt)
{
	int i = 0;
	ssize_t osize = size;

	for (i = 0; i < iovcnt && size; i++) {
		size_t n = iov[i].iov_len;
		if (n > size) n = size;
		memcpy(buf, iov[i].iov_base, n);
		buf += n;
		size -= n;
	}
	return(osize - (ssize_t)size);
}

static ssize_t lread(int fd, void *buf, size_t count)
{
	ssize_t res;
	size_t remain = count;

	do {
		res = read(fd, buf, remain);
		//printf("res:%zd fd:%d buf:%p remain:%zu\n", res, fd, buf, remain);
		if (res < 0) return(res);
		buf += res;
		remain -= res;
	} while (remain && res);
	return(count-remain);
}

static ssize_t lwrite(int fd, const void *buf, size_t count)
{
	ssize_t res;
	size_t remain = count;

	do {
		res = write(fd, buf, remain);
		//printf("res:%zd fd:%d buf:%p remain:%zu\n", res, fd, buf, remain);
		if (res < 0) return(res);
		buf += res;
		remain -= res;
	} while (remain && res);
	return(count-remain);
}

static void oflags(const char *mode, int *flags)
{
	int access = 0;
	int creation = 0;

	while (*mode) {
		switch (*mode++) {
		case 'r':
			access = O_RDONLY;
			creation = 0;
			break;
		case 'w':
			access = O_WRONLY;
			creation = O_CREAT | O_TRUNC;
			break;
		case '+':
			access = O_RDWR;
			break;
		}
	}
	*flags = access | creation;
}

/* Use environment variables for mopen arguments */
JEMALLOC_ATTR(visibility("default"))
int perm_open(void)
{
	char *s, *eptr;
	char *fname;
	char *mode;
	size_t size;
	int res;

	if ((fname = getenv("PERM_FNAME")) == NULL)
		return(0);
	mode = (s = getenv("PERM_MODE")) != NULL ? s : "w+";
	size = strtoul((s = getenv("PERM_SIZE")) != NULL ? s : "1G", &eptr, 0);

	switch (*eptr) {
	case 'K': case 'k': size <<= 10; break;
	case 'M': case 'm': size <<= 20; break;
	case 'G': case 'g': size <<= 30; break;
	case 'T': case 't': size <<= 40; break;
	}

	if ((res = mopen(fname, mode, size))) {
		if (res == -2) return(0); /* already open OK */
		return(res);
	}

	if (atexit((void (*)(void))mflush))
		return(-1);

	return(0);
}

/* Register a block as persistent memory */
JEMALLOC_ATTR(visibility("default"))
int perm(void *ptr, size_t size)
{
	int res = -1;

	malloc_mutex_lock(&perm_mtx);
	if (mfd != -1) {
		fprintf(stderr, "perm: must be called before opening a file\n");
		goto pm_return;
	}
	if (nperm == MAX_IO_BLKS-1) goto pm_return;
	perm_size += size;
	permv[nperm].iov_base = ptr;
	permv[nperm].iov_len = size;
	nperm++;

	res = 0;
pm_return:
	malloc_mutex_unlock(&perm_mtx);
	return(res);
}

/* Open and map file into core memory */
JEMALLOC_ATTR(visibility("default"))
int mopen(const char *fname, const char *mode, size_t size)
{
	ssize_t res = -1;
	bool have_init_lock = false;
	int flags;
	bool malloc_init_hard(void);

	malloc_mutex_lock(&perm_mtx);

	if (mfd != -1) {
		//fprintf(stderr, "mopen: map file already open\n");
		malloc_mutex_unlock(&perm_mtx);
		return(-2);
	}

	malloc_mutex_lock(&init_lock);
	have_init_lock = true;
	/*
	 * To remove this constraint, chunk_boot(), base_boot(), and
	 * chunk_swap_enable() would need to be re-factored. Currently,
	 * the only way to get a new heap is through these one-time
	 * callable functions. swap_enable stays enabled for the life
	 * of the application.
	 */
	if (swap_enabled) {
		fprintf(stderr, "mopen: can only be called once\n");
		goto mo_return;
	}

	/*
	 * If malloc_init() is called first, then the internal heap comes
	 * from anonymous mmapped pages and is already set up with arena
	 * data structures. This constraint also prevents mallctl() from
	 * being called to set options before mopen.
	 */
	/*
	 * jemalloc() will use system memory if mopen is not called first.
	 * The user might think memory is persistent and it is not.
	 */
	if (malloc_initialized == true) {
		fprintf(stderr, "mopen: must be called before malloc\n");
		goto mo_return;
	}

	oflags(mode, &flags);
	mfd = open(fname, flags, (mode_t)0666);
	if (mfd == -1) {
		perror("mopen: error opening map file");
		goto mo_return;
	}

	if (flags & O_CREAT) {
		/* fill map file with zeros up to "size" */
		res = lseek(mfd, size-1, SEEK_SET);
		if (res == -1) {
			perror("mopen: error seeking to end of map file");
			goto mo_return;
		}
		res = lwrite(mfd, "", 1);
		if (res != 1) {
			res = -1;
			perror("mopen: error extending map file");
			goto mo_return;
		}
		res = -1;
	}

	/*
	 * chunk_boot() must be called before chunk_swap_enable()
	 * and before using CHUNK_ADDR2OFFSET() and chunksize.
	 */
	if (base_boot())
		goto mo_return;
	if (chunk_boot())
		goto mo_return;
	if (ctl_boot())
		goto mo_return;

	if (flags & O_CREAT) {
		/* setup swap_base and version_key */
		char *s;
		void *addr;
		/* use PERM_ADDRESS for the start of the persistent heap */
		addr = (void *)strtoul((s = getenv("PERM_ADDRESS")) != NULL ? s : "0", NULL, 0);
		/* Test for chunk alignment */
		if (CHUNK_ADDR2OFFSET(addr)) {
			fprintf(stderr, "mopen: env PERM_ADDRESS must be chuck aligned: 0x%lX\n", chunksize);
			goto mo_return;
		}
		if (addr != NULL) {
			swap_base = addr;
		}
		plib->version_key = PERM_KEY;
	} else {
		/* retrieve persistent heap base address (swap_base) and version_key */
		res = lread(mfd, plib, sizeof(plib_t));
		if (res != sizeof(plib_t)) {
			res = -1;
			perror("mopen: error reading heap header");
			goto mo_return;
		}
		res = -1;
		if (plib->version_key != PERM_KEY) {
			fprintf(stderr,
				"mopen: version key incorrect, found:0x%X expect:0x%X\n",
				plib->version_key, PERM_KEY);
			goto mo_return;
		}
	}

	/*
	 * chunk_swap_enable() sets up a new heap with mmap. Once swap is enabled,
	 * it stays enabled for the life of the application. Also, once swap is
	 * enabled, base_alloc will use chunks from swap for the internal heap.
	 */
	malloc_mutex_lock(&ctl_mtx);
	if (chunk_swap_enable(&mfd, 1, true)) {
		fprintf(stderr, "mopen: error in mapping persistent heap\n");
		goto mo_return;
	}
	swap_fds = &mfd;
	swap_nfds = 1;
	malloc_mutex_unlock(&ctl_mtx);

	if (flags & O_CREAT) {
		/* assume plib is first block of mmap heap */
		plib_t *ptr = base_alloc(sizeof(plib_t));
		if (ptr == NULL || ptr != swap_base)
			goto mo_return;
		/* copy over static plib to mmap heap and then use the copy */
		*ptr = *plib;
		plib = ptr;
		/* redo some chunk_swap init because of copy */
		extent_tree_szad_new(&swap_chunks_szad);
		extent_tree_ad_new(&swap_chunks_ad);
		/* allocate space for globals */
		plib->globals = base_alloc(perm_size);
		if (plib->globals == NULL)
			goto mo_return;
		plib->gsize = perm_size;
	} else {
		/* assume plib is first block of mmap heap */
		plib = swap_base;
		/* restore globals */
		readvb(plib->globals, plib->gsize, permv, nperm);
		plib_initialized = true;
	}

	/* finish initialization */
	malloc_mutex_unlock(&init_lock);
	have_init_lock = false;
	if (malloc_init_hard())
		goto mo_return;
	if (flags & O_CREAT) {
		plib_initialized = true;
		/* save new heap, mflush() */
		jemalloc_prefork(); /* acquire all jemalloc mutexes */
		writevb(plib->globals, plib->gsize, permv, nperm); /* save globals */
		res = msync(swap_base, swap_end-swap_base, MS_SYNC);
		jemalloc_postfork(); /* release all jemalloc mutexes */
		if (res == -1) {
			perror("mopen: error syncing map file");
			goto mo_return;
		}
	}

	res = 0;
mo_return:
	if (have_init_lock == true)
		malloc_mutex_unlock(&init_lock);
	if (res != 0 && mfd >= 0) {
		close(mfd); mfd = -1;
	}
	malloc_mutex_unlock(&perm_mtx);
	return((int)res);
}

/* Close memory-mapped file */
JEMALLOC_ATTR(visibility("default"))
int mclose(void)
{
	/* flush if open for writing */
	if ((O_WRONLY|O_RDWR) & fcntl(mfd, F_GETFL))
		mflush();

	malloc_mutex_lock(&perm_mtx);
	/*
	 * If munmap is called on close before exiting main(), a segmentation
	 * fault may occur in C++ programs. This is because static objects
	 * declared globally or in main() will call their destructor and may
	 * try to access memory that was in the swap region invalidated by
	 * munmap.
	 */
	/* munmap(swap_base, swap_max-swap_base); */
	close(mfd); mfd = -1;
	malloc_mutex_unlock(&perm_mtx);
	return(0);
}

/* Flushes in-core data to files */
JEMALLOC_ATTR(visibility("default"))
int mflush(void)
{
	ssize_t res = -1;

	malloc_mutex_lock(&perm_mtx);
	if (mfd == -1) {
		fprintf(stderr, "mflush: mmap file not open\n");
		goto mf_return;
	}
	jemalloc_prefork(); /* acquire all jemalloc mutexes */

	/* save globals */
	writevb(plib->globals, plib->gsize, permv, nperm);

	res = msync(swap_base, swap_end-swap_base, MS_SYNC);
	if (res == -1) {
		perror("mflush: error syncing map file");
		/* close(mfd); mfd = -1; */
		goto mf_return;
	}

	res = 0;
mf_return:
	if (mfd != -1) jemalloc_postfork(); /* release all jemalloc mutexes */
	malloc_mutex_unlock(&perm_mtx);
	return((int)res);
}

/* Open backup file */
JEMALLOC_ATTR(visibility("default"))
int bopen(const char *fname, const char *mode)
{
	int res = -1;
	int flags;

	malloc_mutex_lock(&perm_mtx);
	if (mfd == -1) {
		fprintf(stderr, "bopen: must open map file first\n");
		/* This is because of the dependency on swap_base/swap_end */
		goto bo_return;
	}
	if (bfd != -1) {
		//fprintf(stderr, "bopen: backup file already open\n");
		res = -2;
		goto bo_return;
	}

	oflags(mode, &flags);
	bfd = open(fname, flags, (mode_t)0666);
	if (bfd == -1) {
		perror("bopen: error opening backup file");
		goto bo_return;
	}

	res = 0;
bo_return:
	malloc_mutex_unlock(&perm_mtx);
	return(res);
}

/* Close backup file */
JEMALLOC_ATTR(visibility("default"))
int bclose(void)
{
	malloc_mutex_lock(&perm_mtx);
	close(bfd); bfd = -1;
	malloc_mutex_unlock(&perm_mtx);
	return(0);
}

/* Backup globals and heap to backup file */
JEMALLOC_ATTR(visibility("default"))
int backup(void)
{
	ssize_t res = -1;

	malloc_mutex_lock(&perm_mtx);
	if (bfd == -1) {
		fprintf(stderr, "backup: backup file not open\n");
		goto bu_return;
	}
	jemalloc_prefork(); /* acquire all jemalloc mutexes */

	/* save globals */
	writevb(plib->globals, plib->gsize, permv, nperm);

	/* write out heap */
	res = lseek(bfd, 0, SEEK_SET);
	if (res == -1) {
		perror("backup: error seeking to heap start");
		goto bu_return;
	}
	res = lwrite(bfd, swap_base, swap_end-swap_base);
	if (res != swap_end-swap_base) {
		perror("backup: error writing heap data");
		goto bu_return;
	}

	res = fsync(bfd);
	if (res == -1) {
		perror("backup: error syncing backup file");
		goto bu_return;
	}

	/* truncate a file longer than (swap_end-swap_base) */
	res = ftruncate(bfd, swap_end-swap_base);
	if (res == -1) {
		perror("backup: error truncating backup file");
		goto bu_return;
	}

	res = 0;
bu_return:
	if (bfd != -1) jemalloc_postfork(); /* release all jemalloc mutexes */
	malloc_mutex_unlock(&perm_mtx);
	return((int)res);
}

/* Restore globals and heap from backup file */
JEMALLOC_ATTR(visibility("default"))
int restore(void)
{
	void *swap_end_ref = swap_end;
	ssize_t res = -1;
	size_t heap_sz;

	malloc_mutex_lock(&perm_mtx);
	if (bfd == -1) {
		fprintf(stderr, "restore: backup file not open\n");
		goto rs_return;
	}
	jemalloc_prefork(); /* acquire all jemalloc mutexes */

	/* check compatibility */
	if (check_header(bfd, &heap_sz)) {
		goto rs_return;
	}

	/* read in heap */
	res = lseek(bfd, 0, SEEK_SET);
	if (res == -1) {
		perror("restore: error seeking to heap start");
		goto rs_return;
	}
	res = lread(bfd, swap_base, heap_sz);
	if (res != heap_sz) {
		perror("restore: error reading heap data");
		goto rs_return;
	}

	/* restore globals */
	readvb(plib->globals, plib->gsize, permv, nperm);

	/* zero out shrinkage in mapped heap */
	if (swap_end_ref > swap_end) {
		memset(swap_end, 0, swap_end_ref - swap_end);
	}

	res = 0;
rs_return:
	if (bfd != -1) jemalloc_postfork(); /* release all jemalloc mutexes */
	malloc_mutex_unlock(&perm_mtx);
	return((int)res);
}

#undef swap_base
#undef swap_end
#undef swap_max
#undef base_pages
#undef base_next_addr
#undef base_past_addr

static int check_header(int fd, size_t *heap_sz)
{
	ssize_t res;
	plib_t fnd;

	/* read in plib */
	res = lseek(fd, 0, SEEK_SET);
	if (res == -1) {
		perror("check_header: error seeking to heap header");
		return(-1);
	}
	res = lread(fd, &fnd, sizeof(plib_t));
	if (res != sizeof(plib_t)) {
		perror("check_header: error reading heap header");
		return(-1);
	}
	*heap_sz = fnd.swap_end - fnd.swap_base;

	/* check */
	if (fnd.version_key != plib->version_key) {
		fprintf(stderr,
			"check_header: version key incorrect, found:0x%X expect:0x%X\n",
			fnd.version_key, plib->version_key);
		return(-1);
	}

	if (fnd.swap_base != plib->swap_base) {
		fprintf(stderr,
			"check_header: mmap base address incorrect, found:%p expect:%p\n",
			fnd.swap_base, plib->swap_base);
		return(-1);
	}
	if (fnd.swap_max != plib->swap_max) {
		fprintf(stderr,
			"check_header: mmap max address incorrect, found:%p expect:%p\n",
			fnd.swap_max, plib->swap_max);
		return(-1);
	}
	if (fnd.swap_end < fnd.swap_base || fnd.swap_end > fnd.swap_max) {
		fprintf(stderr,
			"check_header: mmap end address incorrect, found:%p expect:%p-%p\n",
			fnd.swap_end, fnd.swap_base, fnd.swap_max);
		return(-1);
	}

	if (fnd.base_pages < plib->swap_base || fnd.base_pages > plib->swap_end) {
		fprintf(stderr,
			"check_header: internal heap base address incorrect, found:%p expect:%p-%p\n",
			fnd.base_pages, plib->swap_base, plib->swap_end);
		return(-1);
	}
	if (fnd.base_past_addr < plib->swap_base || fnd.base_past_addr > plib->swap_end) {
		fprintf(stderr,
			"check_header: internal heap max address incorrect, found:%p expect:%p-%p\n",
			fnd.base_past_addr, plib->swap_base, plib->swap_end);
		return(-1);
	}
	if (fnd.base_next_addr < fnd.base_pages || fnd.base_next_addr > fnd.base_past_addr) {
		fprintf(stderr,
			"check_header: internal heap end address incorrect, found:%p expect:%p-%p\n",
			fnd.base_next_addr, fnd.base_pages, fnd.base_past_addr);
		return(-1);
	}

	if (fnd.globals < plib->swap_base || fnd.globals > plib->swap_end) {
		fprintf(stderr,
			"check_header: internal globals address incorrect, found:%p expect:%p-%p\n",
			fnd.globals, plib->swap_base, plib->swap_end);
		return(-1);
	}

	return(0);
}


/* NOTES:
 * Globals can be specified at compile and link time with the PERM attribute.
   Alternatively, they can be dynamically identified with a list created
   through calls to perm(void *, size_t);
 * If separately created heaps with associated global blocks are used in the
   same program, an address conflict can occur. Multiple heaps can't both
   start at virtual address zero.
 * If persistent globals are relocated on restore, for instance when a
   program uses an existing backup file created by another program, global
   pointers to other globals will not be valid.
 * Swap can be enabled before calling base_alloc() so that the internal
   heap can be merged with the mmap heap. This has the drawback of adding
   more chucks (4 MB) to the backup that may not be fully used. The chunk
   will be sparse though.
 * jemalloc handles returning memory to the proper chunk allocator based on
   address range.
 * The internal heap doesn't need to be zeroed on allocation.
 * The first time a thread tries to allocate memory, an arena is chosen
   and then a pointer to it is saved in thread local storage (TLS).
   ARENA_SET() and ARENA_GET() are used to access this pointer in TLS.
   A count of how many threads are using each arena is kept in nthreads.
   Currently, the nthreads count and the lock for an arena are kept in the
   persistent area which necessitates these fields being reinitialized on
   restore.
 * Calling a ctl function will call malloc_init_hard() before ctl_init().
 * Configurations not supported for various reasons:
     JEMALLOC_IVSALLOC - chunk_boot() calls rtree_new(), base_alloc()
     DYNAMIC_PAGE_SHIFT - may change layout of internal structures
     JEMALLOC_TCACHE - tcache_boot() calls base_alloc()
     setting of opt_* variables - may change layout of internal structures
 */

/* TODO:
 * Test if PERM_START symbol is defined before use.
 * Use special symbols (*, $, $t, ...) to indicate auto generated names
   (tmp file, app name, process id, ...) in perm_init().
 * Change method of accessing heap for backup so that mopen is not needed
   when only doing a backup.
 * The checkpoint mechanism need not be dependent on a memory-mapped file.
   The pages could be anonymous. At checkpoint time the anonymous pages
   could be copied to a backup file just as easily as file-backed pages.
   On restore, anonymous pages could be remapped to the needed address.
 * Track chunks allocated in a separate structure. A backup or flush could
   then iterate through the allocated chunks and not require the virtual-
   address space to be contiguous.
 * See if the mmapped file can grow with the heap instead of fixing the
   size up front.
 * See if persistent blocks marked with perm() can be memory mapped.
 * Consider, what is the lifetime of a persistent heap? For example, if
   mopen(,"w+", ) (create) is called a second time within an application
   should the old heap be over written with a new one? Should there be an
   mdelete()?
 * Figure out how the number of arenas should be managed. Currently the
   the max number of arenas is 4x the number of CPUs of the machine that
   created the persistent heap. Should a fixed number of arenas be used
   or should the number be reduced if there are fewer CPUs on restore?
   If arenas are reduced, ctl_stats.arenas would need to be reduced also.
 */
