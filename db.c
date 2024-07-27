#include "db.h"
#include "util.h"


static int _create_tables(sqlite3 *s);


/*
 * Public
 */
int
db_init(Db *d, const char path[])
{
	sqlite3 *sql;
	const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;

	if (sqlite3_threadsafe() == 0)
		log_info("db: db_init: sqlite3_threadsafe: hmm, the sqlite3 library is not thread safe :(");

	if (sqlite3_open_v2(path, &sql, flags, NULL) != 0) {
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
	return -1;
}


void
db_deinit(Db *d)
{
	sqlite3_close(d->sql);
}


int
db_admin_set(Db *d, int64_t chat_id, int64_t user_id, DbAdminRoleType roles)
{
	/* not tested yet! */
	/* params:
	 * 1. chat_id
	 * 2. user_id
	 * 3. roles
	 * 4. chat_id
	 * 5. user_id
	 * 5. roles
	 */
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
db_admin_gban_user_set(Db *d, int64_t chat_id, int64_t user_id, int is_gban, const char reason[])
{
	/* not tested yet! */
	/* params:
	 * 1. chat_id
	 * 2. user_id
	 * 3. is_gban
	 * 4. reason
	 * 5. chat_id
	 * 6. user_id
	 * 7. is_gban
	 */
	const char *const sql = "insert into Gban(chat_id, user_id, is_gban, reason) "
				"select ?, ?, ?, ? "
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
	(void)reason;
	return 0;
}


int
db_admin_gban_user_get(Db *d, DbAdminGbanUser *gban, int64_t chat_id, int64_t user_id)
{
	/* params:
	 * 1. chat_id
	 * 2. user_id
	 */
	const char *const sql = "select is_gban "
				"from Gban "
				"where (chat_id = ?) and (user_id = ?) "
				"order by id desc "
				"limit 1;";

	sqlite3_stmt *stmt;
	int ret = sqlite3_prepare_v2(d->sql, sql, -1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		log_err(0, "db: db_admin_gban_user_get: sqlite3_prepare_v2: %s", sqlite3_errstr(ret));
		return -1;
	}

	ret = sqlite3_bind_int64(stmt, 1, chat_id);
	if (ret != SQLITE_OK) {
		log_err(0, "db: db_admin_gban_user_get: sqlite3_bind_int64: chat_id: %s", sqlite3_errstr(ret));
		ret = -1;
		goto out0;
	}

	ret = sqlite3_bind_int64(stmt, 2, user_id);
	if (ret != SQLITE_OK) {
		log_err(0, "db: db_admin_gban_user_get: sqlite3_bind_int64: user_id: %s", sqlite3_errstr(ret));
		ret = -1;
		goto out0;
	}

	if (sqlite3_step(stmt) != SQLITE_ROW) {
		ret = -1;
		goto out0;
	}

	gban->chat_id = chat_id;
	gban->user_id = user_id;
	gban->is_gban = sqlite3_column_int(stmt, 0);

	const char *const _reason = (char *)sqlite3_column_text(stmt, 1);
	if (_reason != NULL)
		cstr_copy_n(gban->reason, sizeof(gban->reason), _reason);
	else
		gban->reason[0] = '\0';

	ret = 0;

out0:
	sqlite3_finalize(stmt);
	return ret;
}


int
db_cmd_set(Db *d, int64_t chat_id, const char name[], DbCmdArgType args, int is_enable)
{
	/* not tested yet! */
	/* params:
	 * 1. cmd_id
	 * 2. chat_id
	 * 3. args
	 * 4. is_enable
	 * 5. cmd_name
	 * 6. is_enable
	 */
	const char *const sql = "insert into Cmd_Chat(cmd_id, chat_id, args, is_enable) "
				"select ?, ?, ?, ? "
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
	(void)args;
	(void)is_enable;
	return 0;
}


int
db_cmd_get(Db *d, DbCmd *cmd, int64_t chat_id, const char name[])
{
	/* not tested yet! */
	/* params:
	 * 1. cmd_name
	 * 2. chat_id
	 */
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


int
db_cmd_global_get_message(Db *d, char buffer[], size_t size, const char name[])
{
	/* params:
	 * 1. cmd_name
	 */
	const char *const sql = "select message from Cmd_Global where (name = ?) order by id desc limit 1;";


	sqlite3_stmt *stmt;
	int ret = sqlite3_prepare_v2(d->sql, sql, -1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		log_err(0, "db: db_cmd_global_get_message: sqlite3_prepare_v2: %s", sqlite3_errstr(ret));
		return -1;
	}

	ret = sqlite3_bind_text(stmt, 1, name, -1, NULL);
	if (ret != SQLITE_OK) {
		log_err(0, "db: db_cmd_global_get_message: sqlite3_bind_text: cmd_name: %s", sqlite3_errstr(ret));
		ret = -1;
		goto out0;
	}

	ret = sqlite3_step(stmt);
	if (ret != SQLITE_ROW) {
		ret = -1;
		goto out0;
	}

	const char *const res = (char *)sqlite3_column_text(stmt, 0);
	if (res == NULL) {
		ret = -1;
		goto out0;
	}

	cstr_copy_n(buffer, size, res);
	ret = 0;

out0:
	sqlite3_finalize(stmt);
	return ret;
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
				  "reason     varchar(2047) null,"
				  "created_at datetime default (datetime('now', 'localtime')) not null);";

	const char *const cmd = "create table if not exists Cmd("
				"id         integer primary key autoincrement,"
				"name       varchar(33) not null,"
				"file       varchar(1023) not null,"
				"args       integer not null,"			/* O-Ring DbCmdArgType */
				"created_at datetime default (datetime('now', 'localtime')) not null);";

	const char *const cmd_chat = "create table if not exists Cmd_Chat("
				     "id         integer primary key autoincrement,"
				     "cmd_id     integer not null,"
				     "chat_id    bigint not null,"		/* telegram chat id */
				     "is_enable  boolean not null,"
				     "created_at datetime default (datetime('now', 'localtime')) not null);";

	const char *const cmd_global = "create table if not exists Cmd_Global("
				       "id         integer primary key autoincrement,"
				       "name       varchar(33) not null,"
				       "message    varchar(8192) not null,"
				       "created_at datetime default "
				       		"(datetime('now', 'localtime')) not null);";


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

	if (sqlite3_exec(s, cmd_global, NULL, NULL, &err_msg) != 0) {
		log_err(0, "db: _create_tables: cmd_global: %s", err_msg);
		goto err0;
	}

	return 0;

err0:
	sqlite3_free(err_msg);
	return -1;
}
