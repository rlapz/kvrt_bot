#include <errno.h>
#include <sqlite3.h>

#include "model.h"

#include "ev.h"
#include "sqlite_pool.h"
#include "util.h"


static const char *_chat_query(Str *str);
static const char *_admin_query(Str *str);
static const char *_sched_message_query(Str *str);
static const char *_cmd_extern_query(Str *str);
static const char *_cmd_extern_disabled_query(Str *str);
static const char *_cmd_message_query(Str *str);
static int         _sqlite_step_one(sqlite3_stmt *stmt);


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
		{ "Sched_Message", _sched_message_query },
		{ "Cmd_Extern", _cmd_extern_query },
		{ "Cmd_Extern_Disabled", _cmd_extern_disabled_query },
		{ "Cmd_Message", _cmd_message_query },
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
	sqlite_pool_put(conn);
	str_deinit(&str);
	return ret;
}


/*
 * ModelChat
 */
int
model_chat_get_flags(int64_t chat_id)
{
	int flags = 0;
	sqlite3_stmt *stmt;

	DbConn *const conn = sqlite_pool_get();
	if (conn == NULL)
		return -1;

	const char *const query = "SELECT flags FROM Chat WHERE (chat_id = ?);";
	const int ret = sqlite3_prepare_v2(conn->sql, query, -1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		log_err(0, "model: model_chat_get_flags: sqlite3_prepare_v2: %s", sqlite3_errstr(ret));
		goto out0;
	}

	sqlite3_bind_int64(stmt, 1, chat_id);
	if (_sqlite_step_one(stmt) <= 0)
		goto out1;

	flags = sqlite3_column_int(stmt, 0);

out1:
	sqlite3_finalize(stmt);
out0:
	sqlite_pool_put(conn);
	return flags;
}


/*
 * ModelAdmin
 */
