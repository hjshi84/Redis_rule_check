#include <stdio.h>  
#include <stdlib.h>  
#include <time.h>  
#include <string.h>  
#include <assert.h>  
#include <sys/types.h>  
#include <sys/ipc.h>  
#include <sys/msg.h>  
#include <sys/stat.h>  
#include <unistd.h>  
#include <errno.h>
#include <uuid/uuid.h>  
#include "redis.h"
#include "md5.h"
#include "cJSON.h"
//"127.0.0.1"
//"10.200.46.245"


void doTestSub()
{
	time_t start, finish;
   	double  duration;
	redisContext* context = redisConnect("127.0.0.1", 6379);  
	    if ( context->err)  
	    {  
		redisFree(context);  
		printf("Connect to redisServer faile\n");  
		return ;  
	    }  
        printf("Connect to redisServer Success\n");  

	redisReply *reply;
	reply = redisCommand(context,"SUBSCRIBE sensorDB");
	freeReplyObject(reply);
	int num=0;
	while(redisGetReply(context,(void **)&reply) == REDIS_OK) {
		if (num==0)
		{
			time(&start);
		}	
		num++;
		freeReplyObject(reply);
		if(num>=10004)
		{
			time(&finish);
			duration=(double)(finish - start);
			printf("total time is : %f \n",(double)duration);
			num=0;
			break;
		}
	}

}

void add_event(char *event_info)
{
	createConnect();
	cJSON *event=cJSON_Parse(event_info);
	
	//get event hash id 
	cJSON *uid=cJSON_GetObjectItem(event,"uid");
	cJSON *state=cJSON_GetObjectItem(event,"state");
	
	char *total=(char *)calloc(strlen(uid->valuestring)+strlen(state->valuestring)+2,sizeof(char));
	strcat(total,uid->valuestring);
	strcat(total,"~");
	strcat(total,state->valuestring);
	
	//get event identification code
	char *event_hash=NULL;
	event_hash=get_hash_value(total);
	printf("total is:%s\n",total);
	printf("event id is:%s\n",event_hash);
	free(total);
	
	//check if hash exists
	redisReply *reply;
	reply= (redisReply*)redisCommand(context, "sismember event:list %s",event_hash);
	if (reply->type==6)
	{
		printf("Database something wrong!\n");
		goto end;
	}
	else if (reply->integer==1)
	{
		printf("There is already an event in DB. Please change the config!\n");
		goto end;
	}
	freeReplyObject(reply);
	
	//if not exists

	int event_num=cJSON_GetArraySize(event);
	cJSON *attr;
	int i=0;
	int k=0;
	redisAppendCommand(context, "sadd event:list %s",event_hash);
	redisAppendCommand(context, "hset device:%s:stateslist %s %s",uid->valuestring,state->valuestring,event_hash);
	k+=2;
	//if no error found then add user-related info to redis
	for(;i<event_num;i++)
	{
		attr=cJSON_GetArrayItem(event,i);
		k++;
		if (!strcmp(attr->string,"uid")&&!strcmp(attr->string,"state")&&!strcmp(attr->string,"enable"))
		{
			redisAppendCommand(context, "hset event:%s:value %s %s",event_hash,attr->string,attr->valuestring);
		}
		else
		{
			redisAppendCommand(context, "hset user:%s:value %s %s",event_hash,attr->string,attr->valuestring);
		}	
	}
	
	for(i=0;i<k;i++)
	{
		redisGetReply(context,(void **)&reply);
		freeReplyObject(reply);
	}
	
end:	free(event_hash);
	cJSON_Delete(event);
	closeConnect();
}

