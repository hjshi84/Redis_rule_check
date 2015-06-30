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

char *get_json_value(cJSON* arg)
{
	switch (arg->type)
	{
		case cJSON_False:
			return "0";
			break;
		case cJSON_True:
			return "1";
			break;
		case cJSON_NULL:
			return "";
			break;
		case cJSON_Number:
			if (!strcmp(arg->string,"state"))
			{
				sprintf(json_value,"%d",arg->valueint);
			}
			else
			{
				sprintf(json_value,"%1.3f",arg->valuedouble);
			}
			return json_value;
			break;
		case cJSON_String:
			return arg->valuestring;
			break;
		default:
			return "UNKNOW TYPES";
			break;
	}
}

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
	cJSON *userid=cJSON_GetObjectItem(event,"userid");
	cJSON *app=cJSON_GetObjectItem(event,"app");
	cJSON *name=cJSON_GetObjectItem(event,"name");
	if(uid==NULL||state==NULL||userid==NULL||app==NULL||name==NULL)
	{
		printf("Miss arguments!\n");
		goto end1;
	}

	char *uid_value=get_json_value(uid);
	char *state_value=get_json_value(state);
	char *userid_value=	get_json_value(userid);
	char *app_value=get_json_value(app);
	char *name_value=get_json_value(name);
	
	char *total=(char *)calloc(strlen(uid_value)+strlen(state_value)+2,sizeof(char));
	strcat(total,uid_value);
	strcat(total,"~");
	strcat(total,state_value);
	
	//get rule,user,application identification code
	char *event_hash=NULL;
	event_hash=get_hash_value(total);
	printf("total is:%s\n",total);
	printf("event id is:%s\n",event_hash);
	free(total);
	
	char *user_hash=NULL;
	total=(char *)calloc(strlen(userid_value)+strlen(app_value)+2,sizeof(char));
	strcat(total,userid_value);
	strcat(total,"~");
	strcat(total,app_value);
	user_hash=get_hash_value(total);
	printf("user hash is %s\n",user_hash);
	free(total);
	
	char *app_hash=NULL;

	app_hash=get_hash_value(app_value);

	int required_role=-1;
	int event_pre_role=-1;
	//check if event exists
	int event_num=cJSON_GetArraySize(event);
	cJSON *attr;
	int i=0;
	int k=0;
	
	redisReply *reply;
	//check if add_event_user has been added and get his/her role
	redisAppendCommand(context, "sismember user:list %s",user_hash);
	redisAppendCommand(context, "hget user:%s:value role",user_hash);
	redisGetReply(context,(void **)&reply);
	if (reply->integer!=1)
	{
		printf("Database wrong or no such user!\n");
		freeReplyObject(reply);
		redisGetReply(context,(void **)&reply);
		freeReplyObject(reply);
		goto end;
	}
	freeReplyObject(reply);
	redisGetReply(context,(void **)&reply);
	if (reply->type!=1)
	{
		printf("Database wrong or no such user!\n");
		freeReplyObject(reply);
		goto end;
	}
	else
	{
		required_role=atoi(reply->str);
	}
	freeReplyObject(reply);

	reply= (redisReply*)redisCommand(context, "hget event:list %s",event_hash);

	if (reply->type==4)
	{
		//if not exists then add a new one
		
		redisAppendCommand(context, "hset event:list %s %s",event_hash,app_hash);
		redisAppendCommand(context, "hset device:%s:stateslist %s %s",uid_value,state_value,event_hash);
		redisAppendCommand(context, "HINCRBY event:%s:value referCount 1",event_hash);
		redisAppendCommand(context, "hset event:%s:value role %d",event_hash,required_role);
		redisAppendCommand(context, "hset event:%s:value userid %s",event_hash,userid_value);
		redisAppendCommand(context, "hset app:%s:events %s %d",app_hash,event_hash,required_role);
		k+=6;
	}
	else if (reply->type==1)
	{
		//if exists such events
		freeReplyObject(reply);
		redisAppendCommand(context, "hget event:%s:value role",event_hash);
		redisGetReply(context,(void **)&reply);
		if (reply->type!=1||(event_pre_role=atoi(reply->str))<required_role)
		{
			printf("Permission denied!\n");
			freeReplyObject(reply);
			goto end;
		}
		else
		{
			//need to incr referCount or not?
			freeReplyObject(reply);
			redisAppendCommand(context, "keys user:%s:event:%s",user_hash,event_hash);
			redisGetReply(context,(void **)&reply);
			if(reply->type==2&&reply->integer==0)
			{
				redisAppendCommand(context, "HINCRBY event:%s:value referCount 1",event_hash);

				k++;
			}
			
		}
	}
	else 
	{
		printf("Database something wrong event!\n");
		freeReplyObject(reply);
		goto end;
	}
	freeReplyObject(reply);

	//add all information to redis
	
	for(;i<event_num;i++)
	{
		attr=cJSON_GetArrayItem(event,i);
		char *attr_value=get_json_value(attr);
		k++;
		if (!strcmp(attr->string,"uid")||!strcmp(attr->string,"state")||!strcmp(attr->string,"app"))
		{
			redisAppendCommand(context, "hset event:%s:value %s %s",event_hash,attr->string,attr_value);
		}
		else if(!strcmp(attr->string,"role"))
		{

			if (atoi(attr_value)<required_role)
			{
				redisAppendCommand(context, "hset event:%s:value %s %s",event_hash,attr->string,required_role);
				redisAppendCommand(context, "hset app:%s:events %s %d",app_hash,event_hash,required_role);
			}
			else
			{
				redisAppendCommand(context, "hset event:%s:value %s %s",event_hash,attr->string,attr_value);
				redisAppendCommand(context, "hset app:%s:events %s %d",app_hash,event_hash,attr_value);
			}
			redisAppendCommand(context, "hset event:%s:value userid %s",event_hash,userid_value);

			k+=3;
		}
		else
		{
			redisAppendCommand(context, "hset user:%s:event:%s %s %s",user_hash,event_hash,attr->string,attr_value);
		}	
	}

	redisAppendCommand(context, "hset user:%s:events %s %s",user_hash,event_hash,name_value);

	k++;
	for(i=0;i<k;i++)
	{
		redisGetReply(context,(void **)&reply);
		freeReplyObject(reply);
	}


end:	free(event_hash);
	free(user_hash);
	free(app_hash);
end1:	cJSON_Delete(event);
	closeConnect();
}

void add_action(char *action_info)
{
	createConnect();
	cJSON *action=cJSON_Parse(action_info);
	
	//get action hash id 
	cJSON *url=cJSON_GetObjectItem(action,"url");
	cJSON *body=cJSON_GetObjectItem(action,"body");
	cJSON *userid=cJSON_GetObjectItem(action,"userid");
	cJSON *app=cJSON_GetObjectItem(action,"app");
	cJSON *name=cJSON_GetObjectItem(action,"name");
	
	if(url==NULL||body==NULL||userid==NULL||app==NULL||name==NULL)
	{
		printf("Miss arguments!\n");
		goto end1;
	}	

	char *url_value=get_json_value(url);
	char *body_value=get_json_value(body);
	char *userid_value=	get_json_value(userid);
	char *app_value=get_json_value(app);
	char *name_value=get_json_value(name);
	
	char *total=(char *)calloc(strlen(url_value)+strlen(body_value)+2,sizeof(char));
	strcat(total,url_value);
	strcat(total,"~");
	strcat(total,body_value);
	
	//get action identification code
	char *action_hash=NULL;
	action_hash=get_hash_value(total);
	printf("action id is:%s\n",action_hash);
	free(total);
	
	char *user_hash=NULL;
	total=(char *)calloc(strlen(userid_value)+strlen(app_value)+2,sizeof(char));
	strcat(total,userid_value);
	strcat(total,"~");
	strcat(total,app_value);
	user_hash=get_hash_value(total);
	free(total);
	
	char *app_hash=NULL;
	app_hash=get_hash_value(app_value);


	int required_role=-1;

	//check if hash exists
	int action_num=cJSON_GetArraySize(action);
	cJSON *attr;
	int i=0;
	int k=0;
	
	redisReply *reply;

	//check if add_action_user has been added and get his/her role
	redisAppendCommand(context, "sismember user:list %s",user_hash);
	redisAppendCommand(context, "hget user:%s:value role",user_hash);
	redisGetReply(context,(void **)&reply);
	if (reply->integer!=1)
	{
		printf("Database wrong or no such user!\n");
		freeReplyObject(reply);
		redisGetReply(context,(void **)&reply);
		freeReplyObject(reply);
		goto end;
	}
	freeReplyObject(reply);
	redisGetReply(context,(void **)&reply);
	if (reply->type!=1)
	{
		printf("Database wrong or no such user!\n");
		freeReplyObject(reply);
		goto end;
	}
	else
	{
		required_role=atoi(reply->str);
	}
	freeReplyObject(reply);

	
	redisAppendCommand(context, "hget action:list %s",action_hash);
	redisGetReply(context,(void **)&reply);
	if (reply->type==4)
	{
		//if not exists then add a new one

		redisAppendCommand(context, "hset action:list %s %s",action_hash,app_hash);
		redisAppendCommand(context, "HINCRBY action:%s:value referCount 1",action_hash);
		redisAppendCommand(context, "hset action:%s:value role %d",action_hash,required_role);
		redisAppendCommand(context, "hset action:%s:value userid %s",action_hash,userid_value);
		redisAppendCommand(context, "hset app:%s:actions %s %d",app_hash,action_hash,required_role);
		k+=5;
	}
	else if (reply->type==1)
	{
		freeReplyObject(reply);
		redisAppendCommand(context, "hget action:%s:value role",action_hash);
		redisGetReply(context,(void **)&reply);
		if (reply->type!=1||atoi(reply->str)<required_role)
		{
			printf("Permission denied!\n");
			freeReplyObject(reply);
			goto end;
		}
		else
		{
			//need to incr referCount or not?
			freeReplyObject(reply);
			redisAppendCommand(context, "keys user:%s:action:%s",user_hash,action_hash);
			redisGetReply(context,(void **)&reply);
			if(reply->type==2&&reply->integer==0)
			{
				redisAppendCommand(context, "HINCRBY action:%s:value referCount 1",action_hash);
				k++;
			}
		}
	}
	else 
	{
		printf("Database something wrong action!\n");
		freeReplyObject(reply);
		goto end;
	}
	freeReplyObject(reply);

	//if not exists
	for(;i<action_num;i++)
	{
		attr=cJSON_GetArrayItem(action,i);
		char *attr_value=get_json_value(attr);
		k++;
		if (!strcmp(attr->string,"url")||!strcmp(attr->string,"body")||!strcmp(attr->string,"app"))
		{
			redisAppendCommand(context, "hset action:%s:value %s %s",action_hash,attr->string,attr_value);	
		}
		else if(!strcmp(attr->string,"role"))
		{
			if (atoi(attr_value)<required_role)
			{
				redisAppendCommand(context, "hset action:%s:value %s %s",action_hash,attr->string,required_role);
				redisAppendCommand(context, "hset app:%s:actions %s %d",app_hash,action_hash,required_role);
			}
			else
			{
				redisAppendCommand(context, "hset action:%s:value %s %s",action_hash,attr->string,attr_value);
				redisAppendCommand(context, "hset app:%s:actions %s %d",app_hash,action_hash,attr_value);	
			}
				
			redisAppendCommand(context, "hset event:%s:value userid %s",action_hash,userid_value);
			k+=3;
		}
		else
		{
			redisAppendCommand(context, "hset user:%s:action:%s %s %s",user_hash,action_hash,attr->string,attr_value);
		}	
	}

	redisAppendCommand(context, "hset user:%s:actions %s %s",user_hash,action_hash,name_value);
	k++;
	for(i=0;i<k;i++)
	{
		redisGetReply(context,(void **)&reply);
		freeReplyObject(reply);
	}
	
end:	
	free(user_hash);
	free(app_hash);
	free(action_hash);
end1:	cJSON_Delete(action);
	closeConnect();
}

