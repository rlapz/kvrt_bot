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
	int len;
	int size;
	union {
		const char *value_in;
		char       *value_out;
	};
} DataText;

typedef struct data {
	int type;
	union {
		int       int_in;
		int64_t   int64_in;
		int      *int_out;
		int64_t  *int64_out;
		DataText  text;
	};
} Data;

static const char *_enable_wal_mode(Str *str);
static const char *_chat_query(Str *str);
static const char *_admin_query(Str *str);
static const char *_cmd_builtin_query(Str *str);
static const char *_cmd_extern_query(Str *str);
static const char *_cmd_message_query(Str *str);
static const char *_cmd_session_query(Str *str);
static const char *_sched_message_query(Str *str);
static const char *_anime_sched_query(Str *str);
static int         _sqlite_step_one(sqlite3_stmt *stmt);
static int         _sqlite_step_one_wait(sqlite3 *sql, sqlite3_stmt *stmt);
static int         _sqlite_exec_one(const char query[], const Data args[], int args_len,
				    Data *out);


/*
 * Public
 */
int
model_init(void)
{
	Str str;
	int ret = str_init_alloc(&str, 0);
	if (ret < 0) {
		LOG_ERR(ret, "model", "%s", "str_init_alloc");
		return -1;
	}

	char *err_msg;
	struct {
		const char *table_name;
		const char *(*func)(Str *);
	} params[] = {
		{ "WAL", _enable_wal_mode },
		{ "Chat", _chat_query },
		{ "Admin", _admin_query },
		{ "Cmd_Builtin", _cmd_builtin_query },
		{ "Cmd_Extern", _cmd_extern_query },
		{ "Cmd_Message", _cmd_message_query },
		{ "Cmd_Session", _cmd_session_query },
		{ "Sched_Message", _sched_message_query },
		{ "Anime_Sched", _anime_sched_query },
	};

	DbConn *const conn = sqlite_pool_get();
	for (size_t i = 0; i < LEN(params); i++) {
		const char *const query = params[i].func(&str);
		ret = sqlite3_exec(conn->sql, query, NULL, NULL, &err_msg);
		if (ret != SQLITE_OK) {
			const char *const table_name = params[i].table_name;
			LOG_ERRN("model", "sqlite3_exec: %s: %s", table_name, err_msg);
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
	const int flags = MODEL_CHAT_FLAG_ALLOW_CMD_EXTRA | MODEL_CHAT_FLAG_ALLOW_CMD_EXTERN;
	const Data args[] = {
		{ .type = _DATA_TYPE_INT64, .int64_in = chat_id },
		{ .type = _DATA_TYPE_INT, .int_in = flags },
		{ .type = _DATA_TYPE_INT64, .int64_in = time(NULL) },
	};

	int fl = 0;
	Data out = { .type = _DATA_TYPE_INT, .int_out = &fl };
	const char *query = "SELECT 1 FROM Chat WHERE (chat_id = ?);";
	const int ret = _sqlite_exec_one(query, args, 1, &out);
	if (ret < 0)
		return -1;

	if (ret > 0)
		return 0;

	query = "INSERT INTO Chat(chat_id, flags, created_at) VALUES(?, ?, ?);";
	return _sqlite_exec_one(query, args, LEN(args), NULL);
}


int
model_chat_set_flags(int64_t chat_id, int flags)
{
	const Data args[] = {
		{ .type = _DATA_TYPE_INT, .int_in = flags },
		{ .type = _DATA_TYPE_INT64, .int64_in = chat_id },
	};

	const char *const query = "UPDATE Chat SET flags = ? WHERE (chat_id = ?);";
	return _sqlite_exec_one(query, args, LEN(args), NULL);
}


int
model_chat_get_flags(int64_t chat_id)
{
	const Data arg = { .type = _DATA_TYPE_INT64, .int64_in = chat_id };
	const char *const query = "SELECT flags FROM Chat WHERE (chat_id = ?);";

	int flags = 0;
	Data out = { .type = _DATA_TYPE_INT, .int_out = &flags };
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
		LOG_ERRN("model", "sqlite3_prepare_v2: %s", sqlite3_errstr(res));
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
		LOG_ERRN("model", "sqlite3_prepare_v2: %s", sqlite3_errstr(res));
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
		{ .type = _DATA_TYPE_INT64, .int64_in = chat_id },
		{ .type = _DATA_TYPE_INT64, .int64_in = user_id },
	};

	int privs = 0;
	Data out = { .type = _DATA_TYPE_INT, .int_out = &privs };
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

	int flags = MODEL_CMD_FLAG_HIDDEN;
	if (is_private)
		flags |= MODEL_CMD_FLAG_DISALLOW_PRIVATE_CHAT;
	if ((chat_flags & MODEL_CHAT_FLAG_ALLOW_CMD_EXTRA) == 0)
		flags |= MODEL_CMD_FLAG_EXTRA;
	if ((chat_flags & MODEL_CHAT_FLAG_ALLOW_CMD_NSFW) == 0)
		flags |= MODEL_CMD_FLAG_NSFW;

	int res = sqlite3_prepare_v2(conn->sql, query, -1, &stmt, NULL);
	if (res != SQLITE_OK) {
		LOG_ERRN("model", "sqlite3_prepare_v2: %s", sqlite3_errstr(res));
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
			LOG_ERRN("model", "sqlite3_step: %s", sqlite3_errstr(res));
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
		{ .type = _DATA_TYPE_INT, .int_in = c->idx },
		{ .type = _DATA_TYPE_INT, .int_in = c->flags },
		{ .type = _DATA_TYPE_TEXT, .text = (DataText) { .value_in = c->name_in, .len = -1 } },
		{ .type = _DATA_TYPE_TEXT, .text = (DataText) { .value_in = c->description_in, .len = -1 } },
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
		LOG_ERRN("model", "sqlite3_prepare_v2: %s", sqlite3_errstr(res));
		goto out0;
	}

	ret = _sqlite_step_one_wait(conn->sql, stmt);
	if (ret < 0)
		goto out1;

	sqlite3_finalize(stmt);

	query = "DELETE FROM Cmd_Builtin;";
	res = sqlite3_prepare_v2(conn->sql, query, -1, &stmt, NULL);
	if (res != SQLITE_OK) {
		LOG_ERRN("model", "sqlite3_prepare_v2: %s", sqlite3_errstr(res));
		goto out0;
	}

	ret = _sqlite_step_one_wait(conn->sql, stmt);
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
	const Data arg = { .type = _DATA_TYPE_TEXT, .text = (DataText) { .value_in = name, .len = -1 } };
	const char *const query = "SELECT idx FROM Cmd_Builtin WHERE (name = ?);";

	int index = 0;
	Data out = { .type = _DATA_TYPE_INT, .int_out = &index };
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
	const Data arg = { .type = _DATA_TYPE_TEXT, .text = (DataText) { .value_in = name, .len = -1 } };
	const char *const query = "SELECT 1 FROM Cmd_Builtin WHERE (name = ?);";

	int is_exists = 0;
	Data out = { .type = _DATA_TYPE_INT, .int_out = &is_exists };
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
		LOG_ERRN("model", "sqlite3_prepare_v2: %s", sqlite3_errstr(res));
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
	const Data arg = { .type = _DATA_TYPE_TEXT, .text = (DataText) { .value_in = name, .len = -1 } };
	const char *const query = "SELECT 1 FROM Cmd_Extern WHERE (name = ?);";

	int is_exists = 0;
	Data out = { .type = _DATA_TYPE_INT, .int_out = &is_exists };
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
		LOG_ERRN("model", "sqlite3_prepare_v2: %s", sqlite3_errstr(res));
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
		LOG_ERRN("model", "sqlite3_prepare_v2: %s", sqlite3_errstr(res));
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

	ret = _sqlite_step_one_wait(conn->sql, stmt);
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
		{ .type = _DATA_TYPE_INT64, .int64_in = chat_id },
		{ .type = _DATA_TYPE_TEXT, .text = (DataText) { .value_in = name, .len = -1 } },
	};

	const char *const query = "SELECT value FROM Cmd_Message WHERE (chat_id = ?) AND (name = ?);";
	Data out = {
		.type = _DATA_TYPE_TEXT,
		.text = (DataText) { .value_out = value, .size = value_len },
	};

	return _sqlite_exec_one(query, args, LEN(args), &out);
}


