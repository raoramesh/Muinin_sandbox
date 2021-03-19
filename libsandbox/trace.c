/* Limited sandbox support for static binaries
 *
 * Copyright 2009 Gentoo Foundation
 * Licensed under the GPL-2
 */

#include "headers.h"
#include "libsandbox.h"
#include "wrappers.h"
#include "sb_nr.h"

static long do_peekdata(long offset);
/* Note on _do_ptrace argument types:
   glibc defines ptrace as:
     long ptrace(enum __ptrace_request request, pid_t pid, void *addr, void *data);
   musl defines ptrace as:
     long ptrace(int, ...);

   Let's clobber to 'int' lowest common denominator.
 */
typedef int sb_ptrace_req_t;
static long _do_ptrace(sb_ptrace_req_t request, const char *srequest, void *addr, void *data);
#define do_ptrace(request, addr, data) _do_ptrace(request, #request, addr, data)
#define _trace_possible(data) true

#ifdef DEBUG
# define SBDEBUG 1
#else
# define SBDEBUG 0
#endif
#define __sb_debug(fmt, args...) do { if (SBDEBUG) sb_eraw(fmt, ## args); } while (0)
#define _sb_debug(fmt, args...)  do { if (SBDEBUG) sb_ewarn("TRACE (pid=%i):%s: " fmt, getpid(), __func__, ## args); } while (0)
#define sb_debug(fmt, args...)   _sb_debug(fmt "\n", ## args)

#include "trace/os.c"

pid_t trace_pid;

#ifndef SB_NO_TRACE

#ifndef HAVE_TRACE_REGS
# error "unable to find struct for tracing regs"
#endif

#ifdef HAVE_OPEN64
# define sb_openat_pre_check sb_openat64_pre_check
#endif

static void trace_exit(int status)
{
	/* if we were vfork-ed, clear trace_pid and exit */
	trace_pid = 0;
	_exit(status);
}

static long _do_ptrace(sb_ptrace_req_t request, const char *srequest, void *addr, void *data)
{
	long ret;
 try_again:
	errno = 0;
	ret = ptrace(request, trace_pid, addr, data);
	if (ret == -1) {
		/* Child hasn't gotten to the next marker yet ? */
		if (errno == ESRCH) {
			int status;
			if (waitpid(trace_pid, &status, 0) == -1) {
				/* nah, it's dead ... should we whine though ? */
				trace_exit(0);
			}
			sched_yield();
			goto try_again;
		} else if (errno == EIO || errno == EFAULT) {
			/* This comes up when the child itself tries to use a bad pointer.
			 * That's not something the sandbox should abort on. #560396
			 */
			return ret;
		} else if (!errno)
			if (request == PTRACE_PEEKDATA ||
			    request == PTRACE_PEEKTEXT ||
			    request == PTRACE_PEEKUSER)
				return ret;

		sb_ebort("ISE:_do_ptrace: ptrace(%s, ..., %p, %p): %s\n",
			srequest, addr, data, strerror(errno));
	}
	return ret;
}

static long do_peekdata(long offset)
{
	return do_ptrace(PTRACE_PEEKDATA, (void *)offset, NULL);
}