void add_action(char *action_info)
{
	createConnect();
	cJSON *action=cJSON_Parse(action_info);
	
	//get action hash id 
	cJSON *url=cJSON_GetObjectItem(action,"url");
	cJSON *body=cJSON_GetObjectItem(action,"body");
	char *total=(char *)calloc(strlen(url->valuestring)+strlen(body->valuestring)+2,sizeof(char));
	strcat(total,url->valuestring);
	strcat(total,"~");
	strcat(total,body->valuestring);
	
	
	
	
	//get rule identification code
	char *action_hash=NULL;
	action_hash=get_hash_value(total);
	printf("action id is:%s\n",action_hash);
	free(total);
	
	//check if hash exists
	redisReply *reply;
	reply= (redisReply*)redisCommand(context, "sismember action:list %s",action_hash);
	if (reply->integer==1&&reply->type!=6)
	{
		printf("There is already an action in DB. Please change the config!\n");
		free(action_hash);
		cJSON_Delete(action);
		freeReplyObject(reply);
		return;
	}
	freeReplyObject(reply);
	
	//if not exists

	int action_num=cJSON_GetArraySize(action);
	cJSON *attr;
	int i=0;
	redisAppendCommand(context, "sadd action:list %s",action_hash);
	for(;i<action_num;i++)
	{
		attr=cJSON_GetArrayItem(action,i);
		redisAppendCommand(context, "hset action:%s:value %s %s",action_hash,attr->string,attr->valuestring);	
	}
	
	for(i=0;i<action_num+1;i++)
	{
		redisGetReply(context,(void **)&reply);
		freeReplyObject(reply);
	}
	
	cJSON_Delete(action);
	free(action_hash);
	closeConnect();
}

void add_rules(char *rule_info)
{
	createConnect();
	int i=0;
	redisReply* r;	
	
	//get rule info, should be one once , however write multi rules comes in together
	cJSON *rule=cJSON_Parse(rule_info);
	int rule_num=cJSON_GetArraySize(rule);
	
	
	//get events and actions and add them to redis
	
	for(i=0;i<rule_num;i++)
	{
		//each rule contains multi events and actions
		cJSON *rule_content=cJSON_GetArrayItem(rule,i);
		
		//get events, and actions and get the hashvalue
		cJSON *events=cJSON_GetObjectItem(rule_content,"events");
		cJSON *actions=cJSON_GetObjectItem(rule_content,"actions");
		char *temp_events=cJSON_PrintUnformatted(events);
		char *temp_actions=cJSON_PrintUnformatted(actions);
		char *total=(char *)calloc(strlen(temp_events)+strlen(temp_actions)+2,sizeof(char));
		strcat(total,temp_events);
		strcat(total,"~");
		strcat(total,temp_actions);
		
		//get rule identification code
		char *rule_hash=NULL;
		rule_hash=get_hash_value(total);
		printf("rule id is:%s\n",rule_hash);
		free(total);
		
		//check if rule exists
		redisReply *reply;
		reply= (redisReply*)redisCommand(context, "sismember rule:list %s",rule_hash);
		if (reply->integer==1&&reply->type!=6)
		{
			printf("There is already an Rule in DB. Please change the config!\n");
			free(temp_events);
			free(temp_actions);
			free(rule_hash);
			freeReplyObject(reply);
			continue;
		}
		freeReplyObject(reply);
		
		//add rule related info to the redis
		
		int total_info_set=cJSON_GetArraySize(rule_content);
		int j=0;
		int k=0;
		for(;j<total_info_set;j++)
		{
			cJSON *info_body=cJSON_GetArrayItem(rule_content,j);
			char *name=info_body->string;
			char *value=info_body->valuestring;
			
			if(!strcmp(name,"events")||!strcmp(name,"actions"))
			{
				continue;
			}
			else
			{
				//add other info to redis
				k++;
				redisAppendCommand(context, "hset rule:%s:value %s %s",rule_hash,name,value);
			}
		}
		redisAppendCommand(context, "sadd rule:list %s",rule_hash);
		
		for(j=0;j<k+1;j++)
		{
			redisGetReply(context,(void **)&reply);
			freeReplyObject(reply);
		}
		
		//add events and actions to redis
		//add events
		int total_event_num=cJSON_GetArraySize(events);
		for(j=0;j<total_event_num;j++)
		{
			cJSON *each_event=cJSON_GetArrayItem(events,j);
			cJSON *event_hash=cJSON_GetObjectItem(each_event,"id");
			
			redisAppendCommand(context, "sadd event:%s:rules %s",event_hash->valuestring,rule_hash);
			redisAppendCommand(context, "sadd rule:%s:events %s",rule_hash,event_hash->valuestring);
			
			redisGetReply(context,(void **)&reply);
			freeReplyObject(reply);
			redisGetReply(context,(void **)&reply);
			freeReplyObject(reply);
			
		}
		
		//add actions
		int total_action_num=cJSON_GetArraySize(actions);
		for(j=0;j<total_action_num;j++)
		{
			cJSON *each_action=cJSON_GetArrayItem(actions,j);
			cJSON *action_hash=cJSON_GetObjectItem(each_action,"id");

			redisAppendCommand(context, "sadd rule:%s:actions %s",rule_hash,action_hash->valuestring);
			
			redisGetReply(context,(void **)&reply);
			freeReplyObject(reply);

		}
		free(temp_events);
		free(temp_actions);
		free(rule_hash);
	
	}	
	
	cJSON_Delete(rule);
	closeConnect();
}

