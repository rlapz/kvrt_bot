#include <errno.h>
#include <string.h>

#include <db.h>
#include <util.h>


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
db_admin_add(Db *d, const EAdmin admin_list[], int admin_list_len)
{
	const char *const sql = "insert into Admin(chat_id, user_id, privileges) values (?, ?, ?);";
	for (int i = 0; i < admin_list_len; i++) {
		const EAdmin *admin = &admin_list[i];
		const DbArg args[] = {
			{ .type = DB_DATA_TYPE_INT64, .int64 = *admin->chat_id },
			{ .type = DB_DATA_TYPE_INT64, .int64 = *admin->user_id },
			{ .type = DB_DATA_TYPE_INT, .int_ = *admin->privileges },
		};

		if (_exec(d->sql, sql, args, LEN(args), NULL, 0) < 0)
			return -1;
	}

	return 0;
}


int
db_admin_get_privileges(Db *d, TgChatAdminPrivilege *privs, int64_t chat_id, int64_t user_id)
{
	const char *const sql = "select privileges "
				"from Admin "
				"where (chat_id = ?) and (user_id = ?)"
				"order by id desc limit 1;";

	const DbArg args[] = {
		{ .type = DB_DATA_TYPE_INT64, .int64 = chat_id },
		{ .type = DB_DATA_TYPE_INT64, .int64 = user_id },
	};

	DbOut out = {
		.len = 1,
		.items = (DbOutItem[]) {
			{ .type = DB_DATA_TYPE_INT, .int_ = (int *)privs },
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
db_module_extern_toggle(Db *d, int64_t chat_id, const char name[], int is_enable)
{
	const DbArg args[] = {
		{ .type = DB_DATA_TYPE_INT64, .int64 = chat_id },
		{ .type = DB_DATA_TYPE_TEXT, .text = name },
	};

	const char *sql = "select exists(select 1 from Module_Extern where (name = ?) limit 1);";
	const int ret = _exec(d->sql, sql, &args[1], 1, NULL, 0);
	if (ret <= 0)
		return ret;

	if (is_enable)
		sql = "delete from Module_Extern_Disable where (chat_id = ?) and  (module_name = ?);";
	else
		sql = "insert into Module_Extern_Disable(module_name, chat_id) values(?, ?);";

	return _exec(d->sql, sql, args, LEN(args), NULL, 0);
}


int
db_module_extern_get(Db *d, const EModuleExtern *mod, int64_t chat_id, const char name[])
{
	const DbArg args[] = {
		{ .type = DB_DATA_TYPE_INT64, .int64 = chat_id },
		{ .type = DB_DATA_TYPE_TEXT, .text = name },
	};

	DbOut out = {
		.len = 7,
		.items = (DbOutItem[]) {
			{
				.type = DB_DATA_TYPE_INT,
				.int_ = mod->type,
			},
			{
				.type = DB_DATA_TYPE_INT,
				.int_ = mod->flags,
			},
			{
				.type = DB_DATA_TYPE_INT,
				.int_ = mod->args,
			},
			{
				.type = DB_DATA_TYPE_INT,
				.int_ = mod->args_len,
			},
			{
				.type = DB_DATA_TYPE_TEXT,
				.text = { .cstr = mod->name, .size = mod->name_size },
			},
			{
				.type = DB_DATA_TYPE_TEXT,
				.text = { .cstr = mod->file_name, .size = mod->file_name_size },
			},
			{
				.type = DB_DATA_TYPE_TEXT,
				.text = { .cstr = mod->description, .size = mod->description_size },
			},
		},
	};

	const char *sql = "select exists(select 1 from Module_Extern_Disable "
			  "where (chat_id = ?) and (module_name = ?));";

	const int ret = _exec(d->sql, sql, args, LEN(args), NULL, 0);
	if (ret < 0)
		return -1;

	if (ret > 0)
		return 0;

	sql = "select type, flags, args, args_len, name, file_name, description "
	      "from Module_Extern "
	      "where (name = ?) "
	      "order by id desc "
	      "limit 1;";

	return _exec(d->sql, sql, &args[1], 1, &out, 1);
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
db_cmd_message_get(Db *d, const ECmdMessage *msg, int64_t chat_id, const char name[])
{
	const DbArg args[] = {
		{ .type = DB_DATA_TYPE_INT64, .int64 = chat_id },
		{ .type = DB_DATA_TYPE_TEXT, .text = name },
	};

	DbOut out = {
		.len = 5,
		.items = (DbOutItem[]) {
			{
				.type = DB_DATA_TYPE_INT64,
				.int64 = msg->chat_id,
			},
			{
				.type = DB_DATA_TYPE_TEXT,
				.text = { .cstr = msg->name, .size = msg->name_size },
			},
			{
				.type = DB_DATA_TYPE_TEXT,
				.text = { .cstr = msg->message, .size = msg->message_size },
			},
			{
				.type = DB_DATA_TYPE_TEXT,
				.text = { .cstr = msg->created_at, .size = msg->created_at_size },
			},
			{
				.type = DB_DATA_TYPE_INT64,
				.int64 = msg->created_by,
			},
		},
	};

	const char *const sql = "select chat_id, name, message, created_at, created_by "
				"from Cmd_Message "
				"where (chat_id = ?) and (name = ?) "
				"order by id desc "
				"limit 1;";
	int ret = _exec(d->sql, sql, args, LEN(args), &out, 1);
	if (out.items[0].text.len == 0)
		ret = 0;

	return ret;
}


int
db_cmd_message_get_message(Db *d, char buffer[], size_t size, int64_t chat_id, const char name[])
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
		"type        integer not null,"
		"flags       integer not null,"
		"name        varchar(33) not null,"
		"file_name   varchar(1023) not null,"
		"description varchar(255) not null,"
		"args        integer not null,"
		"args_len    integer not null,"
		"created_at  datetime default(datetime('now', 'localtime')) not null);";

	const char *const module_extern_disable =
		"create table if not exists Module_Extern_Disable("
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

	if (sqlite3_exec(s, module_extern_disable, NULL, NULL, &err_msg) != 0) {
		log_err(0, "db: _create_tables: Module_Extern_Disable: %s", err_msg);
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
