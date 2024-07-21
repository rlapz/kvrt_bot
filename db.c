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
db_owner_set(Db *d, int64_t bot_id, int64_t owner_id, const char name[])
{
	(void)d;
	(void)bot_id;
	(void)owner_id;
	(void)name;
	return 0;
}


int
db_admin_set(Db *d, int64_t chat_id, int64_t user_id, DbAdminRoleType roles)
{
	(void)d;
	(void)chat_id;
	(void)user_id;
	(void)roles;
	return 0;
}


int
db_admin_ban_user(Db *d, int is_ban, int64_t chat_id, int64_t user_id)
{
	(void)d;
	(void)is_ban;
	(void)chat_id;
	(void)user_id;
	return 0;
}


int
db_cmd_set_enable(Db *d, int64_t chat_id, const char name[], int is_enable)
{
	const char *const sql = "update a"
					"set a.is_enable = ?"
				"from Cmd_Chat as a"
				"join Cmd as b"
					"on (a.cmd_id = b.id)"
				"where (b.name = ?) and (a.chat_id = ?);";

	(void)sql;
	(void)d;
	(void)chat_id;
	(void)name;
	(void)is_enable;
	return 0;
}


int
db_cmd_get_by_name(Db *d, DbCmd *cmd, int64_t chat_id, const char name[])
{
	const char *const sql= "select a.name, a.file, a.args"
				"from Cmd as a"
				"join Cmd_Chat as b"
					"on (a.id = b.chat_id)"
				"where (a.name = ?) and (b.is_enable = true) and (b.chat_id = ?);";

	(void)sql;
	(void)d;
	(void)cmd;
	(void)chat_id;
	(void)name;
	return 0;
}


/*
 * Private
 */
static int
_create_tables(sqlite3 *s)
{
	const char *const bot = "create table if not exists Bot("
				"id       bigint unique not null,"		/* telegram bot id */
				"owner_id bigint unique not null,"		/* telegram user id */
				"name     varchar(32) unique not null);";

	const char *const admin = "create table if not exists Admin("
				  "id      integer primary key autoincrement,"
				  "chat_id bigint not null,"			/* telegram chat id */
				  "user_id bigint not null,"			/* telegram user id */
				  "roles   integer not null);";			/* O-Ring DbAdminRoleType */

	const char *const cmd = "create table if not exists Cmd("
				"id   integer primary key autoincrement,"
				"name varchar(127) unique not null,"
				"file varchar(1023) not null,"
				"args integer not null);";			/* O-Ring DbCmdArgType */

	const char *const chat_user = "create table if not exists Chat_User("
				      "int     integer primary key autoincrement,"
				      "chat_id bigint not null,"		/* telegram chat id */
				      "user_id bigint not null,"		/* telegram user id */
				      "is_ban  boolean not null);";

	const char *const cmd_chat = "create table if not exists Cmd_Chat("
				     "id        integer primary key autoincrement,"
				     "cmd_id    integer not null,"
				     "chat_id   bigint not null,"		/* telegram chat id */
				     "is_enable boolean not null);";

	char *err_msg;
	if (sqlite3_exec(s, bot, NULL, NULL, &err_msg) != 0) {
		log_err(0, "db: _create_tables: bot: %s", err_msg);
		goto err0;
	}

	if (sqlite3_exec(s, admin, NULL, NULL, &err_msg) != 0) {
		log_err(0, "db: _create_tables: admin: %s", err_msg);
		goto err0;
	}

	if (sqlite3_exec(s, cmd, NULL, NULL, &err_msg) != 0) {
		log_err(0, "db: _create_tables: cmd: %s", err_msg);
		goto err0;
	}

	if (sqlite3_exec(s, chat_user, NULL, NULL, &err_msg) != 0) {
		log_err(0, "db: _create_tables: chat_user: %s", err_msg);
		goto err0;
	}

	if (sqlite3_exec(s, cmd_chat, NULL, NULL, &err_msg) != 0) {
		log_err(0, "db: _create_tables: cmd_chat: %s", err_msg);
		goto err0;
	}

	return 0;

err0:
	sqlite3_free(err_msg);
	return -1;
}