static char *do_peekstr(unsigned long lptr)
{
	size_t len, l;
	char *ret;
	long a, i;
	union {
		long val;
		char x[sizeof(long)];
	} s;

	/* if someone does open(NULL), don't shit a brick over it */
	if (lptr < sizeof(long))
		return NULL;

#ifdef HAVE_PROCESS_VM_READV
	struct iovec liov, riov;

	/* We can't cross remote page boundaries when using this :(.
	 * The first call reads up to the edge of the page boundary since we
	 * have no (easy) way of knowing if the next page is valid.  This is
	 * less effort than reading in the /proc/.../maps file and walking it.
	 */
	l = 0x1000;
	riov.iov_base = (void *)lptr;
	len = l - (lptr % l);
	if (!len)
		len = l;
	liov.iov_base = ret = xmalloc(len);
	riov.iov_len = liov.iov_len = len;

	while (1) {
		if (process_vm_readv(trace_pid, &liov, 1, &riov, 1, 0) == -1) {
			int e = errno;
			if (e == ENOSYS) {
				/* This can happen if run on older kernels but built with newer ones. */
				break;
			} else if (e == EFAULT) {
				/* This can happen if the target process uses a bad pointer. #560396 */
				break;
			}
			sb_ebort("ISE:do_peekstr:process_vm_readv(%i, %p{%p, %#zx}, 1, %p{%p, %#zx}, 1, 0) failed: %s\n",
				trace_pid,
				&liov, liov.iov_base, liov.iov_len,
				&riov, riov.iov_base, riov.iov_len,
				strerror(e));
		}

		if (memchr(liov.iov_base, '\0', liov.iov_len) != NULL)
			return ret;
		riov.iov_base += riov.iov_len;
		riov.iov_len = liov.iov_len = l;
		len += l;
		ret = xrealloc(ret, len);
		liov.iov_base = ret + len - l;
	}
#else
	len = 1024;
	ret = xmalloc(len);
#endif

	l = 0;
	while (1) {
		a = lptr & (sizeof(long) - 1);
		lptr -= a;
		errno = 0;
		s.val = do_peekdata(lptr);
		if (unlikely(errno)) {
			if (errno == EIO || errno == EFAULT) {
				ret[0] = '\0';
				return ret;
			}
			sb_ebort("ISE:do_peekstr:do_peekdata(%#lx) failed: %s\n",
				lptr, strerror(errno));
		}
		for (i = a; i < sizeof(long); ++i) {
			ret[l++] = s.x[i];
			if (!s.x[i])
				return ret;
			if (l >= len) {
				len += 1024;
				ret = xrealloc(ret, len);
			}
		}
		lptr += sizeof(long);
	}
}

static const char *strcld_chld(int cld)
{
	switch (cld) {
#define C(c) case CLD_##c: return "CLD_"#c;
	C(CONTINUED)
	C(DUMPED)
	C(EXITED)
	C(KILLED)
	C(TRAPPED)
	C(STOPPED)
#undef C
	default: return "CLD_???";
	}
}
/* strsignal() translates the string when i want C define */
static const char *strsig(int sig)
{
	switch (sig) {
#define S(s) case SIG##s: return "SIG"#s;
	S(ABRT)
	S(ALRM)
	S(CHLD)
	S(CONT)
	S(HUP)
	S(INT)
	S(KILL)
	S(QUIT)
	S(SEGV)
	S(STOP)
	S(TERM)
	S(TRAP)
#undef S
	default: return "SIG???";
	}
}

static void trace_child_signal(int signo, siginfo_t *info, void *context)
{
	sb_debug("got sig %s(%i): code:%s(%i) status:%s(%i)",
		strsig(signo), signo,
		strcld_chld(info->si_code), info->si_code,
		strsig(info->si_status), info->si_status);

	switch (info->si_code) {
		case CLD_DUMPED:
		case CLD_KILLED:
			trace_exit(128 + info->si_status);

		case CLD_EXITED:
			__sb_debug(" = %i\n", info->si_status);
			trace_exit(info->si_status);

		case CLD_TRAPPED:
			switch (info->si_status) {
				case SIGSTOP:
					kill(trace_pid, SIGCONT);
				case SIGTRAP:
				case SIGCONT:
					return;
			}

			/* For whatever signal the child caught, let's ignore it and
			 * continue on.  If it aborted, segfaulted, whatever, that's
			 * its problem, not ours, so don't whine about it.  We just
			 * have to be sure to bubble it back up.  #265072
			 */
			do_ptrace(PTRACE_CONT, NULL, (void *)(long)info->si_status);
			return;
	}

	sb_eerror("ISE:trace_child_signal: child (%i) signal %s(%i), code %s(%i), status %s(%i)\n",
		trace_pid,
		strsig(signo), signo,
		strcld_chld(info->si_code), info->si_code,
		strsig(info->si_status), info->si_status);
}

static const struct syscall_entry *lookup_syscall_in_tbl(const struct syscall_entry *tbl, int nr)
{
	while (tbl->name)
		if (tbl->nr == nr)
			return tbl;
		else
			++tbl;
	return NULL;
}

