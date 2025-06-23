#include <errno.h>
#include <sqlite3.h>

#include "model.h"

#include "ev.h"
#include "sqlite_pool.h"
#include "util.h"


enum {
	_DATA_TYPE_NULL = 0,
	_DATA_TYPE_INT,
	_DATA_TYPE_INT64,
	_DATA_TYPE_TEXT,
};

typedef struct data_text {
	char *value;
	int   size;
} DataText;

typedef struct data {
	int type;
	union {
		int     *int_;
		int64_t *int64;
		DataText text;
	};
} Data;

static const char *_chat_query(Str *str);
static const char *_admin_query(Str *str);
static const char *_cmd_builtin_query(Str *str);
static const char *_cmd_extern_query(Str *str);
static const char *_cmd_message_query(Str *str);
static const char *_sched_message_query(Str *str);
static int         _sqlite_step_one(sqlite3_stmt *stmt);
static int         _sqlite_exec_one(const char query[], const Data args[], int args_len,
				    const Data *out);


/*
 * Public
 */
int
model_init(void)
{
	Str str;
	int ret = str_init_alloc(&str, 0);
	if (ret < 0) {
		log_err(ret, "model: model_init: str_init_alloc");
		return -1;
	}

	char *err_msg;
	struct {
		const char *table_name;
		const char *(*func)(Str *);
	} params[] = {
		{ "Chat", _chat_query },
		{ "Admin", _admin_query },
		{ "Cmd_Builtin", _cmd_builtin_query },
		{ "Cmd_Extern", _cmd_extern_query },
		{ "Cmd_Message", _cmd_message_query },
		{ "Sched_Message", _sched_message_query },
	};

	DbConn *const conn = sqlite_pool_get();
	for (size_t i = 0; i < LEN(params); i++) {
		const char *const query = params[i].func(&str);
		ret = sqlite3_exec(conn->sql, query, NULL, NULL, &err_msg);
		if (ret != SQLITE_OK) {
			const char *const table_name = params[i].table_name;
			log_err(0, "model: model_init: sqlite3_exec: %s: %s", table_name, err_msg);
			ret = -ret;
			goto out0;
		}
	}

	ret = 0;

out0:
	free(err_msg);
	sqlite_pool_put(conn);
	str_deinit(&str);
	return ret;
}


/*
 * ModelChat
 */
int
model_chat_init(int64_t chat_id)
{
	int flags = MODEL_CHAT_FLAG_ALLOW_CMD_EXTRA | MODEL_CHAT_FLAG_ALLOW_CMD_EXTERN;
	int64_t now = time(NULL);
	const Data args[] = {
		{ .type = _DATA_TYPE_INT64, .int64 = &chat_id },
		{ .type = _DATA_TYPE_INT, .int_ = &flags },
		{ .type = _DATA_TYPE_INT64, .int64 = &now },
		{ .type = _DATA_TYPE_INT64, .int64 = &chat_id },
	};

	const char *const query =
		"INSERT INTO Chat(chat_id, flags, created_at) "
		"SELECT ?, ?, ? "
		"WHERE NOT EXISTS(SELECT 1 FROM Chat WHERE (chat_id = ?));";

	return _sqlite_exec_one(query, args, LEN(args), NULL);
}


int
model_chat_set_flags(int64_t chat_id, int flags)
{
	const Data args[] = {
		{ .type = _DATA_TYPE_INT, .int_ = &flags },
		{ .type = _DATA_TYPE_INT64, .int64 = &chat_id },
	};

	const char *const query = "UPDATE Chat SET flags = ? WHERE (chat_id = ?);";
	return _sqlite_exec_one(query, args, LEN(args), NULL);
}


int
model_chat_get_flags(int64_t chat_id)
{
	const Data arg = { .type = _DATA_TYPE_INT64, .int64 = &chat_id };
	const char *const query = "SELECT flags FROM Chat WHERE (chat_id = ?);";

	int flags = 0;
	const Data out = { .type = _DATA_TYPE_INT, .int_ = &flags };
	if (_sqlite_exec_one(query, &arg, 1, &out) < 0)
		return -1;

	return flags;
}