int
model_cmd_message_is_exists(const char name[])
{
	const Data arg = { .type = _DATA_TYPE_TEXT, .text = (DataText) { .value_in = name, .len = -1 } };
	const char *const query = "SELECT 1 FROM Cmd_Message WHERE (name = ?);";

	int is_exists = 0;
	Data out = { .type = _DATA_TYPE_INT, .int_out = &is_exists };
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
		"SELECT id, type, chat_id, message_id, user_id, value, next_run, expire "
		"FROM Sched_Message "
		"WHERE (? >= next_run) AND (? < (next_run + expire)) "
		"LIMIT ?";

	int res = sqlite3_prepare_v2(conn->sql, query, -1, &stmt, NULL);
	if (res != SQLITE_OK) {
		LOG_ERRN("model", "sqlite3_prepare_v2: %s", sqlite3_errstr(res));
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
			LOG_ERRN("model", "sqlite3_step: %s", sqlite3_errstr(res));
			goto out1;
		}

		ModelSchedMessage *const it = malloc(sizeof(ModelSchedMessage));
		if (it == NULL)
			goto out1;

		it->id = sqlite3_column_int(stmt, 0);
		it->type = sqlite3_column_int(stmt, 1);
		it->chat_id = sqlite3_column_int64(stmt, 2);
		it->message_id = sqlite3_column_int64(stmt, 3);
		it->user_id = sqlite3_column_int64(stmt, 4);
		cstr_copy_n(it->value, LEN(it->value), (const char *)sqlite3_column_text(stmt, 5));
		it->next_run = sqlite3_column_int64(stmt, 6);
		it->expire = sqlite3_column_int64(stmt, 7);

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
		LOG_ERRN("model", "sqlite3_prepare_v2: %s", sqlite3_errstr(res));
		goto out1;
	}

	for (int i = 0; i < len; i++)
		sqlite3_bind_int(stmt, i + 1, list[i]);

	if (_sqlite_step_one_wait(conn->sql, stmt) < 0)
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
		LOG_ERRN("model", "%s", "invalid type");
		return -1;
	}

	if (s->chat_id == 0) {
		LOG_ERRN("model", "%s", "invalid chat_id");
		return -1;
	}

	if (s->user_id == 0) {
		LOG_ERRN("model", "%s", "invalid user_id");
		return -1;
	}

	if ((type == MODEL_SCHED_MESSAGE_TYPE_DELETE) && (s->message_id == 0)) {
		LOG_ERRN("model", "%s", "invalid message_id");
		return -1;
	}

	if (s->expire < 5) {
		LOG_ERRN("model", "%s", "invalid expiration time");
		return -1;
	}

	if (interval_s <= 0) {
		LOG_ERRN("model", "%s", "invalid interval");
		return -1;
	}

	if ((type == MODEL_SCHED_MESSAGE_TYPE_SEND) && cstr_is_empty(s->value_in)) {
		LOG_ERRN("model", "%s", "value is empty");
		return -1;
	}


	Data args[] = {
		{ .type = _DATA_TYPE_INT, .int_in = s->type },
		{ .type = _DATA_TYPE_INT64, .int64_in = s->chat_id },
		{ .type = _DATA_TYPE_INT64, .int64_in = s->message_id },
		{ .type = _DATA_TYPE_INT64, .int64_in = s->user_id },
		{ .type = _DATA_TYPE_TEXT, .text = (DataText) { .value_in = s->value_in, .len = -1 } },
		{ .type = _DATA_TYPE_INT64, .int64_in = (time(NULL) + interval_s) },
		{ .type = _DATA_TYPE_INT64, .int64_in = s->expire },
	};

	if (type != MODEL_SCHED_MESSAGE_TYPE_SEND)
		args[4].type = _DATA_TYPE_NULL;

	const char *const query =
		"INSERT INTO Sched_Message(type, chat_id, message_id, user_id, value, next_run, expire) "
		"VALUES(?, ?, ?, ?, ?, ?, ?);";

	return _sqlite_exec_one(query, args, LEN(args), NULL);
}