char *get_uuid()
{
	uuid_t uuid;
    	char *str=(char *)malloc(37*sizeof(char));
 
    	uuid_generate(uuid);
    	uuid_unparse(uuid, str);
 
    	printf("%s\n", str);
 
	return 0;
}

int combine_user(const char *combine_info)
{
	createConnect();
	//Generate an uuid code and use it as a list for each user
	//because we use hashset (user:$hashid) everytime we will update the content immediately
	
	char *combine_hash=get_uuid();
	
	cJSON *combine=cJSON_Parse(combine_info);
	int combine_num=cJSON_GetArraySize(combine);
	
	redisReply *reply;
	
	int i=0;
	for(;i<combine_num;i++)
	{
		cJSON *combine_content=cJSON_GetArrayItem(combine,i);
		cJSON *id=cJSON_GetObjectItem(combine_content,"uid");
		cJSON *app=cJSON_GetObjectItem(combine_content,"app");
		
		//get app_hash and user hash
		char *total=(char *)calloc(strlen(id->valuestring)+strlen(app->valuestring)+2,sizeof(char));
		strcat(total,id->valuestring);
		strcat(total,"~");
		strcat(total,app->valuestring);
		
		char *user_hash=NULL;
		user_hash=get_hash_value(total);
		free(total);
		char *app_hash=NULL;
		app_hash=get_hash_value(app->valuestring);
		
		//check if user_hash is shared
		reply=(redisReply*)redisCommand(context, "SISMEMBERS app:%s:shareduser %s",app_hash,user_hash); 
		
		if(reply->type!=6&&reply->integer==1)	
		{	
			redisReply *r;
			redisAppendCommand(context, "sadd %s %s",combine_hash,combine_content->string);
			redisAppendCommand(context, "hset user:%s:value combine %s",combine_content->string,combine_hash);
			redisGetReply(context,(void **)&r);
			freeReplyObject(r);
			redisGetReply(context,(void **)&r);
			freeReplyObject(r);
		}
		else
		{
			printf("Cannot combine user_id, check your config carefully\n");
		}
		freeReplyObject(reply);
		free(user_hash);
		free(app_hash);
		
	}
	
	cJSON_Delete(combine);
	closeConnect();
	return 0;
}

char * redis_to_json()
{
	return NULL;
}

