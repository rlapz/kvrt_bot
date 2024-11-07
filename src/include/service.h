#ifndef __SERVICE_H__
#define __SERVICE_H__


#include <stdint.h>
#include <model.h>
#include <db.h>


typedef struct service {
	Str str;
	Db  db;
} Service;

int  service_init(Service *s, const char db_path[]);
void service_deinit(Service *s);


/*
 * Admin
 */
int service_admin_add(Service *s, const Admin admin_list[], int len);
int service_admin_get_privileges(Service *s, int64_t chat_id, int64_t user_id);
int service_admin_clear(Service *s, int64_t chat_id);
int service_admin_reload(Service *s, const Admin admin_list[], int len);


/*
 * CmdMessage
 */
int service_cmd_message_set(Service *s, const CmdMessage *msg);
int service_cmd_message_get(Service *s, CmdMessage *msg);
int service_cmd_message_get_message(Service *s, CmdMessage *msg);
int service_cmd_message_get_list(Service *s, int64_t chat_id, CmdMessage msgs[], int len, int offt);


/*
 * ModuleExtern
 */
int service_module_extern_setup(Service *s, int64_t chat_id);
int service_module_extern_toggle(Service *s, const ModuleExtern *mod, int is_enable);
int service_module_extern_toggle_nsfw(Service *s, int64_t chat_id, int is_enable);
int service_module_extern_get(Service *s, ModuleExtern *mod);


#endif
