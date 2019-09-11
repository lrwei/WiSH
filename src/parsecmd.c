#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "builtin.h"
#include "parsecmd.h"


/*
 * Helper Functions
 */

static char whitespace[] = " \t\n\v\f";
static char symbol[] = "()<>|;&";

/* Ordinary tokenizer that skips blanks at both ends */
char gettoken(char **pp, char **pbegin, char **pend)
{
	char c;
	char *ptr = *pp;

	while (*ptr && strchr(whitespace, *ptr))
		ptr++;
	c = *ptr;
	if (pbegin)
		*pbegin = ptr;
	switch (*ptr) {
		case '\0':
			break;
		case '(':
		case ')':
		case '<':
			/* TODO: add support for here document
			 * Expected to be quite tough though.. */
		case ';':
			ptr++;
			break;
		case '>':
			/* TODO: add support for ">&" */
		case '&':
		case '|':
			ptr++;
			if (*ptr == c) {
				ptr++;
				/* '+': append, 'A': and, 'O': or */
				c = (c == '>') ? '+' : ((c == '&') ? 'A' : 'O');
			}
			break;
		default:
			c = 'a';
			/* cut off useless manipulation of ptr to make peek() efficient
			 * if both pbegin and pend is NULL, won't affect gettoken() calls
			 * in parsesubsh() and after a successful peek() for symbols */
			if (!(pbegin || pend))
				break;
			/* !strchr("xxx", *ptr) implies *ptr != '\0' */
			while (!strchr(whitespace, *ptr) && !strchr(symbol, *ptr))
				ptr++;
			break;
	}
	if (pend)
		*pend = ptr;
	/* skipping the trailing blanks can be convenient */ 
	while (*ptr && strchr(whitespace, *ptr))
		ptr++;
	*pp = ptr;
	return c;
}

/* Have a look at the next token */
int peek(char **pp, char *str)
{
	char tok, *ptr = *pp;

	while (*ptr && strchr(whitespace, *ptr))
		ptr++;
	*pp = ptr;
	/* increment made below will not affect *pp */
	tok = gettoken(&ptr, NULL, NULL);
	return tok && strchr(str, tok);
}

int err;

/* Report parsing error and flush the leftovers */
void error(char **pp, char *description, char *leftover)
{
	char *ptr;

	err = 1;
	dprintf(2, "%s\nleftover: %s", description, leftover);
	if (*leftover == '\0')
		dprintf(2, "\033[1;37;40m%%\033[0m\n");
	/* flush the remaining command line */
	for (ptr = *pp; *ptr; ptr++)
		;
	*pp = ptr;
	return;
}

/* 
 * Construction And Deconstruction For Various Kinds Of Command
 */

struct cmd *execcmd(void)
{
	struct execcmd *cmd;

	cmd = malloc(sizeof(*cmd));
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = EXEC;
	return (struct cmd *) cmd;
}

/* TODO: support descriptor assigning and "&>" operator */
struct cmd *redircmd(struct cmd *cmd, char mode, char *begin, char *end)
{
	struct redircmd *rcmd;

	/* to have only one layer */
	if (cmd->type != REDIR) {
		rcmd = malloc(sizeof(*rcmd));
		memset(rcmd, 0, sizeof(*rcmd));
		rcmd->type = REDIR;
		rcmd->cmd = cmd;
	}
	else 
		rcmd = (struct redircmd *) cmd;
	if (mode == '<') {
		rcmd->in = begin;
		rcmd->ein = end;
	}
	else {
		/* '>' and '+' */
		rcmd->oflag = (mode == '>' ? O_TRUNC : O_APPEND);
		rcmd->out = begin;
		rcmd->eout = end;
	}
	return (struct cmd *) rcmd;
}

struct cmd *pipecmd(struct cmd *left, struct cmd *right)
{
	struct pipecmd *cmd;

	cmd = malloc(sizeof(*cmd));
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = PIPE;
	cmd->left = left;
	cmd->right = right;
	return (struct cmd *) cmd;
}

struct cmd *subshcmd(void)
{
	struct subshcmd *cmd;

	cmd = malloc(sizeof(*cmd));
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = SUBSHELL;
	return (struct cmd *) cmd;
}

/* During the construction of cmds no string is copied
 * instead, only the begin and end pointer is recorded, so null character
 * must be manually set to where end pointers point to, to separate them */
