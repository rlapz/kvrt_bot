#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "sqlite_pool.h"

#include "util.h"


typedef struct db {
	volatile int  is_alive;
	const char   *db_path;
	DList         sql_pool;
	mtx_t         mutex;
	cnd_t         cond;
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
		log_err(0, "sqlite_pool: sqlite_pool_init: mtx_init: failed");
		goto err0;
	}

	if (cnd_init(&d->cond) != thrd_success) {
		log_err(0, "sqlite_pool: sqlite_pool_init: cnd_init: failed");
		goto err1;
	}

	d->is_alive = 1;
	d->db_path = path;
	return 0;

err1:
	mtx_destroy(&d->mutex);
err0:
	_pool_cleanup(d);
	return -1;
}


void
sqlite_pool_deinit(void)
{
	Db *const d = &_instance;

	d->is_alive = 0;
	cnd_broadcast(&d->cond);

	_pool_cleanup(d);
	mtx_destroy(&d->mutex);
	cnd_destroy(&d->cond);
}


DbConn *
sqlite_pool_get(void)
{
	Db *const d = &_instance;
	DListNode *node = NULL;

	mtx_lock(&d->mutex);

	while (d->is_alive) {
		if ((node = dlist_pop(&d->sql_pool)) != NULL)
			break;

		cnd_wait(&d->cond, &d->mutex);
	}

	mtx_unlock(&d->mutex);

	if (node == NULL)
		return NULL;

	return FIELD_PARENT_PTR(DbConn, _node, node);
}


DbConn *
sqlite_pool_get_no_wait(void)
{
	DbConn *ret;
	if (_open(_instance.db_path, &ret) < 0)
		return NULL;

	ret->_in_pool = 0;
	return ret;
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
	cnd_signal(&d->cond);

	mtx_unlock(&d->mutex);
}


int
sqlite_pool_tran_begin(DbConn *conn)
{
	char *err_msg;
	const int ret = sqlite3_exec(conn->sql, "BEGIN TRANSACTION;", NULL, NULL, &err_msg);
	if (ret != SQLITE_OK) {
		log_err(0, "sqlite_pool: sqlite_pool_tran_begin: sqlite3_exec: %s", err_msg);
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
		log_err(0, "sqlite_pool: sqlite_pool_tran_end(%d): sqlite3_exec: %s", is_ok, err_msg);
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
		log_err(errno, "sqlite_pool: _open: malloc");
		return -1;
	}

	const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
	const int ret = sqlite3_open_v2(path, &new_conn->sql, flags, NULL);
	if (ret != SQLITE_OK) {
		log_err(0, "sqlite_pool: _open: sqlite3_open_v2: failed to open: \"%s\": %s",
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
