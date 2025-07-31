#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "sqlite_pool.h"

#include "util.h"


typedef struct db {
	int         in_pool;
	const char *db_path;
	DList       sql_pool;
	mtx_t       mutex;
} Db;

static Db _instance;


static int  _open(const char path[], DbConn **conn);
static void _pool_cleanup(Db *d);


/*
 * Public
 */
int
sqlite_pool_init(const char path[], int conn_pool_size)
{
	Db *const d = &_instance;
	dlist_init(&d->sql_pool);

	DbConn *new_conn;
	for (int i = 0; i < conn_pool_size; i++) {
		if (_open(path, &new_conn) < 0)
			goto err0;

		new_conn->_in_pool = 1;
		dlist_append(&d->sql_pool, &new_conn->_node);
	}

	if (mtx_init(&d->mutex, mtx_plain) != thrd_success) {
		LOG_ERRN("sqlite_pool", "%s", "mtx_init: failed");
		goto err0;
	}

	d->db_path = path;
	return 0;

err0:
	_pool_cleanup(d);
	return -1;
}


void
sqlite_pool_deinit(void)
{
	Db *const d = &_instance;

	_pool_cleanup(d);
	mtx_destroy(&d->mutex);
}


DbConn *
sqlite_pool_get(void)
{
	Db *const d = &_instance;
	DListNode *node = NULL;

	mtx_lock(&d->mutex);

	node = dlist_pop(&d->sql_pool);

	mtx_unlock(&d->mutex);

	if (node == NULL) {
		DbConn *conn;
		if (_open(d->db_path, &conn) < 0)
			return NULL;

		conn->_in_pool = 0;
		node = &conn->_node;
	}

	return FIELD_PARENT_PTR(DbConn, _node, node);
}


void
sqlite_pool_put(DbConn *conn)
{
	Db *const d = &_instance;
	if (conn->_in_pool == 0) {
		sqlite3_close(conn->sql);
		free(conn);
		return;
	}

	mtx_lock(&d->mutex);

	dlist_append(&d->sql_pool, &conn->_node);

	mtx_unlock(&d->mutex);
}


int
sqlite_pool_tran_begin(DbConn *conn)
{
	char *err_msg;
	const int ret = sqlite3_exec(conn->sql, "BEGIN TRANSACTION;", NULL, NULL, &err_msg);
	if (ret != SQLITE_OK) {
		LOG_ERRN("sqlite_pool", "sqlite3_exec: %s", err_msg);
		sqlite3_free(err_msg);
	}

	return -ret;
}


int
sqlite_pool_tran_end(DbConn *conn, int is_ok)
{
	char *err_msg;
	const char *const sql = (is_ok)? "COMMIT TRANSACTION;" : "ROLLBACK TRANSACTION;";
	const int ret = sqlite3_exec(conn->sql, sql, NULL, NULL, &err_msg);
	if (ret != SQLITE_OK) {
		LOG_ERRN("sqlite_pool", "(%d): sqlite3_exec: %s", is_ok, err_msg);
		sqlite3_free(err_msg);
	}

	return -ret;
}


/*
 * Private
 */
static int
_open(const char path[], DbConn **conn)
{
	DbConn *const new_conn = malloc(sizeof(DbConn));
	if (new_conn == NULL) {
		LOG_ERRP("sqlite_pool", "%s", "malloc");
		return -1;
	}

	const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
	const int ret = sqlite3_open_v2(path, &new_conn->sql, flags, NULL);
	if (ret != SQLITE_OK) {
		LOG_ERRN("sqlite_pool", "sqlite3_open_v2: failed to open: \"%s\": %s",
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
		DbConn *const conn = FIELD_PARENT_PTR(DbConn, _node, node);
		sqlite3_close(conn->sql);

		free(conn);
	}
}