int add_user(const char* user_info)
{
	createConnect();
	
	cJSON *user=cJSON_Parse(user_info);
	int user_num=cJSON_GetArraySize(user);
	int i;
	
	redisReply *reply;
	for(i=0;i<user_num;i++)
	{
		cJSON *user_content=cJSON_GetArrayItem(user,i);
		//need to get unicode id
		cJSON *id=cJSON_GetObjectItem(user_content,"uid");
		cJSON *app=cJSON_GetObjectItem(user_content,"app");
		
		char *total=(char *)calloc(strlen(id->valuestring)+strlen(app->valuestring)+2,sizeof(char));
		strcat(total,id->valuestring);
		strcat(total,"~");
		strcat(total,app->valuestring);
		
		char *user_hash=NULL;
		user_hash=get_hash_value(total);
		free(total);
		char *app_hash=NULL;
		app_hash=get_hash_value(app->valuestring);
		
		int total_info_set=cJSON_GetArraySize(user_content);
		int j=0;
		int k=0;
		for(;j<total_info_set;j++)
		{
			cJSON *info_body=cJSON_GetArrayItem(user_content,j);
			char *name=info_body->string;
			char *value=info_body->valuestring;
			k++;

			if(!strcmp(name,"shared"))
			{
				if(!strcmp(value,"0"))
					redisAppendCommand(context, "sadd app:%s:shareduser %s",app_hash,user_hash);
				else
					redisAppendCommand(context, "sadd app:%s:privateuser %s",app_hash,user_hash);
			}
			else if (!strcmp(name,"app"))
			{
				redisAppendCommand(context, "hset user:%s:value _app %s",user_hash,app_hash);
			}
			else
			{
				redisAppendCommand(context, "hset user:%s:value %s %s",user_hash,name,value);
			}

		}
		
		for(j=0;j<k;j++)
		{
			redisGetReply(context,(void **)&reply);
			freeReplyObject(reply);
		}
		free(user_hash);
		free(app_hash);
		
	}

	cJSON_Delete(user);
	closeConnect();
	return 0;

}


int add_application(const char* app_info)
{
	createConnect();
	
	cJSON *app=cJSON_Parse(app_info);
	int app_num=cJSON_GetArraySize(app);
	int i;
	
	redisReply *reply;
	for(i=0;i<app_num;i++)
	{
		cJSON *app_content=cJSON_GetArrayItem(app,i);
		//need to get unicode id
		cJSON *uaid=cJSON_GetObjectItem(app_content,"app");
		
		char *app_hash=NULL;
		app_hash=get_hash_value(uaid->valuestring);
		
		cJSON *roles=cJSON_GetObjectItem(app_content,"roles");
		
		int total_info_set=cJSON_GetArraySize(app_content);
		int j=0;
		int k=0;
		for(;j<total_info_set;j++)
		{
			cJSON *info_body=cJSON_GetArrayItem(app_content,j);
			char *name=info_body->string;
			char *value=info_body->valuestring;
			
			if(!strcmp(name,"roles"))
			{
				continue;
			}
			else
			{
				//add other info to redis
				k++;
				redisAppendCommand(context, "hset app:%s:value %s %s",app_hash,name,value);
			}
		}
		
		int total_roles_num=cJSON_GetArraySize(roles);
		for(j=0;j<total_roles_num;j++)
		{
			cJSON *info_body=cJSON_GetArrayItem(roles,j);
			char *name=info_body->string;
			char *value=info_body->valuestring;
			redisAppendCommand(context, "hset app:%s:roles %s %s",app_hash,name,value);
			k++;
		}
		
		
		redisAppendCommand(context, "sadd app:list %s",app_hash);//will hashset be better? For we can put name in it
		k++;
		for(j=0;j<k;j++)
		{
			redisGetReply(context,(void **)&reply);
			freeReplyObject(reply);
		}
		
		free(app_hash);
		
	}
	
	cJSON_Delete(app);
	closeConnect();
	return 0;
}


void rule_detect(const char* event_id){
	createConnect();
	redisReply* r;

        //get from event:id:rules
    	r = (redisReply*)redisCommand(context, "SMEMBERS event:%s:rules",event_id); 
    	int i=0;
	for(;i<r->elements;i++)
	{
		//get rules:id:events set and compare to the active pool
		//use pipeline would be better 
		//optimize sdiff , first check whether two keys exists!
		redisReply *reply;
		reply= (redisReply*)redisCommand(context, "SDIFF rule:%s:events active:events",r->element[i]->str);
		if(reply->elements==0&&reply->type!=6)
			printf("results: %s\n",r->element[i]->str);		
		freeReplyObject(reply);
		
	}
    	freeReplyObject(r);
    	closeConnect();
}

