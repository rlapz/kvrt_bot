#include <errno.h>
#include <string.h>

#include <db.h>
#include <model.h>
#include <util.h>


static int _create_tables(sqlite3 *s);
static int _exec_set_args(sqlite3_stmt *s, const DbArg args[], int args_len);
static int _exec_get_output(sqlite3_stmt *s, DbOut out[], int out_len);
static int _exec_get_output_values(sqlite3_stmt *s, DbOutItem items[], int len);


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
db_transaction_begin(Db *d)
{
	char *err_msg;
	if (sqlite3_exec(d->sql, "begin transaction;", NULL, NULL, &err_msg) != SQLITE_OK) {
		log_err(0, "db: db_transaction_begin: sqlite3_exec: %s", err_msg);
		return -1;
	}

	return 0;
}


int
db_transaction_end(Db *d, int is_ok)
{
	char *err_msg;
	const char *const sql = (is_ok)? "commit transaction;" : "rollback transaction;";
	if (sqlite3_exec(d->sql, sql, NULL, NULL, &err_msg) != SQLITE_OK) {
		log_err(0, "db: db_transaction_end(%d): sqlite3_exec: %s", is_ok, err_msg);
		return -1;
	}

	return 0;
}


int
db_exec(Db *d, const char query[], const DbArg args[], int args_len, DbOut out[], int out_len)
{
	sqlite3_stmt *stmt;
	int ret = sqlite3_prepare_v2(d->sql, query, -1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		log_err(0, "db: _exec: sqlite3_prepare_v2: %s", sqlite3_errstr(ret));
		return -1;
	}

	ret = _exec_set_args(stmt, args, args_len);
	if (ret < 0)
		goto out0;

	if (out != NULL) {
		ret = _exec_get_output(stmt, out, out_len);
		goto out0;
	}

	ret = sqlite3_step(stmt);
	switch (ret) {
	case SQLITE_DONE:
		ret = 0;
		break;
	case SQLITE_ROW:
		ret = 1;
		break;
	default:
		log_err(0, "db: _exec: sqlite3_step: %s", sqlite3_errstr(ret));
		ret = -1;
		break;
	}

out0:
	sqlite3_finalize(stmt);
	return ret;
}


int
db_changes(Db *s)
{
	return sqlite3_changes(s->sql);
}


/*
 * Private
 */
static int
_create_tables(sqlite3 *s)
{
	const char *const admin =
		"create table if not exists Admin("
		"id         integer primary key autoincrement,"
		"chat_id    bigint not null,"			/* telegram chat id */
		"user_id    bigint not null,"			/* telegram user id */
		"privileges integer not null,"			/* Bitwise OR TgChatAdminPrivilege */
		"created_at datetime default (datetime('now', 'localtime')) not null);";

	const char *const module_extern =
		"create table if not exists Module_Extern("
		"id          integer primary key autoincrement,"
		"flags       integer not null,"
		"name        varchar(33) not null,"
		"file_name   varchar(1023) not null,"
		"description varchar(255) not null,"
		"args        integer not null,"
		"created_at  datetime default(datetime('now', 'localtime')) not null);";

	const char *const module_extern_disabled =
		"create table if not exists Module_Extern_Disabled("
		"id          integer primary key autoincrement,"
		"module_name varchar(33) not null,"
		"chat_id     bigint not null,"			/* telegram chat id */
		"created_at  datetime default(datetime('now','localtime')) not null);";

	const char *const cmd_message =
		"create table if not exists Cmd_Message("
		"id         integer primary key autoincrement,"
		"chat_id    bigint not null,"			/* telegram chat id */
		"name       varchar(33) not null,"
		"message    varchar(8192) not null,"
		"created_at datetime default (datetime('now', 'localtime')) not null,"
		"created_by bigint not null);";			/* telegram user id */


	char *err_msg;
	if (sqlite3_exec(s, admin, NULL, NULL, &err_msg) != 0) {
		log_err(0, "db: _create_tables: Admin: %s", err_msg);
		goto err0;
	}

	if (sqlite3_exec(s, module_extern, NULL, NULL, &err_msg) != 0) {
		log_err(0, "db: _create_tables: Module_Extern: %s", err_msg);
		goto err0;
	}

	if (sqlite3_exec(s, module_extern_disabled, NULL, NULL, &err_msg) != 0) {
		log_err(0, "db: _create_tables: Module_Extern_Disabled: %s", err_msg);
		goto err0;
	}

	if (sqlite3_exec(s, cmd_message, NULL, NULL, &err_msg) != 0) {
		log_err(0, "db: _create_tables: Cmd_Message: %s", err_msg);
		goto err0;
	}

	return 0;

err0:
	sqlite3_free(err_msg);
	return -1;
}