/*
 * ModelAdmin
 */
static int
_admin_add(DbConn *conn, const ModelAdmin list[], int len)
{
	int ret = -1;
	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		return -1;

	const char *const query =
		"INSERT INTO Admin(chat_id, user_id, is_anonymous, privileges, created_at) VALUES ";

	if (str_set(&str, query) == NULL)
		goto out0;

	for (int i = 0; i < len; i++) {
		if (str_append(&str, "(?, ?, ?, ?, ?), ") == NULL)
			goto out0;
	}

	str_pop(&str, 2);

	sqlite3_stmt *stmt;
	const int res = sqlite3_prepare_v2(conn->sql, str.cstr, str.len, &stmt, NULL);
	if (res != SQLITE_OK) {
		log_err(0, "model: _admin_add: sqlite3_prepare_v2: %s", sqlite3_errstr(res));
		goto out0;
	}

	const time_t now = time(NULL);
	for (int i = 0, j = 1; i < len; i++) {
		sqlite3_bind_int64(stmt, j++, list[i].chat_id);
		sqlite3_bind_int64(stmt, j++, list[i].user_id);
		sqlite3_bind_int(stmt, j++, list[i].is_anonymous);
		sqlite3_bind_int(stmt, j++, list[i].privileges);
		sqlite3_bind_int64(stmt, j++, now);
	}

	if (_sqlite_step_one(stmt) < 0)
		goto out1;

	ret = sqlite3_changes(conn->sql);

out1:
	sqlite3_finalize(stmt);
out0:
	str_deinit(&str);
	return ret;
}


static int
_admin_clear(DbConn *conn, int64_t chat_id)
{
	int ret = -1;
	sqlite3_stmt *stmt;

	const char *const query = "DELETE FROM Admin WHERE (chat_id = ?);";
	const int res = sqlite3_prepare_v2(conn->sql, query, -1, &stmt, NULL);
	if (res != SQLITE_OK) {
		log_err(0, "model: _admin_clear: sqlite3_prepare_v2: %s", sqlite3_errstr(res));
		goto out0;
	}

	sqlite3_bind_int64(stmt, 1, chat_id);
	ret = _sqlite_step_one(stmt);

out0:
	sqlite3_finalize(stmt);
	return ret;
}


int
model_admin_reload(const ModelAdmin list[], int len)
{
	int ret = -1;
	int is_ok = 0;
	DbConn *const conn = sqlite_pool_get();
	if (conn == NULL)
		return -1;

	if (sqlite_pool_tran_begin(conn) < 0)
		goto out0;

	if (_admin_clear(conn, list[0].chat_id) < 0)
		goto out1;

	ret = _admin_add(conn, list, len);
	if (ret < 0)
		goto out1;

	is_ok = 1;

out1:
	sqlite_pool_tran_end(conn, is_ok);
out0:
	sqlite_pool_put(conn);
	return ret;
}


int
model_admin_get_privileges(int64_t chat_id, int64_t user_id)
{
	const char *const query =
		"SELECT privileges "
		"FROM Admin "
		"WHERE (chat_id = ?) AND (user_id = ?) "
		"ORDER BY id DESC LIMIT 1;";

	const Data args[] = {
		{ .type = _DATA_TYPE_INT64, .int64 = &chat_id },
		{ .type = _DATA_TYPE_INT64, .int64 = &user_id },
	};

	int privs = 0;
	const Data out = { .type = _DATA_TYPE_INT, .int_ = &privs };
	if (_sqlite_exec_one(query, args, LEN(args), &out) < 0)
		return -1;

	return privs;
}


/*
 * ModelCmd
 */