struct syscall_state {
	void *regs;
	int nr;
	const char *func;
	bool (*pre_check)(const char *func, const char *pathname, int dirfd);
};

static bool _trace_check_syscall_C(struct syscall_state *state, int ibase)
{
	char *path = do_peekstr(trace_arg(state->regs, ibase));
	__sb_debug("(\"%s\")", path);
	bool pre_ret, ret;
	if (state->pre_check)
		pre_ret = state->pre_check(state->func, path, AT_FDCWD);
	else
		pre_ret = true;
	if (pre_ret)
		ret = _SB_SAFE(state->nr, state->func, path);
	else
		ret = true;
	free(path);
	return ret;
}
static bool trace_check_syscall_C(struct syscall_state *state)
{
	return _trace_check_syscall_C(state, 1);
}

static bool __trace_check_syscall_DCF(struct syscall_state *state, int ibase, int flags)
{
	int dirfd = trace_arg(state->regs, ibase);
	char *path = do_peekstr(trace_arg(state->regs, ibase + 1));
	__sb_debug("(%i, \"%s\", %x)", dirfd, path, flags);
	bool pre_ret, ret;
	if (state->pre_check)
		pre_ret = state->pre_check(state->func, path, dirfd);
	else
		pre_ret = true;
	if (pre_ret)
		ret = _SB_SAFE_AT(state->nr, state->func, dirfd, path, flags);
	else
		ret = true;
	free(path);
	return ret;
}
static bool _trace_check_syscall_DCF(struct syscall_state *state, int ibase)
{
	int flags = trace_arg(state->regs, ibase + 2);
	return __trace_check_syscall_DCF(state, ibase, flags);
}
static bool trace_check_syscall_DCF(struct syscall_state *state)
{
	return _trace_check_syscall_DCF(state, 1);
}

static bool _trace_check_syscall_DC(struct syscall_state *state, int ibase)
{
	return __trace_check_syscall_DCF(state, ibase, 0);
}
static bool trace_check_syscall_DC(struct syscall_state *state)
{
	return _trace_check_syscall_DC(state, 1);
}