static int
_exec_set_args(sqlite3_stmt *s, const DbArg args[], int args_len)
{
	for (int i = 0; i < args_len;) {
		int ret;
		const DbArg *const arg = &args[i++];
		switch (arg->type) {
		case DB_DATA_TYPE_INT:
			ret = sqlite3_bind_int(s, i, arg->int_);
			break;
		case DB_DATA_TYPE_INT64:
			ret = sqlite3_bind_int64(s, i, arg->int64);
			break;
		case DB_DATA_TYPE_TEXT:
			ret = sqlite3_bind_text(s, i, arg->text, -1, NULL);
			break;
		case DB_DATA_TYPE_NULL:
			ret = sqlite3_bind_null(s, i);
			break;
		default:
			log_err(0, "db: _exec_set_args: invalid arg type: [%d]:%d", (i - 1), arg->type);
			return -1;
		}

		if (ret != SQLITE_OK) {
			log_err(0, "db: _exec_set_args: bind: failed to bind: %s", sqlite3_errstr(ret));
			return -1;
		}
	}

	return 0;
}


static int
_exec_get_output(sqlite3_stmt *s, DbOut out[], int out_len)
{
	int count = 0;
	for (; count < out_len; count++) {
		const int ret = sqlite3_step(s);
		if (ret == SQLITE_DONE)
			break;

		if (ret != SQLITE_ROW) {
			log_err(0, "db: _exec_get_outputs: sqlite3_step: %s", sqlite3_errstr(ret));
			return -1;
		}

		if (_exec_get_output_values(s, out[count].items, out[count].len) < 0)
			return -1;
	}

	return count;
}


static int
_exec_get_output_values(sqlite3_stmt *s, DbOutItem items[], int len)
{
	int i = 0;
	for (; i < len; i++) {
		DbOutItem *const item = &items[i];
		const int type = sqlite3_column_type(s, i);
		if (type == SQLITE_NULL) {
			memset(item, 0, sizeof(*item));
			item->type = DB_DATA_TYPE_NULL;
			continue;
		}

		switch (item->type) {
		case DB_DATA_TYPE_INT:
			if (type != SQLITE_INTEGER)
				goto err0;

			*item->int_ = sqlite3_column_int(s, i);
			break;
		case DB_DATA_TYPE_INT64:
			if (type != SQLITE_INTEGER)
				goto err0;

			*item->int64 = sqlite3_column_int64(s, i);
			break;
		case DB_DATA_TYPE_TEXT:
			if (type != SQLITE_TEXT)
				goto err0;

			DbOutItemText *const text = &item->text;
			text->len = cstr_copy_n(text->cstr, text->size, (const char *)sqlite3_column_text(s, i));
			break;
		default:
			log_err(0, "db: _exec_get_output_values: invalid out type: [%d:%d]", i, item->type);
			return -1;
		}
	}

	return 0;

err0:
	log_err(0, "db: _exec_get_output_values: [%d:%d]: column type didn't match", i, items[i].type);
	return -1;
}
