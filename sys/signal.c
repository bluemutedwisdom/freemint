/*
 * $Id$
 *
 * This file has been modified as part of the FreeMiNT project. See
 * the file Changes.MH for details and dates.
 *
 *
 * Copyright 1990,1991,1992 Eric R. Smith.
 * Copyright 1992,1993,1994 Atari Corporation.
 * All rights reserved.
 *
 *
 * signal handling routines
 *
 */

# include "signal.h"
# include "global.h"

# include "libkern/libkern.h"

# include "mint/asm.h"
# include "mint/basepage.h"
# include "mint/signal.h"

# include "arch/sig_mach.h"	/* sendsig */

# include "k_exit.h"
# include "k_prot.h"
# include "proc.h"
# include "proc_help.h"
# include "util.h"


/* send_sig: Send signal SIG to process P. If PRIV is non-zero then
 * the signal is generated by the kernel and no permissions should
 * be checked.
 *
 * It is the callers responsibility to call check_sigs() if it is
 * possible that the target process and the sending process are
 * the same.
 *
 * All references to the below function post_sig() should vanish
 * in order to perform permission checks only here.  The rest of
 * post_sig() can then move in here.
 */
static long
send_sig (PROC *p, ushort sig, int priv)
{
	if (suser (get_curproc()->p_cred->ucr))
	{
		priv = 1;
	}
	else if (  get_curproc()->p_cred->ucr->euid == p->p_cred->suid
		|| get_curproc()->p_cred->ucr->euid == p->p_cred->ruid
		|| get_curproc()->p_cred->ruid      == p->p_cred->suid
		|| get_curproc()->p_cred->ruid      == p->p_cred->ruid)
	{
		priv = 1;
	}

	if (!priv && (sig == SIGCONT))
	{
		/* Every process has a permanent ticket to send SIGCONT
		 * to other members of the same process group.
		 */
		if (get_curproc()->pgrp == p->pgrp)
			priv = 1;
	}

	if (!priv)
		return EACCES;	/* The library should set errno to EPERM!  */

	if (sig == 0)
		return 0;	/* Ignore */

	/* R. I. P. */
	if (p->wait_q == ZOMBIE_Q || p->wait_q == TSR_Q)
		return 0;

	assert (p->p_sigacts);

	if (SIGACTION(p, sig).sa_handler == SIG_IGN && !p->ptracer && sig != SIGCONT)
		return 0;

	/* Mark the signal as being pending */
	post_sig (p, sig);

	return 0;
}

/*
 * killgroup(pgrp, sig, priv): send a signal to all members of a process group
 * returns 0 on success, or an error code on failure
 * priv is non-zero if the signal is generated by the kernel, otherwise
 * access privileges are checked
 *
 * Changed by Guido: Don't check for real user id, the effective user
 * id is the one we should obey.
 */
long _cdecl
killgroup (int pgrp, ushort sig, int priv)
{
	PROC *p;
	int found = 0;
	long ret = ENOENT;

	DEBUG (("killgroup(%i, %i, %i)", pgrp, sig, priv));

	if (pgrp < 0)
		return EINTERNAL;

	if (sig >= NSIG)
		return EINVAL;

	for (p = proclist; p; p = p->gl_next)
	{
		if (p->wait_q == ZOMBIE_Q || p->wait_q == TSR_Q)
			continue;

		if (p->pgrp == pgrp)
		{
			long last_error;

			DEBUG (("killgroup: send %i to PID %i (pgrp %i)", sig, p->pid, p->pgrp));

			last_error = send_sig (p, sig, priv);
			if (last_error)
				ret = last_error;
			else
				found = 1;
		}
	}

	if (found)
		return E_OK;

	return ret;
}

/*
 * post_sig: post a signal as being pending. It is assumed that the
 * caller has already verified that "sig" is a valid signal, and
 * moreover it is the caller's responsibility to call check_sigs()
 * if it's possible that p == curproc
 */
