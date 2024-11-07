#include <errno.h>

#include <service.h>
#include <model.h>


#define _MODULE_FLAG_NSFW       STR(MODULE_FLAG_NSFW)
#define _MODULE_FLAG_ADMIN_ONLY STR(MODULE_FLAG_ADMIN_ONLY)


/*
 * Public
 */
int
service_init(Service *s, const char db_path[])
{
	if (db_init(&s->db, db_path) < 0)
		return -1;

	if (str_init_alloc(&s->str, 1024) < 0) {
		log_err(ENOMEM, "service: service_init: str_init_alloc");
		db_deinit(&s->db);
		return -1;
	}

	return 0;
}


void
service_deinit(Service *s)
{
	db_deinit(&s->db);
	str_deinit(&s->str);
}


int
service_admin_add(Service *s, const Admin admin_list[], int len)
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
service_admin_get_privileges(Service *s, int64_t chat_id, int64_t user_id)
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

	DbOut out = {
		.len = 1,
		.items = (DbOutItem[]) {
			{ .type = DB_DATA_TYPE_INT, .int_ = &privileges },
		},
	};

	if (db_exec(&s->db, sql, args, LEN(args), &out, 1) < 0)
		return -1;

	return privileges;
}


int
service_admin_clear(Service *s, int64_t chat_id)
{
	const char *const sql = "delete from Admin where (chat_id = ?);";
	const DbArg arg = { .type = DB_DATA_TYPE_INT64, .int64 = chat_id };
	if (db_exec(&s->db, sql, &arg, 1, NULL, 0) < 0)
		return -1;

	return db_changes(&s->db);
}


int
service_admin_reload(Service *s, const Admin admin_list[], int len)
{
	int is_ok = 0;
	if (len <= 0)
		return 0;

	int ret = db_transaction_begin(&s->db);
	if (ret < 0)
		return -1;

	ret = service_admin_clear(s, admin_list[0].chat_id);
	if (ret < 0)
		goto out0;

	ret = service_admin_add(s, admin_list, len);
	if (ret < 0)
		goto out0;

	is_ok = 1;

out0:
	db_transaction_end(&s->db, is_ok);
	return ret;
}


int
service_cmd_message_set(Service *s, const CmdMessage *msg)
{
	DbArg args[] = {
		{ .type = DB_DATA_TYPE_INT64, .int64 = msg->chat_id },
		{ .type = DB_DATA_TYPE_TEXT, .text = msg->name_ptr },
		{ .type = DB_DATA_TYPE_TEXT, .text = msg->message_ptr },
		{ .type = DB_DATA_TYPE_INT64, .int64 = msg->created_by },
	};

	const char *sql;
	const char *const message = msg->message;
	if ((message == NULL) || (message[0] == '\0')) {
		sql = "select 1 "
		      "from Cmd_Message "
		      "where (chat_id = ?) and (name = ?) and (message != '') "
		      "order by id desc "
		      "limit 1;";

		const int ret = db_exec(&s->db, sql, args, 2, NULL, 0);
		if (ret <= 0)
			return ret;

		/* invalidate cmd message */
		args[2].text = "";
	}

	sql = "insert into Cmd_Message(chat_id, name, message, created_by) values (?, ?, ?, ?)";
	if (db_exec(&s->db, sql, args, LEN(args), NULL, 0) < 0)
		return -1;

	return db_changes(&s->db);
}


int
service_cmd_message_get_message(Service *s, CmdMessage *msg)
{
	const DbArg args[] = {
		{ .type = DB_DATA_TYPE_INT64, .int64 = msg->chat_id },
		{ .type = DB_DATA_TYPE_TEXT, .text = msg->name_ptr },
	};

	DbOut out = {
		.len = 1,
		.items = &(DbOutItem) {
			.type = DB_DATA_TYPE_TEXT,
			.text = { .cstr = msg->message, .size = CMD_MSG_VALUE_SIZE },
		},
	};

	const char *const sql = "select message as message "
				"from Cmd_Message "
				"where (chat_id = ?) and (name = ?) "
				"order by id desc "
				"limit 1;";

	int ret = db_exec(&s->db, sql, args, LEN(args), &out, 1);
	if (ret < 0)
		return -1;

	if (out.items[0].text.len == 0)
		ret = 0;

	return ret;
}


