#include <errno.h>
#include <sqlite3.h>

#include "model.h"

#include "db.h"
#include "ev.h"
#include "util.h"


/*
 * Queries
 */
const char *
model_chat_query(Str *str)
{
	return str_set_fmt(str,
		"CREATE TABLE IF NOT EXISTS Chat(\n"
		"	id		INTEGER PRIMARY KEY AUTOINCREMENT,\n"
		"	chat_id		BIGINT NOT Null,\n"
		"	flags		INTEGER NOT Null,\n"
		"	created_at	TIMESTAMP DEFAULT (UNIXEPOCH()) NOT Null,\n"
		"	created_by	BIGINT NOT Null\n"
		");"
	);
}


const char *
model_param_query(Str *str)
{
	return str_set_fmt(str,
		"CREATE TABLE IF NOT EXISTS Param(\n"
		"	id		INTEGER PRIMARY KEY AUTOINCREMENT,\n"
		"	key		VARCHAR(%d) NOT Null,\n"
		"	value0		VARCHAR(%d),\n"
		"	value1		VARCHAR(%d),\n"
		"	value2		VARCHAR(%d),\n"
		"	is_active	BOOLEAN NOT Null,\n"
		"	created_at	TIMESTAMP DEFAULT (UNIXEPOCH()) NOT Null,\n"
		"	updated_at	TIMESTAMP\n"
		");",
		MODEL_PARAM_KEY_SIZE, MODEL_PARAM_VALUE_SIZE, MODEL_PARAM_VALUE_SIZE, MODEL_PARAM_VALUE_SIZE
	);
}


const char *
model_param_chat_query(Str *str)
{
	return str_set_fmt(str,
		"CREATE TABLE IF NOT EXISTS Param_Chat(\n"
		"	id		INTEGER PRIMARY KEY AUTOINCREMENT,\n"
		"	param_id	INTEGER NOT Null,\n"
		"	chat_id		BIGINT NOT Null\n"
		");"
	);
}


const char *
model_admin_query(Str *str)
{
	return str_set_fmt(str,
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


const char *
model_sched_message_query(Str *str)
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


const char *
model_block_list_query(Str *str)
{
	return str_set_fmt(str,
		"CREATE TABLE IF NOT EXISTS Block_List(\n"
		"	id		INTEGER PRIMARY KEY AUTOINCREMENT,\n"
		"	user_id		BIGINT NOT Null,\n"
		"	chat_id		BIGINT,\n"
		"	reason		VARCHAR(%d) NOT Null,\n"
		"	is_blocked	BOOLEAN NOT Null\n"
		");",
		MODEL_BLOCK_LIST_REASON_SIZE
	);
}


const char *
model_cmd_extern_query(Str *str)
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


const char *
model_cmd_extern_disabled_query(Str *str)
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


const char *
model_cmd_message_query(Str *str)
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
		");"
	);
}


/*
 * ModelChat
 */
int
model_chat_get_flags(int64_t chat_id)
{
	const char *const query = "SELECT flags FROM Chat WHERE (chat_id = ?);";
	const DbArg arg = { .type = DB_DATA_TYPE_INT64, .int64 = chat_id };

	int flags = 0;
	const DbOut out = {
		.len = 1,
		.fields = &(DbOutField) { .type = DB_DATA_TYPE_INT, .int_ = &flags },
	};

	DbConn *const conn = db_conn_get();
	if (db_conn_exec(conn, query, &arg, 1, &out, 1) < 0)
		flags = -1;

	db_conn_put(conn);
	return flags;
}


/*
 * ModelParam
 */
