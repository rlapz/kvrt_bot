#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "sqlite_pool.h"

#include "util.h"


typedef struct db_pool {
	int         in_pool;
	const char *path;
	DList       sql_list;
	mtx_t       mutex;
} DbPool;

static DbPool *_pool_list;
static int     _pool_list_len;


static int  _init_db(DbPool *d, const SqlitePoolParam *param, int index);
static void _deinit_db(DbPool *d);
static int  _open(const char path[], DbConn **conn);


/*
 * Public
 */
int
sqlite_pool_init(const SqlitePoolParam params[], int len)
{
	assert(len > 0);

	DbPool *const list = malloc(sizeof(DbPool) * (size_t)len);
	if (list == NULL) {
		LOG_ERRP("sqlite_pool", "%s", "malloc: DbPoolList");
		return -1;
	}

	int i = 0;
	for (; i < len; i++) {
		if (_init_db(&list[i], &params[i], i) < 0)
			goto err0;
	}

	_pool_list = list;
	_pool_list_len = i;
	return 0;

err0:
	while (i--)
		_deinit_db(&_pool_list[i]);

	free(list);
	return -1;
}


void
sqlite_pool_deinit(void)
{
	for (int i = 0; i < _pool_list_len; i++)
		_deinit_db(&_pool_list[i]);
}


DbConn *
sqlite_pool_get(int index)
{
	assert(index >= 0);
	assert(index < _pool_list_len);

	DbPool *const db = &_pool_list[index];
	DListNode *node = NULL;

	mtx_lock(&db->mutex);
	node = dlist_pop(&db->sql_list);
	mtx_unlock(&db->mutex);

	if (node == NULL) {
		DbConn *conn;
		if (_open(db->path, &conn) < 0)
			return NULL;

		conn->index = index;
		conn->_in_pool = 0;
		node = &conn->_node;
	}

	return FIELD_PARENT_PTR(DbConn, _node, node);
}


void
sqlite_pool_put(DbConn *conn)
{
	const int index = conn->index;
	assert(index >= 0);
	assert(index < _pool_list_len);

	DbPool *const db = &_pool_list[index];
	if (conn->_in_pool == 0) {
		sqlite3_close(conn->sql);
		free(conn);
		return;
	}

	mtx_lock(&db->mutex);
	dlist_append(&db->sql_list, &conn->_node);
	mtx_unlock(&db->mutex);
}


int
sqlite_pool_tran_begin(DbConn *conn)
{
	char *err_msg;
	const int ret = sqlite3_exec(conn->sql, "BEGIN TRANSACTION;", NULL, NULL, &err_msg);
	if (ret != SQLITE_OK) {
		LOG_ERRN("sqlite_pool", "sqlite3_exec[%d]: %s", conn->index, err_msg);
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
		LOG_ERRN("sqlite_pool", "(%d): sqlite3_exe&d->sql_poolc[%d]: %s", is_ok, conn->index, err_msg);
		sqlite3_free(err_msg);
	}

	return -ret;
}


/*
 * Private
 */
static int
_init_db(DbPool *d, const SqlitePoolParam *param, int index)
{
	dlist_init(&d->sql_list);

	DbConn *new_conn;
	for (int i = 0; i < param->size; i++) {
		if (_open(param->path, &new_conn) < 0)
			goto err0;
		
		new_conn->index = index;
		new_conn->_in_pool = 1;
		dlist_append(&d->sql_list, &new_conn->_node);
	}

	if (mtx_init(&d->mutex, mtx_plain) != thrd_success) {
		LOG_ERRN("sqlite_pool", "%s", "mtx_init: failed");
		goto err0;
	}

	d->path = param->path;
	return 0;

err0:
	_deinit_db(d);
	return -1;
}


static void
_deinit_db(DbPool *d)
{
	DListNode *node;
	while ((node = dlist_pop(&d->sql_list)) != NULL) {
		DbConn *const conn = FIELD_PARENT_PTR(DbConn, _node, node);
		sqlite3_close(conn->sql);

		free(conn);
	}

	mtx_destroy(&d->mutex);
}


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