static bool trace_check_syscall(const struct syscall_entry *se, void *regs)
{
	struct syscall_state state;
	bool ret = true;
	int nr;
	const char *name;

	/* These funcs aren't syscalls and so there are no checks:
	 *  - fopen
	 *  - lutimes
	 *  - mkfifo*
	 *  - opendir (uses open())
	 *  - _*x*mknod*
	 *  - 64bit versions of most funcs
	 *  - system / popen / most exec funcs
	 *
	 * Can't use switch() statement here as we auto define missing
	 * SYS_xxx to SB_NR_UNDEF in the build system
	 */
	if (!se)
		goto done;

	state.regs = regs;
	state.nr = nr = se->sys;
	state.func = name = se->name;

	if (nr == SB_NR_UNDEF)          goto done;
	else if (nr == SB_NR_MKDIR)     state.pre_check = sb_mkdirat_pre_check;
	else if (nr == SB_NR_MKDIRAT)   state.pre_check = sb_mkdirat_pre_check;
	else if (nr == SB_NR_UNLINK)    state.pre_check = sb_unlinkat_pre_check;
	else if (nr == SB_NR_UNLINKAT)  state.pre_check = sb_unlinkat_pre_check;
	else                            state.pre_check = NULL;

	/* Hmm, add these functions to the syscall table and avoid this if() ? */
	if (nr == SB_NR_UNDEF)          goto done;
	else if (nr == SB_NR_CHMOD)     return  trace_check_syscall_C  (&state);
	else if (nr == SB_NR_CHOWN)     return  trace_check_syscall_C  (&state);
	else if (nr == SB_NR_CREAT)     return  trace_check_syscall_C  (&state);
	else if (nr == SB_NR_FCHMODAT)  return  trace_check_syscall_DCF(&state);
	else if (nr == SB_NR_FCHOWNAT)  return  trace_check_syscall_DCF(&state);
	else if (nr == SB_NR_FUTIMESAT) return  trace_check_syscall_DC (&state);
	else if (nr == SB_NR_LCHOWN)    return  trace_check_syscall_C  (&state);
	else if (nr == SB_NR_LINK)      return _trace_check_syscall_C  (&state, 2);
	else if (nr == SB_NR_LINKAT)    return _trace_check_syscall_DCF(&state, 3);
	else if (nr == SB_NR_MKDIR)     return  trace_check_syscall_C  (&state);
	else if (nr == SB_NR_MKDIRAT)   return  trace_check_syscall_DC (&state);
	else if (nr == SB_NR_MKNOD)     return  trace_check_syscall_C  (&state);
	else if (nr == SB_NR_MKNODAT)   return  trace_check_syscall_DC (&state);
	else if (nr == SB_NR_RENAME)    return  trace_check_syscall_C  (&state) &&
	                                       _trace_check_syscall_C  (&state, 2);
	else if (nr == SB_NR_RENAMEAT)  return  trace_check_syscall_DC (&state) &&
	                                       _trace_check_syscall_DC (&state, 3);
	else if (nr == SB_NR_RMDIR)     return  trace_check_syscall_C  (&state);
	else if (nr == SB_NR_SYMLINK)   return _trace_check_syscall_C  (&state, 2);
	else if (nr == SB_NR_SYMLINKAT) return _trace_check_syscall_DC (&state, 2);
	else if (nr == SB_NR_TRUNCATE)  return  trace_check_syscall_C  (&state);
	else if (nr == SB_NR_TRUNCATE64)return  trace_check_syscall_C  (&state);
	else if (nr == SB_NR_UNLINK)    return  trace_check_syscall_C  (&state);
	else if (nr == SB_NR_UNLINKAT)  return  trace_check_syscall_DCF(&state);
	else if (nr == SB_NR_UTIME)     return  trace_check_syscall_C  (&state);
	else if (nr == SB_NR_UTIMES)    return  trace_check_syscall_C  (&state);
	else if (nr == SB_NR_UTIMENSAT) return _trace_check_syscall_DCF(&state, 1);

	else if (nr == SB_NR_ACCESS) {
		char *path = do_peekstr(trace_arg(regs, 1));
		int flags = trace_arg(regs, 2);
		__sb_debug("(\"%s\", %x)", path, flags);
		ret = _SB_SAFE_ACCESS(nr, name, path, flags);
		free(path);
		return ret;

	} else if (nr == SB_NR_FACCESSAT) {
		int dirfd = trace_arg(regs, 1);
		char *path = do_peekstr(trace_arg(regs, 2));
		int flags = trace_arg(regs, 3);
		__sb_debug("(%i, \"%s\", %x)", dirfd, path, flags);
		ret = _SB_SAFE_ACCESS_AT(nr, name, dirfd, path, flags);
		free(path);
		return ret;

	} else if (nr == SB_NR_OPEN) {
		char *path = do_peekstr(trace_arg(regs, 1));
		int flags = trace_arg(regs, 2);
		__sb_debug("(\"%s\", %x)", path, flags);
		if (sb_openat_pre_check(name, path, AT_FDCWD, flags))
			ret = _SB_SAFE_OPEN_INT(nr, name, path, flags);
		else
			ret = 1;
		free(path);
		return ret;

	} else if (nr == SB_NR_OPENAT) {
		int dirfd = trace_arg(regs, 1);
		char *path = do_peekstr(trace_arg(regs, 2));
		int flags = trace_arg(regs, 3);
		__sb_debug("(%i, \"%s\", %x)", dirfd, path, flags);
		if (sb_openat_pre_check(name, path, dirfd, flags))
			ret = _SB_SAFE_OPEN_INT_AT(nr, name, dirfd, path, flags);
		else
			ret = 1;
		free(path);
		return ret;
	}

 done:
	__sb_debug("(...)");
	return ret;
}