void
post_sig (PROC *p, ushort sig)
{
	unsigned long sigm;

	/* just to be sure */
	assert(sig < NSIG);

	/* if the process is already dead, do nothing */
	if (p->wait_q == ZOMBIE_Q || p->wait_q == TSR_Q)
		return;

	/* Init cannot be killed or stopped.  */
	if (p->pid == 1 && (sig == SIGKILL || sig == SIGSTOP))
	{
		/* Ignore! */
		return;
	}

	/* An (initialized) SLB cannot be sent signals at all. The code
	 * in slb.c sets and resets the p->p_flag so that p_kill() works
	 * when it should, and doesn't when shouldn't.
	 */
	if (p->p_flag & P_FLAG_SLB)
	{
		/* Ignore! */
		return;
	}

	/* Some other processes, notably the update daemon cannot be killed.
	 * What about the sld daemon started by the mintnet?
	 */
	if (sig == SIGKILL && (p->p_flag & P_FLAG_SYS))
	{
		/* Ignore! */
		return;
	}

	assert(p->p_sigacts);

	/* if process is ignoring this signal, do nothing
	 * also: signal 0 is SIGNULL, and should never be delivered through
	 * the normal channels (indeed, it's filtered out in dossig.c,
	 * but the extra sanity check here is harmless). The kernel uses
	 * signal 0 internally for some purposes, but it is handled
	 * specially (see supexec() in xbios.c, for example).
	 */

	/* If the process is traced, the tracer should always be notified */
	if (sig == 0
		|| (SIGACTION(p, sig).sa_handler == SIG_IGN
			&& !p->ptracer
			&& sig != SIGCONT))
	{
		return;
	}

	/* mark the signal as pending */
	sigm = (1L << (unsigned long) sig);
	p->sigpending |= sigm;

	/* if the signal is masked, do nothing further
	 *
	 * note: some signals can't be masked, and we handle those elsewhere so
	 * that p->p_sigmask is always valid. SIGCONT is among the unmaskable
	 * signals
	 */
	if ((p->p_sigmask & sigm) != 0)
		return;

	/* otherwise, make sure the process is awake */
	{
		unsigned short sr = splhigh();
		if (p->wait_q && p->wait_q != READY_Q)
		{
			rm_q(p->wait_q, p);
			add_q(READY_Q, p);
		}
		spl(sr);
	}
}

/*
 * special version of kill that can be called from an interrupt
 * handler or device driver
 * it also accepts negative numbers to send signals to groups
 */
long _cdecl
ikill (int pid, ushort sig)
{
	PROC *p;
	long r;

	if (sig >= NSIG)
		return EBADARG;

	if (pid < 0)
		r = killgroup (-pid, sig, 1);
	else if (pid == 0)
		r = killgroup (get_curproc()->pgrp, sig, 1);
	else
	{
		p = pid2proc (pid);
		if (p == 0 || p->wait_q == ZOMBIE_Q || p->wait_q == TSR_Q)
			return ENOENT;

		/* if the user sends signal 0, don't deliver it -- for users,
		 * signal 0 is a null signal used to test the existence of a
		 * process
		 */
		if (sig != 0)
			post_sig (p, sig);

		r = E_OK;
	}

	return r;
}

/*
 * check_sigs: see if we have any signals pending. if so,
 * handle them.
 */
