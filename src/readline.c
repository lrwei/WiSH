#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*
 * Naive Readline Facilities, Modify Maybe
 */

/* fflush(stdin) does not work when stdin points to a terminal */
void flush_stdin(void)
{
	char c;

	while ((c = getchar()) != '\n' && c != EOF)
		;
	return;
}

/* Command + '\n' + '\0' */
#define BUFSIZE 128 + 2

char input[BUFSIZE];

char *getcmd(const char *prompt)
{
	printf("%s ", prompt);
	while (fgets(input, sizeof(input), stdin) != NULL) {
		if (input[strlen(input) - 1] != '\n') {
			/* Double Ctrl-D can confuse it */
			printf("Too long, dude.\n");
			flush_stdin();
			printf("%s ", prompt);
		}
		else 
			return input;
	}

	/* Both an EOF and a read error cause fgets to return NULL */
	if (ferror(stdin)) {
		printf("getcmd: Read error\n");
		exit(1);
	}
	return NULL;
}
