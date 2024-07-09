#ifndef __BUILTIN_ANTI_LEWD_H
#define __BUILTIN_ANTI_LEWD_H


#include "../tg.h"
#include "../module.h"


void anti_lewd_detect_text(Module *m, const TgMessage *msg, const char text[]);


#endif
