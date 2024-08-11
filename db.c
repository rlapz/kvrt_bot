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


DbRet
db_admin_set(Db *d, int64_t chat_id, int64_t user_id, int is_creator, TgChatAdminPrivilege privileges)
{
	/* not tested yet! */
	/* params:
	 * 1. chat_id
	 * 2. user_id
	 * 3. is_creator
	 * 4. privileges
	 */
	const char *const sql = "insert into Admin(chat_id, user_id, is_creator, privileges) "
				"values (?, ?, ?, ?)";

	DbRet db_ret = DB_RET_ERROR;
	sqlite3_stmt *stmt;
	int ret = sqlite3_prepare_v2(d->sql, sql, -1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		log_err(0, "db: db_admin_set: sqlite3_prepare_v2: %s", sqlite3_errstr(ret));
		return db_ret;
	}

	ret = sqlite3_bind_int64(stmt, 1, chat_id);
	ret = (ret == SQLITE_OK) ? sqlite3_bind_int64(stmt, 2, user_id) : ret;
	ret = (ret == SQLITE_OK) ? sqlite3_bind_int64(stmt, 3, is_creator) : ret;
	ret = (ret == SQLITE_OK) ? sqlite3_bind_int64(stmt, 4, privileges) : ret;
	if (ret != SQLITE_OK) {
		log_err(0, "db: db_admin_set: bind: failed to bind: %s", sqlite3_errstr(ret));
		goto out0;
	}

	ret = sqlite3_step(stmt);
	switch (ret) {
	case SQLITE_DONE:
		db_ret = DB_RET_OK;
		break;
	default:
		log_err(0, "db: db_admin_set: sqlite3_step: %s", sqlite3_errstr(ret));
		break;
	}

out0:
	sqlite3_finalize(stmt);
	return db_ret;
}


DbRet
db_admin_get(Db *d, DbAdmin *admin, int64_t chat_id, int64_t user_id)
{
	/* not tested yet! */
	/* params:
	 * 1. chat_id
	 * 2. user_id
	 */
	const char *const sql = "select is_creator, privileges "
				"from Admin "
				"where (chat_id = ?) and (user_id = ?)"
				"order by id desc limit 1;";

	DbRet db_ret = DB_RET_ERROR;
	sqlite3_stmt *stmt;
	int ret = sqlite3_prepare_v2(d->sql, sql, -1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		log_err(0, "db: db_admin_get: sqlite3_prepare_v2: %s", sqlite3_errstr(ret));
		return db_ret;
	}

	ret = sqlite3_bind_int64(stmt, 1, chat_id);
	ret = (ret == SQLITE_OK) ? sqlite3_bind_int64(stmt, 2, user_id) : ret;
	if (ret != SQLITE_OK) {
		log_err(0, "db: db_admin_get: bind: failed to bind: %s", sqlite3_errstr(ret));
		goto out0;
	}

	ret = sqlite3_step(stmt);
	switch (ret) {
	case SQLITE_ROW:
		admin->chat_id = chat_id;
		admin->user_id = user_id;
		admin->is_creator = sqlite3_column_int(stmt, 0);
		admin->privileges = sqlite3_column_int(stmt, 1);

		db_ret = DB_RET_OK;
		break;
	case SQLITE_DONE:
		admin->is_creator = 0;
		admin->privileges = 0;

		db_ret = DB_RET_NOT_EXISTS;
		break;
	default:
		log_err(0, "db: db_admin_get: sqlite3_step: %s", sqlite3_errstr(ret));
		break;
	}

out0:
	sqlite3_finalize(stmt);
	return db_ret;
}


