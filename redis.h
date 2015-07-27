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
static char json_value[100];

static int closeCal=0;
#define CLOSETHRES 1000

char* results=NULL;


int combine_user(const char *combine_info);
int add_user(const char* user_info);
int add_application(const char* app_info);
char* device_state_income(const char* content);
char* rule_detect(const char* event_id);
void add_rules(const char *rule_info);

int delete_event_by_user(char *enent_hash,char *app, char *user);
int delete_action_by_user(char *action_hash,char *app, char *user);
int delete_rule_by_user(char *rule_hash,char *app, char *user);


char* get_events_total(char *app, char* userid);
char* get_actions_total(char *app, char* userid);
char* get_rules_total(char *app, char* userid);
char* get_events_totalhash(char *app_hash, char* user_hash);
char* get_actions_totalhash(char *app_hash, char* user_hash);
char* get_rules_totalhash(char *app_hash, char* user_hash);

	
char* get_events_bind(char *app, char* userid);
char* get_actions_bind(char *app, char* userid);
char* get_rules_bind(char *app, char* userid);
char* get_events_bindhash(char *app_hash,char* user_hash);
char* get_actions_bindhash(char *app_hash,char* user_hash);
char* get_rules_bindhash(char *app_hash,char* user_hash);

int delete_apphash(char *app_hash);
int delete_userhash(char *user_hash);
int delete_action(char *action_hash);
int delete_event(char *event_hash);
int delete_rule(char *rule_hash);
int createConnect();
int closeConnect();

void free_result();

#endif

/*as reference
define REDIS_REPLY_STRING 1
define REDIS_REPLY_ARRAY 2
define REDIS_REPLY_INTEGER 3
define REDIS_REPLY_NIL 4
define REDIS_REPLY_STATUS 5
define REDIS_REPLY_ERROR 6

*/
