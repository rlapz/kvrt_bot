#ifndef __DB_H__
#define __DB_H__


#include <stddef.h>
#include <stdint.h>
#include <sqlite3.h>

#include "model.h"
#include "tg.h"
#include "util.h"


enum {
	DB_DATA_TYPE_NULL = 0,
	DB_DATA_TYPE_INT,
	DB_DATA_TYPE_INT64,
	DB_DATA_TYPE_TEXT,
};

/*
 * negative value on error
 */
enum {
	DB_RET_OK   = SQLITE_OK,
	DB_RET_DONE = SQLITE_DONE,
	DB_RET_ROW  = SQLITE_ROW,
	DB_RET_ERR  = -SQLITE_ERROR,
	DB_RET_BUSY = -SQLITE_BUSY,
};

typedef struct db_arg {
	int type;
	union {
		int         int_;
		int64_t     int64;
		const char *text;
	};
} DbArg;

typedef struct db_out_field_text {
	char   *cstr;
	size_t  len;
	size_t  size;
} DbOutFieldText;

/* the callee may change the result type to DB_DATA_TYPE_NULL */
typedef struct db_out_field {
	int type;
	union {
		int            *int_;
		int64_t        *int64;
		DbOutFieldText  text;
	};
} DbOutField;

typedef struct db_out {
	int         len;
	DbOutField *fields;
} DbOut;

typedef struct db_conn DbConn;

int     db_init(const char path[], int conn_pool_size);
void    db_deinit(void);
DbConn *db_conn_get(void);
DbConn *db_conn_get_no_wait(void);
void    db_conn_put(DbConn *conn);
int     db_conn_tran_begin(DbConn *conn);
int     db_conn_tran_end(DbConn *conn, int is_ok);
int     db_conn_exec(DbConn *conn, const char query[], const DbArg args[], int args_len,
		     const DbOut out[], int out_len);
int     db_conn_changes(DbConn *conn);
int     db_conn_busy_timeout(DbConn *conn, int timeout_s);


#endif
