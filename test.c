#include <stdio.h>
#include <string.h>

#include "util.h"
#include "db.h"


int main(void)
{
	const char *const cmd = "/test@eee @eagain";
	BotCmd p;

	memset(&p, 0xa, sizeof(p));

	int ret = bot_cmd_parse(&p, '/', cmd);
	if (ret < 0)
		return 1;

	printf("cmd: |%.*s|\n", (int)p.name_len, p.name);
	printf("args len: %u\n", p.args_len);
	for (unsigned i = 0; i < p.args_len; i++)
		printf("arg: |%.*s|\n", (int)p.args[i].len, p.args[i].name);
	return 0;
}