static int
_admin_add(DbConn *conn, const ModelAdmin list[], int len)
{
	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		return -1;

	int ret = -1;
	if (str_set(&str, "INSERT INTO Admin(chat_id, user_id, is_anonymous, privileges) VALUES ") == NULL)
		goto out0;

	for (int i = 0; i < len; i++) {
		if (str_append_fmt(&str, "(?, ?, ?, ?), ") == NULL)
			goto out0;
	}

	str_pop(&str, 2);

	sqlite3_stmt *stmt;
	const int res = sqlite3_prepare_v2(conn->sql, str.cstr, str.len, &stmt, NULL);
	if (res != SQLITE_OK) {
		log_err(0, "model: _admin_add: sqlite3_prepare_v2: %s", sqlite3_errstr(res));
		goto out0;
	}

	for (int i = 0, j = 1; i < len; i++) {
		sqlite3_bind_int64(stmt, j++, list[i].chat_id);
		sqlite3_bind_int64(stmt, j++, list[i].user_id);
		sqlite3_bind_int(stmt, j++, list[i].is_anonymous);
		sqlite3_bind_int(stmt, j++, list[i].privileges);
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
model_admin_get_privilegs(int64_t chat_id, int64_t user_id)
{
	int privs = 0;
	sqlite3_stmt *stmt;

	DbConn *const conn = sqlite_pool_get();
	if (conn == NULL)
		return -1;

	const char *const query =
		"SELECT privileges "
		"FROM Admin "
		"WHERE (chat_id = ?) AND (user_id = ?) "
		"ORDER BY id DESC LIMIT 1;";

	const int ret = sqlite3_prepare_v2(conn->sql, query, -1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		log_err(0, "model: model_chat_get_flags: sqlite3_prepare_v2: %s", sqlite3_errstr(ret));
		goto out0;
	}

	sqlite3_bind_int64(stmt, 1, chat_id);
	sqlite3_bind_int64(stmt, 2, user_id);
	if (_sqlite_step_one(stmt) <= 0)
		goto out1;

	privs = sqlite3_column_int(stmt, 0);

out1:
	sqlite3_finalize(stmt);
out0:
	sqlite_pool_put(conn);
	return privs;
}


/*
 * ModelSchedMessage
 */
int
model_sched_message_get_list(ModelSchedMessage *list[], int len, int64_t now)
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
		cstr_copy_n(it->value_out, sizeof(it->value_out), (const char *)sqlite3_column_text(stmt, 4));
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
model_sched_message_del(int32_t list[], int len, int64_t now)
{
	int ret = -1;
	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		return -1;

	if (str_set(&str, "DELETE FROM Sched_Message WHERE (? >= next_run) AND id IN (") == NULL)
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
		log_err(0, "model: model_sched_message_del: sqlite3_prepare_v2: %s", sqlite3_errstr(res));
		goto out1;
	}

	int param_count = 1;
	sqlite3_bind_int64(stmt, param_count++, now);
	for (int i = 0; i < len; i++)
		sqlite3_bind_int(stmt, param_count++, list[i]);

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
model_sched_message_send(const ModelSchedMessage *s, int64_t interval_s)
{
	if (cstr_is_empty(s->value_in)) {
		log_err(0, "model: model_sched_message_send: value is empty");
		return -1;
	}

	if (s->expire < 5) {
		log_err(0, "model: model_sched_message_send: invalid expiration time");
		return -1;
	}

	if (interval_s <= 0) {
		log_err(0, "model: model_sched_message_send: invalid interval");
		return -1;
	}

	int ret = -1;
	sqlite3_stmt *stmt;

	DbConn *const conn = sqlite_pool_get();
	if (conn == NULL)
		return -1;

	const char *const query =
		"INSERT INTO Sched_Message(type, chat_id, message_id, value, next_run, expire) "
		"VALUES(?, ?, ?, ?, ?, ?);";

	const int res = sqlite3_prepare_v2(conn->sql, query, -1, &stmt, NULL);
	if (res != SQLITE_OK) {
		log_err(0, "model: model_sched_message_send: sqlite3_prepare_v2: %s", sqlite3_errstr(res));
		goto out0;
	}

	int i = 1;
	sqlite3_bind_int(stmt, i++, MODEL_SCHED_MESSAGE_TYPE_SEND);
	sqlite3_bind_int64(stmt, i++, s->chat_id);
	sqlite3_bind_int64(stmt, i++, s->message_id);
	sqlite3_bind_text(stmt, i++, s->value_in, -1, NULL);
	sqlite3_bind_int64(stmt, i++, time(NULL) + interval_s);
	sqlite3_bind_int64(stmt, i, s->expire);

	if (_sqlite_step_one(stmt) < 0)
		goto out1;

	ret = sqlite3_changes(conn->sql);

out1:
	sqlite3_finalize(stmt);
out0:
	sqlite_pool_put(conn);
	return ret;
}


int
model_sched_message_delete(const ModelSchedMessage *s, int64_t interval_s)
{
	if (s->expire < 5) {
		log_err(0, "model: model_sched_message_delete: invalid expiration time");
		return -1;
	}

	if (interval_s <= 0) {
		log_err(0, "model: model_sched_message_delete: invalid interval");
		return -1;
	}

	int ret = -1;
	sqlite3_stmt *stmt;

	DbConn *const conn = sqlite_pool_get();
	if (conn == NULL)
		return -1;

	const char *const query =
		"INSERT INTO Sched_Message(type, chat_id, message_id, next_run, expire) "
		"VALUES(?, ?, ?, ?, ?);";

	const int res = sqlite3_prepare_v2(conn->sql, query, -1, &stmt, NULL);
	if (res != SQLITE_OK) {
		log_err(0, "model: model_sched_message_delete: sqlite3_prepare_v2: %s", sqlite3_errstr(res));
		goto out0;
	}

	int i = 1;
	sqlite3_bind_int(stmt, i++, MODEL_SCHED_MESSAGE_TYPE_DELETE);
	sqlite3_bind_int64(stmt, i++, s->chat_id);
	sqlite3_bind_int64(stmt, i++, s->message_id);
	sqlite3_bind_int64(stmt, i++, time(NULL) + interval_s);
	sqlite3_bind_int64(stmt, i, s->expire);

	if (_sqlite_step_one(stmt) < 0)
		goto out1;

	ret = sqlite3_changes(conn->sql);

out1:
	sqlite3_finalize(stmt);
out0:
	sqlite_pool_put(conn);
	return ret;
}


/*
 * ModelCmdExtern
 */
int
model_cmd_extern_get_one(ModelCmdExtern *c, int64_t chat_id, const char name[])
{
	int ret = -1;
	sqlite3_stmt *stmt;

	DbConn *const conn = sqlite_pool_get();
	if (conn == NULL)
		return -1;

	const char *const query =
		"SELECT id, is_enable, flags, name, file_name, description "
		"FROM Cmd_Extern "
		"WHERE (name = ?) AND (name NOT IN ( "
		"	SELECT name "
		"	FROM Cmd_Extern_Disabled "
		"	WHERE (chat_id = ?) "
		"));";

	const int res = sqlite3_prepare_v2(conn->sql, query, -1, &stmt, NULL);
	if (res != SQLITE_OK) {
		log_err(0, "model: model_cmd_extern_get_one: sqlite3_prepare_v2: %s", sqlite3_errstr(res));
		goto out0;
	}

	sqlite3_bind_text(stmt, 1, name, -1, NULL);
	sqlite3_bind_int64(stmt, 2, chat_id);
	if (_sqlite_step_one(stmt) <= 0)
		goto out1;

	c->id = sqlite3_column_int(stmt, 0);
	c->is_enable = sqlite3_column_int(stmt, 1);
	c->flags = sqlite3_column_int(stmt, 2);
	cstr_copy_n(c->name_out, sizeof(c->name_out), (const char *)sqlite3_column_text(stmt, 3));
	cstr_copy_n(c->file_name_out, sizeof(c->file_name_out), (const char *)sqlite3_column_text(stmt, 4));
	cstr_copy_n(c->description_out, sizeof(c->description_out), (const char *)sqlite3_column_text(stmt, 5));

	ret = 1;

out1:
	sqlite3_finalize(stmt);
out0:
	sqlite_pool_put(conn);
	return ret;
}


int
model_cmd_extern_get_list(ModelCmdExtern **ret_list[], int start, int len)
{
	(void)ret_list;
	(void)start;
	(void)len;
	return 0;
}


int
model_cmd_extern_is_exists(const char name[])
{
	int is_exists = 0;
	sqlite3_stmt *stmt;

	DbConn *const conn = sqlite_pool_get();
	if (conn == NULL)
		return -1;

	const char *const query = "SELECT 1 FROM Cmd_Extern WHERE (name = ?);";
	const int ret = sqlite3_prepare_v2(conn->sql, query, -1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		log_err(0, "model: model_cmd_extern_is_exists: sqlite3_prepare_v2: %s", sqlite3_errstr(ret));
		goto out0;
	}

	sqlite3_bind_text(stmt, 1, name, -1, NULL);
	if (_sqlite_step_one(stmt) <= 0)
		goto out1;

	is_exists = sqlite3_column_int(stmt, 0);

out1:
	sqlite3_finalize(stmt);
out0:
	sqlite_pool_put(conn);
	return is_exists;
}


/*
 * ModelCmdExternDisabled
 */
int
model_cmd_extern_disabled_setup(int64_t chat_id)
{
	int ret = -1;
	sqlite3_stmt *stmt;

	DbConn *const conn = sqlite_pool_get();
	if (conn == NULL)
		return -1;

	const char *query = "SELECT 1 FROM Cmd_Extern_Disabled WHERE (chat_id = ?);";
	int res = sqlite3_prepare_v2(conn->sql, query, -1, &stmt, NULL);
	if (res != SQLITE_OK) {
		log_err(0, "model: model_cmd_extern_disabled_setup: sqlite3_prepare_v2: %s", sqlite3_errstr(res));
		goto out0;
	}

	sqlite3_bind_int64(stmt, 1, chat_id);
	ret = _sqlite_step_one(stmt);
	if (ret < 0)
		goto out1;

	/* already registered  */
	if (ret > 0)
		goto out1;

	sqlite3_finalize(stmt);

	/* disable all NSFW Cmd(s) */
	query = "INSERT INTO Cmd_Extern_Disabled(name, chat_id) "
		"SELECT name, ? "
		"FROM Cmd_Extern "
		"WHERE ((flags & ?) != 0);";

	res = sqlite3_prepare_v2(conn->sql, query, -1, &stmt, NULL);
	if (res != SQLITE_OK) {
		log_err(0, "model: model_cmd_extern_disabled_setup: sqlite3_prepare_v2: %s", sqlite3_errstr(res));
		goto out0;
	}

	sqlite3_bind_int64(stmt, 1, chat_id);
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
		query = "INSERT INTO Cmd_Message(value, created_by, chat_id, name) "
			"VALUES(?, ?, ?, ?);";
	} else {
		query = "UPDATE Cmd_Message "
			"	SET value = ?, updated_at = UNIXEPOCH(), "
			"	    updated_by = ? "
			"WHERE (chat_id = ?) and (name = ?);";
	}

	res = sqlite3_prepare_v2(conn->sql, query, -1, &stmt, NULL);
	if (res != SQLITE_OK) {
		log_err(0, "model: model_cmd_message_set: sqlite3_prepare_v2: %s", sqlite3_errstr(res));
		goto out0;
	}

	sqlite3_bind_text(stmt, 1, c->value_in, -1, NULL);
	if (ret == 0)
		sqlite3_bind_int64(stmt, 2, c->created_by);
	else
		sqlite3_bind_int64(stmt, 2, c->updated_by);

	sqlite3_bind_int64(stmt, 3, c->chat_id);
	sqlite3_bind_text(stmt, 4, c->name_in, -1, NULL);
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
	int ret = -1;
	sqlite3_stmt *stmt;

	DbConn *const conn = sqlite_pool_get();
	if (conn == NULL)
		return -1;

	const char *const query = "SELECT value FROM Cmd_Message WHERE (chat_id = ?) AND (name = ?);";
	const int res = sqlite3_prepare_v2(conn->sql, query, -1, &stmt, NULL);
	if (res != SQLITE_OK) {
		log_err(0, "model: model_cmd_message_is_exists: sqlite3_prepare_v2: %s", sqlite3_errstr(res));
		goto out0;
	}

	sqlite3_bind_int64(stmt, 1, chat_id);
	sqlite3_bind_text(stmt, 2, name, -1, NULL);
	ret = _sqlite_step_one(stmt);
	if (ret <= 0)
		goto out1;

	cstr_copy_n(value, value_len, (const char *)sqlite3_column_text(stmt, 0));

out1:
	sqlite3_finalize(stmt);
out0:
	sqlite_pool_put(conn);
	return ret;
}


int
model_cmd_message_is_exists(const char name[])
{
	int is_exists = 0;
	sqlite3_stmt *stmt;

	DbConn *const conn = sqlite_pool_get();
	if (conn == NULL)
		return -1;

	const char *const query = "SELECT 1 FROM Cmd_Message WHERE (name = ?);";
	const int ret = sqlite3_prepare_v2(conn->sql, query, -1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		log_err(0, "model: model_cmd_message_is_exists: sqlite3_prepare_v2: %s", sqlite3_errstr(ret));
		goto out0;
	}

	sqlite3_bind_text(stmt, 1, name, -1, NULL);
	if (_sqlite_step_one(stmt) <= 0)
		goto out1;

	is_exists = sqlite3_column_int(stmt, 0);

out1:
	sqlite3_finalize(stmt);
out0:
	sqlite_pool_put(conn);
	return is_exists;
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
		"	created_at	TIMESTAMP DEFAULT (UNIXEPOCH()) NOT Null,\n"
		"	created_by	BIGINT NOT Null\n"
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
		"	created_at	TIMESTAMP DEFAULT (UNIXEPOCH()) NOT Null\n"
		");"
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
		"	message_id	BIGINT,\n"
		"	value		VARCHAR(%d),\n"
		"	next_run	TIMESTAMP NOT Null,\n"
		"	expire		TIMESTAMP NOT Null\n"
		");",
		MODEL_SCHED_MESSAGE_VALUE_SIZE
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
		"	args		INTEGER NOT Null,\n"
		"	name		VARCHAR(%d) NOT Null,\n"
		"	file_name	VARCHAR(%d) NOT Null,\n"
		"	description	VARCHAR(%d) NOT Null\n"
		");",
		MODEL_CMD_NAME_SIZE, MODEL_CMD_EXTERN_FILENAME_SIZE, MODEL_CMD_DESC_SIZE
	);
}


static const char *
_cmd_extern_disabled_query(Str *str)
{
	return str_set_fmt(str,
		"CREATE TABLE IF NOT EXISTS Cmd_Extern_Disabled(\n"
		"	id		INTEGER PRIMARY KEY AUTOINCREMENT,\n"
		"	chat_id		BIGINT NOT Null,\n"
		"	name		VARCHAR(%d) NOT Null,\n"
		"	created_at	TIMESTAMP DEFAULT (UNIXEPOCH()) NOT Null\n"
		");",
		MODEL_CMD_NAME_SIZE
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
		"	value		VARCHAR(%d),\n"
		"	created_by	BIGINT NOT Null,\n"
		"	created_at	TIMESTAMP DEFAULT (UNIXEPOCH()) NOT Null,\n"
		"	updated_by	BIGINT,\n"
		"	updated_at	BIGINT\n"
		");",
		MODEL_CMD_MESSAGE_NAME_SIZE, MODEL_CMD_MESSAGE_VALUE_SIZE
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