/*
 * ModelCmdSession
 */
int
model_cmd_session_add(const ModelCmdSession *c)
{
	if (c->chat_id == 0) {
		LOG_ERRN("model", "%s", "invalid chat_id");
		return -1;
	}

	if (c->user_id == 0) {
		LOG_ERRN("model", "%s", "invalid user_id");
		return -1;
	}

	if (cstr_is_empty(c->ctx)) {
		LOG_ERRN("model", "%s", "ctx is empty");
		return -1;
	}

	int fl = 0;
	Data out = { .type = _DATA_TYPE_INT, .int_out = &fl };
	const Data args[] = {
		{ .type = _DATA_TYPE_INT64, .int64_in = c->chat_id },
		{ .type = _DATA_TYPE_INT64, .int64_in = c->user_id },
		{ .type = _DATA_TYPE_TEXT, .text = (DataText) { .value_in = c->ctx, .len = -1 } },
		{ .type = _DATA_TYPE_INT64, .int64_in = time(NULL) },
	};

	const char *query =
		"SELECT 1 FROM Cmd_Session "
		"WHERE (chat_id = ?) AND (user_id = ?) AND (ctx = LOWER(?)) "
			"AND ((? - created_at) < " MODEL_CMD_SESSION_DEF_EXP ") "
		"ORDER BY id DESC "
		"LIMIT 1;";
	int ret = _sqlite_exec_one(query, args, LEN(args), &out);
	if (ret < 0)
		return -1;
	if (ret > 0)
		return -2;

	query = "INSERT INTO Cmd_Session(chat_id, user_id, ctx, created_at) VALUES(?, ?, LOWER(?), ?);";
	return _sqlite_exec_one(query, args, LEN(args), &out);
}