int
model_cmd_get_list(ModelCmd list[], int len, int offset, int *total, int chat_flags, int is_private)
{
	int ret = -1;
	const char *const query =
		"SELECT SUM(size), Null, Null, Null, Null FROM ( "
			"SELECT COUNT(1) AS size FROM Cmd_Builtin WHERE ((flags & ?) = 0) "
				"UNION ALL "
			"SELECT COUNT(1) AS size FROM Cmd_Extern "
			"WHERE (? != 0) AND ((flags & ?) = 0) AND (is_enable = True) "
		") "
		"UNION ALL "
		"SELECT * FROM ("
			"SELECT 0, is_builtin, flags, name, description FROM ( "
				"SELECT 1 AS is_builtin, flags, name, description "
				"FROM Cmd_Builtin "
				"WHERE ((flags & ?) = 0) "
				"UNION ALL "
				"SELECT 0 AS is_builtin, flags, name, description "
				"FROM Cmd_Extern "
				"WHERE (? != 0) AND ((flags & ?) = 0) AND (is_enable = True) "
			")"
			"ORDER BY name "
			"LIMIT ? OFFSET ? "
		");";


	sqlite3_stmt *stmt;
	DbConn *const conn = sqlite_pool_get();
	if (conn == NULL)
		return -1;

	int show_extern = 0;
	if (chat_flags & MODEL_CHAT_FLAG_ALLOW_CMD_EXTERN)
		show_extern = 1;

	int flags = 0;
	if (is_private)
		flags |= MODEL_CMD_FLAG_DISALLOW_PRIVATE_CHAT;
	if ((chat_flags & MODEL_CHAT_FLAG_ALLOW_CMD_EXTRA) == 0)
		flags |= MODEL_CMD_FLAG_EXTRA;
	if ((chat_flags & MODEL_CHAT_FLAG_ALLOW_CMD_NSFW) == 0)
		flags |= MODEL_CMD_FLAG_NSFW;

	int res = sqlite3_prepare_v2(conn->sql, query, -1, &stmt, NULL);
	if (res != SQLITE_OK) {
		log_err(0, "model: model_cmd_get_list: sqlite3_prepare_v2: %s", sqlite3_errstr(res));
		goto out0;
	}

	sqlite3_bind_int(stmt, 1, flags);
	sqlite3_bind_int(stmt, 2, show_extern);
	sqlite3_bind_int(stmt, 3, flags);
	sqlite3_bind_int(stmt, 4, flags);
	sqlite3_bind_int(stmt, 5, show_extern);
	sqlite3_bind_int(stmt, 6, flags);
	sqlite3_bind_int(stmt, 7, len);
	sqlite3_bind_int(stmt, 8, offset);

	ret = _sqlite_step_one(stmt);
	if (ret <= 0)
		goto out1;

	*total = sqlite3_column_int(stmt, 0);

	int count = 0;
	for (; count < len; count++) {
		res = sqlite3_step(stmt);
		if (res == SQLITE_DONE)
			break;

		if (res != SQLITE_ROW) {
			log_err(0, "model: model_cmd_get_list: sqlite3_step: %s", sqlite3_errstr(res));
			goto out1;
		}

		ModelCmd *const mc = &list[count];
		mc->is_builtin = sqlite3_column_int(stmt, 1);
		mc->flags = sqlite3_column_int(stmt, 2);
		cstr_copy_n(mc->name, LEN(mc->name), (const char *)sqlite3_column_text(stmt, 3));
		cstr_copy_n(mc->description, LEN(mc->description), (const char *)sqlite3_column_text(stmt, 4));
	}

	ret = count;

out1:
	sqlite3_finalize(stmt);
out0:
	sqlite_pool_put(conn);
	return ret;
}


/*
 * ModelCmdBuiltin
 */
int
model_cmd_builtin_add(const ModelCmdBuiltin *c)
{
	const Data args[] = {
		{ .type = _DATA_TYPE_INT, .int_ = (int *)&c->idx },
		{ .type = _DATA_TYPE_INT, .int_ = (int *)&c->flags },
		{ .type = _DATA_TYPE_TEXT, .text = (DataText) { .value = (char *)c->name_in } },
		{ .type = _DATA_TYPE_TEXT, .text = (DataText) { .value = (char *)c->description_in } },
	};

	const char *const query =
		"INSERT INTO Cmd_Builtin(idx, flags, name, description) "
		"VALUES(?, ?, ?, ?);";

	return _sqlite_exec_one(query, args, LEN(args), NULL);
}


