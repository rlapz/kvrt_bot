#include <errno.h>

#include <repo.h>
#include <model.h>


/*
 * Public
 */
int
repo_init(Repo *s, const char db_path[])
{
	if (db_init(&s->db, db_path) < 0)
		return -1;

	if (str_init_alloc(&s->str, 0) < 0) {
		log_err(ENOMEM, "repo: repo_init: str_init_alloc");
		db_deinit(&s->db);
		return -1;
	}

	return 0;
}


void
repo_deinit(Repo *s)
{
	db_deinit(&s->db);
	str_deinit(&s->str);
}


int
repo_admin_add(Repo *s, const Admin admin_list[], int len)
{
	int ret = 0;
	const char *const sql = "insert into Admin(chat_id, user_id, privileges) values (?, ?, ?);";
	for (int i = 0; i < len; i++) {
		const Admin *admin = &admin_list[i];
		const DbArg args[] = {
			{ .type = DB_DATA_TYPE_INT64, .int64 = admin->chat_id },
			{ .type = DB_DATA_TYPE_INT64, .int64 = admin->user_id },
			{ .type = DB_DATA_TYPE_INT, .int_ = admin->privileges },
		};

		if (db_exec(&s->db, sql, args, LEN(args), NULL, 0) < 0)
			return -1;

		ret += db_changes(&s->db);
	}

	return ret;
}


int
repo_admin_get_privileges(Repo *s, int64_t chat_id, int64_t user_id)
{
	int privileges = 0;
	const char *const sql = "select privileges "
				"from Admin "
				"where (chat_id = ?) and (user_id = ?)"
				"order by id desc limit 1;";

	const DbArg args[] = {
		{ .type = DB_DATA_TYPE_INT64, .int64 = chat_id },
		{ .type = DB_DATA_TYPE_INT64, .int64 = user_id },
	};

	const DbOut out = {
		.len = 1,
		.fields = (DbOutField[]) {
			{ .type = DB_DATA_TYPE_INT, .int_ = &privileges },
		},
	};

	if (db_exec(&s->db, sql, args, LEN(args), &out, 1) < 0)
		return -1;

	return privileges;
}


int
repo_admin_clear(Repo *s, int64_t chat_id)
{
	const char *const sql = "delete from Admin where (chat_id = ?);";
	const DbArg arg = { .type = DB_DATA_TYPE_INT64, .int64 = chat_id };
	if (db_exec(&s->db, sql, &arg, 1, NULL, 0) < 0)
		return -1;

	return db_changes(&s->db);
}


int
repo_admin_reload(Repo *s, const Admin admin_list[], int len)
{
	int is_ok = 0;
	if (len <= 0)
		return 0;

	int ret = db_transaction_begin(&s->db);
	if (ret < 0)
		return -1;

	ret = repo_admin_clear(s, admin_list[0].chat_id);
	if (ret < 0)
		goto out0;

	ret = repo_admin_add(s, admin_list, len);
	if (ret < 0)
		goto out0;

	is_ok = 1;

out0:
	db_transaction_end(&s->db, is_ok);
	return ret;
}


int
repo_cmd_message_set(Repo *s, const CmdMessage *msg)
{
	DbArg args[] = {
		{ .type = DB_DATA_TYPE_TEXT, .text = msg->message_ptr },
		{ .type = DB_DATA_TYPE_INT64, .int64 = msg->created_by },
		{ .type = DB_DATA_TYPE_INT64, .int64 = msg->chat_id },
		{ .type = DB_DATA_TYPE_TEXT, .text = msg->name_ptr },
	};

	const char *sql = "select 1 from Cmd_Message where (chat_id = ?) and (name = ?);";
	const int ret = db_exec(&s->db, sql, &args[2], 2, NULL, 0);
	if (ret < 0)
		return -1;

	if (ret == 0) {
		if (msg->message_ptr == NULL)
			return 0;

		sql = "insert into Cmd_Message(message, created_by, chat_id, name) values(?, ?, ?, ?);";
	} else {
		sql = "update Cmd_Message "
		      "set message = ?, updated_at = datetime('now', 'localtime'), updated_by = ? "
		      "where (chat_id = ?) and (name = ?);";

		args[1].int64 = msg->updated_by;
	}

	if (db_exec(&s->db, sql, args, LEN(args), NULL, 0) < 0)
		return -1;

	return db_changes(&s->db);
}