int
model_param_get(ModelParam *p, const char key[])
{
	const char *const query =
		"SELECT id, is_active, key, value0, value1, value2, created_at, updated_at "
		"FROM Param "
		"WHERE (key = ?) AND (is_active = True) "
		"ORDER BY id DESC "
		"LIMIT 1;";

	const DbArg arg = { .type = DB_DATA_TYPE_TEXT, .text = key };
	const DbOut out = {
		.len = 8,
		.fields = (DbOutField[]) {
			{ .type = DB_DATA_TYPE_INT, .int_ = &p->id },
			{ .type = DB_DATA_TYPE_INT, .int_ = &p->is_active },
			{ .type = DB_DATA_TYPE_TEXT, .text = (DbOutFieldText) {
				.cstr = p->key_out,
				.size = sizeof(p->key_out),
			}},
			{ .type = DB_DATA_TYPE_TEXT, .text = (DbOutFieldText) {
				.cstr = p->value0_out,
				.size = sizeof(p->value0_out),
			}},
			{ .type = DB_DATA_TYPE_TEXT, .text = (DbOutFieldText) {
				.cstr = p->value1_out,
				.size = sizeof(p->value1_out),
			}},
			{ .type = DB_DATA_TYPE_TEXT, .text = (DbOutFieldText) {
				.cstr = p->value2_out,
				.size = sizeof(p->value2_out),
			}},
			{ .type = DB_DATA_TYPE_INT64, .int64 = &p->created_at },
			{ .type = DB_DATA_TYPE_INT64, .int64 = &p->updated_at },
		},
	};

	DbConn *const conn = db_conn_get();
	const int ret = db_conn_exec(conn, query, &arg, 1, &out, 1);

	db_conn_put(conn);
	return ret;
}


/*
 * ModelParamChat
 */
int
model_param_chat_get(ModelParamChat *p, int64_t chat_id, const char key[])
{
	const char *const query =
		"SELECT a.id, a.chat_id, b.id, b.is_active, b.key, b.value0, b.value1, b.value2, "
			"b.created_at, b.updated_at "
		"FROM Param_Chat as a "
		"JOIN Param as b on (b.param_id = a.id) "
		"WHERE (a.chat_id = ?) AND (b.key = ?) AND (b.is_active = True) "
		"ORDER BY a.id DESC "
		"LIMIT 1;";

	const DbArg args[] = {
		{ .type = DB_DATA_TYPE_INT64, .int64 = chat_id },
		{ .type = DB_DATA_TYPE_TEXT, .text = key },
	};

	const DbOut out = {
		.len = 10,
		.fields = (DbOutField[]) {
			{ .type = DB_DATA_TYPE_INT, .int_ = &p->id },
			{ .type = DB_DATA_TYPE_INT64, .int64 = &p->chat_id },
			{ .type = DB_DATA_TYPE_INT, .int_ = &p->param.id },
			{ .type = DB_DATA_TYPE_INT, .int_ = &p->param.is_active },
			{ .type = DB_DATA_TYPE_TEXT, .text = (DbOutFieldText) {
				.cstr = p->param.key_out,
				.size = sizeof(p->param.key_out),
			}},
			{ .type = DB_DATA_TYPE_TEXT, .text = (DbOutFieldText) {
				.cstr = p->param.value0_out,
				.size = sizeof(p->param.value0_out),
			}},
			{ .type = DB_DATA_TYPE_TEXT, .text = (DbOutFieldText) {
				.cstr = p->param.value1_out,
				.size = sizeof(p->param.value1_out),
			}},
			{ .type = DB_DATA_TYPE_TEXT, .text = (DbOutFieldText) {
				.cstr = p->param.value2_out,
				.size = sizeof(p->param.value2_out),
			}},
			{ .type = DB_DATA_TYPE_INT64, .int64 = &p->param.created_at },
			{ .type = DB_DATA_TYPE_INT64, .int64 = &p->param.updated_at },
		},
	};

	DbConn *const conn = db_conn_get();
	const int ret = db_conn_exec(conn, query, args, LEN(args), &out, 1);

	db_conn_put(conn);
	return ret;
}


/*
 * ModelAdmin
 */
