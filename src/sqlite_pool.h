#ifndef __SQLITE_POOL_H__
#define __SQLITE_POOL_H__


#include <sqlite3.h>

#include "util.h"


typedef struct sqlite_pool_param {
	int         size;
	const char *path;
} SqlitePoolParam;


typedef struct db_conn {
	int      index;
	sqlite3 *sql;

	/* internal */
	DListNode _node;
	int       _in_pool;
} DbConn;


int  sqlite_pool_init(const SqlitePoolParam params[], int len);
void sqlite_pool_deinit(void);

DbConn *sqlite_pool_get(int index);
void    sqlite_pool_put(DbConn *conn);

int sqlite_pool_tran_begin(DbConn *conn);
int sqlite_pool_tran_end(DbConn *conn, int is_ok);


#endif