int
repo_cmd_message_get_message(Repo *s, CmdMessage *msg)
{
	const DbArg args[] = {
		{ .type = DB_DATA_TYPE_INT64, .int64 = msg->chat_id },
		{ .type = DB_DATA_TYPE_TEXT, .text = msg->name_ptr },
	};

	const DbOut out = {
		.len = 1,
		.fields = &(DbOutField) {
			.type = DB_DATA_TYPE_TEXT,
			.text = { .cstr = msg->message, .size = CMD_MSG_VALUE_SIZE },
		},
	};

	const char *const sql = "select message "
				"from Cmd_Message "
				"where (chat_id = ?) and (name = ?) and (message is not null) "
				"order by id desc limit 1;";

	int ret = db_exec(&s->db, sql, args, LEN(args), &out, 1);
	if (ret < 0)
		return -1;

	return ret;
}


int
repo_cmd_message_get_list(Repo *s, int64_t chat_id, CmdMessage msgs[], int len, int offt, int *max_len)
{
	const char *sql = "select chat_id, name, message, created_at, created_by, updated_at, updated_by "
			  "from Cmd_Message "
			  "where (chat_id = ?) and (message is not null) "
			  "order by name "
			  "limit ? offset ?;";

	int ret = -1;
	const int fields_len = 7;
	const DbArg args[] = {
		{ .type = DB_DATA_TYPE_INT64, .int64 = chat_id },
		{ .type = DB_DATA_TYPE_INT, .int_ = len },
		{ .type = DB_DATA_TYPE_INT, .int_ = offt },
	};

	DbOut *const out_list = malloc(sizeof(DbOut) * len);
	if (out_list == NULL)
		return -1;

	DbOutField *const fields = malloc(sizeof(DbOutField) * fields_len * len);
	if (fields == NULL)
		goto out0;

	for (int i = 0, j = 0; i < len; i++) {
		CmdMessage *const m = &msgs[i];
		out_list[i].fields = &fields[j];
		out_list[i].len = fields_len;

		fields[j++] = (DbOutField) { .type = DB_DATA_TYPE_INT64, .int64 = &m->chat_id };
		fields[j++] = (DbOutField) {
			.type = DB_DATA_TYPE_TEXT,
			.text = (DbOutFieldText) { .cstr = m->name, .size = sizeof(m->name) },
		};
		fields[j++] = (DbOutField) {
			.type = DB_DATA_TYPE_TEXT,
			.text = (DbOutFieldText) { .cstr = m->message, .size = sizeof(m->message) },
		};
		fields[j++] = (DbOutField) {
			.type = DB_DATA_TYPE_TEXT,
			.text = (DbOutFieldText) { .cstr = m->created_at, .size = sizeof(m->created_at) },
		};
		fields[j++] = (DbOutField) { .type = DB_DATA_TYPE_INT64, .int64 = &m->created_by };
		fields[j++] = (DbOutField) {
			.type = DB_DATA_TYPE_TEXT,
			.text = (DbOutFieldText) { .cstr = m->updated_at, .size = sizeof(m->updated_at) },
		};
		fields[j++] = (DbOutField) { .type = DB_DATA_TYPE_INT64, .int64 = &m->updated_by };
	}

	const int rcount = db_exec(&s->db, sql, args, LEN(args), out_list, len);
	if (rcount < 0)
		goto out1;

	const DbOut out_count = {
		.len = 1,
		.fields = &(DbOutField) { .type = DB_DATA_TYPE_INT, .int_ = max_len },
	};

	sql = "select count(1) from Cmd_Message where (chat_id = ?) and (message is not null);";
	ret = db_exec(&s->db, sql, &args[0], 1, &out_count, 1);
	if (ret > 0)
		ret = rcount;

out1:
	free(fields);
out0:
	free(out_list);
	return ret;
}


int
repo_module_extern_setup(Repo *s, int64_t chat_id)
{
	const DbArg args[] = {
		{ .type = DB_DATA_TYPE_INT64, .int64 = chat_id },
		{ .type = DB_DATA_TYPE_INT, .int_ = MODULE_FLAG_NSFW },
	};

	const char *sql = "select 1 from Module_Extern_Disabled where (chat_id = ?);";
	const int ret = db_exec(&s->db, sql, args, 1, NULL, 0);
	if (ret < 0)
		return -1;

	if (ret > 0)
		return 0;

	/* disable all NSFW modules */
	sql = "insert into Module_Extern_Disabled(module_name, chat_id) "
	      "select name, ? "
	      "from Module_Extern "
	      "where ((flags & ?) != 0);";

	if (db_exec(&s->db, sql, args, LEN(args), NULL, 0) < 0)
		return -1;

	return db_changes(&s->db);
}


