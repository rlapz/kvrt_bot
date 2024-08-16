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


static int _create_tables(sqlite3 *s);
static int _exec_set_args(sqlite3_stmt *s, const DbArg args[], int args_len);
static int _exec_get_outputs(sqlite3_stmt *s, DbOut out[], int out_len);
static int _exec(sqlite3 *s, const char query[], const DbArg args[], int args_len, DbOut out[], int out_len);


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

		if (_exec(d->sql, sql, args, LEN(args), NULL, 0) < 0)
			return -1;
	}

	return 0;
}


int
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

	DbOut out = {
		.len = 2,
		.items = (DbOutItem[]) {
			{ .type = DB_DATA_TYPE_INT, .int_ = &admin->is_creator },
			{ .type = DB_DATA_TYPE_INT, .int_ = (int *)&admin->privileges },
		},
	};

	return _exec(d->sql, sql, args, LEN(args), &out, 1);
}


int
db_admin_clear(Db *d, int64_t chat_id)
{
	const char *const sql = "delete from Admin where (chat_id = ?);";
	const DbArg arg = { .type = DB_DATA_TYPE_INT64, .int64 = chat_id };
	return _exec(d->sql, sql, &arg, 1, NULL, 0);
}


int
db_cmd_set(Db *d, int64_t chat_id, const char name[], int is_enable)
{
	const DbArg args[] = {
		{ .type = DB_DATA_TYPE_INT64, .int64 = chat_id },
		{ .type = DB_DATA_TYPE_INT, .int_ = is_enable },
		{ .type = DB_DATA_TYPE_TEXT, .text = name },
	};

	int is_exists = 0;
	const char *sql = "select exists(select 1 from Cmd where (name = ?) limit 1);";

	DbOut out = {
		.len = 1,
		.items = &(DbOutItem) {
			.type = DB_DATA_TYPE_INT,
			.int_ = &is_exists,
		},
	};

	const int ret = _exec(d->sql, sql, &args[2], 1, &out, 1);
	if (ret <= 0)
		return ret;

	sql = "insert into Cmd_Chat(cmd_id, chat_id, is_enable) "
	      "select a.id, ?, ? "
	      "from Cmd as a "
	      "where (a.name = ?) "
	      "order by a.id desc "
	      "limit 1;";

	return _exec(d->sql, sql, args, LEN(args), NULL, 0);
}


int
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

	DbOut out = {
		.len = 3,
		.items = (DbOutItem[]) {
			{ .type = DB_DATA_TYPE_TEXT, .text = { .cstr = cmd->name, .size = LEN(cmd->name) } },
			{ .type = DB_DATA_TYPE_TEXT, .text = { .cstr = cmd->file, .size = LEN(cmd->file) } },
			{ .type = DB_DATA_TYPE_INT, .int_ = &cmd->is_enable },
		},
	};

	return _exec(d->sql, sql, args, LEN(args), &out, 1);
}


int
db_cmd_message_set(Db *d, int64_t chat_id, int64_t user_id, const char name[], const char message[])
{
	if (message == NULL)
		message = "";

	const DbArg args[] = {
		{ .type = DB_DATA_TYPE_INT64, .int64 = chat_id },
		{ .type = DB_DATA_TYPE_TEXT, .text = name },
		{ .type = DB_DATA_TYPE_TEXT, .text = message },
		{ .type = DB_DATA_TYPE_INT64, .int64 = user_id },
	};

	const char *const sql = "insert into Cmd_Message(chat_id, name, message, created_by) values (?, ?, ?, ?)";
	return _exec(d->sql, sql, args, LEN(args), NULL, 0);
}


