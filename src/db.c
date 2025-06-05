#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "db.h"

#include "model.h"
#include "util.h"


typedef struct db_conn {
	int        in_pool;
	sqlite3   *sql;
	DListNode  node;
} DbConn;

typedef struct db {
	volatile int  is_alive;
	const char   *db_path;
	DList         sql_pool;
	mtx_t         mutex;
	cnd_t         cond;
} Db;

static Db _instance;


static int  _open_sql(const char path[], DbConn **conn);
static void _pool_cleanup(Db *d);
static int  _create_tables(void);
static int  _exec_set_args(sqlite3_stmt *s, const DbArg args[], int args_len);
static int  _exec_get_output(sqlite3_stmt *s, const DbOut out[], int out_len);
static int  _exec_get_output_values(sqlite3_stmt *s, DbOutField fields[], int len);


/*
 * Public
 */
int
db_init(const char path[], int conn_pool_size)
{
	Db *const d = &_instance;
	dlist_init(&d->sql_pool);

	DbConn *new_conn;
	for (int i = 0; i < conn_pool_size; i++) {
		if (_open_sql(path, &new_conn) < 0)
			goto err0;

		new_conn->in_pool = 1;
		dlist_append(&d->sql_pool, &new_conn->node);
	}

	if (mtx_init(&d->mutex, mtx_plain) != thrd_success) {
		log_err(0, "db: db_init: mtx_init: failed");
		goto err0;
	}

	if (cnd_init(&d->cond) != thrd_success) {
		log_err(0, "db: db_init: cnd_init: failed");
		goto err1;
	}

	d->is_alive = 1;
	d->db_path = path;
	if (_create_tables() < 0)
		goto err2;

	return DB_RET_OK;

err2:
	cnd_destroy(&d->cond);
err1:
	mtx_destroy(&d->mutex);
err0:
	_pool_cleanup(d);
	return DB_RET_ERR;
}


void
db_deinit(void)
{
	Db *const d = &_instance;

	d->is_alive = 0;
	cnd_broadcast(&d->cond);

	_pool_cleanup(d);
	mtx_destroy(&d->mutex);
	cnd_destroy(&d->cond);
}


DbConn *
db_conn_get(void)
{
	Db *const d = &_instance;
	DListNode *node = NULL;
	DbConn *conn = NULL;

	mtx_lock(&d->mutex);

	while (d->is_alive) {
		if ((node = dlist_pop(&d->sql_pool)) != NULL)
			break;

		cnd_wait(&d->cond, &d->mutex);
	}

	if (node != NULL)
		conn = FIELD_PARENT_PTR(DbConn, node, node);

	mtx_unlock(&d->mutex);
	return conn;
}


DbConn *
db_conn_get_no_wait(void)
{
	DbConn *ret;
	if (_open_sql(_instance.db_path, &ret) < 0)
		return NULL;

	ret->in_pool = 0;
	return ret;
}


void
db_conn_put(DbConn *conn)
{
	Db *const d = &_instance;
	if (conn->in_pool == 0) {
		sqlite3_close(conn->sql);
		return;
	}

	mtx_lock(&d->mutex);

	dlist_append(&d->sql_pool, &conn->node);
	cnd_signal(&d->cond);

	mtx_unlock(&d->mutex);
}


int
db_conn_tran_begin(DbConn *conn)
{
	char *err_msg;
	const int ret = sqlite3_exec(conn->sql, "begin transaction;", NULL, NULL, &err_msg);
	if (ret != SQLITE_OK) {
		log_err(0, "db: db_conn_tran_begin: sqlite3_exec: %s", err_msg);
		sqlite3_free(err_msg);
	}

	return -ret;
}


int
db_conn_tran_end(DbConn *conn, int is_ok)
{
	char *err_msg;
	const char *const sql = (is_ok)? "commit transaction;" : "rollback transaction;";
	const int ret = sqlite3_exec(conn->sql, sql, NULL, NULL, &err_msg);
	if (ret != SQLITE_OK) {
		log_err(0, "db: db_conn_tran_end(%d): sqlite3_exec: %s", is_ok, err_msg);
		sqlite3_free(err_msg);
	}

	return -ret;
}


int
db_conn_exec(DbConn *conn, const char query[], const DbArg args[], int args_len,
	     const DbOut out[], int out_len)
{
	sqlite3_stmt *stmt;
	int ret = sqlite3_prepare_v2(conn->sql, query, -1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		log_err(0, "db: db_conn_exec: sqlite3_prepare_v2: %s", sqlite3_errstr(ret));
		return -ret;
	}

	ret = _exec_set_args(stmt, args, args_len);
	if (ret < 0)
		goto out0;

	if (out != NULL) {
		ret = _exec_get_output(stmt, out, out_len);
		goto out0;
	}

	log_debug("db: db_conn_exec: query: %s", query);

	ret = sqlite3_step(stmt);
	switch (ret) {
	case SQLITE_DONE:
	case SQLITE_ROW:
		break;
	default:
		log_err(0, "db: db_conn_exec: sqlite3_step: %s", sqlite3_errstr(ret));
		ret = -ret;
		break;
	}

out0:
	sqlite3_finalize(stmt);
	return ret;
}


int
db_conn_changes(DbConn *conn)
{
	return sqlite3_changes(conn->sql);
}


