/*
 * File: localdecls.h
 *
 * Copyright 1999-2004 Gentoo Foundation
 * Distributed under the terms of the GNU General Public License v2
 * $Header$
 */

#ifndef __LOCALDECLS_H__
#define __LOCALDECLS_H__

/* take care of broken ld loading */
#if defined(__GLIBC__)

# if __GLIBC__ <= 2 && __GLIBC_MINOR__ <= 2
#  define BROKEN_RTLD_NEXT
#  define LIBC 5
# endif

# if !defined(BROKEN_RTLD_NEXT)
#  if defined(__mips__)
#   define BROKEN_RTLD_NEXT
#  endif
# endif

#endif

#define GLIBC_MINOR __GLIBC_MINOR__

#ifdef PATH_MAX
# define SB_PATH_MAX PATH_MAX * 2
# if (SB_PATH_MAX >= INT_MAX) || (SB_PATH_MAX < PATH_MAX)
#  undef SB_PATH_MAX
#  define SB_PATH_MAX PATH_MAX + 25
#  if (SB_PATH_MAX >= INT_MAX) || (SB_PATH_MAX < PATH_MAX)
#   error SB_PATH_MAX too big!
#  endif
# endif
#else
# error PATH_MAX not defined!
#endif

#endif