void
check_sigs (void)
{
	struct proc *p = get_curproc();
	ulong sigs, sigm;
	int i;
	short deliversig;

	if (p->pid == 0)
		return;
top:
	assert (p->p_sigacts);
	sigs = p->sigpending;

	/* Always notify the tracer about signals sent */
	if (!p->ptracer || p->sigpending & 1L)
		sigs &= ~(p->p_sigmask);

	if (sigs)
	{
		sigm = 2;

		/* with tracing we need a mechanism to allow a signal to be
		 * delivered to the child (curproc); Fcntl(...TRACEGO...)
		 * passes a SIGNULL to indicate that we should really deliver
		 * the signal, hence its always safe to remove it from pending.
		 */
		deliversig = (p->sigpending & 1L);
		p->sigpending &= ~1L;

		for (i = 1; i < NSIG; i++)
		{
			if (sigs & sigm)
			{
				p->sigpending &= ~sigm;
				if (p->ptracer && !deliversig
					&& (i != SIGCONT) && (i != SIGKILL))
				{
					TRACE (("tracer being notified of signal %d", i));
					stop (i);

					/* the parent may reset our pending
					 * signals, so check again
					 */
					goto top;
				}
				else
				{
					ulong omask;

					omask = p->p_sigmask;

					/* sigextra gives which extra signals
					 * should also be masked
					 */
					p->p_sigmask |= SIGACTION(p, i).sa_mask;
					if (!(SIGACTION(p,i).sa_flags & SA_NODEFER) || (SIGACTION(p, i).sa_mask & sigm)) {
						p->p_sigmask |= sigm;
					} else {
						p->p_sigmask &= ~sigm;
					}

					handle_sig (i);

/*
 * POSIX.1-3.3.4.2(723) "If and when the user's signal handler returns
 * normally, the original signal mask is restored."
 *
 * BUG?: This unmasking could unmask a pending signal which we will not
 * see this time around (if the signal number is less than i) and which
 * was not pending when we started; should we detect this condition and
 * loop around for a second try? POSIX only guarantees delivery of
 * one signal per kernel entry, so this shouldn't really be a problem.
 */
					/* unmask signals */
					p->p_sigmask = omask;
				}
			}

			sigm <<= 1;
		}
	}
}

/*
 * raise: cause a signal to be raised in the current process
 */
void _cdecl
raise (ushort sig)
{
	post_sig (get_curproc(), sig);
	check_sigs ();
}

/*
 * handle_sig: do whatever is appropriate to handle a signal
 */
void
handle_sig (ushort sig)
{
	/* just to be sure */
	assert (sig < NSIG);
	assert (get_curproc()->p_sigacts);
	/* notify proc extensions */
	proc_ext_on_signal(get_curproc(), sig);

	get_curproc()->last_sig = sig;
	if (SIGACTION(get_curproc(), sig).sa_handler == SIG_IGN)
		return;

	if (SIGACTION(get_curproc(), sig).sa_handler == SIG_DFL)
	{
_default:
		switch (sig)
		{
# if 0
		/* Note: SIGNULL is filtered out in dossig.c and is never
		 * actually delivered (its only purpose for the user is to
		 * test for the existence of a process, it isn't a real
		 * signal). The kernel uses SIGNULL internally, but all such
		 * code does the signal handling "by hand" and so no default
		 * handling is necessary.
		 */
		case SIGNULL:
# endif
		case SIGWINCH:
		case SIGCHLD:
		/* SIGFPE is divide by 0
		 * TOS ignores this, so we will too
		 */
		case SIGFPE:
			return;		/* do nothing */

		case SIGSTOP:
		case SIGTSTP:
		case SIGTTIN:
		case SIGTTOU:
			stop (sig);
			return;

		case SIGCONT:
			get_curproc()->sigpending &= ~STOPSIGS;
			return;

		/* here are the fatal signals. for SIGINT, we use
		 * kernel_pterm(ENOSYS) so that TOS programs that catch ^C
		 * via the vector at 0x400 and which expect TOS's error code
		 * (ENOSYS) to be sent will work. For most other signals, we
		 * kernel_pterm with an error code; for SIGKILL, we don't want
		 * to allow the program any chance to recover, so we call
		 * terminate() directly to avoid calling through to the user's
		 * terminate vector.
		 */
		case SIGINT:		/* ^C */
			if (get_curproc()->domain == DOM_TOS)
			{
				get_curproc()->signaled = 1;
				kernel_pterm (get_curproc(), ENOSYS);
				return;
			}
			/* otherwise, fall through */
		default:
			/* Mark the process as being killed
			 * by a signal.  Used by Pwaitpid.
			 */
			get_curproc()->signaled = 1;

			/* tell the user what happened */
			bombs (sig);

			/* the "p_sigmask" check is in case a bus error happens
			 * in the user's term_vec code; we don't want to get
			 * stuck in an infinite loop!
			 */
			if ((get_curproc()->p_sigmask & 1L) || sig == SIGKILL)
				terminate (get_curproc(), sig << 8, ZOMBIE_Q);
			else
				kernel_pterm (get_curproc(), sig << 8);
		}
	}
	else
	{
		if (sendsig (sig))
			goto _default;
	}
}