int
db_conn_busy_timeout(DbConn *conn, int timeout_s)
{
	const int ret = sqlite3_busy_timeout(conn->sql, 1000 * timeout_s);
	if (ret != SQLITE_OK) {
		log_err(0, "db: db_conn_busy_timeout: sqlite3_busy_timeout: %s", sqlite3_errstr(ret));
		return -ret;
	}

	return DB_RET_OK;
}


/*
 * Private
 */
static int
_open_sql(const char path[], DbConn **conn)
{
	DbConn *const new_conn = malloc(sizeof(DbConn));
	if (new_conn == NULL) {
		log_err(errno, "db: _open_sql: malloc");
		return -1;
	}

	const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
	const int ret = sqlite3_open_v2(path, &new_conn->sql, flags, NULL);
	if (ret != SQLITE_OK) {
		log_err(0, "db: _open_sql: sqlite3_open_v2: failed to open: \"%s\": %s",
			path, sqlite3_errstr(ret));

		free(new_conn);
		return -1;
	}

	*conn = new_conn;
	return 0;
}


static void
_pool_cleanup(Db *d)
{
	DListNode *node;
	while ((node = dlist_pop(&d->sql_pool)) != NULL) {
		DbConn *const conn = FIELD_PARENT_PTR(DbConn, node, node);
		sqlite3_close(conn->sql);

		free(conn);
	}
}


static int
_create_tables(void)
{
	Str str;
	int ret = str_init_alloc(&str, 0);
	if (ret < 0) {
		log_err(ret, "db: _create_tables: str_init_alloc");
		return DB_RET_ERR;
	}

	char *err_msg;
	struct {
		const char *table_name;
		const char *(*func)(Str *);
	} params[] = {
		{ "Chat", model_chat_query },
		{ "Param", model_param_query },
		{ "Param_Chat", model_param_chat_query },
		{ "Admin", model_admin_query },
		{ "Sched_Message", model_sched_message_query },
		{ "Block_List", model_block_list_query },
		{ "Cmd_Extern", model_cmd_extern_query },
		{ "Cmd_Extern_Disabled", model_cmd_extern_disabled_query },
		{ "Cmd_Message", model_cmd_message_query },
	};

	ret = DB_RET_ERR;

	DbConn *const conn = db_conn_get();
	for (size_t i = 0; i < LEN(params); i++) {
		const char *const query = params[i].func(&str);
		ret = sqlite3_exec(conn->sql, query, NULL, NULL, &err_msg);
		if (ret != SQLITE_OK) {
			const char *const table_name = params[i].table_name;
			log_err(0, "db: _create_tables: sqlite3_exec: %s: %s", table_name, err_msg);
			ret = -ret;
			goto out0;
		}
	}

	ret = DB_RET_OK;

out0:
	db_conn_put(conn);
	sqlite3_free(err_msg);
	str_deinit(&str);
	return ret;
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
			log_err(0, "db: _exec_set_args: bind: %s", sqlite3_errstr(ret));
			return -ret;
		}
	}

	return DB_RET_OK;
}


static int
_exec_get_output(sqlite3_stmt *s, const DbOut out[], int out_len)
{
	int i = 0;
	for (; i < out_len; i++) {
		int ret = sqlite3_step(s);
		if (ret == SQLITE_DONE)
			break;

		if (ret != SQLITE_ROW) {
			log_err(0, "db: _exec_get_output: sqlite3_step: %s", sqlite3_errstr(ret));
			return -ret;
		}

		ret = _exec_get_output_values(s, out[i].fields, out[i].len);
		if (ret < 0)
			return ret;
	}

	return i;
}


static int
_exec_get_output_values(sqlite3_stmt *s, DbOutField fields[], int len)
{
	int i = 0;
	for (; i < len; i++) {
		DbOutField *const field = &fields[i];
		const int type = sqlite3_column_type(s, i);
		if (type == SQLITE_NULL) {
			switch (field->type) {
			case DB_DATA_TYPE_INT:
				*field->int_ = 0;
				break;
			case DB_DATA_TYPE_INT64:
				*field->int64 = 0;
				break;
			case DB_DATA_TYPE_TEXT:
				memset(field->text.cstr, 0, field->text.size);
				break;
			}

			field->type = DB_DATA_TYPE_NULL;
			continue;
		}

		switch (field->type) {
		case DB_DATA_TYPE_INT:
			if (type != SQLITE_INTEGER)
				goto err0;

			*field->int_ = sqlite3_column_int(s, i);
			break;
		case DB_DATA_TYPE_INT64:
			if (type != SQLITE_INTEGER)
				goto err0;

			*field->int64 = sqlite3_column_int64(s, i);
			break;
		case DB_DATA_TYPE_TEXT:
			if (type != SQLITE3_TEXT)
				goto err0;

			const char *const rtext = (const char *)sqlite3_column_text(s, i);
			DbOutFieldText *const text = &field->text;
			text->len = cstr_copy_n(text->cstr, text->size, rtext);
			break;
		default:
			log_err(0, "db: _exec_get_output_values: invalid out type: [%d:%d]",
				i, field->type);
			break;
		}
	}

	return DB_RET_OK;

err0:
	log_err(0, "db: _exec_get_output_values: [%d:%d]: invalid column type pairs", i, fields[i].type);
	return DB_RET_ERR;
}