int
model_cmd_session_delete(const ModelCmdSession *c)
{
	if (c->chat_id == 0) {
		LOG_ERRN("model", "%s", "invalid chat_id");
		return -1;
	}

	if (c->user_id == 0) {
		LOG_ERRN("model", "%s", "invalid user_id");
		return -1;
	}

	if (cstr_is_empty(c->ctx)) {
		LOG_ERRN("model", "%s", "ctx is empty");
		return -1;
	}

	const Data args[] = {
		{ .type = _DATA_TYPE_INT64, .int64_in = c->chat_id },
		{ .type = _DATA_TYPE_INT64, .int64_in = c->user_id },
		{ .type = _DATA_TYPE_TEXT, .text = (DataText) { .value_in = c->ctx, .len = -1 } },
	};

	const char *const query =
		"DELETE FROM Cmd_Session "
		"WHERE (chat_id = ?) AND (user_id = ?) AND (ctx = LOWER(?));";
	return _sqlite_exec_one(query, args, LEN(args), NULL);
}


/*
 * ModelAnimeSched
 */
int
model_anime_sched_delete_by(const char filter[])
{
	const Data arg = { .type = _DATA_TYPE_TEXT, .text = (DataText) { .value_in = filter, .len = -1 } };
	const char *const query = "DELETE FROM Anime_Sched WHERE (filter = ?);";

	return _sqlite_exec_one(query, &arg, 1, NULL);
}