void device_state_income(const char* device_info)//const char* uid,const char* state)
{
	createConnect();	
	
	cJSON *json=cJSON_Parse(device_info);
	//only one device state will come at each time! however we try to catch one more json 
	int device_all=cJSON_GetArraySize(json);
	int temp_loc=0;
	for(;temp_loc<device_all;temp_loc++)
	{
		cJSON *temp_dev=cJSON_GetArrayItem(json,temp_loc);
		cJSON *temp_uid=cJSON_GetObjectItem(temp_dev,"uid");
		cJSON *temp_state=cJSON_GetObjectItem(temp_dev,"state");
		char *uid=temp_uid->valuestring;
		char *state=temp_state->valuestring;
		printf("uid is %s, state is %s\n\n\n\n",uid,state);
		const char* command1;
		char* event_id=NULL;
		int pass=0;//pass the repeat check;
		redisReply* r;

		redisAppendCommand(context, "keys device:%s",uid);
		redisAppendCommand(context, "hget device:%s state",uid);
		//redisAppendCommand(context, "hset device:%s state %s",uid,state);
		redisGetReply(context,(void **)&r);

		if(r->type==REDIS_REPLY_ARRAY&&r->elements==0&&r->type!=6)
		{
			printf("first discovered device!!!\n");
			pass=1;
		}
		freeReplyObject(r);
	
		redisGetReply(context,(void **)&r);

		if (!pass&&!strcmp(r->str,state)&&r->type!=6)
		{
			printf("nothing changed!!\n");
			freeReplyObject(r);
			cJSON_Delete(json);
			closeConnect();
			return;
		}
		freeReplyObject(r);

		
		//add otherinfo of device if first add
		if(1)
		{
			int whole_info=cJSON_GetArraySize(temp_dev);
			int tempv=0;
			for(;tempv<whole_info;tempv++)
			{
				cJSON *temp_json=cJSON_GetArrayItem(temp_dev,tempv);
				char *json_name=temp_json->string;
				char *json_value=temp_json->valuestring;
				redisAppendCommand(context, "hset device:%s %s %s",uid,json_name,json_value);
			}
			tempv=0;
			redisReply* temp_r;
			for(;tempv<whole_info;tempv++)
			{
				redisGetReply(context,(void **)&temp_r);
				freeReplyObject(temp_r);
			}
		}
	
		redisAppendCommand(context, "HGETALL device:%s:stateslist",uid);


		redisGetReply(context,(void **)&r);
		int i=0;
		int res=0;
		int num=r->elements;
		if(r->type!=6)
		{
			for(;i<num;i=i+2)
			{
				char *vals=(r->element[i])->str;
				char *event_vals=(r->element[i+1])->str;
				if(res==0&&!strcmp(vals,state)&&r->type!=6)
				{
					event_id=(char *)malloc(sizeof(char)*(strlen(event_vals)+1));
					strcpy(event_id,event_vals);
					res=1;
					redisAppendCommand(context, "SADD active:events %s",event_id);
				}
				else if(r->type!=6)
				{
					redisAppendCommand(context, "SREM active:events %s",event_vals);
				}
			}	
		}
	
		freeReplyObject(r);
	
		for(i=0;i<num;i=i+2)
		{
			redisReply *reply;
			redisGetReply(context,(void **)&reply);
			freeReplyObject(reply);
		}
	
	
		if (event_id!=NULL)	
		{
			redisReply *reply=(redisReply*)redisCommand(context, "SADD active:events %s",event_id);
			freeReplyObject(reply);
			rule_detect(event_id);
		}
	
		free(event_id);
	
	}
	cJSON_Delete(json);
	closeConnect();
}

int createConnect()
{
	redisReply *reply;
	if (context==NULL)
	{
		context = redisConnect("127.0.0.1", 6379);  
		//context = redisConnect("10.200.43.146", 6379);  
		if (context->err)  
		{  
			goto error1;
		}
		redisAppendCommand(context, "AUTH redis");
		redisAppendCommand(context, "SELECT 1");
		redisGetReply(context,(void **)&reply);
		freeReplyObject(reply);
		redisGetReply(context,(void **)&reply);
		freeReplyObject(reply);
		return 0;  
	}  
	else 
	{
		printf("Connection exists!!\n");
		return 1;
	}

error1:
	redisFree(context);  
	printf("Connect to redisServer faile\n");  
	exit(EXIT_FAILURE);
	return -1;
}