int
service_cmd_message_get_list(Service *s, int64_t chat_id, CmdMessage msgs[], int len, int offt)
{
	const char *const sql = "select chat_id, name, message, max(created_at), created_by "
				"from Cmd_Message where (chat_id = ?) "
				"group by name "
				"having (message != '') "
				"limit ? offset ?;";

	int ret = -1;
	const int _fields_len = 5;
	const DbArg args[] = {
		{ .type = DB_DATA_TYPE_INT64, .int64 = chat_id },
		{ .type = DB_DATA_TYPE_INT, .int_ = len },
		{ .type = DB_DATA_TYPE_INT, .int_ = offt },
	};

	DbOut *const out_list = malloc(sizeof(DbOut) * len);
	if (out_list == NULL)
		return -1;

	DbOutItem *const items = malloc(sizeof(DbOutItem) * _fields_len * len);
	if (items == NULL)
		goto out0;

	for (int i = 0, j = 0; i < len; i++) {
		int k = i + j;
		out_list[i].items = &items[k];
		out_list[i].len = _fields_len;

		items[k++] = (DbOutItem) {
			.type = DB_DATA_TYPE_INT64,
			.int64 = &msgs[i].chat_id,
		};

		items[k++] = (DbOutItem) {
			.type = DB_DATA_TYPE_TEXT,
			.text = (DbOutItemText) {
				.cstr = msgs[i].name,
				.size = sizeof(msgs[i].name),
			},
		};

		items[k++] = (DbOutItem) {
			.type = DB_DATA_TYPE_TEXT,
			.text = (DbOutItemText) {
				.cstr = msgs[i].message,
				.size = sizeof(msgs[i].message),
			},
		};

		items[k++] = (DbOutItem) {
			.type = DB_DATA_TYPE_TEXT,
			.text = (DbOutItemText) {
				.cstr = msgs[i].created_at,
				.size = sizeof(msgs[i].created_at),
			},
		};

		items[k] = (DbOutItem) {
			.type = DB_DATA_TYPE_INT64,
			.int64 = &msgs[i].created_by,
		};

		j = k;
	}

	ret = db_exec(&s->db, sql, args, LEN(args), out_list, len);
	free(items);

out0:
	free(out_list);
	return ret;
}


int
service_module_extern_setup(Service *s, int64_t chat_id)
{
	const DbArg args = { .type = DB_DATA_TYPE_INT64, .int64 = chat_id };
	const char *sql = "select 1 from Module_Extern_Disabled where (chat_id = ?);";
	const int ret = db_exec(&s->db, sql, &args, 1, NULL, 0);
	if (ret < 0)
		return -1;

	if (ret > 0)
		return 0;

	/* disable all NSFW modules */
	sql = "insert into Module_Extern_Disabled(module_name, chat_id) "
	      "select name, ? "
	      "from Module_Extern "
	      "where ((flags & " _MODULE_FLAG_NSFW ") != 0);";

	if (db_exec(&s->db, sql, &args, 1, NULL, 0) < 0)
		return -1;

	return db_changes(&s->db);
}


int
service_module_extern_toggle(Service *s, const ModuleExtern *mod, int is_enable)
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
service_module_extern_toggle_nsfw(Service *s, int64_t chat_id, int is_enable)
{
	int is_ok = 0;
	const DbArg args = { .type = DB_DATA_TYPE_INT64, .int64 = chat_id };

	if (db_transaction_begin(&s->db) < 0)
		return -1;

	const char *sql = "delete a "
			  "from Module_Extern_Disabled as a "
			  "join Module_Extern as b on (a.module_name = b.name) "
			  "where (a.chat_id = ?) and "
			  "((b.flags & " _MODULE_FLAG_NSFW ") != 0);";
	int ret = db_exec(&s->db, sql, &args, 1, NULL, 0);
	if (ret < 0)
		goto out0;

	if (is_enable == 0) {
		sql = "insert into Module_Extern_Disabled(chat_id, module_name) "
		      "select ?, name "
		      "from Module_Extern "
		      "where ((flags & " _MODULE_FLAG_NSFW ") != 0);";
		ret = db_exec(&s->db, sql, &args, 1, NULL, 0);
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
service_module_extern_get(Service *s, ModuleExtern *mod)
{
	const DbArg args[] = {
		{ .type = DB_DATA_TYPE_INT64, .int64 = mod->chat_id },
		{ .type = DB_DATA_TYPE_TEXT, .text = mod->name_ptr },
	};

	DbOut out = {
		.len = 6,
		.items = (DbOutItem[]) {
			{ .type = DB_DATA_TYPE_INT, .int_ = &mod->flags },
			{ .type = DB_DATA_TYPE_INT, .int_ = &mod->args },
			{
				.type = DB_DATA_TYPE_TEXT,
				.text = { .cstr = mod->name, .size = MODULE_EXTERN_NAME_SIZE },
			},
			{
				.type = DB_DATA_TYPE_TEXT,
				.text = { .cstr = mod->file_name, .size = MODULE_EXTERN_FILE_NAME_SIZE },
			},
			{
				.type = DB_DATA_TYPE_TEXT,
				.text = { .cstr = mod->description, .size = MODULE_EXTERN_DESC_SIZE },
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

	sql = "select flags, args, name, file_name, description "
	      "from Module_Extern "
	      "where (name = ?) "
	      "order by id desc "
	      "limit 1;";

	return db_exec(&s->db, sql, &args[1], 1, &out, 1);
}
