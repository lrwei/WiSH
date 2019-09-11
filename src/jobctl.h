#ifndef JOBCTL_H
#define JOBCTL_H

#include <unistd.h>

struct job {
	unsigned id;
	pid_t pgid;
	/* TODO: keep this for informative reason */
	char *cmdline;
	struct job *prev;
};

extern struct job *the_last_job;

#endif
