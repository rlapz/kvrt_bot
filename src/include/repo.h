#ifndef __REPO_H__
#define __REPO_H__


#include <stdint.h>
#include <model.h>
#include <db.h>


typedef struct repo {
	Str str;
	Db  db;
} Repo;

int  repo_init(Repo *s, const char db_path[]);
void repo_deinit(Repo *s);


/*
 * Admin
 */
int repo_admin_add(Repo *s, const Admin admin_list[], int len);
int repo_admin_get_privileges(Repo *s, int64_t chat_id, int64_t user_id);
int repo_admin_clear(Repo *s, int64_t chat_id);
int repo_admin_reload(Repo *s, const Admin admin_list[], int len);


/*
 * CmdMessage
 */
int repo_cmd_message_set(Repo *s, const CmdMessage *msg);
int repo_cmd_message_get_message(Repo *s, CmdMessage *msg);
int repo_cmd_message_get_list(Repo *s, int64_t chat_id, CmdMessage msgs[], int len,
			      int offt, int *max_len);


/*
 * ModuleExtern
 */
int repo_module_extern_setup(Repo *s, int64_t chat_id);
int repo_module_extern_toggle(Repo *s, const ModuleExtern *mod, int is_enable);
int repo_module_extern_toggle_nsfw(Repo *s, int64_t chat_id, int is_enable);
int repo_module_extern_get(Repo *s, ModuleExtern *mod);


#endif
