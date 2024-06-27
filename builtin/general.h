#ifndef __BUILTIN_GENERAL_H__
#define __BUILTIN_GENERAL_H__


#include "../tg.h"
#include "../tg_api.h"


void general_start(TgApi *t, const TgMessage *message, const char args[]);
void general_help(TgApi *t, const TgMessage *message, const char args[]);


#endif