/* TODO: optimize: only call db_conn_exec() once */
static int
_admin_add(DbConn *conn, const ModelAdmin list[], int len)
{
	const char *const query =
		"INSERT INTO Admin(chat_id, user_id, is_anonymous, privileges) "
		"VALUES(?, ?, ?, ?);";

	int ret = 0;
	for (int i = 0; i < len; i++) {
		const ModelAdmin *admin = &list[i];
		const DbArg args[] = {
			{ .type = DB_DATA_TYPE_INT64, .int64 = admin->chat_id },
			{ .type = DB_DATA_TYPE_INT64, .int64 = admin->user_id },
			{ .type = DB_DATA_TYPE_INT, .int_ = admin->is_anonymous },
			{ .type = DB_DATA_TYPE_INT, .int_ = admin->privileges },
		};

		ret = db_conn_exec(conn, query, args, LEN(args), NULL, 0);
		if (ret < 0)
			return -1;

		ret += db_conn_changes(conn);
	}

	return ret;
}


static int
_admin_clear(DbConn *conn, int64_t chat_id)
{
	const char *const query = "DELETE FROM Admin WHERE (chat_id = ?);";
	const DbArg arg = { .type = DB_DATA_TYPE_INT64, .int64 = chat_id };
	if (db_conn_exec(conn, query, &arg, 1, NULL, 0) < 0)
		return -1;

	return db_conn_changes(conn);
}


int
model_admin_reload(const ModelAdmin list[], int len)
{
	if (len <= 0)
		return 0;

	int is_ok = 0;
	DbConn *const conn = db_conn_get();
	int ret = db_conn_tran_begin(conn);
	if (ret < 0)
		goto out0;

	ret = _admin_clear(conn, list[0].chat_id);
	if (ret < 0)
		goto out1;

	ret = _admin_add(conn, list, len);
	if (ret < 0)
		goto out1;

	is_ok = 1;

out1:
	db_conn_tran_end(conn, is_ok);
out0:
	db_conn_put(conn);
	return ret;
}


int
model_admin_get_privilegs(int64_t chat_id, int64_t user_id)
{
	const char *const query =
		"SELECT privileges "
		"FROM Admin "
		"WHERE (chat_id = ?) AND (user_id = ?) "
		"ORDER BY id DESC LIMIT 1;";

	const DbArg args[] = {
		{ .type = DB_DATA_TYPE_INT64, .int64 = chat_id },
		{ .type = DB_DATA_TYPE_INT64, .int64 = user_id },
	};

	int privs;
	const DbOut out = {
		.len = 1,
		.fields = &(DbOutField) { .type = DB_DATA_TYPE_INT, .int_ = &privs },
	};

	DbConn *const conn = db_conn_get();
	if (db_conn_exec(conn, query, args, LEN(args), &out, 1) < 0)
		privs = -1;

	db_conn_put(conn);
	return privs;
}


/*
 * ModelSchedMessage
 */