DbRet
db_admin_clear(Db *d, int64_t chat_id)
{
	/* not tested yet! */
	/* params:
	 * 1. chat_id
	 */
	const char *const sql = "delete from Admin where (chat_id = ?);";

	DbRet db_ret = DB_RET_ERROR;
	sqlite3_stmt *stmt;
	int ret = sqlite3_prepare_v2(d->sql, sql, -1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		log_err(0, "db: db_admin_clear: sqlite3_prepare_v2: %s", sqlite3_errstr(ret));
		return db_ret;
	}

	ret = sqlite3_bind_int64(stmt, 1, chat_id);
	if (ret != SQLITE_OK) {
		log_err(0, "db: db_admin_clear: bind: failed to bind: %s", sqlite3_errstr(ret));
		goto out0;
	}

	ret = sqlite3_step(stmt);
	switch (ret) {
	case SQLITE_DONE:
		db_ret = DB_RET_OK;
		break;
	default:
		log_err(0, "db: db_admin_clear: sqlite3_step: %s", sqlite3_errstr(ret));
		break;
	}

out0:
	sqlite3_finalize(stmt);
	return db_ret;
}


DbRet
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

	DbRet db_ret = DB_RET_ERROR;
	sqlite3_stmt *stmt;
	int ret = sqlite3_prepare_v2(d->sql, sql, -1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		log_err(0, "db: db_admin_gban_user_set: sqlite3_prepare_v2: %s", sqlite3_errstr(ret));
		return db_ret;
	}

	ret = sqlite3_bind_int64(stmt, 1, chat_id);
	ret = (ret == SQLITE_OK) ? sqlite3_bind_int64(stmt, 2, user_id) : ret;
	ret = (ret == SQLITE_OK) ? sqlite3_bind_int(stmt, 3, is_gban) : ret;
	ret = (ret == SQLITE_OK) ? sqlite3_bind_text(stmt, 4, reason, -1, NULL) : ret;
	ret = (ret == SQLITE_OK) ? sqlite3_bind_int64(stmt, 5, chat_id) : ret;
	ret = (ret == SQLITE_OK) ? sqlite3_bind_int64(stmt, 6, user_id) : ret;
	ret = (ret == SQLITE_OK) ? sqlite3_bind_int(stmt, 7, is_gban) : ret;
	if (ret != SQLITE_OK) {
		log_err(0, "db: db_admin_gban_user_set: bind: failed to bind: %s", sqlite3_errstr(ret));
		goto out0;
	}

	ret = sqlite3_step(stmt);
	switch (ret) {
	case SQLITE_DONE:
		db_ret = DB_RET_OK;
		break;
	default:
		log_err(0, "db: db_admin_set: sqlite3_step: %s", sqlite3_errstr(ret));
		break;
	}

out0:
	sqlite3_finalize(stmt);
	return db_ret;
}


DbRet
db_admin_gban_user_get(Db *d, DbAdminGbanUser *gban, int64_t chat_id, int64_t user_id)
{
	/* params:
	 * 1. chat_id
	 * 2. user_id
	 */
	const char *const sql = "select is_gban, reason "
				"from Gban "
				"where (chat_id = ?) and (user_id = ?) "
				"order by id desc "
				"limit 1;";

	DbRet db_ret = DB_RET_ERROR;
	sqlite3_stmt *stmt;
	int ret = sqlite3_prepare_v2(d->sql, sql, -1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		log_err(0, "db: db_admin_gban_user_get: sqlite3_prepare_v2: %s", sqlite3_errstr(ret));
		return db_ret;
	}

	ret = sqlite3_bind_int64(stmt, 1, chat_id);
	ret = (ret == SQLITE_OK) ? sqlite3_bind_int64(stmt, 2, user_id) : ret;
	if (ret != SQLITE_OK) {
		log_err(0, "db: db_admin_gban_user_get: bind: failed to bind: %s", sqlite3_errstr(ret));
		goto out0;
	}

	ret = sqlite3_step(stmt);
	switch (ret) {
	case SQLITE_ROW:
		gban->chat_id = chat_id;
		gban->user_id = user_id;
		gban->is_gban = sqlite3_column_int(stmt, 0);
		cstr_copy_n(gban->reason, LEN(gban->reason), (const char *)sqlite3_column_text(stmt, 1));

		db_ret = DB_RET_OK;
		break;
	case SQLITE_DONE:
		db_ret = DB_RET_NOT_EXISTS;
		break;
	default:
		log_err(0, "db: db_admin_gban_user_get: sqlite3_step: %s", sqlite3_errstr(ret));
		break;
	}

out0:
	sqlite3_finalize(stmt);
	return db_ret;
}