int
model_anime_sched_add_list(const ModelAnimeSched list[], int len)
{
	int ret = -1;
	Str str;
	if (str_init_alloc(&str, 4096) < 0)
		return -1;

	const char *const query =
		"INSERT INTO Anime_Sched(mal_id, filter, url, title, title_ja, title_en, type, "
			"source, broadcast, duration, rating, score, episodes, year, "
			"genres, themes, demographics, created_at "
		") VALUES ";

	if (str_set(&str, query) == 0)
		goto out0;

	for (int i = 0; i < len; i++) {
		if (str_append(&str, "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?), ") == NULL)
			goto out0;
	}

	str_pop(&str, 2);

	DbConn *const conn = sqlite_pool_get();
	if (conn == NULL)
		goto out0;

	sqlite3_stmt *stmt;
	const int res = sqlite3_prepare_v2(conn->sql, str.cstr, str.len, &stmt, NULL);
	if (res != SQLITE_OK) {
		LOG_ERRN("model", "sqlite3_prepare_v2: %s", sqlite3_errstr(res));
		goto out1;
	}

	const time_t now = time(NULL);
	for (int i = 0, j = 1; i < len; i++) {
		sqlite3_bind_int(stmt, j++, list[i].mal_id);
		sqlite3_bind_text(stmt, j++, list[i].filter_in, -1, NULL);
		sqlite3_bind_text(stmt, j++, list[i].url_in, -1, NULL);
		sqlite3_bind_text(stmt, j++, list[i].title_in, -1, NULL);
		sqlite3_bind_text(stmt, j++, list[i].title_japanese_in, -1, NULL);
		sqlite3_bind_text(stmt, j++, list[i].title_english_in, -1, NULL);
		sqlite3_bind_text(stmt, j++, list[i].type_in, -1, NULL);
		sqlite3_bind_text(stmt, j++, list[i].source_in, -1, NULL);
		sqlite3_bind_text(stmt, j++, list[i].broadcast_in, -1, NULL);
		sqlite3_bind_text(stmt, j++, list[i].duration_in, -1, NULL);
		sqlite3_bind_text(stmt, j++, list[i].rating_in, -1, NULL);

		sqlite3_bind_double(stmt, j++, list[i].score);
		sqlite3_bind_int(stmt, j++, list[i].episodes);
		sqlite3_bind_int(stmt, j++, list[i].year);

		sqlite3_bind_text(stmt, j++, list[i].genres_in, -1, NULL);
		sqlite3_bind_text(stmt, j++, list[i].themes_in, -1, NULL);
		sqlite3_bind_text(stmt, j++, list[i].demographics_in, -1, NULL);

		sqlite3_bind_int64(stmt, j++, now);
	}

	ret = _sqlite_step_one(stmt);
	if (ret < 0)
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
model_anime_sched_get_list(ModelAnimeSched list[], int len, const char filter[], int offset, int *total)
{
	int ret = -1;
	const char *const query =
		"SELECT COUNT(1), Null, Null, Null, Null, Null, Null, Null, Null, Null, Null, Null, "
			"Null, Null, Null, Null, Null, Null, Null, Null "
		"FROM Anime_Sched WHERE (filter = ?) "
		"UNION ALL "
		"SELECT * FROM ("
			"SELECT 0, id, mal_id, filter, url, title, title_ja, title_en, type, source, "
				"broadcast, duration, rating, score, episodes, year, genres, themes, "
				"demographics, created_at "
			"FROM Anime_Sched "
			"WHERE (filter = ?) "
			"ORDER BY score DESC "
			"LIMIT ? OFFSET ? "
		");";

	sqlite3_stmt *stmt;
	DbConn *const conn = sqlite_pool_get();
	if (conn == NULL)
		return -1;

	int res = sqlite3_prepare_v2(conn->sql, query, -1, &stmt, NULL);
	if (res != SQLITE_OK) {
		LOG_ERRN("model", "sqlite3_prepare_v2: %s", sqlite3_errstr(res));
		goto out0;
	}

	sqlite3_bind_text(stmt, 1, filter, -1, NULL);
	sqlite3_bind_text(stmt, 2, filter, -1, NULL);
	sqlite3_bind_int(stmt, 3, len);
	sqlite3_bind_int(stmt, 4, offset);

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
			LOG_ERRN("model", "sqlite3_step: %s", sqlite3_errstr(res));
			goto out1;
		}

		int i = 1;
		ModelAnimeSched *const ma = &list[count];
		ma->id = sqlite3_column_int(stmt, i++);
		ma->mal_id = sqlite3_column_int(stmt, i++);
		cstr_copy_n(ma->filter, LEN(ma->filter), (const char *)sqlite3_column_text(stmt, i++));
		cstr_copy_n(ma->url, LEN(ma->url), (const char *)sqlite3_column_text(stmt, i++));
		cstr_copy_n(ma->title, LEN(ma->title), (const char *)sqlite3_column_text(stmt, i++));
		cstr_copy_n(ma->title_japanese, LEN(ma->title_japanese), (const char *)sqlite3_column_text(stmt, i++));
		cstr_copy_n(ma->title_english, LEN(ma->title_english), (const char *)sqlite3_column_text(stmt, i++));
		cstr_copy_n(ma->type, LEN(ma->type), (const char *)sqlite3_column_text(stmt, i++));
		cstr_copy_n(ma->source, LEN(ma->source), (const char *)sqlite3_column_text(stmt, i++));
		cstr_copy_n(ma->broadcast, LEN(ma->broadcast), (const char *)sqlite3_column_text(stmt, i++));
		cstr_copy_n(ma->duration, LEN(ma->duration), (const char *)sqlite3_column_text(stmt, i++));
		cstr_copy_n(ma->rating, LEN(ma->rating), (const char *)sqlite3_column_text(stmt, i++));

		ma->score = sqlite3_column_double(stmt, i++);
		ma->episodes = sqlite3_column_int(stmt, i++);
		ma->year = sqlite3_column_int(stmt, i++);

		cstr_copy_n(ma->genres, LEN(ma->genres), (const char *)sqlite3_column_text(stmt, i++));
		cstr_copy_n(ma->themes, LEN(ma->themes), (const char *)sqlite3_column_text(stmt, i++));
		cstr_copy_n(ma->demographics, LEN(ma->demographics), (const char *)sqlite3_column_text(stmt, i++));

		ma->created_at = sqlite3_column_int64(stmt, i++);
	}

	ret = count;