int
model_cmd_builtin_clear(void)
{
	int ret = -1;
	sqlite3_stmt *stmt;

	DbConn *const conn = sqlite_pool_get();
	if (conn == NULL)
		return -1;

	const char *query = "UPDATE sqlite_sequence SET seq = 0 WHERE (name = 'Cmd_Builtin');";
	int res = sqlite3_prepare_v2(conn->sql, query, -1, &stmt, NULL);
	if (res != SQLITE_OK) {
		log_err(0, "model: model_cmd_builtin_clear: sqlite3_prepare_v2: %s", sqlite3_errstr(res));
		goto out0;
	}

	ret = _sqlite_step_one(stmt);
	if (ret < 0)
		goto out1;

	sqlite3_finalize(stmt);

	query = "DELETE FROM Cmd_Builtin;";
	res = sqlite3_prepare_v2(conn->sql, query, -1, &stmt, NULL);
	if (res != SQLITE_OK) {
		log_err(0, "model: model_cmd_builtin_clear: sqlite3_prepare_v2: %s", sqlite3_errstr(res));
		goto out0;
	}

	ret = _sqlite_step_one(stmt);
	if (ret < 0)
		goto out1;

	ret = sqlite3_changes(conn->sql);

out1:
	sqlite3_finalize(stmt);
out0:
	sqlite_pool_put(conn);
	return ret;
}


int
model_cmd_builtin_get_index(const char name[])
{
	const Data arg = { .type = _DATA_TYPE_TEXT, .text = (DataText) { .value = (char *)name } };
	const char *const query = "SELECT idx FROM Cmd_Builtin WHERE (name = ?);";

	int index = 0;
	const Data out = { .type = _DATA_TYPE_INT, .int_ = &index };
	const int ret = _sqlite_exec_one(query, &arg, 1, &out);
	if (ret < 0)
		return -1;

	if (ret == 0)
		return -2;

	return index;
}


int
model_cmd_builtin_is_exists(const char name[])
{
	const Data arg = { .type = _DATA_TYPE_TEXT, .text = (DataText) { .value = (char *)name } };
	const char *const query = "SELECT 1 FROM Cmd_Builtin WHERE (name = ?);";

	int is_exists = 0;
	const Data out = { .type = _DATA_TYPE_INT, .int_ = &is_exists };
	if (_sqlite_exec_one(query, &arg, 1, &out) < 0)
		return -1;

	return is_exists;
}


/*
 * ModelCmdExtern
 */
int
model_cmd_extern_get(ModelCmdExtern *c, const char name[])
{
	int ret = -1;
	sqlite3_stmt *stmt;

	DbConn *const conn = sqlite_pool_get();
	if (conn == NULL)
		return -1;

	const char *const query =
		"SELECT id, is_enable, flags, name, file_name, description "
		"FROM Cmd_Extern "
		"WHERE (name = ?); ";

	const int res = sqlite3_prepare_v2(conn->sql, query, -1, &stmt, NULL);
	if (res != SQLITE_OK) {
		log_err(0, "model: model_cmd_extern_get: sqlite3_prepare_v2: %s", sqlite3_errstr(res));
		goto out0;
	}

	sqlite3_bind_text(stmt, 1, name, -1, NULL);
	ret = _sqlite_step_one(stmt);
	if (ret <= 0)
		goto out1;

	c->id = sqlite3_column_int(stmt, 0);
	c->is_enable = sqlite3_column_int(stmt, 1);
	c->flags = sqlite3_column_int(stmt, 2);
	cstr_copy_n(c->name, LEN(c->name), (const char *)sqlite3_column_text(stmt, 3));
	cstr_copy_n(c->file_name, LEN(c->file_name), (const char *)sqlite3_column_text(stmt, 4));
	cstr_copy_n(c->description, LEN(c->description), (const char *)sqlite3_column_text(stmt, 5));

out1:
	sqlite3_finalize(stmt);
out0:
	sqlite_pool_put(conn);
	return ret;
}