int check_role(char *type_hash,char *user_hash,char *eventoraction)
{
	//start a new connect 
	redisReply *reply;
	redisContext* tempcontext = redisConnect("10.200.43.146", 6379);  
	//redisContext* tempcontext = redisConnect("127.0.0.1", 6379);  
	
	if (tempcontext->err)  
	{  
		redisFree(tempcontext);  
		printf("Connect to redisServer faile\n");  
		exit(EXIT_FAILURE);
		return -1;
	}
	redisAppendCommand(tempcontext, "AUTH redis");
	redisAppendCommand(tempcontext, "SELECT 2");
	redisGetReply(tempcontext,(void **)&reply);
	freeReplyObject(reply);
	redisGetReply(tempcontext,(void **)&reply);
	freeReplyObject(reply);

	int res=-1;
	
	char *app_hash=NULL;
	int required_role=0;
	
	int i=0;
	int user_combined_num=0;
	redisAppendCommand(tempcontext, "hget %s:list %s",eventoraction,type_hash);
	redisAppendCommand(tempcontext, "hget %s:%s:value role",eventoraction,type_hash);
	redisAppendCommand(tempcontext, "hget user:%s:value combine",user_hash);
	//1.
	redisGetReply(tempcontext,(void **)&reply);
	if(reply->type==1)
	{
		copestring(&app_hash,reply->str);
	}
	else
	{
		freeReplyObject(reply);
		redisGetReply(tempcontext,(void **)&reply);
		freeReplyObject(reply);
		redisGetReply(tempcontext,(void **)&reply);
		freeReplyObject(reply);
		goto end;
	}
	freeReplyObject(reply);
	//2.
	redisGetReply(tempcontext,(void **)&reply);
	if (reply->type==1)
		required_role=atoi(reply->str);
	else
	{
		freeReplyObject(reply);
		redisGetReply(tempcontext,(void **)&reply);
		freeReplyObject(reply);
		goto end;
	}
	freeReplyObject(reply);
	//3.
	redisGetReply(tempcontext,(void **)&reply);
	char *combine_pool=reply->str;


	redisAppendCommand(tempcontext, "SMEMBERS %s",combine_pool);
	freeReplyObject(reply);
	//4
	redisGetReply(tempcontext,(void **)&reply);
	user_combined_num=reply->elements;
	for(;i<user_combined_num;i++)
	{
		char *user_combined_hash=reply->element[i]->str;
		redisReply *r;

		r=(redisReply*)redisCommand(tempcontext, "hget user:%s:value app",user_combined_hash);
		
		if (!strcmp(r->str,app_hash))
		{
			freeReplyObject(r);
			r=(redisReply*)redisCommand(tempcontext, "hget user:%s:value role",user_combined_hash);
			int user_role=atoi(r->str);
			if (user_role<=required_role)
			{
				freeReplyObject(r);
				res=0;
				break;
			}
			else
			{
				freeReplyObject(r);
				continue;
			}
			
		}
		freeReplyObject(r);
	}
	freeReplyObject(reply);
	
	free(app_hash);
	//end of operation and relase the resources
	redisFree(tempcontext);
	
end:
	return res;

}


void add_rules(const char *rule_info)
{
	createConnect();
	int i=0;
	redisReply* r;	
	int deleteRuleFlag=0;
	
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
		cJSON *userid=cJSON_GetObjectItem(rule_content,"userid");
		cJSON *app=cJSON_GetObjectItem(rule_content,"app");
		cJSON *enable=cJSON_GetObjectItem(rule_content,"enable");
		cJSON *name=cJSON_GetObjectItem(rule_content,"name");
		if(events==NULL||actions==NULL||userid==NULL||app==NULL||enable==NULL||name==NULL)
		{
			printf("Miss arguments!\n");
			continue;
		}
		
		char *temp_events=cJSON_PrintUnformatted(events);
		char *temp_actions=cJSON_PrintUnformatted(actions);
		char *total=(char *)calloc(strlen(temp_events)+strlen(temp_actions)+2,sizeof(char));
		strcat(total,temp_events);
		strcat(total,"~");
		strcat(total,temp_actions);

		char *rule_hash=NULL;
		rule_hash=get_hash_value(total);
		printf("rule id is:%s\n",rule_hash);
		free(total);
	

		char *userid_value=	get_json_value(userid);
		char *app_value=get_json_value(app);
		char *name_value=get_json_value(name);
		char *enable_value=get_json_value(enable);

		char *user_hash=NULL;//need to be a user in istack? check in combine
		total=(char *)calloc(strlen(userid_value)+strlen(app_value)+2,sizeof(char));
		strcat(total,userid_value);
		strcat(total,"~");
		strcat(total,app_value);
		user_hash=get_hash_value(total);
		free(total);
	
		char *app_hash=NULL;
		app_hash=get_hash_value(app_value);

		
		redisReply *reply;
		
		//check if rule exists
		
		//if add is not ok, need to run delete_rule
		int j=0;
		int k=0;
		
		reply= (redisReply*)redisCommand(context, "sismember rule:list %s",rule_hash);
		if (reply->type!=3)
		{
			printf("Database something wrong!\n");
			freeReplyObject(reply);
			goto end;
		}
		else if (reply->integer==0)
		{
			//if not exists then add a new one
			redisAppendCommand(context, "sadd rule:list %s",rule_hash);
			redisAppendCommand(context, "HINCRBY rule:%s:value referCount 1",rule_hash);
			redisAppendCommand(context, "hset rule:%s:value userid %s",rule_hash,userid_value);
			k+=3;
		}
		else
		{
			freeReplyObject(reply);
			redisAppendCommand(context, "keys user:%s:rule:%s",user_hash,rule_hash);
			redisGetReply(context,(void **)&reply);
			if(reply->type==2&&reply->integer==0)
			{
				redisAppendCommand(context, "HINCRBY rule:%s:value referCount 1",rule_hash);
				k++;
			}
		}
		freeReplyObject(reply);


		//add rule related info to the redis
		
		int total_info_set=cJSON_GetArraySize(rule_content);

		/*/*why i need to add this? remains to be a question
		redisAppendCommand(context, "sadd app:%s:rules %s",app_hash,rule_hash);
		k++;
		**/
		for(;j<total_info_set;j++)
		{
			cJSON *info_body=cJSON_GetArrayItem(rule_content,j);
			char *name=info_body->string;
			char *value=get_json_value(info_body);

			if(!strcmp(name,"events")||!strcmp(name,"actions")||!strcmp(name,"enable")||!strcmp(name,"userid"))
			{
				continue;
			}
			else
			{
				//add other info to redis
				k++;
				redisAppendCommand(context, "hset user:%s:rule:%s %s %s",user_hash,rule_hash,name,value);
			}
		}
		
		redisAppendCommand(context, "hset user:%s:rules %s %s",user_hash,rule_hash,name_value);
		k++;

		//add events and actions to redis
		//add events
		int total_event_num=cJSON_GetArraySize(events);
		for(j=0;j<total_event_num;j++)
		{
			cJSON *each_event=cJSON_GetArrayItem(events,j);
			cJSON *event_hash_pkg=cJSON_GetObjectItem(each_event,"id");
			//Judge if event exists and meet the required rule
			char *event_hash=get_json_value(event_hash_pkg);
			if (!check_role(event_hash,user_hash,"event"))
			{
				redisAppendCommand(context, "sadd event:%s:rules %s",event_hash,rule_hash);
				redisAppendCommand(context, "sadd rule:%s:events %s",rule_hash,event_hash);
			
				k+=2;
			}
			else
			{
				//go to delete operation
				printf("should not be in\n");
				deleteRuleFlag=1;
			}
		}
		
		//add actions
		int total_action_num=cJSON_GetArraySize(actions);
		for(j=0;j<total_action_num;j++)
		{
			cJSON *each_action=cJSON_GetArrayItem(actions,j);
			cJSON *action_hash_pkg=cJSON_GetObjectItem(each_action,"id");
			char *action_hash=get_json_value(action_hash_pkg);
			if (!check_role(action_hash,user_hash,"action"))
			{
				redisAppendCommand(context, "sadd action:%s:rules %s",action_hash,rule_hash);
				redisAppendCommand(context, "sadd rule:%s:actions %s",rule_hash,action_hash);
				k+=2;
			}
			else
			{
				//go to delete operation
				printf("should not be in2\n");
				deleteRuleFlag=1;
			}

		}
		
		//add enable
		
		redisAppendCommand(context, "hset rule:%s:value %s %s",rule_hash,enable->string,enable_value);
		k++;
		
		for(j=0;j<k;j++)
		{
			redisGetReply(context,(void **)&reply);
			freeReplyObject(reply);
		}

		if(deleteRuleFlag==1)
		{
			delete_rule(rule_hash);
		}

end:		
		free(temp_events);
		free(temp_actions);
		free(rule_hash);
		free(user_hash);
		free(app_hash);
	  
	}	
	
