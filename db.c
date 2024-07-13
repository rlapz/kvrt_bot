#include "db.h"
#include "util.h"


static int _create_tables(sqlite3 *s);


/*
 * Public
 */
int
db_init(Db *d, const char path[])
{
	if (pthread_mutex_init(&d->mutex, NULL) != 0) {
		log_err(0, "db: db_init: pthread_mutex_init: failed");
		return -1;
	}

	sqlite3 *sql;
	if (sqlite3_open(path, &sql) != 0) {
		log_err(0, "db: db_init: sqlite3_open: %s: %s", path, sqlite3_errmsg(sql));
		goto err0;
	}

	if (_create_tables(sql) < 0)
		goto err0;

	d->sql = sql;
	d->path = path;
	return 0;

err0:
	sqlite3_close(sql);
	pthread_mutex_destroy(&d->mutex);
	return -1;
}


void
db_deinit(Db *d)
{
	sqlite3_close(d->sql);
	pthread_mutex_destroy(&d->mutex);
}


int
db_chat_get_by_id(Db *d, DbChat *chat, int64_t id)
{
	(void)d;
	(void)chat;
	(void)id;
	return 0;
}


int
db_chat_get_by_name(Db *d, DbChat *chat, const char name[])
{
	(void)d;
	(void)chat;
	(void)name;
	return 0;
}


int
db_chat_add_blocked_user(Db *d, int64_t id, int64_t user_id)
{
	(void)d;
	(void)id;
	(void)user_id;
	return 0;
}


int
db_chat_del_blocked_user(Db *d, int64_t id, int64_t user_id)
{
	(void)d;
	(void)id;
	(void)user_id;
	return 0;
}


int
db_chat_add_allowed_command(Db *d, int64_t id, int64_t cmd_id)
{
	(void)d;
	(void)id;
	(void)cmd_id;
	return 0;
}


int
db_chat_del_allowed_command(Db *d, int64_t id, int64_t cmd_id)
{
	(void)d;
	(void)id;
	(void)cmd_id;
	return 0;
}


int
db_command_add(Db *d, const char name[], const char path[], const char *args[], int32_t args_len)
{
	(void)d;
	(void)name;
	(void)path;
	(void)args;
	(void)args_len;
	return 0;
}


int
db_command_del(Db *d, int64_t id)
{
	(void)d;
	(void)id;
	return 0;
}


int
db_command_disable(Db *d, int64_t id)
{
	(void)d;
	(void)id;
	return 0;
}


int
db_command_get_by_id(Db *d, DbCommand *cmd, int64_t id)
{
	(void)d;
	(void)cmd;
	(void)id;
	return 0;
}


int
db_command_get_by_name(Db *d, DbCommand *cmd, const char name[])
{
	(void)d;
	(void)cmd;
	(void)name;
	return 0;
}


/*
 * Private
 */
static int
_create_tables(sqlite3 *s)
{
	const char *const chat_table = "CREATE TABLE IF NOT EXISTS Chat("
				       "id BIGINT PRIMARY KEY NOT NULL,"
				       "type VARCHAR(16) NOT NULL,"
				       "name VARCHAR(255) NULL,"
				       "admin_list JSON NULL,"
				       "allowed_commands JSON NULL,"
				       "blocked_users JSON NULL);";

	const char *const command_table = "CREATE TABLE IF NOT EXISTS Command("
					  "id INTEGER PRIMARY KEY AUTOINCREMENT,"
					  "name VARCHAR(255) NOT NULL,"
					  "executable_file VARCHAR(4096) NOT NULL,"
					  "args JSON NULL);";

	const char *const command_name_unique = "CREATE UNIQUE INDEX IF NOT EXISTS "
						"CommandNameUnique ON Command(name);";

	const char *const command_name_index = "CREATE INDEX IF NOT EXISTS "
					       "CommandNameIndex ON Command(name);";

	const char *const chat_name_index = "CREATE INDEX IF NOT EXISTS ChatNameIndex ON Chat(name);";


	char *err_msg;
	if (sqlite3_exec(s, chat_table, NULL, NULL, &err_msg) != 0) {
		log_err(0, "db: _create_tables: chat_table: %s", err_msg);
		goto err0;
	}

	if (sqlite3_exec(s, command_table, NULL, NULL, &err_msg) != 0) {
		log_err(0, "db: _create_tables: command_table: %s", err_msg);
		goto err0;
	}

	if (sqlite3_exec(s, command_name_unique, NULL, NULL, &err_msg) != 0) {
		log_err(0, "db: _create_tables: command_name_unique: %s", err_msg);
		goto err0;
	}

	if (sqlite3_exec(s, command_name_index, NULL, NULL, &err_msg) != 0) {
		log_err(0, "db: _create_tables: command_name_index: %s", err_msg);
		goto err0;
	}

	if (sqlite3_exec(s, chat_name_index, NULL, NULL, &err_msg) != 0) {
		log_err(0, "db: _create_tables: chat_name_index: %s", err_msg);
		goto err0;
	}

	return 0;

err0:
	sqlite3_free(err_msg);
	return -1;
}