int
repo_module_extern_toggle(Repo *s, const ModuleExtern *mod, int is_enable)
{
	const DbArg args[] = {
		{ .type = DB_DATA_TYPE_INT64, .int64 = mod->chat_id },
		{ .type = DB_DATA_TYPE_TEXT, .text = mod->name_ptr },
	};

	const char *sql = "select 1 from Module_Extern where (name = ?) limit 1;";
	const int ret = db_exec(&s->db, sql, &args[1], 1, NULL, 0);
	if (ret < 0)
		return -1;

	if (ret == 0)
		return 0;

	if (is_enable)
		sql = "delete from Module_Extern_Disabled where (chat_id = ?) and (module_name = ?);";
	else
		sql = "insert into Module_Extern_Disabled(module_name, chat_id) values(?, ?);";

	if (db_exec(&s->db, sql, args, LEN(args), NULL, 0) < 0)
		return -1;

	return db_changes(&s->db);
}


int
repo_module_extern_toggle_nsfw(Repo *s, int64_t chat_id, int is_enable)
{
	int is_ok = 0;
	const DbArg args[] = {
		{ .type = DB_DATA_TYPE_INT64, .int64 = chat_id },
		{ .type = DB_DATA_TYPE_INT, .int_ = MODULE_FLAG_NSFW },
	};

	if (db_transaction_begin(&s->db) < 0)
		return -1;

	const char *sql = "delete a "
			  "from Module_Extern_Disabled as a "
			  "join Module_Extern as b on (a.module_name = b.name) "
			  "where (a.chat_id = ?) and "
			  "((b.flags & ?) != 0);";

	int ret = db_exec(&s->db, sql, args, LEN(args), NULL, 0);
	if (ret < 0)
		goto out0;

	if (is_enable == 0) {
		sql = "insert into Module_Extern_Disabled(chat_id, module_name) "
		      "select ?, name "
		      "from Module_Extern "
		      "where ((flags & ?) != 0);";

		ret = db_exec(&s->db, sql, args, LEN(args), NULL, 0);
		if (ret < 0)
			goto out0;
	}

	is_ok = 1;

out0:
	db_transaction_end(&s->db, is_ok);
	if (is_ok == 0)
		return ret;

	return db_changes(&s->db);
}


int
repo_module_extern_get(Repo *s, ModuleExtern *mod)
{
	const DbArg args[] = {
		{ .type = DB_DATA_TYPE_INT64, .int64 = mod->chat_id },
		{ .type = DB_DATA_TYPE_TEXT, .text = mod->name_ptr },
	};

	const DbOut out = {
		.len = 5,
		.fields = (DbOutField[]) {
			{ .type = DB_DATA_TYPE_INT64, .int64 = &mod->chat_id },
			{ .type = DB_DATA_TYPE_INT, .int_ = &mod->flags },
			{ .type = DB_DATA_TYPE_INT, .int_ = &mod->args },
			{
				.type = DB_DATA_TYPE_TEXT,
				.text = { .cstr = mod->name, .size = MODULE_NAME_SIZE },
			},
			{
				.type = DB_DATA_TYPE_TEXT,
				.text = { .cstr = mod->file_name, .size = MODULE_EXTERN_FILE_NAME_SIZE },
			},
			{
				.type = DB_DATA_TYPE_TEXT,
				.text = { .cstr = mod->description, .size = MODULE_DESC_SIZE },
			},
		},
	};

	const char *sql = "select 1 from Module_Extern_Disabled "
			  "where (chat_id = ?) and (module_name = ?);";

	const int ret = db_exec(&s->db, sql, args, LEN(args), NULL, 0);
	if (ret < 0)
		return -1;

	if (ret > 0)
		return 0;

	sql = "select ?, flags, args, name, file_name, description "
	      "from Module_Extern "
	      "where (name = ?) "
	      "order by id desc "
	      "limit 1;";

	return db_exec(&s->db, sql, args, LEN(args), &out, 1);
}