closed:	cJSON_Delete(rule);
	closeConnect();
}

char *get_uuid()
{
	uuid_t uuid;
	char *str=(char *)malloc(37*sizeof(char));

	uuid_generate(uuid);
	uuid_unparse(uuid, str);

	printf("%s\n", str);
 
	return str;
}

//need to be rewrite about something 
int combine_user(const char *combine_info)//new
{
	createConnect();
	//Generate an uuid code and use it as a list for each user
	//because we use hashset (user:$hashid) everytime we will update the content immediately
	
	char *combine_hash=get_uuid();

	redisReply *reply;
	
	cJSON *combine=cJSON_Parse(combine_info);
	cJSON *owner_name=cJSON_GetObjectItem(combine,"userid");
	cJSON *owner_app=cJSON_GetObjectItem(combine,"app");
	cJSON *owner_content=cJSON_GetObjectItem(combine,"combine");
	if(combine==NULL||owner_name==NULL||owner_app==NULL||owner_content==NULL)
	{
		printf("Miss arguments!\n");
	}
	else
	{
		char *owner_userid_value=get_json_value(owner_name);
		char *owner_app_value=get_json_value(owner_app);

		char *owner_total=(char *)calloc(strlen(owner_userid_value)+strlen(owner_app_value)+2,sizeof(char));
		strcat(owner_total,owner_userid_value);
		strcat(owner_total,"~");
		strcat(owner_total,owner_app_value);
		
		char *owner_user_hash=NULL;
        printf("\n\n\n user id :%s\n",owner_total);
		owner_user_hash=get_hash_value(owner_total);
		printf("\n\n\n owner user hash id :%s\n",owner_user_hash);

		free(owner_total);
		
		char *owner_app_hash=NULL;
		owner_app_hash=get_hash_value(owner_app_value);
		//printf("user_hash:%s\n",user_hash);

		//delete pre-combine
		reply=redisCommand(context, "hget user:%s:value combine",owner_user_hash);
		if(reply->type==1)
		{
			redisReply *temp=redisCommand(context, "del %s",reply->str);
			freeReplyObject(temp);
		}
		else
		{
			printf("user info is wrong!");
			freeReplyObject(reply);
			free(owner_user_hash);
			free(owner_app_hash);
			free(combine_hash);
			goto end;
		}
		freeReplyObject(reply);

		reply=redisCommand(context,"hset user:%s:value combine %s",owner_user_hash,combine_hash);
		freeReplyObject(reply);

		int combine_num=cJSON_GetArraySize(owner_content);
		int i=0;
		for(;i<combine_num;i++)
		{
			cJSON *combine_content=cJSON_GetArrayItem(owner_content,i);

			char *app_value = combine_content->string;
			char *userid_value = get_json_value(combine_content);
			
			//get app_hash and user hash
			char *total=(char *)calloc(strlen(userid_value)+strlen(app_value)+2,sizeof(char));
			strcat(total,userid_value);
			strcat(total,"~");
			strcat(total,app_value);
			
			char *user_hash=NULL;
			user_hash=get_hash_value(total);
			free(total);
			char *app_hash=NULL;
			app_hash=get_hash_value(app_value);
			
			//check if user_hash is shared
			printf("combine is %s",user_hash);
			reply=redisCommand(context, "sadd %s %s",combine_hash,user_hash);
			freeReplyObject(reply);

			free(user_hash);
			free(app_hash);

		}
		free(owner_user_hash);
		free(owner_app_hash);
	}
	
	free(combine_hash);
	cJSON_Delete(combine);
	closeConnect();
	return 0;
end:	
	cJSON_Delete(combine);
	closeConnect();
	return -1;
}


int combine_user_old(const char *combine_info)
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

		char *app_value = combine_content->string;
		char *userid_value = get_json_value(combine_content);
		
		//get app_hash and user hash
		char *total=(char *)calloc(strlen(userid_value)+strlen(app_value)+2,sizeof(char));
		strcat(total,userid_value);
		strcat(total,"~");
		strcat(total,app_value);
		
		char *user_hash=NULL;
		user_hash=get_hash_value(total);
		free(total);
		char *app_hash=NULL;
		app_hash=get_hash_value(app_value);
		
		//check if user_hash is shared
		reply=(redisReply*)redisCommand(context, "hget app:%s:users %s",app_hash,user_hash); 
		
		if(reply->type!=6&&!strcmp(reply->element[0]->str,"1"))	
		{	
			redisReply *r;
			redisAppendCommand(context, "hget user:%s:value combine",combine_content->string);
			redisGetReply(context,(void **)&r);
			if (r->type==1)
			{
				redisReply *temp;
				redisAppendCommand(context, "del %s",r->str);
				redisGetReply(context,(void **)&temp);
				freeReplyObject(temp);
			}
			freeReplyObject(r);
			redisAppendCommand(context, "sadd %s %s",combine_hash,combine_content->string);
			redisAppendCommand(context, "hset user:%s:value combine %s",user_hash,combine_hash);
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
		cJSON *id=cJSON_GetObjectItem(user_content,"userid");
		cJSON *app=cJSON_GetObjectItem(user_content,"app");
		cJSON *role=cJSON_GetObjectItem(user_content,"role");

		char *userid_value=	get_json_value(id);
		char *app_value=get_json_value(app);
		char *role_value=get_json_value(role);
		
		if(id==NULL||app==NULL||role==NULL)
		{
			printf("Miss arguments!\n");
			continue;
		}
		
		char *total=(char *)calloc(strlen(userid_value)+strlen(app_value)+2,sizeof(char));
		strcat(total,userid_value);
		strcat(total,"~");
		strcat(total,app_value);
		
		char *user_hash=NULL;
        printf("user id :%s\n",total);
		user_hash=get_hash_value(total);
		free(total);
		char *app_hash=NULL;
		app_hash=get_hash_value(app_value);
		printf("user_hash:%s\n",user_hash);
		//check if app_hash exists then user_hash exists
		redisAppendCommand(context, "sismember app:list %s",app_hash);
		redisAppendCommand(context, "sismember user:list %s",user_hash);
		redisGetReply(context,(void **)&reply);
		if (reply->type!=3)
		{
			printf("something wrong on redis!\n");
			freeReplyObject(reply);
			goto end;

		}
		else if(reply->integer==0)
		{
			printf("No such application, please check your config!\n");
			freeReplyObject(reply);
			goto end;
		}
		freeReplyObject(reply);

		redisGetReply(context,(void **)&reply);
		if (reply->type!=3)
		{
			printf("something wrong on redis!\n");
			freeReplyObject(reply);
			goto end;

		}
		else if(reply->integer!=0)
		{
			printf("User has already been added, please check your config!\n");
			freeReplyObject(reply);
			goto end;
		}
		freeReplyObject(reply);

		
		int total_info_set=cJSON_GetArraySize(user_content);
		int j=0;
		int k=0;
		int shared_user_flag=0;
		for(;j<total_info_set;j++)
		{
			cJSON *info_body=cJSON_GetArrayItem(user_content,j);
			char *name=info_body->string;
			char *value=get_json_value(info_body);
			k++;

			if(!strcmp(name,"shared"))
			{
				shared_user_flag=1;
				redisAppendCommand(context, "hset app:%s:users %s %s",app_hash,user_hash,value);
			}
			else if (!strcmp(name,"app"))
			{
				redisAppendCommand(context, "hset user:%s:value app %s",user_hash,app_hash);
			}
			else
			{
				redisAppendCommand(context, "hset user:%s:value %s %s",user_hash,name,value);
			}

		}

		if(shared_user_flag==0)
		{
			redisAppendCommand(context, "hset app:%s:users %s 1",app_hash,user_hash);
			k++;
		}

		//once add user need to combine iteself or not? here we use yes
		char *combine_hash=get_uuid();
		redisAppendCommand(context,"hset user:%s:value combine %s",user_hash,combine_hash);
		redisAppendCommand(context,"sadd %s %s",combine_hash,user_hash);
		free(combine_hash);
		redisAppendCommand(context, "sadd user:list %s",user_hash);
		k+=3;

		for(j=0;j<k;j++)
		{
			redisGetReply(context,(void **)&reply);
			freeReplyObject(reply);
		}
end:	free(user_hash);
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

		char *app_value=get_json_value(uaid);
		if(uaid==NULL)
		{
			printf("Miss arguments!\n");
			continue;
		}

		char *app_hash=NULL;
		app_hash=get_hash_value(app_value);
		printf("application value is %s\n",app_value);
		printf("application hash is %s\n",app_hash);
		/*
		cJSON *roles=cJSON_GetObjectItem(app_content,"roles");
		*/
		redisAppendCommand(context, "flushdb");
		redisGetReply(context,(void **)&reply);
		freeReplyObject(reply);
		redisAppendCommand(context, "sismember app:list %s",app_hash);
		redisGetReply(context,(void **)&reply);
		if (reply->type!=3)
		{
			printf("something wrong on redis!\n");
			freeReplyObject(reply);
			goto end;

		}
		else if(reply->integer!=0)
		{
			printf("Application has been added, please check your config!\n");
			freeReplyObject(reply);
			goto end;
		}
		freeReplyObject(reply);


		int total_info_set=cJSON_GetArraySize(app_content);
		int j=0;
		int k=0;
		for(;j<total_info_set;j++)
		{
			cJSON *info_body=cJSON_GetArrayItem(app_content,j);
			char *name=info_body->string;
			char *value=get_json_value(info_body);
			
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
		/*
		int total_roles_num=cJSON_GetArraySize(roles);
		for(j=0;j<total_roles_num;j++)
		{
			cJSON *info_body=cJSON_GetArrayItem(roles,j);
			char *name=info_body->string;
			char *value=info_body->valuestring;
			redisAppendCommand(context, "hset app:%s:roles %s %s",app_hash,name,value);
			k++;
		}
		*/
		
		redisAppendCommand(context, "sadd app:list %s",app_hash);//will hashset be better? For we can put name in it
		k++;
		for(j=0;j<k;j++)
		{
			redisGetReply(context,(void **)&reply);
			freeReplyObject(reply);
		}
		
end:	free(app_hash);
		
	}
	
	cJSON_Delete(app);
	closeConnect();
	return 0;
}


