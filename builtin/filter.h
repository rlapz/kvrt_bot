#ifndef __BUILTIN_FILTER_H
#define __BUILTIN_FILTER_H


#include "../module.h"
#include "../tg.h"


void filter_text(Module *m, const TgMessage *msg, const char text[]);


#endif