out1:
	sqlite3_finalize(stmt);
out0:
	sqlite_pool_put(conn);
	return ret;
}


int
model_anime_sched_get_creation_time(const char filter[], time_t *cre_dt)
{
	const Data arg = { .type = _DATA_TYPE_TEXT, .text = (DataText) { .value_in = filter, .len = -1 } };
	const char *const query = "SELECT created_at FROM Anime_Sched WHERE (filter = ?) LIMIT 1;";

	Data out = { .type = _DATA_TYPE_INT64, .int64_out = cre_dt };
	return _sqlite_exec_one(query, &arg, 1, &out);
}


/*
 * Private
 */
static const char *
_enable_wal_mode(Str *str)
{
	return str_set(str, "PRAGMA journal_mode=WAL;");
}


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
_cmd_session_query(Str *str)
{
	return str_set_fmt(str,
		"CREATE TABLE IF NOT EXISTS Cmd_Session(\n"
		"	id		INTEGER PRIMARY KEY AUTOINCREMENT,\n"
		"	chat_id		BIGINT NOT Null,\n"
		"	user_id		BIGINT NOT Null,\n"
		"	ctx		VARCHAR(%d) NOT Null,\n"
		"	created_at	TIMESTAMP NOT Null\n"
		");",
		(MODEL_CMD_NAME_SIZE - 1)
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
		"	user_id		BIGINT NOT Null,\n"
		"	value		VARCHAR(%d) Null,\n"
		"	next_run	TIMESTAMP NOT Null,\n"
		"	expire		TIMESTAMP NOT Null\n"
		");",
		(MODEL_SCHED_MESSAGE_VALUE_SIZE - 1)
	);
}