int
db_cmd_message_get(Db *d, char buffer[], size_t size, int64_t chat_id, const char name[])
{
	const DbArg args[] = {
		{ .type = DB_DATA_TYPE_INT64, .int64 = chat_id },
		{ .type = DB_DATA_TYPE_TEXT, .text = name },
	};

	DbOut out = {
		.len = 1,
		.items = &(DbOutItem) {
			.type = DB_DATA_TYPE_TEXT,
			.text = { .cstr = buffer, .size = size },
		},
	};

	const char *const sql = "select message "
				"from Cmd_Message "
				"where (chat_id = ?) and (name = ?) "
				"order by id desc "
				"limit 1;";
	return _exec(d->sql, sql, args, LEN(args), &out, 1);
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
				  "privileges integer not null,"		/* Bitwise TgChatAdminPrivilege */
				  "created_at datetime default (datetime('now', 'localtime')) not null,"
				  "updated_at datetime);";

	const char *const cmd = "create table if not exists Cmd("
				"id         integer primary key autoincrement,"
				"name       varchar(33) unique not null,"
				"file       varchar(1023) not null,"
				"args       integer not null,"			/* Bitwise DbCmdArgType */
				"is_nsfw    boolean not null,"
				"created_at datetime default (datetime('now', 'localtime')) not null,"
				"updated_at datetime);";

	const char *const cmd_chat = "create table if not exists Cmd_Chat("
				     "id         integer primary key autoincrement,"
				     "chat_id    bigint not null,"		/* telegram chat id */
				     "cmd_name   varchar(33) not null,"
				     "is_enable  boolean not null,"
				     "created_at datetime default (datetime('now', 'localtime')) not null,"
				     "updated_at datetime,"
				     "updated_by bigint);";			/* telegram user id */

	const char *const cmd_message = "create table if not exists Cmd_Message("
					"id         integer primary key autoincrement,"
					"chat_id    bigint not null,"		/* telegram chat id */
					"name       varchar(33) not null,"
					"message    varchar(8192) not null,"
					"created_at datetime default (datetime('now', 'localtime')) not null,"
					"created_by bigint);";			/* telegram user id */


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


static int
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
_exec_get_outputs(sqlite3_stmt *s, DbOut out[], int out_len)
{
	int count = 0;
	for (; count < out_len; count++) {
		const int ret = sqlite3_step(s);
		switch (ret) {
		case SQLITE_DONE:
			/* success */
			return count;
		case SQLITE_ROW:
			break;
		default:
			log_err(0, "db: _exec_get_outputs: sqlite3_step: %s", sqlite3_errstr(ret));
			return -1;
		}

		DbOut *const p = &out[count];
		for (int i = 0; i < p->len; i++) {
			DbOutItem *const o = &p->items[i];
			const int type = sqlite3_column_type(s, i);
			switch (o->type) {
			case DB_DATA_TYPE_INT:
				if (type != SQLITE_INTEGER)
					goto err0;

				*o->int_ = sqlite3_column_int(s, i);
				break;
			case DB_DATA_TYPE_INT64:
				if (type != SQLITE_INTEGER)
					goto err0;

				*o->int64 = sqlite3_column_int64(s, i);
				break;
			case DB_DATA_TYPE_TEXT:
				if (type != SQLITE_TEXT)
					goto err0;

				cstr_copy_n(o->text.cstr, o->text.size, (const char *)sqlite3_column_text(s, i));
				break;
			default:
				log_err(0, "db: _exec_get_outputs: invalid out type: [%d]:%d", i, o->type);
				return -1;
			}
		}
	}

	/* success */
	return count;

err0:
	log_err(0, "db: _exec_get_outputs: column type doesn't match");
	return -1;
}


static int
_exec(sqlite3 *s, const char query[], const DbArg args[], int args_len, DbOut out[], int out_len)
{
	sqlite3_stmt *stmt;
	int ret = sqlite3_prepare_v2(s, query, -1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		log_err(0, "db: _exec: sqlite3_prepare_v2: %s", sqlite3_errstr(ret));
		return -1;
	}

	ret = _exec_set_args(stmt, args, args_len);
	if (ret < 0)
		goto out0;

	if ((out == NULL) || (out_len == 0)) {
		ret = sqlite3_step(stmt);
		switch (ret) {
		case SQLITE_ROW:
		case SQLITE_DONE:
			ret = 0;
			break;
		default:
			log_err(0, "db: _exec: sqlite3_step: %s", sqlite3_errstr(ret));
			ret = -1;
			break;
		}
	} else {
		ret = _exec_get_outputs(stmt, out, out_len);
	}

out0:
	sqlite3_finalize(stmt);
	return ret;
}