char* rule_detect(const char* event_id){
	if(results!=NULL)
		free(results);
	results=NULL;

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
		{
			char *rule_res_hash=r->element[i]->str;
			printf("results: %s\n",rule_res_hash);
			char *tempchar=results;
			if(tempchar==NULL)
				results=(char*)calloc(strlen(rule_res_hash)+4,sizeof(char));
			else
			{
				tempchar=(char*)realloc(results,sizeof(char)*(strlen(results)+strlen(rule_res_hash)+2));
				if(tempchar!=NULL)
					results=tempchar;
			}
			strcat(results,rule_res_hash);
			strcat(results,",");
		}
					
		freeReplyObject(reply);
		
	}
    freeReplyObject(r);
    closeConnect();
    return results;
}

char* device_state_income(const char* device_info)//const char* uid,const char* state)
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
		char *uid=get_json_value(temp_uid);
		char *state=get_json_value(temp_state);
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

		if (r->type==1&&!pass&&!strcmp(r->str,state))
		{
			printf("nothing changed!!\n");
			freeReplyObject(r);
			cJSON_Delete(json);
			closeConnect();
			return NULL;
		}
		else if(r->type==6)
		{
			freeReplyObject(r);
			printf("Database error 1!\n");
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
				char *json_value=get_json_value(temp_json);
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

	return results;
}
//for pipe limit all the delete function mustnot use pipe function
int delete_app(char *app_hash)
{
	createConnect();
	redisReply *reply;
	int i=0;

	//
	reply=(redisReply*)redisCommand(context, "SMEMBERS app:%s:events",app_hash);
	if (reply->type==2)
	{
		int count=reply->elements;
		for(i=0;i<count;i++)
		{
			char *event_hash=reply->element[i]->str;
			delete_event(event_hash);
		}
		freeReplyObject(reply);
	}
	else
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}

	reply=(redisReply*)redisCommand(context, "SMEMBERS app:%s:actions",app_hash);
	if (reply->type==2)
	{
		int count=reply->elements;
		for(i=0;i<count;i++)
		{
			char *action_hash=reply->element[i]->str;
			delete_action(action_hash);
		}
		freeReplyObject(reply);
	}
	else
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}

	reply=(redisReply*)redisCommand(context, "hkeys app:%s:users",app_hash);
	if (reply->type==2)
	{
		int count=reply->elements;
		for(i=0;i<count;i++)
		{
			char *user_hash=reply->element[i]->str;
			delete_user(user_hash);
		}
		freeReplyObject(reply);
	}
	else
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}

	reply=(redisReply*)redisCommand(context, "del app:%s:value",app_hash);
	if (reply->type==6)
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}
	freeReplyObject(reply);

	reply=(redisReply*)redisCommand(context, "del app:%s:events",app_hash);
	if (reply->type==6)
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}
	freeReplyObject(reply);

	reply=(redisReply*)redisCommand(context, "del app:%s:actions",app_hash);
	if (reply->type==6)
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}
	freeReplyObject(reply);

	reply=(redisReply*)redisCommand(context, "srem app:list %s",app_hash);
	if (reply->type==6)
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}
	freeReplyObject(reply);

	return 0;

end:

	closeConnect();
	return -1;
}

int delete_user(char *user_hash)
{
	//decr user related events actions rule,if eq 0 .delete them
	createConnect();
	redisReply *reply;
	redisReply *r;
	int i=0;

	reply=(redisReply*)redisCommand(context, "hkeys user:%s:rules",user_hash);
	if (reply->type==2)
	{
		int count=reply->elements;
		for(i=0;i<count;i++)
		{
			char *rule_hash=reply->element[i]->str;
			r=(redisReply*)redisCommand(context, "HINCRBY rule:%s:value referCount -1",rule_hash);
			if(r->type==3&&r->integer<=0)
			{
				delete_rule(rule_hash);
			}
			freeReplyObject(r);
		}
		freeReplyObject(reply);
	}
	else
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}

	reply=(redisReply*)redisCommand(context, "hkeys user:%s:events",user_hash);
	if (reply->type==2)
	{
		int count=reply->elements;
		for(i=0;i<count;i++)
		{
			char *event_hash=reply->element[i]->str;
			r=(redisReply*)redisCommand(context, "HINCRBY event:%s:value referCount -1",event_hash);
			if(r->type==3&&r->integer<=0)
			{
				delete_event(event_hash);
			}
			freeReplyObject(r);
		}
		freeReplyObject(reply);
	}
	else
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}

	reply=(redisReply*)redisCommand(context, "hkeys user:%s:actions",user_hash);
	if (reply->type==2)
	{
		int count=reply->elements;
		for(i=0;i<count;i++)
		{
			char *action_hash=reply->element[i]->str;
			r=(redisReply*)redisCommand(context, "HINCRBY action:%s:value referCount -1",action_hash);
			if(r->type==3&&r->integer<=0)
			{
				delete_action(action_hash);
			}
			freeReplyObject(r);
		}
		freeReplyObject(reply);
	}
	else
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}

	//delete user info
	reply=(redisReply*)redisCommand(context, "hget user:%s:value combine",user_hash);
	if(reply->type==1)
	{
		char *combine_hash=reply->str;
		r=(redisReply*)redisCommand(context, "del %s",combine_hash);
		freeReplyObject(r);
		freeReplyObject(reply);
	}
	else
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}

	reply=(redisReply*)redisCommand(context, "hget user:%s:value app",user_hash);
	if(reply->type==1)
	{
		char *app_hash=reply->str;
		r=(redisReply*)redisCommand(context, "hdel app:%s:users %s",app_hash,user_hash);
		freeReplyObject(r);
		freeReplyObject(reply);
	}
	else
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}

	reply=(redisReply*)redisCommand(context, "keys user:%s*",user_hash);
	if (reply->type==2)
	{
		int count=reply->elements;
		for(i=0;i<count;i++)
		{
			redisAppendCommand(context,"del %s",reply->element[i]->str);
		}
		freeReplyObject(reply);
		for(i=0;i<count;i++)
		{
			redisGetReply(context,(void **)&reply);
			freeReplyObject(reply);
		}
	}
	else
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}


	reply=(redisReply*)redisCommand(context, "srem user:list %s",user_hash);
	if (reply->type==6)
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}
	freeReplyObject(reply);

	return 0;
end:
	closeConnect();
	return -1;
}

int delete_action(char *action_hash)
{
	//delete action info and action related rule
	createConnect();
	redisReply *reply;
	int i=0;

	reply=(redisReply*)redisCommand(context, "SMEMBERS action:%s:rules",action_hash);
	if (reply->type==2)
	{
		int count=reply->elements;
		for(i=0;i<count;i++)
		{
			delete_rule(reply->element[i]->str);
		}
		freeReplyObject(reply);
	}
	else
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}


	reply=(redisReply*)redisCommand(context, "keys user:*:actions:%s",action_hash);
	if (reply->type==2)
	{
		int count=reply->elements;
		for(i=0;i<count;i++)
		{
			redisAppendCommand(context,"del %s",reply->element[i]->str);
		}
		freeReplyObject(reply);
		for(i=0;i<count;i++)
		{
			redisGetReply(context,(void **)&reply);
			freeReplyObject(reply);
		}
	}
	else
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}

	reply=(redisReply*)redisCommand(context, "del action:%s:value",action_hash);
	if (reply->type==6)
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}
	freeReplyObject(reply);

	reply=(redisReply*)redisCommand(context, "hdel action:list %s",action_hash);
	if (reply->type==6)
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}
	freeReplyObject(reply);


	return 0;
end:
	closeConnect();
	return -1;
}

int delete_event(char *event_hash)
{
	//delete event info and event related rule
	createConnect();
	redisReply *reply;
	int i=0;

	reply=(redisReply*)redisCommand(context, "SMEMBERS event:%s:rules",event_hash);
	if (reply->type==2)
	{
		int count=reply->elements;
		for(i=0;i<count;i++)
		{
			delete_rule(reply->element[i]->str);
		}
		freeReplyObject(reply);
	}
	else
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}


	reply=(redisReply*)redisCommand(context, "keys user:*:event:%s",event_hash);
	if (reply->type==2)
	{
		int count=reply->elements;
		for(i=0;i<count;i++)
		{
			redisAppendCommand(context,"del %s",reply->element[i]->str);
		}
		freeReplyObject(reply);
		for(i=0;i<count;i++)
		{
			redisGetReply(context,(void **)&reply);
			freeReplyObject(reply);
		}
	}
	else
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}

	reply=(redisReply*)redisCommand(context, "del event:%s:value",event_hash);
	if (reply->type==6)
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}
	freeReplyObject(reply);

	reply=(redisReply*)redisCommand(context, "hdel event:list %s",event_hash);
	if (reply->type==6)
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}
	freeReplyObject(reply);

	reply=(redisReply*)redisCommand(context, "srem active:events %s",event_hash);
	if (reply->type==6)
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}
	freeReplyObject(reply);


	return 0;
end:
	closeConnect();
	return -1;
}

