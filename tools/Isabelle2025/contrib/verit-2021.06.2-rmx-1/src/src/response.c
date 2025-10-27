#include "response.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
  --------------------------------------------------------------
  response helpers
  --------------------------------------------------------------
*/

FILE* veriT_out_file;
FILE* veriT_err_file;

void
veriT_out(char* format, ...)
{
	va_list params;
	va_start(params, format);
	vfprintf(veriT_out_file, format, params);
	va_end(params);
	fprintf(veriT_out_file, "\n");
}

void
veriT_out_no_newline(char* format, ...)
{
	va_list params;
	va_start(params, format);
	vfprintf(veriT_out_file, format, params);
	va_end(params);
}

void
veriT_err(char* format, ...)
{
	va_list params;
	fprintf(veriT_err_file, "error \"");
	va_start(params, format);
	vfprintf(veriT_err_file, format, params);
	va_end(params);
	fprintf(veriT_err_file, "\"\n");
}

void
veriT_error(char* format, ...)
{
	va_list params;
	fprintf(veriT_err_file, "(error \"");
	va_start(params, format);
	vfprintf(veriT_err_file, format, params);
	va_end(params);
	fprintf(veriT_err_file, "\")\n");
	exit(-1);
}

void
veriT_set_err_file(char* str)
{
	if (veriT_err_file != stderr && veriT_err_file != stdout)
		fclose(veriT_err_file);
	if (!strcmp(str, "stderr") || !strcmp(str, ""))
		veriT_err_file = stderr;
	else if (!strcmp(str, "stdout"))
		veriT_err_file = stdout;
	else
		veriT_err_file = fopen(str, "a");
}

void
veriT_set_out_file(char* str)
{
	if (veriT_out_file != stderr && veriT_out_file != stdout)
		fclose(veriT_out_file);
	if (!strcmp(str, "stderr"))
		veriT_out_file = stderr;
	else if (!strcmp(str, "stdout") || !strcmp(str, ""))
		veriT_out_file = stdout;
	else
		veriT_out_file = fopen(str, "a");
}

void
response_init(void)
{
	veriT_out_file = stdout;
	veriT_err_file = stderr;
}

void
response_done(void)
{
	veriT_set_out_file("stdout");
	veriT_set_err_file("stderr");
}