int
model_cmd_extern_is_exists(const char name[])
{
	const Data arg = { .type = _DATA_TYPE_TEXT, .text = (DataText) { .value = (char *)name } };
	const char *const query = "SELECT 1 FROM Cmd_Extern WHERE (name = ?);";

	int is_exists = 0;
	const Data out = { .type = _DATA_TYPE_INT, .int_ = &is_exists };
	if (_sqlite_exec_one(query, &arg, 1, &out) < 0)
		return -1;

	return is_exists;
}


/*
 * ModelCmdMessage
 */
int
model_cmd_message_set(const ModelCmdMessage *c)
{
	int ret = -1;
	sqlite3_stmt *stmt;

	DbConn *const conn = sqlite_pool_get();
	if (conn == NULL)
		return -1;

	const char *query = "SELECT 1 FROM Cmd_Message WHERE (chat_id = ?) AND (name = ?);";
	int res = sqlite3_prepare_v2(conn->sql, query, -1, &stmt, NULL);
	if (res != SQLITE_OK) {
		log_err(0, "model: model_cmd_message_set: sqlite3_prepare_v2: %s", sqlite3_errstr(res));
		goto out0;
	}

	sqlite3_bind_int64(stmt, 1, c->chat_id);
	sqlite3_bind_text(stmt, 2, c->name_in, -1, NULL);
	ret = _sqlite_step_one(stmt);
	if (ret < 0)
		goto out1;

	if ((ret == 0) && (c->value_in == NULL))
		goto out1;

	sqlite3_finalize(stmt);

	if (ret == 0) {
		query = "INSERT INTO Cmd_Message(value, created_by, chat_id, name, created_at) "
			"VALUES(?, ?, ?, ?, ?);";
	} else {
		query = "UPDATE Cmd_Message "
				"SET value = ?, updated_by = ?, updated_at = ? "
			"WHERE (chat_id = ?) and (name = ?);";
	}

	res = sqlite3_prepare_v2(conn->sql, query, -1, &stmt, NULL);
	if (res != SQLITE_OK) {
		log_err(0, "model: model_cmd_message_set: sqlite3_prepare_v2: %s", sqlite3_errstr(res));
		goto out0;
	}

	const time_t now = time(NULL);

	sqlite3_bind_text(stmt, 1, c->value_in, -1, NULL);
	if (ret == 0) {
		sqlite3_bind_int64(stmt, 2, c->created_by);
		sqlite3_bind_int64(stmt, 3, c->chat_id);
		sqlite3_bind_text(stmt, 4, c->name_in, -1, NULL);
		sqlite3_bind_int64(stmt, 5, now);
	} else {
		sqlite3_bind_int64(stmt, 2, c->updated_by);
		sqlite3_bind_int64(stmt, 3, now);
		sqlite3_bind_int64(stmt, 4, c->chat_id);
		sqlite3_bind_text(stmt, 5, c->name_in, -1, NULL);
	}

	ret = _sqlite_step_one(stmt);
	if (ret < 0)
		goto out1;

	ret = sqlite3_changes(conn->sql);

out1:
	sqlite3_finalize(stmt);
out0:
	sqlite_pool_put(conn);
	return ret;
}


int
model_cmd_message_get_value(int64_t chat_id, const char name[], char value[], size_t value_len)
{
	const Data args[] = {
		{ .type = _DATA_TYPE_INT64, .int64 = &chat_id },
		{ .type = _DATA_TYPE_TEXT, .text = (DataText) { .value = (char *)name } },
	};

	const char *const query = "SELECT value FROM Cmd_Message WHERE (chat_id = ?) AND (name = ?);";
	const Data out = {
		.type = _DATA_TYPE_TEXT,
		.text = (DataText) { .value = value, .size = value_len },
	};

	return _sqlite_exec_one(query, args, LEN(args), &out);
}


