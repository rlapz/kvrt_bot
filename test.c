#include <stdio.h>
#include <string.h>

#include "util.h"
#include "db.h"


int main(void)
{
	const char *const cmd = "/test eee            xxxx  xxx   xxx ee  ";
	CmdParser p;

	memset(&p, 0xa, sizeof(p));

	int ret = cmd_parser_parse(&p, '/', cmd);
	if (ret < 0)
		return 1;

	printf("cmd: |%.*s|\n", (int)p.name_len, p.name);
	printf("args len: %u\n", p.args_len);
	for (unsigned i = 0; i < p.args_len; i++)
		printf("arg: |%.*s|\n", (int)p.args[i].len, p.args[i].arg);
	return 0;
}