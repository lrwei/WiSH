#ifndef PARSECMD_H
#define PARSECMD_H

#define BUILTIN		0
#define EXEC		1
#define REDIR		2
#define PIPE		3
#define SUBSHELL	4

/* generic cmd type */
struct cmd {
	int type;
};

#define MAXARGS 9

struct execcmd {
	int type;
	int argc;
	char *argv[MAXARGS + 1];	/* +1 for the trailing NULL ptr */
	char *eargv[MAXARGS + 1];
	int (*function)(int argc, char **argv);
};

struct redircmd {
	int type;
	int oflag;
	char *in;
	char *ein;
	char *out;
	char *eout;
	struct cmd *cmd;
};

struct pipecmd {
	int type;
	struct cmd *left;
	struct cmd *right;
};

struct subshcmd {
	int type;
	char *cmdline;
	char *ecmdline;
};

#endif
