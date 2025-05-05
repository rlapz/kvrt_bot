#ifndef __WEBHOOK_H__
#define __WEBHOOK_H__


#include "config.h"


int webhook_set(const Config *cfg);
int webhook_del(const Config *cfg);
int webhook_info(const Config *cfg);


#endif
