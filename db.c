#include <errno.h>
#include <string.h>

#include "db.h"
#include "util.h"


typedef enum db_data_type {
	DB_DATA_TYPE_INT,
	DB_DATA_TYPE_INT64,
	DB_DATA_TYPE_TEXT,
} DbDataType;

typedef struct db_arg {
	DbDataType type;
	union {
		int         int_;
		int64_t     int64;
		const char *text;
	};
} DbArg;

typedef struct db_out_text {
	char   *cstr;
	size_t  size;
} DbOutItemText;

typedef struct db_out_item {
	DbDataType type;
	union {
		int           *int_;
		int64_t       *int64;
		DbOutItemText  text;
	};
} DbOutItem;

typedef struct db_out {
	int        len;
	DbOutItem *items;
} DbOut;


static int   _create_tables(sqlite3 *s);
static DbRet _exec_set_args(sqlite3_stmt *s, const DbArg args[], int args_len);
static DbRet _exec_get_outputs(sqlite3_stmt *s, DbOut out[], int *out_len);
static DbRet _exec(sqlite3 *s, const char query[], const DbArg args[], int args_len, DbOut out[], int *out_len);


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
db_admin_set(Db *d, const DbAdmin admin_list[], int admin_list_len)
{
	const char *const sql = "insert into Admin(is_creator, chat_id, user_id, privileges) values (?, ?, ?, ?);";
	for (int i = 0; i < admin_list_len; i++) {
		const DbAdmin *admin = &admin_list[i];
		const DbArg args[] = {
			{ .type = DB_DATA_TYPE_INT, .int_ = admin->is_creator },
			{ .type = DB_DATA_TYPE_INT64, .int64 = admin->chat_id },
			{ .type = DB_DATA_TYPE_INT64, .int64 = admin->user_id },
			{ .type = DB_DATA_TYPE_INT, .int_ = admin->privileges },
		};

		if (_exec(d->sql, sql, args, LEN(args), NULL, NULL) == DB_RET_ERROR)
			return DB_RET_ERROR;
	}

	return DB_RET_OK;
}


DbRet
db_admin_get(Db *d, DbAdmin *admin, int64_t chat_id, int64_t user_id)
{
	const char *const sql = "select is_creator, privileges "
				"from Admin "
				"where (chat_id = ?) and (user_id = ?)"
				"order by id desc limit 1;";

	const DbArg args[] = {
		{ .type = DB_DATA_TYPE_INT64, .int64 = chat_id },
		{ .type = DB_DATA_TYPE_INT64, .int64 = user_id },
	};

	int row_len = 1;
	DbOut out = {
		.len = 2,
		.items = (DbOutItem[]) {
			{ .type = DB_DATA_TYPE_INT, .int_ = &admin->is_creator },
			{ .type = DB_DATA_TYPE_INT, .int_ = (int *)&admin->privileges },
		},
	};

	const DbRet ret = _exec(d->sql, sql, args, LEN(args), &out, &row_len);
	if (ret == DB_RET_ERROR)
		return ret;

	if (row_len == 0) {
		admin->is_creator = 0;
		admin->privileges = 0;
		return DB_RET_EMPTY;
	}

	return ret;
}


DbRet
db_admin_clear(Db *d, int64_t chat_id)
{
	const char *const sql = "delete from Admin where (chat_id = ?);";
	const DbArg arg = { .type = DB_DATA_TYPE_INT64, .int64 = chat_id };
	return _exec(d->sql, sql, &arg, 1, NULL, NULL);
}


DbRet
db_cmd_set(Db *d, int64_t chat_id, const char name[], int is_enable)
{
	const DbArg args[] = {
		{ .type = DB_DATA_TYPE_INT64, .int64 = chat_id },
		{ .type = DB_DATA_TYPE_INT, .int_ = is_enable },
		{ .type = DB_DATA_TYPE_TEXT, .text = name },
	};

	int is_exists = 0;
	const char *sql = "select exists(select 1 from Cmd where (name = ?) limit 1);";

	int row_len = 1;
	DbOut out = {
		.len = 1,
		.items = &(DbOutItem) {
			.type = DB_DATA_TYPE_INT,
			.int_ = &is_exists,
		},
	};

	DbRet ret = _exec(d->sql, sql, &args[2], 1, &out, &row_len);
	if (ret == DB_RET_ERROR)
		return ret;

	if ((row_len == 0) || (is_exists == 0))
		return DB_RET_EMPTY;

	sql = "insert into Cmd_Chat(cmd_id, chat_id, is_enable) "
	      "select a.id, ?, ? "
	      "from Cmd as a "
	      "where (a.name = ?) "
	      "order by a.id desc "
	      "limit 1;";

	return _exec(d->sql, sql, args, LEN(args), NULL, NULL);
}