int delete_rule(char *rule_hash)
{
	//delete rule info
	createConnect();
	redisReply *reply=NULL;
	int i=0;	

	reply=(redisReply*)redisCommand(context, "SMEMBERS rule:%s:events",rule_hash);
	if (reply->type==2)
	{
		int count=reply->elements;
		for(;i<count;i++)
		{

			redisAppendCommand(context,"srem event:%s:rules %s",reply->element[i]->str,rule_hash);
		}
		freeReplyObject(reply);
		for(i=0;i<count;i++)
		{
			redisGetReply(context,(void **)&reply);
			freeReplyObject(reply);
		}
	}
	else
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}

	reply=(redisReply*)redisCommand(context, "SMEMBERS rule:%s:actions",rule_hash);
	if (reply->type==2)
	{
		int count=reply->elements;
		for(i=0;i<count;i++)
		{
			redisAppendCommand(context,"srem action:%s:rules %s",reply->element[i]->str,rule_hash);
		}
		freeReplyObject(reply);
		for(i=0;i<count;i++)
		{
			redisGetReply(context,(void **)&reply);
			freeReplyObject(reply);
		}
	}
	else
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}

	reply=(redisReply*)redisCommand(context, "keys *rule:%s*",rule_hash);
	if (reply->type==2)
	{
		int count=reply->elements;
		for(i=0;i<count;i++)
		{
			redisAppendCommand(context,"del %s",reply->element[i]->str);
		}
		freeReplyObject(reply);
		for(i=0;i<count;i++)
		{
			redisGetReply(context,(void **)&reply);
			freeReplyObject(reply);
		}
	}
	else
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}

	reply=(redisReply*)redisCommand(context, "srem rule:list %s",rule_hash);
	if (reply->type==6)
	{
		printf("srem rule:list %s\n",rule_hash);
		freeReplyObject(reply);
		goto end;
	}
	freeReplyObject(reply);


	return 0;

end: closeConnect();
	return -1;
}

int delete_event_by_userhash(char *event_hash,char *user_hash)
{
	createConnect();

	//ger role;
	redisReply *reply;
	int required_role;
	int user_role;
	redisAppendCommand(context,"hget user:%s:value role",user_hash);
	redisAppendCommand(context,"hget event:%s:value role",event_hash);
	redisGetReply(context,(void **)&reply);
	if (reply->type==1)
		user_role=atoi(reply->str);
	else
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		redisGetReply(context,(void **)&reply);
		freeReplyObject(reply);
		goto end;
	}
	freeReplyObject(reply);
	redisGetReply(context,(void **)&reply);
	if (reply->type==1)
		required_role=atoi(reply->str);
	else
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}
	freeReplyObject(reply);

	if(required_role<user_role)
	{
		printf("Permission denied!\n");
	}
	else
	{
		redisAppendCommand(context,"del user:%s:event:%s",user_hash,event_hash);
		redisAppendCommand(context,"hdel user:%s:events %s",user_hash,event_hash);
		redisAppendCommand(context,"HINCRBY event:%s:value referCount -1",event_hash);
		redisGetReply(context,(void **)&reply);
		freeReplyObject(reply);
		redisGetReply(context,(void **)&reply);
		freeReplyObject(reply);
		redisGetReply(context,(void **)&reply);
		if(reply->type==3&&reply->integer<1)
		{
			delete_event(event_hash);
		}
		freeReplyObject(reply);
	}


	closeConnect();
	return 0;
end:

	closeConnect();
	return -1;
}

int delete_action_by_userhash(char *action_hash,char *user_hash)
{
	createConnect();

	//ger role;
	redisReply *reply;
	int required_role;
	int user_role;
	redisAppendCommand(context,"hget user:%s:value role",user_hash);
	redisAppendCommand(context,"hget action:%s:value role",action_hash);
	redisGetReply(context,(void **)&reply);
	if (reply->type==1)
		user_role=atoi(reply->str);
	else
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		redisGetReply(context,(void **)&reply);
		freeReplyObject(reply);
		goto end;
	}
	freeReplyObject(reply);
	redisGetReply(context,(void **)&reply);
	if (reply->type==1)
		required_role=atoi(reply->str);
	else
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}
	freeReplyObject(reply);

	if(required_role<user_role)
	{
		printf("Permission denied!\n");
	}
	else
	{
		redisAppendCommand(context,"del user:%s:action:%s",user_hash,action_hash);
		redisAppendCommand(context,"hdel user:%s:actions %s",user_hash,action_hash);
		redisAppendCommand(context,"HINCRBY action:%s:value referCount -1",action_hash);
		redisGetReply(context,(void **)&reply);
		freeReplyObject(reply);
		redisGetReply(context,(void **)&reply);
		if(reply->type==3&&reply->integer<1)
		{
			delete_action(action_hash);
		}
		freeReplyObject(reply);
	}

	free(user_hash);
	closeConnect();
	return 0;
end:
	free(user_hash);
	closeConnect();
	return -1;
}
int delete_rule_by_userhash(char *rule_hash,char *user_hash)
{
	createConnect();

	//ger role;
	redisReply *reply;
	redisAppendCommand(context,"hget user:%s:rules %s",user_hash,rule_hash);
	redisGetReply(context,(void **)&reply);
	if (reply->type==1)
	{
		freeReplyObject(reply);
		redisAppendCommand(context,"hdel user:%s:rules %s",user_hash,rule_hash);
		redisAppendCommand(context,"HINCRBY rule:%s:value referCount -1",rule_hash);
		redisGetReply(context,(void **)&reply);
		freeReplyObject(reply);
		redisGetReply(context,(void **)&reply);
		if(reply->type==3&&reply->integer<1)
		{
			delete_rule(rule_hash);
		}
		freeReplyObject(reply);
	}
	else if(reply->type==4)
	{
		printf("rule do not belongs to you! Cannot delete it !!\n");
		freeReplyObject(reply);
		goto end;
	}
	else
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}
	
	free(user_hash);
	closeConnect();
	return 0;
end:
	free(user_hash);
	closeConnect();
	return -1;
}

int delete_event_by_user(char *event_hash,char *app, char *userid)
{
	//check role then we can use the normal function 
	int res=-1;

	char *user_hash=NULL;//need to be a user in istack? check in combine
	char *total=(char *)calloc(strlen(userid)+strlen(app)+2,sizeof(char));
	strcat(total,userid);
	strcat(total,"~");
	strcat(total,app);
	user_hash=get_hash_value(total);
	free(total);

	char *app_hash=NULL;
	app_hash=get_hash_value(app);

	//ger role;
	res=delete_event_by_userhash(event_hash,user_hash);

	free(user_hash);
	free(app_hash);
	return res;
}

int delete_action_by_user(char *action_hash,char *app, char *userid)
{
	//check role then we can use the normal function 
	int res=-1;

	char *user_hash=NULL;//need to be a user in istack? check in combine
	char *total=(char *)calloc(strlen(userid)+strlen(app)+2,sizeof(char));
	strcat(total,userid);
	strcat(total,"~");
	strcat(total,app);
	user_hash=get_hash_value(total);
	free(total);

	char *app_hash=NULL;
	app_hash=get_hash_value(app);

	//ger role;
	res=delete_action_by_userhash(action_hash,user_hash);

	free(user_hash);
	free(app_hash);

	return -1;
}

int delete_rule_by_user(char *rule_hash,char *app, char *userid)
{
		//check role then we can use the normal function 
	int res=-1;

	char *user_hash=NULL;//need to be a user in istack? check in combine
	char *total=(char *)calloc(strlen(userid)+strlen(app)+2,sizeof(char));
	strcat(total,userid);
	strcat(total,"~");
	strcat(total,app);
	user_hash=get_hash_value(total);
	free(total);

	char *app_hash=NULL;
	app_hash=get_hash_value(app);

	res=delete_rule_by_userhash(rule_hash,user_hash);

	free(user_hash);
	free(app_hash);

	return res;
}

char* get_events_totalhash(char *app_hash, char* user_hash)
{
	if(results!=NULL)
		free(results);

	results=NULL;
	//for total need to get the role below him and get all the things needed
	createConnect();

	redisReply* reply;
	redisReply* r;
	reply=(redisReply*)redisCommand(context,"hget user:%s:value combine",user_hash);
	if(reply->type==1)
	{
		char *combine_hash=reply->str;
		r=(redisReply*)redisCommand(context,"SMEMBERS %s",combine_hash);
		freeReplyObject(reply);
		if(r->type==2)
		{
			int count_combine_users=r->elements;
			int i=0;
			for(;i<count_combine_users;i++)
			{
				char *temp_user_hash=r->element[i]->str;

				int user_role;

				reply=(redisReply*)redisCommand(context,"hget user:%s:value role",temp_user_hash);
				user_role=atoi(reply->str);
				freeReplyObject(reply);

				reply=(redisReply*)redisCommand(context,"hget user:%s:value app",temp_user_hash);
				if (reply->type==1)
				{
					char *temp_app_hash=reply->str;
					redisReply *temp_reply;
					temp_reply=(redisReply*)redisCommand(context,"HGETALL app:%s:events",temp_app_hash);
					int j=0;
					int count_app_events=temp_reply->elements;
					if (temp_reply->type==2)
					{
						for(;j<count_app_events;j+=2)
						{
							int required_role=atoi(temp_reply->element[j+1]->str);
							char *temp_event_hash=temp_reply->element[j]->str;
							if (required_role>=user_role)
							{
								//okay !we get it! add name to it
								char *event_name="Lower User Add";
								redisReply *get_events_name=(redisReply*)redisCommand(context,"hget user:%s:events %s"
									,temp_user_hash,temp_event_hash);
								if (get_events_name->type==1)
								{
									event_name=get_events_name->str;
								}
								char *tempchar=results; 
								if(tempchar==NULL)
									//add 4 is for mem align??? i don't know why for valgrind tell me this
									results=(char*)calloc(strlen(temp_event_hash)+1+strlen(event_name)+4,sizeof(char));
								else
								{
									tempchar=(char*)realloc(results,sizeof(char)*(strlen(results)+strlen(temp_event_hash)+1+strlen(event_name)+2));
									if(tempchar!=NULL)
										results=tempchar;
								}
		
								strcat(results,temp_event_hash);
								strcat(results,"~");
								strcat(results,event_name);
								strcat(results,",");

								freeReplyObject(get_events_name);

							}
						}
					}
					freeReplyObject(temp_reply);
				}
				freeReplyObject(reply);
			}
		}
		freeReplyObject(r);
	}
	else
	{
		freeReplyObject(reply);
	}
	closeConnect();
	return 0;
}


