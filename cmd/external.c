#include "external.h"


/*
 * Public
 */
int
cmd_external_exec(Update *u, const TgMessage *msg, const BotCmd *cmd, json_object *json)
{
	/* TODO */
	(void)u;
	(void)msg;
	(void)cmd;
	(void)json;
	return 0;
}


void
cmd_external_list(Update *u)
{
	str_append_fmt(&u->str, "[TODO]\n");
}
