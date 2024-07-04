#include "db.h"


int _create_tables(Db *d);


/*
 * Public
 */
int
db_init(Db *d, const char path[])
{
	return 0;
}


void
db_deinit(Db *d)
{
}


int
db_chat_get_by_id(Db *d, DbChat *chat, int64_t id)
{
	return 0;
}


int
db_chat_get_by_name(Db *d, DbChat *chat, const char name[])
{
	return 0;
}


int
db_chat_add_blocked_user(Db *d, int64_t id, int64_t user_id)
{
	return 0;
}


int
db_chat_del_blocked_user(Db *d, int64_t id, int64_t user_id)
{
	return 0;
}


int
db_chat_add_allowed_command(Db *d, int64_t id, int64_t cmd_id)
{
	return 0;
}


int
db_chat_del_allowed_command(Db *d, int64_t id, int64_t cmd_id)
{
	return 0;
}


int
db_command_add(Db *d, const char name[], const char path[], const char *args[], int32_t args_len)
{
	return 0;
}


int
db_command_del(Db *d, int64_t id)
{
	return 0;
}


int
db_command_disable(Db *d, int64_t id)
{
	return 0;
}


int
db_command_get_by_id(Db *d, DbCommand *cmd, int64_t id)
{
	return 0;
}


int
db_command_get_by_name(Db *d, DbCommand *cmd, const char name[])
{
	return 0;
}


/*
 * Private
 */
int
_create_tables(Db *d)
{
	return 0;
}
