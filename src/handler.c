#include <sys/wait.h>

#include "jobctl.h"

/* Signal Handlers */

void handle_sigchld(int signo)
{
	struct job *j;

	for (j = the_last_job; j; j = j->prev)
		while (waitpid(-j->pgid, NULL, WNOHANG) > 0)
			;
	return;
}