int closeConnect()
{
	redisFree(context);
	context=NULL;
	return 1;
}

int copestring(char **dest,char *ori)
{
	(*dest)=(char *)malloc(sizeof(char)*(1+strlen(ori)));
	strcpy(*dest,ori);
	return 1;
}
        
int main(int argc,char *argv[])  
{  
	
	createConnect();



    	double t_start,t_end;
    	
    	//t_start=clock();
	add_event("{\"type\":\"PIR\",\"uid\":\"001\",\"state\":\"1\",\"admin\":\"aaa\"}");
	add_event("{\"type\":\"PIR\",\"uid\":\"002\",\"state\":\"1\",\"admin\":\"aaa\"}");
	add_action("{\"url\":\"hjshi84@163.com\",\"body\":\"give my five\",\"others\":\"things\",\"admin\":\"aaa\"}");
	add_rules("[{\"name\": \"1\",\"events\": [{\"uid\":\"001\",\"id\":\"ae2bac2e4b4da805d01b2952d7e35ba4\",\"name\":\"1\"},{\"uid\":\"002\",\"id\":\"d9f5e405a7f74ed652a8f0b31a87f636\",\"name\":\"2\"}],\"actions\": [{\"id\":\"1\", \"name\":\"action_a\"},{\"id\":\"5\", \"name\":\"action_b\"}] ,\"enable\": \"0\",\"repeatable\": \"0\",\"time_constr\":  \"this\",\"admin\": \"admin@inesa.com\"}]");
	////device_state_income("[{\"name\":\"People%20In\",\"uid\":\"10.200.45.148\",\"state\":\"100\"}]");    
	device_state_income("[{\"name\":\"People%20In\",\"uid\":\"001\",\"state\":\"1\"}]"); 
	device_state_income("[{\"name\":\"People%20In\",\"uid\":\"002\",\"state\":\"1\"}]"); 
	//t_end=clock();
		
 	//printf("%lf\n", (double)(t_end-t_start)/CLOCKS_PER_SEC);
 	closeConnect();
	return 0;  
}  


/*//use pip
int main(int argc,char *argv[])  
{

	if (createConnect()==-1)
	{
		printf("Cannot Connect To Redis Server! Please Check Your Connection!\n");
		return -1;
	}
	
	int ret;  
        int msg_id;  
  	
        msg_id = msgget((key_t)1002,0666|IPC_CREAT);  
        if(msg_id == -1){  
                printf("msgget failed\n");  
                closeConnect();
                return -1;  
        }  

        msg_buf mb;  

	char *content;
  	while(1)
  	{
		ret = msgrcv(msg_id,(void *)&mb, sizeof(((msg_buf *)0)->msg),0,0);  
		if(ret == -1){  
		        printf("msgrcv failed:%d\n",errno);  
		        closeConnect();
		        return -1;  
		}  
		
		switch(mb.msg_type)
		{	
			//device come
			case 3:
				copestring(&content,mb.msg);
				device_state_income(content);
				free(content);
				content=NULL;
				break;
			//add rule
			case 4:
				copestring(&content,mb.msg);
				add_rules(content);
				free(content);
				content=NULL;
				break;
			//add events
			case 5:
				copestring(&content,mb.msg);
				add_rules(content);
				free(content);
				content=NULL;
				break;
			//add action
			case 6:
				copestring(&content,mb.msg);
				add_rules(content);
				free(content);
				content=NULL;
				break;
			//del rule
			case 7:
				break;
			//del event
			case 8:
				break;
			//del action
			case 9:
				break;
		}
		

			
        }
        
        if(msgctl(msg_id, IPC_RMID, 0) == -1)  
    	{  
        	fprintf(stderr, "msgctl(IPC_RMID) failed\n");  
        	exit(EXIT_FAILURE);  
    	}  
        
        closeConnect();
        return 0;  
}

*/