DbRet
db_cmd_get(Db *d, DbCmd *cmd, int64_t chat_id, const char name[])
{
	const char *const sql = "select a.name, a.file, a.args, b.is_enable "
				"from Cmd as a "
				"join Cmd_Chat as b on (a.id = b.cmd_id) "
				"where (a.name = ?) and (b.chat_id = ?) "
				"order by b.id desc "
				"limit 1;";

	const DbArg args[] = {
		{ .type = DB_DATA_TYPE_TEXT, .text = name },
		{ .type = DB_DATA_TYPE_INT64, .int64 = chat_id },
	};

	int row_len = 1;
	DbOut out = {
		.len = 3,
		.items = (DbOutItem[]) {
			{ .type = DB_DATA_TYPE_TEXT, .text = { .cstr = cmd->name, .size = LEN(cmd->name) } },
			{ .type = DB_DATA_TYPE_TEXT, .text = { .cstr = cmd->file, .size = LEN(cmd->file) } },
			{ .type = DB_DATA_TYPE_INT, .int_ = &cmd->is_enable },
		},
	};

	const DbRet ret = _exec(d->sql, sql, args, LEN(args), &out, &row_len);
	if (ret == DB_RET_ERROR)
		return ret;

	if (row_len == 0)
		return DB_RET_EMPTY;

	return DB_RET_OK;
}


DbRet
db_cmd_message_get(Db *d, char buffer[], size_t size, const char name[])
{
	const DbArg arg = {
		.type = DB_DATA_TYPE_TEXT,
		.text = name,
	};

	int row_len = 1;
	DbOut out = {
		.len = 1,
		.items = &(DbOutItem) {
			.type = DB_DATA_TYPE_TEXT,
			.text = { .cstr = buffer, .size = size },
		},
	};

	const char *const sql = "select message from Cmd_Message where (name = ?) order by id desc limit 1;";
	const DbRet ret = _exec(d->sql, sql, &arg, 1, &out, &row_len);
	if (ret == DB_RET_ERROR)
		return ret;

	if (row_len == 0)
		return DB_RET_EMPTY;

	return DB_RET_OK;
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


static DbRet
_exec_set_args(sqlite3_stmt *s, const DbArg args[], int args_len)
{
	for (int i = 0; i < args_len; i++) {
		int ret;
		const DbArg *const arg = &args[i];
		switch (arg->type) {
		case DB_DATA_TYPE_INT:
			ret = sqlite3_bind_int(s, (i + 1), arg->int_);
			break;
		case DB_DATA_TYPE_INT64:
			ret = sqlite3_bind_int64(s, (i + 1), arg->int64);
			break;
		case DB_DATA_TYPE_TEXT:
			ret = sqlite3_bind_text(s, (i + 1), arg->text, -1, NULL);
			break;
		default:
			log_err(0, "db: _exec_set_args: invalid arg type: [%d]:%d", i, arg->type);
			return DB_RET_ERROR;
		}

		if (ret != SQLITE_OK) {
			log_err(0, "db: _exec_set_args: bind: failed to bind: %s", sqlite3_errstr(ret));
			return DB_RET_ERROR;
		}
	}

	return DB_RET_OK;
}


static DbRet
_exec_get_outputs(sqlite3_stmt *s, DbOut out[], int *out_len)
{
	DbRet db_ret = DB_RET_ERROR;
	int count = 0;
	if ((out == NULL) || (out_len == NULL))
		return DB_RET_OK;

	int is_type_matched = 0;
	const int row_len = *out_len;
	for (; count < row_len; count++) {
		int ret = sqlite3_step(s);
		switch (ret) {
		case SQLITE_DONE:
			goto out1;
		case SQLITE_ROW:
			break;
		default:
			log_err(0, "db: _exec_get_outputs: sqlite3_step: %s", sqlite3_errstr(ret));
			goto out0;
		}

		DbOut *const p = &out[count];
		for (int i = 0; i < p->len; i++) {
			DbOutItem *const o = &p->items[i];
			const int type = sqlite3_column_type(s, i);
			switch (o->type) {
			case DB_DATA_TYPE_INT:
				if (type != SQLITE_INTEGER)
					goto out2;

				*o->int_ = sqlite3_column_int(s, i);
				break;
			case DB_DATA_TYPE_INT64:
				if (type != SQLITE_INTEGER)
					goto out2;

				*o->int64 = sqlite3_column_int64(s, i);
				break;
			case DB_DATA_TYPE_TEXT:
				if (type != SQLITE_TEXT)
					goto out2;

				cstr_copy_n(o->text.cstr, o->text.size, (const char *)sqlite3_column_text(s, i));
				break;
			default:
				log_err(0, "db: _exec_get_outputs: invalid out type: [%d]:%d", i, o->type);
				goto out0;
			}
		}
	}

	is_type_matched = 1;

out2:
	if (is_type_matched == 0) {
		log_err(0, "db: _exec_get_outputs: column type doesn't match");
		goto out0;
	}
out1:
	db_ret = DB_RET_OK;
out0:
	*out_len = count;
	return db_ret;
}


static DbRet
_exec(sqlite3 *s, const char query[], const DbArg args[], int args_len, DbOut out[], int *out_len)
{
	sqlite3_stmt *stmt;
	const int ret = sqlite3_prepare_v2(s, query, -1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		log_err(0, "db: _exec_many: sqlite3_prepare_v2: %s", sqlite3_errstr(ret));
		return DB_RET_ERROR;
	}

	DbRet db_ret = _exec_set_args(stmt, args, args_len);
	if (db_ret == DB_RET_ERROR)
		goto out0;

	db_ret = _exec_get_outputs(stmt, out, out_len);

out0:
	sqlite3_finalize(stmt);
	return db_ret;
}
