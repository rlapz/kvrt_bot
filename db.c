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
db_admin_set(Db *d, int64_t chat_id, int64_t user_id, DbAdminRoleType roles)
{
	/* not tested yet! */
	const char *const sql = "insert into Admin(chat_id, user_id, roles) "
				"select ?, ?, ? "
				"where ("
					"select roles "
					"from Admin "
					"where (chat_id = ?) and (user_id = ?) "
					"order by id desc "
					"limit 1 "
				") != ?;";

	(void)sql;
	(void)d;
	(void)chat_id;
	(void)user_id;
	(void)roles;
	return 0;
}


int
db_admin_gban_user_set(Db *d, int64_t chat_id, int64_t user_id, int is_gban)
{
	/* not tested yet! */
	const char *const sql = "insert into Gban(chat_id, user_id, is_gban) "
				"select ?, ?, ? "
				"where ("
					"select is_gban "
					"from Gban "
					"where (chat_id = ?) and (user_id = ?) "
					"order by id desc "
					"limit 1 "
				") != ?;";

	(void)sql;
	(void)d;
	(void)chat_id;
	(void)user_id;
	(void)is_gban;
	return 0;
}


int
db_admin_gban_user_get(Db *d, int64_t chat_id, int64_t user_id, int *is_gban)
{
	/* not tested yet! */
	const char *const sql = "select is_gban "
				"from Gban "
				"where (chat_id = ?) and (user_id = ?) "
				"order by id desc "
				"limit 1;";

	(void)sql;
	(void)d;
	(void)chat_id;
	(void)user_id;
	(void)is_gban;
	return 0;
}


int
db_cmd_set(Db *d, int64_t chat_id, const char name[], int is_enable)
{
	/* not tested yet! */
	const char *const sql = "insert into Cmd_Chat(cmd_id, chat_id, is_enable) "
				"select ?, ?, ? "
				"where ("
					"select b.is_enable "
					"from Cmd as a "
					"join Cmd_Chat as b on (a.id = b.cmd_id) "
					"where (a.name = ?) "
					"order by a.id, b.id desc "
					"limit 1 "
				") != ?;";

	(void)sql;
	(void)d;
	(void)chat_id;
	(void)name;
	(void)is_enable;
	return 0;
}


int
db_cmd_get(Db *d, DbCmd *cmd, int64_t chat_id, const char name[])
{
	/* not tested yet! */
	const char *const sql = "select a.name, a.file, a.args, b.is_enable "
				"from Cmd as a "
				"join Cmd_Chat as b on (a.id = b.chat_id) "
				"where (a.name = ?) and (b.chat_id = ?) "
				"order by b.id desc "
				"limit 1;";

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
	const char *const admin = "create table if not exists Admin("
				  "id         integer primary key autoincrement,"
				  "chat_id    bigint not null,"			/* telegram chat id */
				  "user_id    bigint not null,"			/* telegram user id */
				  "roles      integer not null,"		/* O-Ring DbAdminRoleType */
				  "created_at datetime default (datetime('now', 'localtime')) not null);";

	const char *const gban = "create table if not exists Gban("
				  "id         integer primary key autoincrement,"
				  "user_id    bigint not null,"			/* telegram user id */
				  "chat_id    bigint not null,"			/* telegram chat id */
				  "is_gban    boolean not null,"
				  "created_at datetime default (datetime('now', 'localtime')) not null);";

	const char *const cmd = "create table if not exists Cmd("
				"id         integer primary key autoincrement,"
				"name       varchar(127) not null,"
				"file       varchar(1023) not null,"
				"args       integer not null,"			/* O-Ring DbCmdArgType */
				"created_at datetime default (datetime('now', 'localtime')) not null);";

	const char *const cmd_chat = "create table if not exists Cmd_Chat("
				     "id         integer primary key autoincrement,"
				     "cmd_id     integer not null,"
				     "chat_id    bigint not null,"		/* telegram chat id */
				     "is_enable  boolean not null,"
				     "created_at datetime default (datetime('now', 'localtime')) not null);";

	char *err_msg;
	if (sqlite3_exec(s, admin, NULL, NULL, &err_msg) != 0) {
		log_err(0, "db: _create_tables: admin: %s", err_msg);
		goto err0;
	}

	if (sqlite3_exec(s, gban, NULL, NULL, &err_msg) != 0) {
		log_err(0, "db: _create_tables: user_gban: %s", err_msg);
		goto err0;
	}

	if (sqlite3_exec(s, cmd, NULL, NULL, &err_msg) != 0) {
		log_err(0, "db: _create_tables: cmd: %s", err_msg);
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
