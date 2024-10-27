#include <errno.h>
#include <string.h>

#include "db.h"
#include "util.h"


typedef enum db_data_type {
	DB_DATA_TYPE_NULL,
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

typedef struct db_out_item_text {
	char   *cstr;
	size_t  len;
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
static int _exec_get_output(sqlite3_stmt *s, DbOut out[], int out_len);
static int _exec_get_output_values(sqlite3_stmt *s, DbOutItem items[], int len);
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


/* TODO */
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
	//const char *const sql = "select a.name, a.file, a.args, a.description, b.is_enable, "
	//			"a.is_nsfw, a.is_admin_only "
	//			"from Cmd as a "
	//			"join Cmd_Chat as b on (a.name = b.cmd_name) "
	//			"where (a.name = ?) and (b.chat_id = ?) "
	//			"order by b.id desc "
	//			"limit 1;";
	const char *const sql = "select name, file, args, description, 1 as is_enable, "
				"is_nsfw, is_admin_only "
				"from Cmd "
				"where (name = ?) ";
				//"order by id desc "
				//"limit 1;";

	const DbArg args[] = {
		{ .type = DB_DATA_TYPE_TEXT, .text = name },
		//{ .type = DB_DATA_TYPE_INT64, .int64 = chat_id },
	};

	DbOut out = {
		.len = 7,
		.items = (DbOutItem[]) {
			{ .type = DB_DATA_TYPE_TEXT, .text = { .cstr = cmd->name, .size = DB_CMD_NAME_SIZE } },
			{ .type = DB_DATA_TYPE_TEXT, .text = { .cstr = cmd->file, .size = DB_CMD_FILE_NAME_SIZE } },
			{ .type = DB_DATA_TYPE_INT, .int_ = (int *)&cmd->args },
			{ .type = DB_DATA_TYPE_TEXT, .text = { .cstr = cmd->description, .size = DB_CMD_DESC_SIZE } },
			{ .type = DB_DATA_TYPE_INT, .int_ = &cmd->is_enable },
			{ .type = DB_DATA_TYPE_INT, .int_ = &cmd->is_nsfw },
			{ .type = DB_DATA_TYPE_INT, .int_ = &cmd->is_admin_only },
		},
	};

	cmd->chat_id = chat_id;
	return _exec(d->sql, sql, args, LEN(args), &out, 1);
}


int
db_cmd_message_set(Db *d, int64_t chat_id, int64_t user_id, const char name[], const char message[])
{
	DbArg args[] = {
		{ .type = DB_DATA_TYPE_INT64, .int64 = chat_id },
		{ .type = DB_DATA_TYPE_TEXT, .text = name },
		{ .type = DB_DATA_TYPE_TEXT, .text = message },
		{ .type = DB_DATA_TYPE_INT64, .int64 = user_id },
	};

	const char *sql;
	if ((message == NULL) || (message[0] == '\0')) {
		int is_exists = 0;
		DbOut out = {
			.len = 1,
			.items = &(DbOutItem) {
				.type = DB_DATA_TYPE_INT,
				.int_ = &is_exists,
			},
		};

		sql = "select iif((message = ''), 0, 1) "
		      "from Cmd_Message "
		      "where (chat_id = ?) and (name = ?) "
		      "order by id desc "
		      "limit 1;";

		const int ret = _exec(d->sql, sql, args, 2, &out, 1);
		if (ret <= 0)
			return ret;

		if (is_exists == 0)
			return 0;

		/* invalidate cmd message */
		args[2].text = "";
	}

	/* avoid 'delete' query */

	sql = "insert into Cmd_Message(chat_id, name, message, created_by) values (?, ?, ?, ?)";
	if (_exec(d->sql, sql, args, LEN(args), NULL, 0) < 0)
		return -1;

	return 1;
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
	int ret = _exec(d->sql, sql, args, LEN(args), &out, 1);
	if (out.items[0].text.len == 0)
		ret = 0;

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
				  "is_creator boolean not null,"
				  "chat_id    bigint not null,"			/* telegram chat id */
				  "user_id    bigint not null,"			/* telegram user id */
				  "privileges integer not null,"		/* Bitwise OR TgChatAdminPrivilege */
				  "created_at datetime default (datetime('now', 'localtime')) not null,"
				  "updated_at datetime);";

	const char *const cmd = "create table if not exists Cmd("
				"id            integer primary key autoincrement,"
				"name          varchar(33) unique not null,"
				"file          varchar(1023) not null,"
				"args          integer not null,"		/* Bitwise OR DbCmdArgType */
				"description   varchar(255),"
				"is_nsfw       boolean not null,"
				"is_admin_only boolean not null,"
				"created_at    datetime default (datetime('now', 'localtime')) not null,"
				"updated_at    datetime);";

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
	int i = 0;
	for (; i < out_len; i++) {
		const int ret = sqlite3_step(s);
		if (ret == SQLITE_DONE)
			break;

		if (ret != SQLITE_ROW) {
			log_err(0, "db: _exec_get_outputs: sqlite3_step: %s", sqlite3_errstr(ret));
			return -1;
		}

		if (_exec_get_output_values(s, out[i].items, out[i].len) < 0)
			return -1;
	}

	return i;
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
			return 0;
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
	log_err(0, "db: _exec_get_output_values: [%d:%d]: column type doesn't match", i, items[i].type);
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

	if (out != NULL) {
		ret = _exec_get_output(stmt, out, out_len);
		goto out0;
	}

	switch (sqlite3_step(stmt)) {
	case SQLITE_ROW:
	case SQLITE_DONE:
		ret = 0;
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
