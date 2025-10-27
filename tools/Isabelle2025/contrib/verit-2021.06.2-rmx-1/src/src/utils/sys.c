#include "utils/sys.h"

#include "utils/general.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

FILE*
sys_open_file(const char* name, const char* mode)
{
	FILE* result;
	result = fopen(name, mode);
	if (result == 0) {
		my_error("cannot open file \"%s\".\n", name);
		exit(SYS_EXIT_ERROR_IO);
	}
	return result;
}

void
sys_close_file(FILE* file)
{
	if (fclose(file) == -1) {
		my_error("cannot close file.\n");
		exit(SYS_EXIT_ERROR_IO);
	}
}

