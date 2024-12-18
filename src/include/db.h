#ifndef __DB_H__
#define __DB_H__


#include <stdint.h>
#include <stddef.h>
#include <sqlite3.h>

#include <tg.h>
#include <model.h>
#include <util.h>


enum {
	DB_DATA_TYPE_NULL = 0,
	DB_DATA_TYPE_INT,
	DB_DATA_TYPE_INT64,
	DB_DATA_TYPE_TEXT,
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

/* the callee maybe change result type -> DB_DATA_TYPE_NULL */
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


typedef struct db {
	const char *path;
	sqlite3    *sql;
} Db;

int  db_init(Db *d, const char path[]);
void db_deinit(Db *d);
int  db_transaction_begin(Db *d);
int  db_transaction_end(Db *d, int is_ok);
int  db_exec(Db *d, const char query[], const DbArg args[], int args_len, const DbOut out[], int out_len);
int  db_changes(Db *s);


#endif
