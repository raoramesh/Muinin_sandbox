/* 32-bit & 64-bit systems use the same syscall table, so handling
 * multiple personalities is simple -- nothing to do!
 */

#define trace_reg_sysnum gpr[0]

static long trace_raw_ret(void *vregs)
{
	trace_regs *regs = vregs;
	return regs->gpr[3];
}

static void trace_set_ret(void *vregs, int err)
{
	trace_regs *regs = vregs;
	regs->gpr[0] = -1;
	regs->gpr[3] = err;
	trace_set_regs(regs);
}

static unsigned long trace_arg(void *vregs, int num)
{
	trace_regs *regs = vregs;
	if (num == 1)
		return regs->orig_gpr3;
	if (num < 7)
		return regs->gpr[3 + num - 1];
	else
		return -1;
}
