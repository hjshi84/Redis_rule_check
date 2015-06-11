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

void device_state_income(const char* content);
void rule_detect(const char* event_id);
void add_rules(char *rule_info);
int createConnect();
int closeConnect();
#endif