static void trace_loop(void)
{
	trace_regs regs;
	bool before_exec, before_syscall, fake_syscall_ret;
	long ret;
	int nr, status;
	const struct syscall_entry *se, *tbl_after_fork;

	before_exec = true;
	before_syscall = false;
	fake_syscall_ret = false;
	tbl_after_fork = NULL;
	do {
		ret = do_ptrace(PTRACE_SYSCALL, NULL, NULL);
		waitpid(trace_pid, &status, 0);

		if (before_exec) {
			unsigned event = ((unsigned)status >> 16);
			if (event == PTRACE_EVENT_EXEC) {
				_sb_debug("hit exec!");
				before_exec = false;
			} else
				_sb_debug("waiting for exec; status: %#x", status);
			ret = trace_get_regs(&regs);
			tbl_after_fork = trace_check_personality(&regs);
			continue;
		}

		ret = trace_get_regs(&regs);
		nr = trace_get_sysnum(&regs);

		se = lookup_syscall_in_tbl(tbl_after_fork, nr);
		ret = trace_get_regs(&regs);
		if (before_syscall) {
			_sb_debug("%s:%i", se ? se->name : "IDK", nr);
			if (!trace_check_syscall(se, &regs)) {
				sb_debug_dyn("trace_loop: forcing EPERM after %s\n", se->name);
				trace_set_sysnum(&regs, -1);
				fake_syscall_ret = true;
			}
		} else {
			int err;

			if (unlikely(fake_syscall_ret)) {
				ret = -1;
				err = EPERM;
				trace_set_ret(&regs, err);
				fake_syscall_ret = false;
			} else
				ret = trace_result(&regs, &err);

			__sb_debug(" = %li", ret);
			if (err)
				__sb_debug(" (errno: %i: %s)", err, strerror(err));
			__sb_debug("\n");
		}

		before_syscall = !before_syscall;
	} while (1);
}

void trace_main(const char *filename, char *const argv[])
{
	struct sigaction sa, old_sa;

	sa.sa_flags = SA_RESTART | SA_SIGINFO;
	sa.sa_sigaction = trace_child_signal;
	sigaction(SIGCHLD, &sa, &old_sa);

	sb_debug_dyn("trace_main: tracing: %s\n", filename);

	if (trace_pid)
		sb_ebort("ISE: trace code assumes multiple threads are not forking\n");

	trace_pid = fork();
	if (unlikely(trace_pid == -1)) {
		sb_ebort("ISE: vfork() failed: %s\n", strerror(errno));
	} else if (trace_pid) {
		sb_debug("parent waiting for child (pid=%i) to signal", trace_pid);
		waitpid(trace_pid, NULL, 0);
		do_ptrace(PTRACE_SETOPTIONS, NULL,
			(void *)(PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACEEXEC));
		sb_close_all_fds();
		trace_loop();
		sb_ebort("ISE: child should have quit, as should we\n");
	}

	sb_debug("child setting up ...");
	sigaction(SIGCHLD, &old_sa, NULL);
	do_ptrace(PTRACE_TRACEME, NULL, NULL);
	kill(getpid(), SIGSTOP);
	/* child returns */
}

#else

#undef _trace_possible
#define _trace_possible(data) false

void trace_main(const char *filename, char *const argv[])
{
	/* trace_possible() triggers a warning for us */
}

#endif

static char *flatten_args(char *const argv[])
{
	char *ret;
	size_t i, len;

	len = 1;
	for (i = 0; argv[i]; ++i) {
		len += strlen(argv[i]) + 1;
		if (strchr(argv[i], ' '))
			len += 2;
	}

	ret = xmalloc(len);
	ret[0] = '\0';
	for (i = 0; argv[i]; ++i) {
		if (strchr(argv[i], ' ')) {
			strcat(ret, "'");
			strcat(ret, argv[i]);
			strcat(ret, "'");
		} else
			strcat(ret, argv[i]);
		strcat(ret, " ");
	}

	return ret;
}

bool trace_possible(const char *filename, char *const argv[], const void *data)
{
	if (_trace_possible(data))
		return true;

	char *args = flatten_args(argv);
	sb_eqawarn("Unable to trace static ELF: %s: %s\n", filename, args);
	free(args);
	return false;
}