# if 0
/*
 * sigaltstack controls
 */
# define SS_ONSTACK	1
# define SS_DISABLE	2

# define MINSIGSTKSZ	2048
# define SIGSTKSZ	8192

/* True if we are on the alternate signal stack */

INLINE int
on_sig_stack (PROC *p, ulong sp)
{
	return (sp - p->sigaltstack < p->sigaltstacksize);
}

INLINE int
sas_ss_flags (PROC *p, ulong sp)
{
	return (p->sigaltstacksize == 0 ? SS_DISABLE
		: on_sig_stack (sp) ? SS_ONSTACK : 0);
}

long _cdecl
p_sigaltstack (const stack_t *uss, stack_t *uoss)
{
	return do_sigaltstack (get_curproc(), uss, uoss, xxx);
}

long
do_sigaltstack (PROC *p, const stack_t *ss, stack_t *oss, ulong sp)
{
	if (oss)
	{
		oss->ss_sp = p->sigaltstack;
		oss->ss_size = p->sigaltstacksize;
		oss->ss_flags = sas_ss_flags (p, sp);
	}

	if (uss)
	{
		void *stack;
		ulong size;
		long flags;

		/* err = EFAULT; XXX verify user data */
		stack = ss->ss_sp;
		size = ss->ss_size;
		flags = ss->ss_flags;

		if (on_sig_stack (p, sp))
			return EPERM;

		if (flags & ~SS_DISABLE)
			return EINVAL;

		if (flags & SS_DISABLE)
		{
			stack = NULL;
			size = 0;
		}
		else if (size < MINSIGSTKSZ)
			return ENOMEM;

		p->sigaltstack = stack;
		p->sigaltstacksize = size;
	}

	return 0;
}
# endif

/*
 * stop a process because of signal "sig"
 */
void _cdecl
stop (ushort sig)
{
	ushort code;
	ulong oldmask;

	/* just to be sure */
	assert (sig < NSIG);

	code = sig << 8;

	if (get_curproc()->pid == 0)
	{
		FORCE ("attempt to stop MiNT");
		return;
	}

	/* notify parent */
	if (get_curproc()->ptracer)
	{
		DEBUG (("stop (%i) SIGCHLD -> tracer pid %i", sig, get_curproc()->ptracer->pid));
		post_sig (get_curproc()->ptracer, SIGCHLD);
	}
	else
	{
		PROC *p = pid2proc (get_curproc()->ppid);
		if (p) assert (p->p_sigacts);
		if (p && !(SIGACTION(p, SIGCHLD).sa_flags & SA_NOCLDSTOP))
		{
			unsigned short sr;

			post_sig (p, SIGCHLD);

			sr = splhigh ();
			if (p->wait_q == WAIT_Q && p->wait_cond == (long) sys_pwaitpid)
			{
				rm_q (WAIT_Q, p);
				add_q (READY_Q, p);
			}
			spl (sr);
		}
	}

	oldmask = get_curproc()->p_sigmask;

	if (!get_curproc()->ptracer)
	{
		assert ((1L << sig) & STOPSIGS);

		/* mask out most signals */
		get_curproc()->p_sigmask |= ~(UNMASKABLE | SIGTERM);
	}

	/* notify proc extensions */
	proc_ext_on_stop(get_curproc(), sig);

	/* sleep until someone signals us awake */
	sleep (STOP_Q, (long) code | 0177);

	/* when we wake up, restore the signal mask */
	get_curproc()->p_sigmask = oldmask;

	/* and discard any signals that would cause us to stop again */
	get_curproc()->sigpending &= ~STOPSIGS;
}
