#ifndef __MODULE_H__
#define __MODULE_H__


#include <stdint.h>
#include <json.h>

#include <update.h>
#include <tg_api.h>
#include <model.h>
#include <util.h>


int module_builtin_exec(Update *update, const ModuleParam *param);
int module_extern_exec(Update *update, const ModuleParam *param);


#endif
