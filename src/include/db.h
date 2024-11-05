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

typedef struct db_out_item_text {
	char   *cstr;
	size_t  len;
	size_t  size;
} DbOutItemText;

typedef struct db_out_item {
	int type;
	union {
		int           *int_;
		int64_t       *int64;
		DbOutItemText  text;
	};
} DbOutItem;

typedef struct db_out {
	int        len;
	DbOutItem *items;
} DbOut;


typedef struct db {
	const char *path;
	sqlite3    *sql;
} Db;

int  db_init(Db *d, const char path[]);
void db_deinit(Db *d);
int  db_transaction_begin(Db *d);
int  db_transaction_end(Db *d, int is_ok);
int  db_exec(Db *d, const char query[], const DbArg args[], int args_len, DbOut out[], int out_len);
int  db_changes(Db *s);


#endif
