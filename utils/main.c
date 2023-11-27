#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "backtrace.h"

extern void terrible_function();

static void segv_handler(int signal)
{
	backtrace_dump("SIGSEGV", NULL, true);
	abort();
}

int main()
{
	/* 信号处理 */
	struct sigaction action;
	action.sa_handler = segv_handler;
	sigaction(SIGSEGV, &action, NULL);

	/* backtrace初始化 */
	backtrace_init();

	terrible_function();

	backtrace_deinit();
	return 0;
}

