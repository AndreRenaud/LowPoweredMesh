/**
 * \file	imx_usb_console/parser.h
 * \date	2013-Sep-12
 * \author	Andre Renaud
 * \copyright	Aiotec Ltd/Bluewater Systems
 * \brief       Rudimentary command line parser for running scripts
 */
#ifndef PARSER_H
#define PARSER_H

typedef int (*parser_function_ptr)(int argc, char *argv[]);
struct parser_function {
	char *name;
	parser_function_ptr func;
};
int parse_file(FILE *file, int cont_on_error, struct parser_function *functions,
        int nfunctions);
int parse_filename(const char *file, int cont_on_error,
        struct parser_function *functions, int nfunctions);
int parse_line(char *line, struct parser_function *functions, int nfunctions);


#endif