int
model_cmd_message_is_exists(const char name[])
{
	const Data arg = { .type = _DATA_TYPE_TEXT, .text = (DataText) { .value = (char *)name } };
	const char *const query = "SELECT 1 FROM Cmd_Message WHERE (name = ?);";

	int is_exists = 0;
	const Data out = { .type = _DATA_TYPE_INT, .int_ = &is_exists };
	if (_sqlite_exec_one(query, &arg, 1, &out) < 0)
		return -1;

	return is_exists;
}


/*
 * ModelSchedMessage
 */
int
model_sched_message_get_list(ModelSchedMessage *list[], int len, time_t now)
{
	int ret = -1;
	sqlite3_stmt *stmt;

	DbConn *const conn = sqlite_pool_get();
	if (conn == NULL)
		return -1;

	const char *const query =
		"SELECT id, type, chat_id, message_id, value, next_run, expire "
		"FROM Sched_Message "
		"WHERE (? >= next_run) AND (? < (next_run + expire)) "
		"LIMIT ?";

	int res = sqlite3_prepare_v2(conn->sql, query, -1, &stmt, NULL);
	if (res != SQLITE_OK) {
		log_err(0, "model: model_sched_message_get_list: sqlite3_prepare_v2: %s", sqlite3_errstr(res));
		goto out0;
	}

	sqlite3_bind_int64(stmt, 1, now);
	sqlite3_bind_int64(stmt, 2, now);
	sqlite3_bind_int(stmt, 3, len);

	int count = 0;
	for (; count < len; count++) {
		res = sqlite3_step(stmt);
		if (res == SQLITE_DONE)
			break;

		if (res != SQLITE_ROW) {
			log_err(0, "model: model_sched_message_get_list: sqlite3_step: %s", sqlite3_errstr(res));
			goto out1;
		}

		ModelSchedMessage *const it = malloc(sizeof(ModelSchedMessage));
		if (it == NULL)
			goto out1;

		it->id = sqlite3_column_int(stmt, 0);
		it->type = sqlite3_column_int(stmt, 1);
		it->chat_id = sqlite3_column_int64(stmt, 2);
		it->message_id = sqlite3_column_int64(stmt, 3);
		cstr_copy_n(it->value, LEN(it->value), (const char *)sqlite3_column_text(stmt, 4));
		it->next_run = sqlite3_column_int64(stmt, 5);
		it->expire = sqlite3_column_int64(stmt, 6);

		list[count] = it;
	}

	ret = count;

out1:
	sqlite3_finalize(stmt);
	for (int i = 0; (i < count) && (ret < 0); i++)
		free(list[i]);
out0:
	sqlite_pool_put(conn);
	return ret;
}


int
model_sched_message_delete(int32_t list[], int len)
{
	int ret = -1;
	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		return -1;

	if (str_set(&str, "DELETE FROM Sched_Message WHERE id IN (") == NULL)
		goto out0;

	for (int i = 0; i < len; i++) {
		if (str_append_n(&str, "?, ", 3) == NULL)
			goto out0;
	}

	str_pop(&str, 2);
	if (str_append_n(&str, ");", 2) == NULL)
		goto out0;

	DbConn *const conn = sqlite_pool_get();
	if (conn == NULL)
		goto out0;

	sqlite3_stmt *stmt;
	const int res = sqlite3_prepare_v2(conn->sql, str.cstr, str.len, &stmt, NULL);
	if (res != SQLITE_OK) {
		log_err(0, "model: model_sched_message_delete: sqlite3_prepare_v2: %s", sqlite3_errstr(res));
		goto out1;
	}

	for (int i = 0; i < len; i++)
		sqlite3_bind_int(stmt, i + 1, list[i]);

	if (_sqlite_step_one(stmt) < 0)
		goto out2;

	ret = sqlite3_changes(conn->sql);

out2:
	sqlite3_finalize(stmt);
out1:
	sqlite_pool_put(conn);
out0:
	str_deinit(&str);
	return ret;
}


