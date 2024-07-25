#include <stdio.h>
#include <string.h>

#include "util.h"
#include "db.h"


int main(void)
{
	log_init(4096);

	Db db;
	char buff[DB_BOT_CMD_BUILTIN_OPT_SIZE];

	db_init(&db, "db.sql");

	const char *res = db_cmd_builtin_get_opt(&db, buff, "/test");
	if (res != NULL)
		printf("|%s|\n", res);

	db_deinit(&db);
	log_deinit();
	return 0;
}