/* TODO: simplify & optimize */
int
model_sched_message_get_list(int64_t now, ModelSchedMessage **ret_list[])
{
	int ret = -1;
	const char *const query_len =
		"SELECT COUNT(*) "
		"FROM Sched_Message "
		"WHERE (? >= next_run) AND (? < (next_run + expire));";
	const char *const query =
		"SELECT id, type, chat_id, message_id, value, next_run, expire "
		"FROM Sched_Message "
		"WHERE (? >= next_run) AND (? < (next_run + expire));";

	const DbArg args[] = {
		{ .type = DB_DATA_TYPE_INT64, .int64 = now },
		{ .type = DB_DATA_TYPE_INT64, .int64 = now },
	};

	int list_len = 0;
	const DbOut out_list_len = {
		.len = 1,
		.fields = &(DbOutField) { .type = DB_DATA_TYPE_INT, .int_ = &list_len },
	};

	DbConn *const conn = db_conn_get();
	if (db_conn_exec(conn, query_len, args, LEN(args), &out_list_len, 1) < 0)
		goto out0;

	if (list_len == 0) {
		ret = 0;
		goto out0;
	}

	int iter = 0;
	DbOut *const out_list = malloc(sizeof(DbOut) * (size_t)list_len);
	if (out_list == NULL) {
		log_err(errno, "model: model_sched_message_get_list: malloc: DbOut");
		goto out0;
	}

	ModelSchedMessage **const ptr_list = malloc(sizeof(ModelSchedMessage *) * (size_t)list_len);
	if (ptr_list == NULL) {
		log_err(errno, "model: model_sched_message_get_list: malloc: SchedMessage *");
		goto out1;
	}

	const int fields_len = 7;
	DbOutField *const fields = malloc(sizeof(DbOutField) * (size_t)(fields_len * list_len));
	if (fields == NULL) {
		log_err(errno, "model: model_sched_message_get_list: malloc: DbOutField *");
		goto out2;
	}

	/* remapping */
	for (int j = 0; iter < list_len; iter++) {
		ModelSchedMessage *const sm = malloc(sizeof(ModelSchedMessage));
		if (sm == NULL)
			goto out3;

		out_list[iter].fields = &fields[j];
		out_list[iter].len = fields_len;

		fields[j++] = (DbOutField) { .type = DB_DATA_TYPE_INT, .int_ = &sm->id };
		fields[j++] = (DbOutField) { .type = DB_DATA_TYPE_INT, .int_ = &sm->type };
		fields[j++] = (DbOutField) { .type = DB_DATA_TYPE_INT64, .int64 = &sm->chat_id };
		fields[j++] = (DbOutField) { .type = DB_DATA_TYPE_INT64, .int64 = &sm->message_id };
		fields[j++] = (DbOutField) {
			.type = DB_DATA_TYPE_TEXT,
			.text = (DbOutFieldText) { .cstr = sm->value_out, .size = sizeof(sm->value_out) },
		};
		fields[j++] = (DbOutField) { .type = DB_DATA_TYPE_INT64, .int64 = &sm->next_run };
		fields[j++] = (DbOutField) { .type = DB_DATA_TYPE_INT64, .int64 = &sm->expire };

		ptr_list[iter] = sm;
	}

	if (db_conn_exec(conn, query, args, LEN(args), out_list, list_len) < 0)
		goto out3;

	*ret_list = ptr_list;
	ret = list_len;

out3:
	free(fields);
out2:
	if (ret < 0) {
		for (int i = 0; i < iter; i++)
			free(ptr_list[i]);
	}

	if (ret < 0)
		free(ptr_list);
out1:
	free(out_list);
out0:
	db_conn_put(conn);
	return ret;
}