int
model_sched_message_add(const ModelSchedMessage *s, time_t interval_s)
{
	const int type = s->type;
	if ((type < MODEL_SCHED_MESSAGE_TYPE_SEND) || (type >= _MODEL_SCHED_MESSAGE_TYPE_SIZE)) {
		log_err(0, "model: model_sched_message_add: invalid type");
		return -1;
	}

	if (s->expire < 5) {
		log_err(0, "model: model_sched_message_add: invalid expiration time");
		return -1;
	}

	if (interval_s <= 0) {
		log_err(0, "model: model_sched_message_add: invalid interval");
		return -1;
	}

	if ((type == MODEL_SCHED_MESSAGE_TYPE_SEND) && cstr_is_empty(s->value_in)) {
		log_err(0, "model: model_sched_message_add: value is empty");
		return -1;
	}

	int64_t next_run = time(NULL) + interval_s;
	Data args[] = {
		{ .type = _DATA_TYPE_INT, .int_ = (int *)&s->type },
		{ .type = _DATA_TYPE_INT64, .int64 = (int64_t *)&s->chat_id },
		{ .type = _DATA_TYPE_INT64, .int64 = (int64_t *)&s->message_id },
		{ .type = _DATA_TYPE_TEXT, .text = (DataText) { .value = (char *)s->value_in } },
		{ .type = _DATA_TYPE_INT64, .int64 = &next_run },
		{ .type = _DATA_TYPE_INT64, .int64 = (int64_t *)&s->expire },
	};

	if (type != MODEL_SCHED_MESSAGE_TYPE_SEND)
		args[3].type = _DATA_TYPE_NULL;

	const char *const query =
		"INSERT INTO Sched_Message(type, chat_id, message_id, value, next_run, expire) "
		"VALUES(?, ?, ?, ?, ?, ?);";

	return _sqlite_exec_one(query, args, LEN(args), NULL);
}


/*
 * Private
 */
static const char *
_chat_query(Str *str)
{
	return str_set(str,
		"CREATE TABLE IF NOT EXISTS Chat(\n"
		"	id		INTEGER PRIMARY KEY AUTOINCREMENT,\n"
		"	chat_id		BIGINT NOT Null,\n"
		"	flags		INTEGER NOT Null,\n"
		"	created_at	TIMESTAMP NOT Null\n"
		");"
	);
}


static const char *
_admin_query(Str *str)
{
	return str_set(str,
		"CREATE TABLE IF NOT EXISTS Admin(\n"
		"	id		INTEGER PRIMARY KEY AUTOINCREMENT,\n"
		"	chat_id		BIGINT NOT Null,\n"
		"	user_id		BIGINT NOT Null,\n"
		"	is_anonymous	BOOLEAN NOT Null,\n"
		"	privileges	INTEGER NOT NUll,\n"
		"	created_at	TIMESTAMP NOT Null\n"
		");"
	);
}


static const char *
_cmd_builtin_query(Str *str)
{
	return str_set_fmt(str,
		"CREATE TABLE IF NOT EXISTS Cmd_Builtin(\n"
		"	id		INTEGER PRIMARY KEY AUTOINCREMENT,\n"
		"	idx		INTEGER NOT Null,\n"
		"	flags		INTEGER NOT Null,\n"
		"	name		VARCHAR(%d) NOT Null,\n"
		"	description	VARCHAR(%d) NOT Null\n"
		");",
		(MODEL_CMD_NAME_SIZE - 1), (MODEL_CMD_DESC_SIZE - 1)
	);
}


static const char *
_cmd_extern_query(Str *str)
{
	return str_set_fmt(str,
		"CREATE TABLE IF NOT EXISTS Cmd_Extern(\n"
		"	id		INTEGER PRIMARY KEY AUTOINCREMENT,\n"
		"	is_enable	BOOLEAN NOT Null,\n"
		"	flags		INTEGER NOT Null,\n"
		"	name		VARCHAR(%d) NOT Null,\n"
		"	file_name	VARCHAR(%d) NOT Null,\n"
		"	description	VARCHAR(%d) NOT Null\n"
		");",
		(MODEL_CMD_NAME_SIZE - 1), (MODEL_CMD_EXTERN_FILE_NAME_SIZE - 1),
		(MODEL_CMD_DESC_SIZE - 1)
	);
}