void nulterminate(struct cmd *cmd)
{
	int i;
	struct execcmd *ecmd;
	struct redircmd *rcmd;
	struct pipecmd *pcmd;
	struct subshcmd *scmd;

	/* empty command, typically "\n" */
	if (!cmd)
		return;

	switch (cmd->type) {
		case EXEC:
			ecmd = (struct execcmd *) cmd;
			for (i = 0; ecmd->argv[i]; i++) 
				*ecmd->eargv[i] = '\0';
			/* check whether this is a builtin command */
			if ((ecmd->function = check_builtin(ecmd->argv[0])))
				ecmd->type = BUILTIN;
			break;
		case REDIR:
			rcmd = (struct redircmd *) cmd;
			nulterminate(rcmd->cmd);
			if (rcmd->ein)
				*rcmd->ein = '\0';
			if (rcmd->eout)
				*rcmd->eout = '\0';
			break;
		case PIPE:
			pcmd = (struct pipecmd *) cmd;
			nulterminate(pcmd->left);
			nulterminate(pcmd->right);
			break;
		case SUBSHELL:
			scmd = (struct subshcmd *) cmd;
			*scmd->ecmdline = '\0';
			break;
		default:
			/* not supposed to be here */
			break;
	}
	return;
}

void deconstruct(struct cmd *cmd)
{
	struct redircmd *rcmd;
	struct pipecmd *pcmd;

	/* broken command like " ; ls" or " | cat" */
	if (!cmd)
		return;

	switch (cmd->type) {
		case BUILTIN:
		case EXEC:
		case SUBSHELL:
			free(cmd);
			break;
		case REDIR:
			rcmd = (struct redircmd *) cmd;
			deconstruct(rcmd->cmd);
			free(cmd);
			break;
		case PIPE:
			pcmd = (struct pipecmd *) cmd;
			deconstruct(pcmd->left);
			deconstruct(pcmd->right);
			free(cmd);
			break;
		default:
			/* not supposed to be here */
			break;
	}
	return;
}


/*
 * Parsing Functions
 */

struct cmd *parseredir(char **pp, struct cmd *cmd)
{
	char tok, *begin, *end;

	while (peek(pp, "<>+")) {
		tok = gettoken(pp, NULL, NULL);
		if (gettoken(pp, &begin, &end) != 'a') {
			error(pp, "syntax error -- redirection target expected", begin);
			return cmd;
		}
		cmd = redircmd(cmd, tok, begin, end);
	}
	return cmd;
}

struct cmd *parsesubsh(char **pp)
{
	char tok;
	int cnt = 1;
	struct subshcmd *scmd;

	scmd = (struct subshcmd *) subshcmd();
	/* cmdline points to the character following '(' */
	gettoken(pp, NULL, &scmd->cmdline);
	/* ecmdline will eventually point to corresponding ')' */
	while (cnt != 0 && (tok = gettoken(pp, &scmd->ecmdline, NULL)))
		if (tok == '(')
			cnt++;
		else if (tok == ')')
			cnt--;
	if (cnt != 0) 
		error(pp, "syntax error -- missing close parenthese", scmd->cmdline);
	return (struct cmd *) scmd;
}

struct cmd *parseexec(char **pp)
{
	int argc = 0;
	struct cmd *cmd;
	struct execcmd *ecmd;
	char *begin, *end;

	if (peek(pp, "("))
		cmd = parsesubsh(pp);
	else if (peek(pp, "a")) {
		ecmd = (struct execcmd *) execcmd();
		do {
			if (argc >= MAXARGS) {
				error(pp, "too much arguments", end);
				return (struct cmd *) ecmd;
			}
			gettoken(pp, &begin, &end);
			ecmd->argv[argc] = begin;
			ecmd->eargv[argc] = end;
			argc++;
		} while (peek(pp, "a"));
		ecmd->argv[argc] = NULL;
		ecmd->eargv[argc] = NULL;
		ecmd->argc = argc;	/* for builtin command */
		cmd = (struct cmd *) ecmd;
	}
	else {
		/* whether returning NULL here is an error can't be decided at this
		 * stage, the command line may consist of solely "\n" which is fine
		 * or something like "echo hello |", "| cat" and ";", which are not
		 * besides, this is the only place where NULL cmd originates */
		return NULL;
	}
	return parseredir(pp, cmd);
}

struct cmd *parsepipe(char **pp)
{
	struct cmd *cmd, *pcmd;

	cmd = parseexec(pp);
	if (peek(pp, "|")) {
		if (!cmd)
			error(pp, "syntax error -- unexpected pipe", *pp);
		else {
			gettoken(pp, NULL, NULL);
			pcmd = parsepipe(pp);
			if (!pcmd)
				error(pp, "syntax error -- execcmd expected", *pp);
			else 
				cmd = pipecmd(cmd, pcmd);
		}
	}
	return cmd;
}

struct cmd *parsecmd(char **pp, char *pseparator)
{
	struct cmd *cmd;

	cmd = parsepipe(pp);
	/* if neither ";&AO" nor '\0' */
	if (!peek(pp, ";&AO") && **pp)
		error(pp, "syntax error -- separator expected", *pp);
	/* if NULLcmd but not '\0' */
	else if (!cmd && **pp)
		error(pp, "syntax error -- execcmd expected", *pp);
	if (err) {
		err = 0;
		deconstruct(cmd);
		return NULL;
	}
	*pseparator = gettoken(pp, NULL, NULL);
	nulterminate(cmd);
	return cmd;
}
