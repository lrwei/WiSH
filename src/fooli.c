#include <stdio.h>  // dprintf
#include <signal.h> // signal
#include <stdlib.h> // getenv

#include "handler.h"
#include "readline.h"
#include "runcmd.h"

int main()
{
	char *ptr;
	const char *prompt;
	
	if ((prompt = getenv("PROMPT")) == NULL)
		prompt = ">";

	signal(SIGCHLD, handle_sigchld);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);

	while ((ptr = getcmd(prompt)))
		parse_n_run(ptr);

	dprintf(1, "exit\n");
	return 0;
}