static const char *
_cmd_message_query(Str *str)
{
	return str_set_fmt(str,
		"CREATE TABLE IF NOT EXISTS Cmd_Message(\n"
		"	id		INTEGER PRIMARY KEY AUTOINCREMENT,\n"
		"	chat_id		BIGINT NOT Null,\n"
		"	name		VARCHAR(%d) NOT Null,\n"
		"	value		VARCHAR(%d) Null,\n"
		"	created_by	BIGINT NOT Null,\n"
		"	created_at	TIMESTAMP NOT Null,\n"
		"	updated_by	BIGINT,\n"
		"	updated_at	TIMESTAMP\n"
		");",
		(MODEL_CMD_MESSAGE_NAME_SIZE - 1), (MODEL_CMD_MESSAGE_VALUE_SIZE - 1)
	);
}


static const char *
_sched_message_query(Str *str)
{
	return str_set_fmt(str,
		"CREATE TABLE IF NOT EXISTS Sched_Message("
		"	id		INTEGER PRIMARY KEY AUTOINCREMENT,\n"
		"	type		INTEGER NOT Null,\n"
		"	chat_id		BIGINT NOT Null,\n"
		"	message_id	BIGINT Null,\n"
		"	value		VARCHAR(%d) Null,\n"
		"	next_run	TIMESTAMP NOT Null,\n"
		"	expire		TIMESTAMP NOT Null\n"
		");",
		(MODEL_SCHED_MESSAGE_VALUE_SIZE - 1)
	);
}


static int
_sqlite_step_one(sqlite3_stmt *stmt)
{
	int ret = sqlite3_step(stmt);
	if (ret == SQLITE_DONE)
		return 0;

	if (ret != SQLITE_ROW) {
		log_err(0, "model: _sqlite_step_one: sqlite3_step: %s", sqlite3_errstr(ret));
		return -1;
	}

	return 1;
}


static int
_sqlite_exec_one(const char query[], const Data args[], int args_len, const Data *out)
{
	int ret = -1;
	sqlite3_stmt *stmt;

	DbConn *const conn = sqlite_pool_get();
	if (conn == NULL)
		return -1;

	const int res = sqlite3_prepare_v2(conn->sql, query, -1, &stmt, NULL);
	if (res != SQLITE_OK) {
		log_err(0, "model: _sqlite_exec_one: sqlite3_prepare_v2: %s", sqlite3_errstr(ret));
		goto out0;
	}

	for (int i = 0, j = 1; i < args_len; i++, j++) {
		const Data *const a = &args[i];
		switch (a->type) {
		case _DATA_TYPE_INT:
			sqlite3_bind_int(stmt, j, *a->int_);
			break;
		case _DATA_TYPE_INT64:
			sqlite3_bind_int64(stmt, j, *a->int64);
			break;
		case _DATA_TYPE_TEXT:
			sqlite3_bind_text(stmt, j, a->text.value, -1, NULL);
			break;
		case _DATA_TYPE_NULL:
			sqlite3_bind_null(stmt, j);
			break;
		default:
			log_err(0, "model: _sqlite_exec_one: arg: invalid type: [%d]:%d", i, a->type);
			goto out1;
		}
	}

	ret = _sqlite_step_one(stmt);
	if (ret < 0)
		goto out1;

	if (out == NULL) {
		ret = sqlite3_changes(conn->sql);
		goto out1;
	}

	if (ret == 0)
		goto out1;

	switch (out->type) {
	case _DATA_TYPE_INT:
		*out->int_ = sqlite3_column_int(stmt, 0);
		break;
	case _DATA_TYPE_INT64:
		*out->int64 = sqlite3_column_int64(stmt, 0);
		break;
	case _DATA_TYPE_TEXT:
		cstr_copy_n(out->text.value, out->text.size, (const char *)sqlite3_column_text(stmt, 0));
		break;
	default:
		log_err(0, "model: _sqlite_exec_one: out: invalid type: %d", out->type);
		ret = -1;
		break;
	}

out1:
	sqlite3_finalize(stmt);
out0:
	sqlite_pool_put(conn);
	return ret;
}