char* get_actions_totalhash(char *app_hash, char* user_hash)
{
	if(results!=NULL)
		free(results);

	results=NULL;
	//for total need to get the role below him and get all the things needed
	createConnect();

	redisReply* reply;
	redisReply* r;
	reply=(redisReply*)redisCommand(context,"hget user:%s:value combine",user_hash);
	if(reply->type==1)
	{
		char *combine_hash=reply->str;
		r=(redisReply*)redisCommand(context,"SMEMBERS %s",combine_hash);
		freeReplyObject(reply);
		if(r->type==2)
		{
			int count_combine_users=r->elements;
			int i=0;
			for(;i<count_combine_users;i++)
			{
				char *temp_user_hash=r->element[i]->str;

				int user_role;

				reply=(redisReply*)redisCommand(context,"hget user:%s:value role",temp_user_hash);
				user_role=atoi(reply->str);
				freeReplyObject(reply);

				reply=(redisReply*)redisCommand(context,"hget user:%s:value app",temp_user_hash);
				if (reply->type==1)
				{
					char *temp_app_hash=reply->str;
					redisReply *temp_reply;
					temp_reply=(redisReply*)redisCommand(context,"HGETALL app:%s:actions",temp_app_hash);
					int j=0;
					int count_app_actions=temp_reply->elements;
					if (temp_reply->type==2)
					{
						for(;j<count_app_actions;j+=2)
						{
							int required_role=atoi(temp_reply->element[j+1]->str);
							char *temp_action_hash=temp_reply->element[j]->str;
							if (required_role>=user_role)
							{
								//okay !we get it!

								char *action_name="Lower User Add";
								redisReply *get_action_name=(redisReply*)redisCommand(context,"hget user:%s:actions %s"
									,temp_user_hash,temp_action_hash);
								if (get_action_name->type==1)
								{
									action_name=get_action_name->str;
								}
								else
								{
									//we can get the add user ??
								}
								char *tempchar=results; 
								if(tempchar==NULL)
									//add 4 is for mem align??? i don't know why for valgrind tell me this
									results=(char*)calloc(strlen(temp_action_hash)+1+strlen(action_name)+4,sizeof(char));
								else
								{
									tempchar=(char*)realloc(results,sizeof(char)*(strlen(results)+strlen(temp_action_hash)+1+strlen(action_name)+2));
									if(tempchar!=NULL)
										results=tempchar;
								}
		
								strcat(results,temp_action_hash);
								strcat(results,"~");
								strcat(results,action_name);
								strcat(results,",");

								freeReplyObject(get_action_name);

							}
						}
					}
					freeReplyObject(temp_reply);
				}
				freeReplyObject(reply);
			}
		}
		freeReplyObject(r);

	}
	else
	{
		freeReplyObject(reply);
	}
	closeConnect();
	return results;
}

char* get_events_total(char *app, char* userid)
{
	//for total need to get the role below him and get all the things needed
	char *user_hash=NULL;//need to be a user in istack? check in combine
	char *total=(char *)calloc(strlen(userid)+strlen(app)+2,sizeof(char));
	strcat(total,userid);
	strcat(total,"~");
	strcat(total,app);
	user_hash=get_hash_value(total);
	free(total);

	char *app_hash=NULL;
	app_hash=get_hash_value(app);

	get_events_totalhash(app_hash, user_hash);

	free(app_hash);
	free(user_hash);

	return results;
}

char* get_actions_total(char *app, char* userid)
{
	char *user_hash=NULL;//need to be a user in istack? check in combine
	char *total=(char *)calloc(strlen(userid)+strlen(app)+2,sizeof(char));
	strcat(total,userid);
	strcat(total,"~");
	strcat(total,app);
	user_hash=get_hash_value(total);
	free(total);

	char *app_hash=NULL;
	app_hash=get_hash_value(app);

	get_actions_totalhash(app_hash, user_hash);

	free(app_hash);
	free(user_hash);

	return results;
}

char* get_rules_total(char *app, char* userid)
{
	return 0;
}

char* get_events_bindstore(char *app_hash,char* user_hash)
{

	char *events_store=get_uuid();
	createConnect();
	redisReply* reply;
	redisReply* r;
	reply=(redisReply*)redisCommand(context,"hget user:%s:value combine",user_hash);
	if(reply->type==1)
	{
		char *combine_hash=reply->str;
		r=(redisReply*)redisCommand(context,"SMEMBERS %s",combine_hash);
		if(r->type==2)
		{
			int count=r->elements;
			int i=0;
			for(;i<count;i++)
			{
				char *user_temp=r->element[i]->str;
				int user_role=-1;
				redisReply* temp_reply;
				//get user role
				temp_reply=(redisReply*)redisCommand(context,"hget user:%s:value role",user_temp);
				if(temp_reply->type==1)
				{
					user_role=atoi(temp_reply->str);
				}
				else
				{
					printf("Database wrong\n");
					freeReplyObject(temp_reply);
					freeReplyObject(r);
					freeReplyObject(reply);
					goto end;
				}
				freeReplyObject(temp_reply);

				temp_reply=(redisReply*)redisCommand(context,"hkeys user:%s:events",user_temp);
				if(temp_reply->type==2)
				{
					//check if all the events are exists if not exist then delete it 
					int count_temp_events=temp_reply->elements;
					int j=0;
					for(;j<count_temp_events;j++)
					{
						redisReply* temp;
						char *temp_event_hash=temp_reply->element[j]->str;

						//if role is not permitted we should delete it 
						temp=(redisReply*)redisCommand(context,"hget event:%s:value role",temp_event_hash);
						if(temp->type==1)
						{
							int required_role=atoi(temp->str);
							if (required_role<user_role)
							{
								delete_event_by_userhash(temp_event_hash,user_temp);
							}
						}
						else if(temp->type!=4)
						{
							printf("Database wrong!\n");
							freeReplyObject(temp);
							freeReplyObject(temp_reply);
							freeReplyObject(r);
							freeReplyObject(reply);
						}
						freeReplyObject(temp);
						//if role permit
						temp=(redisReply*)redisCommand(context,"keys user:%s:event:%s",user_temp,temp_event_hash);
						
						if(temp->type==2&&temp->elements==0)
						{
							freeReplyObject(temp);
							temp=(redisReply*)redisCommand(context,"hdel user:%s:events %s",user_temp,temp_event_hash);
							freeReplyObject(temp);
						}
						else if(temp->type==2&&temp->elements>0)
						{
							//add the results to results 
							freeReplyObject(temp);
							temp=(redisReply*)redisCommand(context,"sadd %s %s",events_store,temp_event_hash);
							freeReplyObject(temp);
						}
						else
						{
							printf("Database wrong!\n");
							freeReplyObject(temp);
							freeReplyObject(temp_reply);
							freeReplyObject(r);
							freeReplyObject(reply);
							goto end;
						}

					}
				}
				else
				{
					printf("Database wrong\n");
					freeReplyObject(temp_reply);
					freeReplyObject(r);
					freeReplyObject(reply);
					goto end;
				}
				freeReplyObject(temp_reply);
			}

		}
		else
		{
			freeReplyObject(r);
			freeReplyObject(reply);
			goto end;
		}
		freeReplyObject(r);
	}
	else
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}
	freeReplyObject(reply);

	return events_store;
end:
	return events_store;
}

char* get_events_bindhash(char *app_hash,char* user_hash)
{

	if(results!=NULL)
	free(results);

	results=NULL;//use realloc
	createConnect();
	redisReply* reply;
	redisReply* r;
	reply=(redisReply*)redisCommand(context,"hget user:%s:value combine",user_hash);
	if(reply->type==1)
	{
		char *combine_hash=reply->str;
		r=(redisReply*)redisCommand(context,"SMEMBERS %s",combine_hash);
		if(r->type==2)
		{
			int count=r->elements;
			int i=0;
			for(;i<count;i++)
			{
				char *user_temp=r->element[i]->str;
				int user_role=-1;
				redisReply* temp_reply;
				//get user role
				temp_reply=(redisReply*)redisCommand(context,"hget user:%s:value role",user_temp);
				if(temp_reply->type==1)
				{
					user_role=atoi(temp_reply->str);
				}
				else
				{
					printf("Database wrong\n");
					freeReplyObject(temp_reply);
					freeReplyObject(r);
					freeReplyObject(reply);
					goto end;
				}
				freeReplyObject(temp_reply);

				temp_reply=(redisReply*)redisCommand(context,"hkeys user:%s:events",user_temp);
				if(temp_reply->type==2)
				{
					//check if all the events are exists if not exist then delete it 
					int count_temp_events=temp_reply->elements;
					int j=0;
					for(;j<count_temp_events;j++)
					{
						redisReply* temp;
						char *temp_event_hash=temp_reply->element[j]->str;

						//if role is not permitted we should delete it 
						temp=(redisReply*)redisCommand(context,"hget event:%s:value role",temp_event_hash);
						if(temp->type==1)
						{
							int required_role=atoi(temp->str);
							if (required_role>user_role)
							{
								delete_event_by_userhash(temp_event_hash,user_temp);
							}
						}
						else if(temp->type!=4)
						{
							printf("Database wrong!\n");
							freeReplyObject(temp);
							freeReplyObject(temp_reply);
							freeReplyObject(r);
							freeReplyObject(reply);
						}
						freeReplyObject(temp);
						//if role permit
						temp=(redisReply*)redisCommand(context,"keys user:%s:event:%s",user_temp,temp_event_hash);
						
						if(temp->type==2&&temp->elements==0)
						{
							freeReplyObject(temp);
							temp=(redisReply*)redisCommand(context,"hdel user:%s:events %s",user_temp,temp_event_hash);
							freeReplyObject(temp);
						}
						else if(temp->type==2&&temp->elements>0)
						{
							//add the results to results 
							freeReplyObject(temp);
							char *tempchar=results;
							if(tempchar==NULL)
								//add 4 is for mem align??? i don't know why for valgrind tell me this
								results=(char*)calloc(strlen(temp_event_hash)+4,sizeof(char));
							else
							{
								tempchar=(char*)realloc(results,sizeof(char)*(strlen(results)+strlen(temp_event_hash)+2));
								if(tempchar!=NULL)
									results=tempchar;
							}
	
							strcat(results,temp_event_hash);
							strcat(results,",");
						}
						else
						{
							printf("Database wrong!\n");
							freeReplyObject(temp);
							freeReplyObject(temp_reply);
							freeReplyObject(r);
							freeReplyObject(reply);
							goto end;
						}
					}
				}
				else
				{
					printf("Database wrong\n");
					freeReplyObject(temp_reply);
					freeReplyObject(r);
					freeReplyObject(reply);
					goto end;
				}
				freeReplyObject(temp_reply);
			}

		}
		else
		{
			freeReplyObject(r);
			freeReplyObject(reply);
			goto end;
		}
		freeReplyObject(r);
	}
	else
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}
	freeReplyObject(reply);

	closeConnect();
	return results;