int
model_sched_message_del_all(int64_t now)
{
	int ret;
	const char *const query = "DELETE FROM Sched_Message WHERE (? >= next_run);";
	const DbArg args = { .type = DB_DATA_TYPE_INT64, .int64 = now };

	DbConn *const conn = db_conn_get();
	while (ev_is_alive()) {
		ret = db_conn_exec(conn, query, &args, 1, NULL, 0);
		if (ret == DB_RET_BUSY) {
			db_conn_busy_timeout(conn, 3);
			continue;
		}

		if (ret < 0)
			goto out0;

		break;
	}

	ret = db_conn_changes(conn);

out0:
	db_conn_put(conn);
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

	const char *const query =
		"INSERT INTO Sched_Message(type, chat_id, message_id, value, next_run, expire) "
		"VALUES(?, ?, ?, ?, ?, ?);";

	const int64_t now = time(NULL);
	const DbArg args[] = {
		{ .type = DB_DATA_TYPE_INT, .int_ = MODEL_SCHED_MESSAGE_TYPE_SEND },
		{ .type = DB_DATA_TYPE_INT64, .int64 = s->chat_id },
		{ .type = DB_DATA_TYPE_INT64, .int64 = s->message_id },
		{ .type = DB_DATA_TYPE_TEXT, .text = s->value_in },
		{ .type = DB_DATA_TYPE_INT64, .int64 = now + interval_s },
		{ .type = DB_DATA_TYPE_INT64, .int64 = s->expire },
	};

	int ret = -1;
	DbConn *const conn = db_conn_get();
	if (db_conn_exec(conn, query, args, LEN(args), NULL, 0) < 0)
		goto out0;

	ret = db_conn_changes(conn);

out0:
	db_conn_put(conn);
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

	const char *const query =
		"INSERT INTO Sched_Message(type, chat_id, message_id, next_run, expire) "
		"VALUES(?, ?, ?, ?, ?);";

	const int64_t now = time(NULL);
	const DbArg args[] = {
		{ .type = DB_DATA_TYPE_INT, .int_ = MODEL_SCHED_MESSAGE_TYPE_DELETE },
		{ .type = DB_DATA_TYPE_INT64, .int64 = s->chat_id },
		{ .type = DB_DATA_TYPE_INT64, .int64 = s->message_id },
		{ .type = DB_DATA_TYPE_INT64, .int64 = now + interval_s },
		{ .type = DB_DATA_TYPE_INT64, .int64 = s->expire },
	};

	int ret = -1;
	DbConn *const conn = db_conn_get();
	if (db_conn_exec(conn, query, args, LEN(args), NULL, 0) < 0)
		goto out0;

	ret = db_conn_changes(conn);

out0:
	db_conn_put(conn);
	return ret;
}


/*
 * ModelCmdExtern
 */
int
model_cmd_extern_get_one(ModelCmdExtern *c, int64_t chat_id, const char name[])
{
	const char *const query =
		"SELECT id, is_enable, flags, name, file_name, description "
		"FROM Cmd_Extern "
		"WHERE (name = ?) AND (name NOT IN ( "
		"	SELECT name "
		"	FROM Cmd_Extern_Disabled "
		"	WHERE (chat_id = ?) "
		"));";

	const DbArg db_args[] = {
		{ .type = DB_DATA_TYPE_TEXT, .text = name },
		{ .type = DB_DATA_TYPE_INT64, .int64 = chat_id },
	};

	const DbOut out = {
		.len = 6,
		.fields = (DbOutField[]) {
			{ .type = DB_DATA_TYPE_INT, .int_ = &c->id },
			{ .type = DB_DATA_TYPE_INT, .int_ = &c->is_enable },
			{ .type = DB_DATA_TYPE_INT, .int_ = &c->flags },
			{ .type = DB_DATA_TYPE_TEXT, .text = (DbOutFieldText) {
				.cstr = c->name_out, .size = sizeof(c->name_out),
			} },
			{ .type = DB_DATA_TYPE_TEXT, .text = (DbOutFieldText) {
				.cstr = c->file_name_out, .size = sizeof(c->file_name_out),
			} },
			{ .type = DB_DATA_TYPE_TEXT, .text = (DbOutFieldText) {
				.cstr = c->description_out, .size = sizeof(c->description_out),
			} },
		},
	};

	DbConn *const conn = db_conn_get();
	const int ret = db_conn_exec(conn, query, db_args, LEN(db_args), &out, 1);

	db_conn_put(conn);
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
	const char *const query = "SELECT 1 FROM Cmd_Extern WHERE (name = ?);";
	DbArg arg = { .type = DB_DATA_TYPE_TEXT, .text = name };

	int res;
	DbOut out = { .len = 1, .fields = &(DbOutField) { .int_ = &res } };

	DbConn *const conn = db_conn_get();
	const int ret = db_conn_exec(conn, query, &arg, 1, &out, 1);

	db_conn_put(conn);
	return ret;
}


/*
 * ModelCmdExternDisabled
 */
