#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "parsecmd.h"
#include "runcmd.h"
#include "jobctl.h"

struct job *the_last_job;

/* TODO: Split the job control code */

/*
 * Run Command
 */

/* Fooli adopted a different way in handling pipe commands than xv6's shell
 * resulting a separation of the original runcmd() function
 *
 * runpipe() serves to make all child processes created be direct descendants
 * of the shell so that termination signals (like SIGCHLD) will be sent to the
 * shell itself and not some of its child*/

/* Customized fork() function for runpipe(), does 2 things:
 * 1. call setpgid() both in parent process and child process
 * to avoid race condition
 * 2. hand over the write end of the old pipe to child process
 * while close them in parent process */
pid_t pfork(int ofd[2], pid_t pgid)
{
	/* nonsense: ofd looks just like ofo hit the wall */
	pid_t pid;
	
	if ((pid = fork()) > 0) {
		setpgid(pid, pgid);
		if (ofd) 
			close(ofd[0]);
	}
	else if (pid == 0) {
		setpgid(pid, pgid);
		if (ofd) {
			dup2(ofd[0], 0);
			close(ofd[0]);
		}
	}
	else {
		dprintf(2, "fork error\n");
		exit(2);
	}
	return pid;
}

/* ss */
int runcmd(struct cmd *cmd)
{
	int ret;
	struct execcmd *ecmd;
	struct redircmd *rcmd;
	struct subshcmd *scmd;

	switch (cmd->type) {
		case BUILTIN:
		case EXEC:
			ecmd = (struct execcmd *) cmd;
			if (ecmd->type == BUILTIN)
				ret = ecmd->function(ecmd->argc, ecmd->argv);
			else {
				execvp(ecmd->argv[0], ecmd->argv);
				dprintf(2, "execvp(%s, ..) failed\n", ecmd->argv[0]);
				exit(1);
			}
			break;
		case REDIR:
			rcmd = (struct redircmd *) cmd;
			if (rcmd->in) {
				close(0);
				if (open(rcmd->in, O_RDONLY) < 0) {
					dprintf(2, "open(%s, ..) failed\n", rcmd->in);
					exit(1);
				}
			}
			if (rcmd->out) {
				close(1);
				if (open(rcmd->out, O_WRONLY | O_CREAT | rcmd->oflag, 
						S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) < 0) {
					dprintf(2, "open(%s, ..) failed\n", rcmd->out);
					exit(1);
				}
			}
			ret = runcmd(rcmd->cmd);
			break;
		case SUBSHELL:
			scmd = (struct subshcmd *) cmd;
			/* jobs inherited from parent shall be cleared first */
			while (the_last_job) {
				struct job *j = the_last_job;
				the_last_job = j->prev;
				free(j);
			}
			ret = parse_n_run(scmd->cmdline);
			break;
		default:
			exit(1);
			/* not supposed to be here */
			break;
	}
	return ret;
}

/* Runpipe() does all works needed to build up a job
 * which consists of the following:
 * 1. call fork() to create jobs and setpgid()
 * 2. call pipe() to create pipes and hand the right end to the right child
 * 3. call runcmd() in child processes to get them to work */
pid_t runpipe(struct cmd *cmd, pid_t *ppgid, int ofd[2])
{
	int fd[2];
	pid_t pid;
	struct pipecmd *pcmd;

	if (cmd->type == PIPE) {
		pcmd = (struct pipecmd *) cmd;
		pipe(fd);
		if ((pid = pfork(ofd, *ppgid)) > 0) {
			*ppgid = getpgid(pid);
			close(fd[1]);
			return runpipe(pcmd->right, ppgid, fd);
		}
		else {
			close(fd[0]);
			dup2(fd[1], 1);
			close(fd[1]);
			/* child process shall never return */
			exit(runcmd(pcmd->left));
		}
	}
	else if (cmd->type != BUILTIN) {
		if ((pid = pfork(ofd, *ppgid)) > 0) {
			*ppgid = getpgid(pid);
			/* the exit status of last piece of pipe command
			 * will determines further execution flow */
			return pid;
		}
		else {
			/* child process shall never return */
			exit(runcmd(cmd));
		}
	}
	else 
		/* builtin at the end of a pipe will function normally
		 * always return negative values to differ itself from a valid pid */
		return -runcmd(cmd);
}



int waitpgid(pid_t pid, pid_t pgid, char separator)
{
	int status;
	struct job *job;

	/* neither wait nor care about exit status */
	if (separator == '&') {
		if (pgid == 0) {
			printf("Geez! Where did you get these damn fool ideas?\n");
			printf("Err.. I mean your \'&\' will be ignored, for this time.\n");
		}
		else {
			job = malloc(sizeof(*job));
			job->pgid = pgid;
			job->prev = the_last_job;
			job->id = (!job->prev ? 1 : job->prev->id + 1);
			the_last_job = job;
		}
		return 0;
	}
	/* get exit status */
	if (pid > 0) {
		waitpid(pid, &status, 0);
		if (WIFEXITED(status))
			status = WEXITSTATUS(status);
		else { 
			printf("A job launched in foreground has terminated abnormally\n");
			exit(2);
		}
	}
	else 
		/* this is actually the return value of some builtin command */
		status = -pid;
	/* wait on child process */
	if (pgid != 0)
		while (waitpid(-pgid, NULL, 0) != -1)
			;
	return status;
}

/* support for "&&" and "||" */ 
int decide(int status, int separator)
{
	int skip;

	switch (separator) {
		case 'A':
			skip = !(status == 0);
			break;
		case 'O':
			skip = (status == 0);
			break;
		default:
			skip = 0;
			break;
	}
	return skip;
}

/* traverse the job list */
void jobinfo(void)
{
	struct job *j, **hold;

	/* damn these lists */
	for (hold = &the_last_job; ; hold = &(*hold)->prev)	{
		while (*hold) {
			j = *hold;
			/* dirty hack to check whether a process group is empty */
			if (waitpid(-j->pgid, NULL, WNOHANG) == -1 && errno == ECHILD) {
				/* printf("[%d] Done\t\t%s\n", j->id, j->cmdline); */
				printf("[%d] Done\t\t%d\n", j->id, j->pgid);
				*hold = j->prev;
				free(j);
			}
			else 
				break;
		}
		if (!*hold)
			break;
	}
	return;
}

void deconstruct(struct cmd *);
struct cmd *parsecmd(char **, char *);

int parse_n_run(char *ptr)
{
	pid_t pid, pgid;
	struct cmd *cmd;
	char separator, status, skip;

	status = skip = 0;
	for ( ; ; deconstruct(cmd)) {
		cmd = parsecmd(&ptr, &separator);
		/* print information on job control */
		jobinfo();

		if (!cmd)
			break;
		/* support for "&&" and "||" */
		if (skip == 1) {
			skip = decide(status, separator);
			/* no memory is leaked here */
			continue;
		}

		/* initializing pgid to be 0 handles the first call to setpgid() */
		pgid = 0;
		pid = runpipe(cmd, &pgid, NULL);

		/* FIXME: manage the controlling terminal */
		
		/*
		 * I managed to put foreground jobs to its process group
		 * But failed to give controlling terminal and take it back
		 * So program like "cat" will not be able to read input from cli
		 * other programs like "vim" will not be able to run at all :(
		 */

		tcsetpgrp(0, pgid);
		status = waitpgid(pid, pgid, separator);

		tcsetpgrp(0, getpgrp());
		skip = decide(status, separator);
	}
	/* return value for subshell command */
	return status;
}
