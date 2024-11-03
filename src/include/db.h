#ifndef __DB_H__
#define __DB_H__


#include <stdint.h>
#include <stddef.h>
#include <sqlite3.h>

#include <tg.h>
#include <entity.h>
#include <util.h>


typedef struct db {
	const char *path;
	sqlite3    *sql;
} Db;

int  db_init(Db *d, const char path[]);
void db_deinit(Db *d);
int  db_transaction_begin(Db *d);
int  db_transaction_end(Db *d, int is_ok);


/*
 * return:
 * <0: error
 *  0: success, 0 row
 * >0: success, >= 1 row(s)
 *
 */
int db_admin_add(Db *d, const EAdmin admin_list[], int admin_list_len);
int db_admin_get_privileges(Db *d, TgChatAdminPrivilege *privs, int64_t chat_id, int64_t user_id);
int db_admin_clear(Db *d, int64_t chat_id);

int db_module_extern_toggle(Db *d, int64_t chat_id, const char name[], int is_enable);
int db_module_extern_get(Db *d, const EModuleExtern *mod, int64_t chat_id, const char name[]);

int db_cmd_message_set(Db *d, int64_t chat_id, int64_t user_id, const char name[], const char message[]);
int db_cmd_message_get(Db *d, const ECmdMessage *msg, int64_t chat_id, const char name[]);
int db_cmd_message_get_message(Db *d, char buffer[], size_t size, int64_t chat_id, const char name[]);


#endif
