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

#ifndef _PERMA_H
#define _PERMA_H

#include <stddef.h> /* size_t */

/* Adapted from gnu-stabs.h */
/* Specify the section when used in a declaration */
#define symbol_set_section(set) \
  __attribute__ ((section(#set)))

/* Declare SET for use in this module, if defined in another module.  */
#define symbol_set_declare(set) \
  extern void *__start_##set, *__stop_##set;

/* Return a pointer (void *) to the start of SET.  */
#define symbol_set_start(set) \
  ((void *)&__start_##set)

/* Return a pointer (void *) to the end of SET.  */
#define symbol_set_end(set) \
  ((void *)&__stop_##set)

/* Return the size (size_t) of SET. */
#define symbol_set_size(set) \
  (((size_t)&__stop_##set)-((size_t)&__start_##set))

/* Return true if PTR is within the address range of SET */
#define symbol_set_in(set, ptr) \
  ((void *)(ptr) >= (void *)&__start_##set && (void *)(ptr) < (void *)&__stop_##set)

#define PERM       symbol_set_section(persistent)
#define PERM_START symbol_set_start(persistent)
#define PERM_END   symbol_set_end(persistent)
#define PERM_SIZE  symbol_set_size(persistent)
symbol_set_declare(persistent)


/* Register a block as persistent memory */
int perm(void *ptr, size_t size);

/* Open and map file into core memory */
int mopen(const char *fname, const char *mode, size_t size);

/* Close memory-mapped file */
int mclose(void);

/* Flushes in-core data to memory-mapped file */
int mflush(void);

/* Open backup file */
int bopen(const char *fname, const char *mode);

/* Close backup file */
int bclose(void);

/* Backup globals and heap to backup file */
int backup(void);

/* Restore globals and heap from backup file */
int restore(void);

#endif /* _PERMA_H */
