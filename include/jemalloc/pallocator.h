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

/**
 *  @file pallocator.h
 */

#ifndef _PALLOCATOR_H
#define _PALLOCATOR_H 1

#include <new> // bad_alloc
#include "jemalloc/jemalloc.h"

#define PERM_NEW(type) new(::JEMALLOC_P(malloc)(sizeof(type))) (type)
#define PERM_DELETE(ptr,type) (ptr)->~type(); ::JEMALLOC_P(free)(ptr)
#define PERM_FREE(ptr) ::JEMALLOC_P(free)(ptr)

#define PERM_NS persistent

namespace PERM_NS
{
  template<typename _Tp>
    class allocator;

  /// allocator<void> specialization.
  template<>
    class allocator<void>
    {
    public:
      typedef size_t      size_type;
      typedef ptrdiff_t   difference_type;
      typedef void*       pointer;
      typedef const void* const_pointer;
      typedef void        value_type;

      template<typename _Tp1>
        struct rebind
        { typedef allocator<_Tp1> other; };
    };

  /**
   *  @brief  An allocator that uses JEMALLOC_P(malloc), as per [20.4].
   *
   *  This is precisely the allocator defined in the C++ Standard.
   *    - all allocation calls operator new
   *    - all deallocation calls operator delete
   */
  template<typename _Tp>
    class allocator
    {
    public:
      typedef size_t     size_type;
      typedef ptrdiff_t  difference_type;
      typedef _Tp*       pointer;
      typedef const _Tp* const_pointer;
      typedef _Tp&       reference;
      typedef const _Tp& const_reference;
      typedef _Tp        value_type;

      template<typename _Tp1>
        struct rebind
        { typedef allocator<_Tp1> other; };

      allocator() throw() { }

      allocator(const allocator&) throw() { }

      template<typename _Tp1>
        allocator(const allocator<_Tp1>&) throw() { }

      ~allocator() throw() { }

      pointer
      address(reference __x) const { return &__x; }

      const_pointer
      address(const_reference __x) const { return &__x; }

      pointer
      allocate(size_type __n, const void* = 0)
      {
        void *p = ::JEMALLOC_P(malloc)(__n * sizeof(_Tp));
        if (p == NULL)
          throw std::bad_alloc();
        return static_cast<_Tp*>(p);
      }

      void
      deallocate(pointer __p, size_type)
      { ::JEMALLOC_P(free)(static_cast<void*>(__p)); }

      size_type
      max_size() const throw()
      { return size_t(-1) / sizeof(_Tp); }

      void
      construct(pointer __p, const _Tp& __val)
      { ::new(__p) _Tp(__val); }

      void
      destroy(pointer __p) { __p->~_Tp(); }
    };

  template<typename _T1, typename _T2>
    inline bool
    operator==(const allocator<_T1>&, const allocator<_T2>&)
    { return true; }

  template<typename _T1, typename _T2>
    inline bool
    operator!=(const allocator<_T1>&, const allocator<_T2>&)
    { return false; }

} // namespace

// Override global operator new and delete
#ifdef PERM_OVERRIDE_NEW

void *operator new(size_t size)
{
  void *p = ::JEMALLOC_P(malloc)(size);
  if (p == NULL)
    throw std::bad_alloc();
  return p;
}

void operator delete(void *p)
{
  ::JEMALLOC_P(free)(p);
}

void *operator new[](size_t size)
{
  void *p = ::JEMALLOC_P(malloc)(size);
  if (p == NULL)
    throw std::bad_alloc();
  return p;
}

void operator delete[](void *p)
{
  ::JEMALLOC_P(free)(p);
}

#endif // OVERRIDE_NEW

#endif // _PALLOCATOR_H
