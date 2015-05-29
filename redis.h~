#ifndef _REDIS_TA_H
#define _REDIS_TA_H

#include <hiredis/hiredis.h>  
#include "list.h"
#define MAX_DEV_LEN 100
typedef struct {  
        long int msg_type; 
        char msg[512];  
}msg_buf;  

static redisContext* context=NULL;

void device_state_income(const char* uid,const char* state);
void rule_detect(const char* event_id);
void add_rules(int argc,char *argv[]);
int createConnect();
int closeConnect();
#endif