DbRet
db_cmd_set_enable(Db *d, int64_t chat_id, const char name[], int is_enable)
{
	/* TODO */
	(void)d;
	(void)chat_id;
	(void)name;
	(void)is_enable;
	return DB_RET_OK;
}


DbRet
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

	DbRet db_ret = DB_RET_ERROR;
	sqlite3_stmt *stmt;
	int ret = sqlite3_prepare_v2(d->sql, sql, -1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		log_err(0, "db: db_cmd_get: sqlite3_prepare_v2: %s", sqlite3_errstr(ret));
		return db_ret;
	}

	ret = sqlite3_bind_text(stmt, 1, name, -1, NULL);
	ret = (ret == SQLITE_OK) ? sqlite3_bind_int64(stmt, 2, chat_id) : ret;
	if (ret != SQLITE_OK) {
		log_err(0, "db: db_cmd_get: bind: failed to bind: %s", sqlite3_errstr(ret));
		goto out0;
	}

	ret = sqlite3_step(stmt);
	switch (ret) {
	case SQLITE_ROW:
		cmd->chat_id = chat_id;
		cstr_copy_n(cmd->name, LEN(cmd->name), (const char *)sqlite3_column_text(stmt, 0));
		cstr_copy_n(cmd->file, LEN(cmd->file), (const char *)sqlite3_column_text(stmt, 1));

		cmd->args = sqlite3_column_int(stmt, 2);
		cmd->is_enable = sqlite3_column_int(stmt, 3);

		db_ret = DB_RET_OK;
		break;
	case SQLITE_DONE:
		db_ret = DB_RET_NOT_EXISTS;
		break;
	default:
		log_err(0, "db: db_cmd_get: sqlite3_step: %s", sqlite3_errstr(ret));
		break;
	}

out0:
	sqlite3_finalize(stmt);
	return db_ret;
}


DbRet
db_cmd_message_get(Db *d, char buffer[], size_t size, const char name[])
{
	/* params:
	 * 1. cmd_name
	 */
	const char *const sql = "select message from Cmd_Message where (name = ?) order by id desc limit 1;";

	DbRet db_ret = DB_RET_ERROR;
	sqlite3_stmt *stmt;
	int ret = sqlite3_prepare_v2(d->sql, sql, -1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		log_err(0, "db: db_cmd_message_get: sqlite3_prepare_v2: %s", sqlite3_errstr(ret));
		return db_ret;
	}

	ret = sqlite3_bind_text(stmt, 1, name, -1, NULL);
	if (ret != SQLITE_OK) {
		log_err(0, "db: db_cmd_message_get: bind: failed to bind: %s", sqlite3_errstr(ret));
		goto out0;
	}

	ret = sqlite3_step(stmt);
	switch (ret) {
	case SQLITE_ROW:
		cstr_copy_n(buffer, size, (const char *)sqlite3_column_text(stmt, 0));
		db_ret = DB_RET_OK;
		break;
	case SQLITE_DONE:
		db_ret = DB_RET_NOT_EXISTS;
		goto out0;
	default:
		log_err(0, "db: db_cmd_message_get: sqlite3_step: %s", sqlite3_errstr(ret));
		break;
	}

out0:
	sqlite3_finalize(stmt);
	return db_ret;
}


/*
 * Private
 */
static int
_create_tables(sqlite3 *s)
{
	const char *const admin = "create table if not exists Admin("
				  "id         integer primary key autoincrement,"
				  "is_creator boolean not null,"
				  "chat_id    bigint not null,"			/* telegram chat id */
				  "user_id    bigint not null,"			/* telegram user id */
				  "privileges integer not null,"		/* O-Ring TgChatAdminPrivilege */
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

	const char *const cmd_message = "create table if not exists Cmd_Message("
					"id         integer primary key autoincrement,"
					"name       varchar(33) not null,"
					"message    varchar(8192) not null,"
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

	if (sqlite3_exec(s, cmd_message, NULL, NULL, &err_msg) != 0) {
		log_err(0, "db: _create_tables: cmd_message: %s", err_msg);
		goto err0;
	}

	return 0;

err0:
	sqlite3_free(err_msg);
	return -1;
}
