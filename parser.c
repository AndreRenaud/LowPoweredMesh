/**
 * \file	imx_usb_console/parser.c
 * \date	2013-Sep-12
 * \author	Andre Renaud
 * \copyright	Aiotec Ltd/Bluewater Systems
 * \brief       Implementation of a rudimentary command line parser
 * \description Parsers files/lines of the form
 *      command arg1 arg2 arg3
 * Calls appropriate callback functions based on 'command'
 */
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "parser.h"

/**
 * Split the line into separate words, putting them into
 * params. It modifies the original string by inserting nul
 * terminators at the end of each word, and supplies pointers
 * to the start of each word into params.
 * @return Number of parameters decoded
 */
static int decode_line(char *line, char **params, int max_params)
{
	char *pos;
	int nparams = 0;

        /* Drop out any comments */
        pos = strstr(line, "//");
        if (pos)
            *pos = '\0';

        pos = strstr(line, "#");
        if (pos && strncmp(&pos[1], "define", 6) != 0) // we allow #define
            *pos = '\0';

	pos = line;
	while (pos && nparams < max_params) {
		char *end;
                int done;

		/* Skip any leading white space */
		while (*pos && isblank(*pos))
			pos++;

		/* Are we at the end? */
		if (!*pos || *pos == '\r' || *pos == '\n')
			break;

		/* Find the end, stopping at whitespace, or a nul char */
		end = pos+1;
		while (*end && !strchr(" \t\n\r", *end))
			end++;

		/* Nul terminate the string */
                done = *end == '\0';
		*end = '\0';

		params[nparams++] = pos;

		/* Move on to the next word */
                if (done)
                    pos = NULL;
                else
                    pos = end + 1;
	}

	return nparams;
}

int parse_line(char *line, struct parser_function *functions, int nfunctions)
{
    char *args[20];
    int f;
    int nparams = decode_line(line, args, 20);

#if 0
    int i;
    printf("%d params\n", nparams);
    for (i = 0; i < nparams; i++)
        printf("%d: %s\n", i, args[i]);
#endif

    if (nparams > 0) {
        if (strcmp(args[0], "help") == 0) {
            printf("Commands:\n");
            for (f = 0; f < nfunctions; f++)
                printf("\t%s\n", functions[f].name);
        } else {
            for (f = 0; f < nfunctions; f++) {
                if (strcmp(functions[f].name, args[0]) == 0)
                    return functions[f].func(nparams, args);
            }
            if (f == nfunctions) {
                fprintf(stderr, "Invalid function: %s\n", args[0]);
                return -EINVAL;
            }
        }
    }

    return 0;
}

int parse_file(FILE *file, int cont_on_error, struct parser_function *functions,
        int nfunctions)
{
    char buffer[1024];
    int retval = 0;
    /* Read from stdin, decoding & executing the commands supplied */
    while (fgets(buffer, sizeof(buffer), file)) {
        int e = parse_line(buffer, functions, nfunctions);
        if (e < 0 && !cont_on_error)
            return e;
        retval = retval || e;
    }

    return retval;
}

int parse_filename(const char *file, int cont_on_error,
        struct parser_function *functions, int nfunctions)
{
    FILE *fp;
    int e;
    fp = fopen(file, "rb");
    if (!fp) {
        fprintf(stderr, "Failed to open %s: %s\n", file, strerror(errno));
        return -errno;
    }
    e = parse_file(fp, cont_on_error, functions, nfunctions);
    fclose(fp);
    return e;
}