end:
	closeConnect();
	results=NULL;
	return results;
}
//this function cannot be use by thirdpart application
char* get_actions_bindstore(char *app_hash,char* user_hash)
{


	char *actions_store=get_uuid();//remember to free it

	createConnect();

	//ger role;
	redisReply* reply;
	redisReply* r;
	reply=(redisReply*)redisCommand(context,"hget user:%s:value combine",user_hash);
	if(reply->type==1)
	{
		char *combine_hash=reply->str;
		r=(redisReply*)redisCommand(context,"SMEMBERS %s",combine_hash);
		if(r->type==2)
		{
			int count=r->elements;
			int i=0;
			for(;i<count;i++)
			{
				char *user_temp=r->element[i]->str;

				int user_role=-1;
				redisReply* temp_reply;
				//get user role
				temp_reply=(redisReply*)redisCommand(context,"hget user:%s:value role",user_temp);
				if(temp_reply->type==1)
				{
					user_role=atoi(temp_reply->str);
				}
				else
				{
					printf("Database wrong\n");
					freeReplyObject(temp_reply);
					freeReplyObject(r);
					freeReplyObject(reply);
					goto end;
				}
				freeReplyObject(temp_reply);

				temp_reply=(redisReply*)redisCommand(context,"hkeys user:%s:actions",user_temp);
				if(temp_reply->type==2)
				{
					//check if all the actions are exists if not exist then delete it 
					int count_temp_actions=temp_reply->elements;
					int j=0;
					for(;j<count_temp_actions;j++)
					{
						redisReply* temp;
						char *temp_action_hash=temp_reply->element[j]->str;
						//if role is not permitted we should delete it 
						temp=(redisReply*)redisCommand(context,"hget action:%s:value role",temp_action_hash);
						if(temp->type==1)
						{
							int required_role=atoi(temp->str);
							if (required_role<user_role)
							{
								delete_action_by_userhash(temp_action_hash,user_temp);
							}
						}
						else if(temp->type!=4)
						{
							printf("Database wrong!\n");
							freeReplyObject(temp);
							freeReplyObject(temp_reply);
							freeReplyObject(r);
							freeReplyObject(reply);
						}
						freeReplyObject(temp);

						temp=(redisReply*)redisCommand(context,"keys user:%s:action:%s",user_hash,temp_action_hash);
						if(temp->type==2&&temp->elements==0)
						{
							freeReplyObject(temp);
							temp=(redisReply*)redisCommand(context,"hdel user:%s:actions %s",user_hash,temp_action_hash);
							freeReplyObject(temp);
						}
						else if(temp->type==2&&temp->elements>0)
						{
							freeReplyObject(temp);
							temp=(redisReply*)redisCommand(context,"sadd %s %s",actions_store,temp_action_hash);
							freeReplyObject(temp);
						}
						else
						{
							freeReplyObject(temp);
							goto end;
						}
					}
				}
				else
				{
					freeReplyObject(temp_reply);
					freeReplyObject(r);
					freeReplyObject(reply);
					goto end;
				}
				freeReplyObject(temp_reply);
			}

		}
		else
		{
			freeReplyObject(r);
			freeReplyObject(reply);
			goto end;
		}
		freeReplyObject(r);
	}
	else
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}
	freeReplyObject(reply);

	return actions_store;
end:

	return actions_store;
}

char* get_actions_bindhash(char *app_hash,char* user_hash)
{
	if(results!=NULL)
		free(results);

	results=NULL;

	createConnect();

	//ger role;
	redisReply* reply;
	redisReply* r;
	reply=(redisReply*)redisCommand(context,"hget user:%s:value combine",user_hash);
	if(reply->type==1)
	{
		char *combine_hash=reply->str;
		r=(redisReply*)redisCommand(context,"SMEMBERS %s",combine_hash);
		if(r->type==2)
		{
			int count=r->elements;
			int i=0;
			for(;i<count;i++)
			{
				char *user_temp=r->element[i]->str;

				int user_role=-1;
				redisReply* temp_reply;
				//get user role
				temp_reply=(redisReply*)redisCommand(context,"hget user:%s:value role",user_temp);
				if(temp_reply->type==1)
				{
					user_role=atoi(temp_reply->str);
				}
				else
				{
					printf("Database wrong\n");
					freeReplyObject(temp_reply);
					freeReplyObject(r);
					freeReplyObject(reply);
					goto end;
				}
				freeReplyObject(temp_reply);

				temp_reply=(redisReply*)redisCommand(context,"hkeys user:%s:actions",user_temp);
				if(temp_reply->type==2)
				{
					//check if all the actions are exists if not exist then delete it 
					int count_temp_actions=temp_reply->elements;
					int j=0;
					for(;j<count_temp_actions;j++)
					{
						redisReply* temp;
						char *temp_action_hash=temp_reply->element[j]->str;
						//if role is not permitted we should delete it 
						temp=(redisReply*)redisCommand(context,"hget action:%s:value role",temp_action_hash);
						if(temp->type==1)
						{
							int required_role=atoi(temp->str);
							if (required_role<user_role)
							{
								delete_action_by_userhash(temp_action_hash,user_temp);
							}
						}
						else if(temp->type!=4)
						{
							printf("Database wrong!\n");
							freeReplyObject(temp);
							freeReplyObject(temp_reply);
							freeReplyObject(r);
							freeReplyObject(reply);
						}
						freeReplyObject(temp);

						temp=(redisReply*)redisCommand(context,"keys user:%s:action:%s",user_hash,temp_action_hash);
						if(temp->type==2&&temp->elements==0)
						{
							freeReplyObject(temp);
							temp=(redisReply*)redisCommand(context,"hdel user:%s:actions %s",user_hash,temp_action_hash);
							freeReplyObject(temp);
						}
						else if(temp->type==2&&temp->elements>0)
						{
							//add the results to results 
							freeReplyObject(temp);
							char *tempchar=results;
							if(tempchar==NULL)
								//add 4 is for mem align??? i don't know why for valgrind tell me this
								results=(char*)calloc(strlen(temp_action_hash)+4,sizeof(char));
							else
							{
								tempchar=(char*)realloc(results,sizeof(char)*(strlen(results)+strlen(temp_action_hash)+2));
								if(tempchar!=NULL)
									results=tempchar;
							}
	
							strcat(results,temp_action_hash);
							strcat(results,",");
						}
						else
						{
							freeReplyObject(temp);
							goto end;
						}
					}
				}
				else
				{
					freeReplyObject(temp_reply);
					freeReplyObject(r);
					freeReplyObject(reply);
					goto end;
				}
				freeReplyObject(temp_reply);
			}

		}
		else
		{
			freeReplyObject(r);
			freeReplyObject(reply);
			goto end;
		}
		freeReplyObject(r);
	}
	else
	{
		printf("Database wrong!\n");
		freeReplyObject(reply);
		goto end;
	}
	freeReplyObject(reply);
	closeConnect();
	return results;
end:
	results=NULL;
	closeConnect();
	return results;
}

char* get_rules_bindhash(char *app_hash,char* user_hash)
{
	if(results!=NULL)
	free(results);

	results=NULL;

	createConnect();

	//ger role;
	redisReply* reply;
	redisReply* r;
	//get events_bind get_actions_bind then search each rule and use sdiff!! i am so clever!
	char *events_store;
	char *actions_store;
	events_store=get_events_bindstore(app_hash,user_hash);//need get_all function instead

	actions_store=get_actions_bindstore(app_hash,user_hash);

	//redisAppendCommand(context,"hkeys user:%s:rules ",user_hash);
	//redisGetReply(context,(void **)&reply);
	reply=(redisReply*)redisCommand(context,"hkeys user:%s:rules ",user_hash);
	
	if(reply->type==2)
	{
		int count_rules=reply->elements;
		int i=0;
		for(;i<count_rules;i++)
		{
			char *temp_rule_hash=reply->element[i]->str;
			int check_pass=1;

			int events_total;
			int actions_total;
			//get rule related elements and actions then check it !!!
			redisAppendCommand(context,"SMEMBERS %s",events_store);
			redisAppendCommand(context,"SMEMBERS %s",actions_store);
			redisAppendCommand(context,"sdiff %s rule:%s:events",events_store,temp_rule_hash);
			redisAppendCommand(context,"sdiff %s rule:%s:actions",actions_store,temp_rule_hash);
			redisGetReply(context,(void **)&r);
			events_total=r->elements;
			freeReplyObject(r);
			redisGetReply(context,(void **)&r);
			actions_total=r->elements;
			freeReplyObject(r);
			redisGetReply(context,(void **)&r);
			if(r->type==2&&r->elements==0)
			{
				//okay!event check pass
				check_pass=1;

			}
			else if(r->type==2&&r->elements>0)
			{

				//delete rule from user
				check_pass=-1;
			}
			else
			{
				freeReplyObject(r);
				redisGetReply(context,(void **)&r);
				freeReplyObject(r);
				freeReplyObject(reply);
				printf("Database wrong!\n");
				goto end;
			}
			freeReplyObject(r);
			redisGetReply(context,(void **)&r);
			if(check_pass==1&&r->type==2&&r->elements==0)
			{
				//all pass!okay ,it canbe used
				char *tempchar=results;
				if(tempchar==NULL)
					//add 4 is for mem align??? i don't know why for valgrind tell me this
					results=(char*)calloc(strlen(temp_rule_hash)+4,sizeof(char));
				else
				{
					tempchar=(char*)realloc(results,sizeof(char)*(strlen(results)+strlen(temp_rule_hash)+2));
					if(tempchar!=NULL)
						results=tempchar;
				}

				strcat(results,temp_rule_hash);
				strcat(results,",");
			}
			else if(check_pass!=1||(r->type==2&&r->elements>0))
			{
				//no need to do anything
				//delete_rule_by_userhash(temp_rule_hash,user_hash);
			}
			else
			{

				freeReplyObject(r);
				freeReplyObject(reply);
				printf("Database wrong!\n");
				goto end;
			}
			freeReplyObject(r);
		}
	}
	else
	{

		freeReplyObject(reply);
		printf("Database error!\n");
		goto end;
	}
	freeReplyObject(reply);
	redisAppendCommand(context,"del %s",events_store);
	redisAppendCommand(context,"del %s",actions_store);
	redisGetReply(context,(void **)&reply);
	freeReplyObject(reply);
	redisGetReply(context,(void **)&reply);
	freeReplyObject(reply);
	closeConnect();
	free(events_store);
	free(actions_store);
	return results;
end:
	redisAppendCommand(context,"del %s",events_store);
	redisAppendCommand(context,"del %s",actions_store);
	redisGetReply(context,(void **)&r);
	freeReplyObject(r);
	redisGetReply(context,(void **)&r);
	freeReplyObject(r);
	free(events_store);
	free(actions_store);
	results=NULL;
	closeConnect();
	return results;
}

