#include <stdio.h>
#include <string.h>

#include "util.h"


int
main(void)
{
	char buf[30];

	cstr_copy_n2(buf, sizeof(buf), "aaaaaaaa", 4);
	puts(buf);

	return 0;
}