static const char *
_anime_sched_query(Str *str)
{
	ModelAnimeSched ma;
	return str_set_fmt(str,
		"CREATE TABLE IF NOT EXISTS Anime_Sched("
		"	id		INTEGER PRIMARY KEY AUTOINCREMENT,\n"
		"	mal_id		INTEGER NOT Null,\n"
		"	filter		VARCHAR(%d) Null,\n"
		"	url		VARCHAR(%d) Null,\n"
		"	title		VARCHAR(%d) Null,\n"
		"	title_ja	VARCHAR(%d) Null,\n"
		"	title_en	VARCHAR(%d) Null,\n"
		"	type		VARCHAR(%d) Null,\n"
		"	source		VARCHAR(%d) Null,\n"
		"	broadcast	VARCHAR(%d) Null,\n"
		"	duration	VARCHAR(%d) Null,\n"
		"	rating		VARCHAR(%d) Null,\n"
		"	score		REAL Null,\n"
		"	episodes	INTEGER Null,\n"
		"	year		INTEGER Null,\n"
		"	genres		VARCHAR(%d) Null,\n"
		"	themes		VARCHAR(%d) Null,\n"
		"	demographics	VARCHAR(%d) Null,\n"
		"	created_at	TIMESTAMP NOT Null\n"
		");",
		LEN(ma.filter) -1, LEN(ma.url) -1, LEN(ma.title) -1, LEN(ma.title_japanese) -1,
		LEN(ma.title_english) - 1, LEN(ma.type) -1, LEN(ma.source) -1,
		LEN(ma.broadcast) -1, LEN(ma.duration), LEN(ma.rating) -1, LEN(ma.genres) -1,
		LEN(ma.themes) -1, LEN(ma.demographics) -1
	);
}


static int
_sqlite_step_one(sqlite3_stmt *stmt)
{
	const int ret = sqlite3_step(stmt);
	if (ret == SQLITE_DONE)
		return 0;

	if (ret != SQLITE_ROW) {
		LOG_ERRN("model", "sqlite3_step: %s", sqlite3_errstr(ret));
		return -1;
	}

	return 1;
}


static int
_sqlite_step_one_wait(sqlite3 *sql, sqlite3_stmt *stmt)
{
	while (ev_is_alive()) {
		const int ret = sqlite3_step(stmt);
		switch (ret) {
		case SQLITE_BUSY:
			LOG_INFO("model", "sqlite3_step: %s", sqlite3_errstr(ret));
			sqlite3_busy_timeout(sql, CFG_DB_WAIT);
			continue;
		case SQLITE_DONE: return 0;
		case SQLITE_ROW: return 1;
		}

		LOG_ERRN("model", "sqlite3_step: %s", sqlite3_errstr(ret));
		break;
	}

	return -1;
}


static int
_sqlite_exec_one(const char query[], const Data args[], int args_len, Data *out)
{
	int ret = -1;
	sqlite3_stmt *stmt;

	DbConn *const conn = sqlite_pool_get();
	if (conn == NULL)
		return -1;

	const int res = sqlite3_prepare_v2(conn->sql, query, -1, &stmt, NULL);
	if (res != SQLITE_OK) {
		LOG_ERRN("model", "sqlite3_prepare_v2: %s", sqlite3_errstr(ret));
		goto out0;
	}

	for (int i = 0, j = 1; i < args_len; i++, j++) {
		const Data *const a = &args[i];
		switch (a->type) {
		case _DATA_TYPE_INT:
			sqlite3_bind_int(stmt, j, a->int_in);
			break;
		case _DATA_TYPE_INT64:
			sqlite3_bind_int64(stmt, j, a->int64_in);
			break;
		case _DATA_TYPE_TEXT:
			sqlite3_bind_text(stmt, j, a->text.value_in, a->text.len, NULL);
			break;
		case _DATA_TYPE_NULL:
			sqlite3_bind_null(stmt, j);
			break;
		default:
			LOG_ERRN("model", "arg: invalid type: [%d]:%d", i, a->type);
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

	const int type = sqlite3_column_type(stmt, 0);
	if (type == SQLITE_NULL) {
		out->type = _DATA_TYPE_NULL;
		goto out1;
	}

	switch (out->type) {
	case _DATA_TYPE_INT:
		*out->int_out = sqlite3_column_int(stmt, 0);
		break;
	case _DATA_TYPE_INT64:
		*out->int64_out = sqlite3_column_int64(stmt, 0);
		break;
	case _DATA_TYPE_TEXT:
		out->text.len = cstr_copy_n(out->text.value_out, out->text.size,
					    (const char *)sqlite3_column_text(stmt, 0));
		break;
	default:
		LOG_ERRN("model", "out: invalid type: %d", out->type);
		ret = -1;
		break;
	}

out1:
	sqlite3_finalize(stmt);
out0:
	sqlite_pool_put(conn);
	return ret;
}