char* get_events_bind(char *app, char* userid)
{
	//for bind just get what he bind is enough

	char *user_hash=NULL;//need to be a user in istack? check in combine
	char *total=(char *)calloc(strlen(userid)+strlen(app)+2,sizeof(char));
	strcat(total,userid);
	strcat(total,"~");
	strcat(total,app);
	user_hash=get_hash_value(total);
	free(total);

	char *app_hash=NULL;
	app_hash=get_hash_value(app);

	get_events_bindhash(app_hash, user_hash);



	free(app_hash);
	free(user_hash);

	return results;

}

char* get_actions_bind(char *app, char* userid)
{

	char *user_hash=NULL;//need to be a user in istack? check in combine
	char *total=(char *)calloc(strlen(userid)+strlen(app)+2,sizeof(char));
	strcat(total,userid);
	strcat(total,"~");
	strcat(total,app);
	user_hash=get_hash_value(total);
	free(total);

	char *app_hash=NULL;
	app_hash=get_hash_value(app);

	//ger role;
	get_actions_bindhash(app_hash, user_hash);


	free(app_hash);
	free(user_hash);
	return results;

}

char* get_rules_bind(char *app, char* userid)
{

	char *user_hash=NULL;//need to be a user in istack? check in combine
	char *total=(char *)calloc(strlen(userid)+strlen(app)+2,sizeof(char));
	strcat(total,userid);
	strcat(total,"~");
	strcat(total,app);
	user_hash=get_hash_value(total);
	free(total);

	char *app_hash=NULL;
	app_hash=get_hash_value(app);

	//ger role;
	get_rules_bindhash(app_hash, user_hash);

	free(app_hash);
	free(user_hash);
	return results;
}

int createConnect()
{
	redisReply *reply;
	if (context==NULL)
	{
		//context = redisConnect("127.0.0.1", 6379);  
		context = redisConnect("10.200.43.146", 6379);  
		if (context->err)  
		{  
			goto error1;
		}
		redisAppendCommand(context, "AUTH redis");
		redisAppendCommand(context, "SELECT 2");
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
	closeCal++;
	if(closeCal>CLOSETHRES)
	{
		redisFree(context);
		context=NULL;
		closeCal=0;
		printf("connect close!\n");
	}
	return 1;
}

void free_result()
{
	free(results);
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
    float i=0;
    while(i<1)
    {
    add_application("[{\"app\":\"istack\"}]");
    add_user("[{\"app\":\"istack\",\"userid\":\"demo\",\"role\":\"1\"}]");
    add_user("[{\"app\":\"istack\",\"userid\":\"admin\",\"role\":\"0\"}]");
    get_events_bind("istack", "admin");
    get_actions_bind("istack","admin");    

   	char *res=get_events_total("istack","admin");
    //device_state_income("[{ \"uid\": \"11\", \"type\": \"1\", \"state\": 0, \"admin\": \"Test_Admin\" }]");
    if (res!=NULL)
    	printf("total:%s\n",res);
   
    res=get_actions_total("istack","admin");
    //device_state_income("[{ \"uid\": \"11\", \"type\": \"1\", \"state\": 0, \"admin\": \"Test_Admin\" }]");
    if (res!=NULL)
    	printf("total action:%s\n",res);
    i+=1;
    free(results);
    results=NULL;
    combine_user("{\"app\":\"istack\",\"userid\":\"demo\",\"combine\":{\"istack\":\"admin\",\"istack\":\"demo\"}}");

    add_event("{\"name\":\"event1\",\"app\":\"istack\",\"userid\":\"demo\",\"type\":\"PIR\",\"uid\":\"001\",\"state\":\"1\",\"admin\":\"aaa\",\"haha\":1}");
	add_event("{\"name\":\"event2\",\"app\":\"istack\",\"userid\":\"admin\",\"type\":\"PIR\",\"uid\":\"001\",\"state\":\"1\",\"admin\":\"aaa\"}");
	//delete_event_by_user("a2fbd8b4c83979280c571b5f7c4e8683","istack", "admin");
    add_action("{\"name\":\"action1\",\"app\":\"istack\",\"userid\":\"admin\",\"url\":\"hjshi84@163.com\",\"body\":\"give my five\",\"others\":\"things\",\"admin\":\"aaa\"}");
	add_rules("[{\"name\":\"rule1\",\"app\":\"istack\",\"userid\":\"admin\",\"name\": \"1\",\"events\": [{\"uid\":\"001\",\"id\":\"a2fbd8b4c83979280c571b5f7c4e8683\",\"name\":\"1\"}],\"actions\": [{\"id\":\"b4a53925d976b85a0b5b413c1e0a0b3d\", \"name\":\"action_a\"}] ,\"enable\": \"0\",\"repeatable\": \"0\",\"time_constr\":  \"this\",\"admin\": \"admin@inesa.com\"}]");
    add_action("{\"url\":\"http://1.1.1.1/\",\"body\":{\"a\":1},\"app\":\"istack\",\"userid\":\"admin\",\"name\":\"aaa\"}");
	} 
    /*add_user("[{\"app\":\"hjshi\",\"userid\":\"tempid\",\"role\":\"1\"}]");
	add_event("{\"name\":\"event1\",\"app\":\"hjshi\",\"userid\":\"tempid\",\"type\":\"PIR\",\"uid\":\"001\",\"state\":\"1\",\"admin\":\"aaa\"}");
	add_event("{\"name\":\"event2\",\"app\":\"hjshi\",\"userid\":\"adminid\",\"type\":\"PIR\",\"uid\":\"001\",\"state\":\"1\",\"admin\":\"aaa\"}");
	add_event("{\"name\":\"event3\",\"app\":\"hjshi\",\"userid\":\"adminid\",\"type\":\"PIR\",\"uid\":\"002\",\"state\":\"1\",\"admin\":\"aaa\"}");
	add_action("{\"name\":\"action1\",\"app\":\"hjshi\",\"userid\":\"tempid\",\"url\":\"hjshi84@163.com\",\"body\":\"give my five\",\"others\":\"things\",\"admin\":\"aaa\"}");
	add_rules("[{\"name\":\"rule1\",\"app\":\"hjshi\",\"userid\":\"tempid\",\"name\": \"1\",\"events\": [{\"uid\":\"001\",\"id\":\"a2fbd8b4c83979280c571b5f7c4e8683\",\"name\":\"1\"}],\"actions\": [{\"id\":\"b4a53925d976b85a0b5b413c1e0a0b3d\", \"name\":\"action_a\"}] ,\"enable\": \"0\",\"repeatable\": \"0\",\"time_constr\":  \"this\",\"admin\": \"admin@inesa.com\"}]");
	//add_rules("[{\"app\":\"hjshi\",\"userid\":\"tempid\",\"name\": \"1\",\"events\": [{\"uid\":\"001\",\"id\":\"ae2bac2e4b4da805d01b2952d7e35ba4\",\"name\":\"1\"},{\"uid\":\"002\",\"id\":\"d9f5e405a7f74ed652a8f0b31a87f636\",\"name\":\"2\"}],\"actions\": [{\"id\":\"1\", \"name\":\"action_a\"},{\"id\":\"5\", \"name\":\"action_b\"}] ,\"enable\": \"0\",\"repeatable\": \"0\",\"time_constr\":  \"this\",\"admin\": \"admin@inesa.com\"}]");
	////device_state_income("[{\"name\":\"People%20In\",\"uid\":\"10.200.45.148\",\"state\":\"100\"}]");    
	device_state_income("[{\"name\":\"People%20In\",\"uid\":\"001\",\"state\":\"1\"}]"); 
	device_state_income("[{\"name\":\"People%20In\",\"uid\":\"002\",\"state\":\"1\"}]");
	char *a=get_actions_bind("hjshi","tempid");
	printf("events are:%s\n",a);
	free(a);
	//delete_rule_by_user("b783c3f4548ff428707ff6f131b1f369","hjshi","tempid");
	//delete_event_by_user("a2fbd8b4c83979280c571b5f7c4e8683","hjshi", "tempid");
	
	//delete_user("1ad078d2f0358dabddcd0d684ab3144f");
	//delete_rule("b783c3f4548ff428707ff6f131b1f369");
	//delete_action("b4a53925d976b85a0b5b413c1e0a0b3d"); 
	//delete_event("a2fbd8b4c83979280c571b5f7c4e8683");
	//delete_app("cc2469f10dd7b41a6c5ea99d81bfda4c");

	//t_end=clock();
		
 	//printf("%lf\n", (double)(t_end-t_start)/CLOCKS_PER_SEC);*/
 	redisFree(context);
	context=NULL;
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