int
model_cmd_extern_disabled_setup(int64_t chat_id)
{
	const char *query = "SELECT 1 FROM Cmd_Extern_Disabled WHERE (chat_id = ?);";
	const DbArg args[] = {
		{ .type = DB_DATA_TYPE_INT64, .int64 = chat_id },
		{ .type = DB_DATA_TYPE_INT, .int_ = MODEL_CMD_FLAG_NSFW },
	};

	DbConn *const conn = db_conn_get();
	int ret = db_conn_exec(conn, query, args, 1, NULL, 0);
	if (ret < 0)
		goto out0;

	/* already registered  */
	if (ret > 0)
		goto out0;

	/* disable all NSFW Cmd(s) */
	query = "INSERT INTO Cmd_Extern_Disabled(name, chat_id) "
		"SELECT name, ? "
		"FROM Cmd_Extern "
		"WHERE ((flags & ?) != 0);";

	ret = db_conn_exec(conn, query, args, LEN(args), NULL, 0);
	if (ret < 0)
		goto out0;

	ret = db_conn_changes(conn);

out0:
	db_conn_put(conn);
	return ret;
}


/*
 * ModelCmdMessage
 */
int
model_cmd_message_set(const ModelCmdMessage *c)
{
	const char *query = "SELECT 1 FROM Cmd_Message WHERE (chat_id = ?) AND (name = ?);";
	DbArg args[] = {
		{ .type = DB_DATA_TYPE_TEXT, .text = c->value_in },
		{ .type = DB_DATA_TYPE_INT64, .int64 = c->created_by }, /* or updated by */
		{ .type = DB_DATA_TYPE_INT64, .int64 = c->chat_id },
		{ .type = DB_DATA_TYPE_TEXT, .text = c->name_in },
	};

	int is_exists = 0;
	DbOut out = {
		.len = 1,
		.fields = &(DbOutField) { .type = DB_DATA_TYPE_INT, .int_ = &is_exists },
	};

	DbConn *const conn = db_conn_get();
	int ret = db_conn_exec(conn, query, &args[2], 2, &out, 1);
	if (ret < 0)
		goto out0;

	if (ret == 0) {
		if (c->value_in == NULL)
			goto out0;

		query = "INSERT INTO Cmd_Message(value, created_by, chat_id, name) VALUES(?, ?, ?, ?);";
	} else {
		query = "UPDATE Cmd_Message "
			"	SET value = ?, updated_at = UNIXEPOCH(), "
			"	    updated_by = ? "
			"WHERE (chat_id = ?) and (name = ?);";

		args[1].int64 = c->updated_by;
	}

	ret = db_conn_exec(conn, query, args, LEN(args), NULL, 0);
	if (ret < 0)
		goto out0;

	ret = db_conn_changes(conn);

out0:
	db_conn_put(conn);
	return ret;
}


int
model_cmd_message_get_value(int64_t chat_id, const char name[], char value[], size_t value_len)
{
	const char *const query =
		"SELECT value FROM Cmd_Message WHERE (chat_id = ?) AND (name = ?);";

	const DbArg args[] = {
		{ .type = DB_DATA_TYPE_INT64, .int64 = chat_id },
		{ .type = DB_DATA_TYPE_TEXT, .text = name },
	};

	const DbOut out = {
		.len = 1,
		.fields = &(DbOutField) {
			.type = DB_DATA_TYPE_TEXT,
			.text = (DbOutFieldText) {
				.cstr = value,
				.size = value_len,
			},
		},
	};

	DbConn *const conn = db_conn_get();
	const int ret = db_conn_exec(conn, query, args, LEN(args), &out, 1);

	db_conn_put(conn);
	return ret;
}


int
model_cmd_message_is_exists(const char name[])
{
	const char *const query = "SELECT 1 FROM Cmd_Message WHERE (name = ?);";
	DbArg arg = { .type = DB_DATA_TYPE_TEXT, .text = name };

	int res;
	DbOut out = { .len = 1, .fields = &(DbOutField) { .int_ = &res } };

	DbConn *const conn = db_conn_get();
	const int ret = db_conn_exec(conn, query, &arg, 1, &out, 1);

	db_conn_put(conn);
	return ret;
}
