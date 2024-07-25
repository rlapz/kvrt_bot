#include <stdio.h>
#include <string.h>

#include "util.h"
#include "db.h"


int main(void)
{
	log_init(4096);

	Db db;
	Str str;
	char buffer[4096];

	str_init(&str, buffer, sizeof(buffer));
	db_init(&db, "db.sql");

	int is_gban = 10;
	if (db_admin_gban_user_get(&db, 0, 0, &is_gban) == 0)
		printf("ret: %d\n", is_gban);

	db_deinit(&db);
	log_deinit();
	return 0;
}