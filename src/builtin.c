#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/* builtin commands shall never return negative value */

int builtin_cd(int argc, char **argv)
{
	char *dir;

	if (argc == 1) {
		if ((dir = getenv("HOME")) == NULL) {
			dprintf(2, "cd: environment variable HOME is not defined\n");
			return 1;
		}
	}
	else if (argc == 2)
		dir = argv[1];
	else {
		dprintf(2, "cd: too much arguments\n");
		return 1;
	}
	if (chdir(dir)) {
		dprintf(2, "cd: failed to change directory to %s\n", dir);
		return 1;
	}
	return 0;
}

int builtin_exit(int argc, char **argv)
{
	exit(0);
}

struct builtin {
	char *name;
	int (*function)(int, char **);
};

struct builtin builtin_list[] = {
	{"cd", builtin_cd},
	{"exit", builtin_exit},
};

int (*check_builtin(char *name))(int, char **)
{
	int i;

	for (i = 0; i < sizeof(builtin_list) / sizeof(builtin_list[0]); i++)
		if (!strcmp(name, builtin_list[i].name))
			return builtin_list[i].function;
	return NULL;
}
