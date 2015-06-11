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
#include "redis.h"

//"127.0.0.1"
//"10.200.46.245"
void doTest()  
{  
    redisContext* c = redisConnect("127.0.0.1", 6379);  
    if ( c->err)  
    {  
        redisFree(c);  
        printf("Connect to redisServer faile\n");  
        return ;  
    }  
    printf("Connect to redisServer Success\n");  
      
    const char* command1 = "set stest1 value1";  
    redisReply* r = (redisReply*)redisCommand(c, command1);  
      
    if( NULL == r)  
    {  
        printf("Execut command1 failure\n");  
        redisFree(c);  
        return;  
    }  
    if( !(r->type == REDIS_REPLY_STATUS && strcasecmp(r->str,"OK")==0))  
    {  
        printf("Failed to execute command[%s]\n",command1);  
        freeReplyObject(r);  
        redisFree(c);  
        return;  
    }     
    freeReplyObject(r);  
    printf("Succeed to execute command[%s]\n", command1);  
      
    const char* command2 = "strlen stest1";  
    r = (redisReply*)redisCommand(c, command2);  
    if ( r->type != REDIS_REPLY_INTEGER)  
    {  
        printf("Failed to execute command[%s]\n",command2);  
        freeReplyObject(r);  
        redisFree(c);  
        return;  
    }  
    int length =  r->integer;  
    freeReplyObject(r);  
    printf("The length of 'stest1' is %d.\n", length);  
    printf("Succeed to execute command[%s]\n", command2);  
      
      
    const char* command3 = "get stest1";  
    r = (redisReply*)redisCommand(c, command3);  
    if ( r->type != REDIS_REPLY_STRING)  
    {  
        printf("Failed to execute command[%s]\n",command3);  
        freeReplyObject(r);  
        redisFree(c);  
        return;  
    }  
    printf("The value of 'stest1' is %s\n", r->str);  
    freeReplyObject(r);  
    printf("Succeed to execute command[%s]\n", command3);  
      
    const char* command4 = "get stest2";  
    r = (redisReply*)redisCommand(c, command4);  
    if ( r->type != REDIS_REPLY_NIL)  
    {  
        printf("Failed to execute command[%s]\n",command4);  
        freeReplyObject(r);  
        redisFree(c);  
        return;  
    }  
    freeReplyObject(r);  
    printf("Succeed to execute command[%s]\n", command4);     
      
      
    redisFree(c);  
      
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



void doTestIot()
{	
	long long int ID;
	//get redisContext
	redisContext* context = redisConnect("127.0.0.1", 6379);  
	if ( context->err)  
	{  
		redisFree(context);  
		printf("Connect to redisServer faile\n");  
		return ;  
	}  
        printf("Connect to redisServer Success\n");  
	redisReply *r;
	r = redisCommand(context, "INCR deviceID");
	if( NULL == r)  
	{  
		printf("Execut command1 failure\n");  
		redisFree(context);  
		return;  
	}  
	if( !(r->type == REDIS_REPLY_INTEGER ))  
	{  
		printf("Failed to execute command[]\n");  
		freeReplyObject(r);  
		redisFree(context);  
		return;  
	}     

	ID=r->integer;
	printf("\n%lld\n",r->integer);
    	freeReplyObject(r);  

	//use pipeline
	//hash insert
	redisReply *reply;


	redisAppendCommand(context,"hset device:%lld states %lld",ID,ID);
	redisAppendCommand(context,"hget device:%lld states",ID);
	redisAppendCommand(context,"hget device:%lld states",ID);
	redisGetReply(context,(void **)&reply); // reply for SET
	freeReplyObject(reply);
	redisGetReply(context,(void **)&reply); // reply for GET
	freeReplyObject(reply);
}

void add_rules(int argc,char *argv[])
{
	long long rule_id=0l;
	//get events and actions and add them to redis
	int i=0;//if from normal main the first argument is ./redis, from signal no ./redis
	redisReply* r;	
	
	redisAppendCommand(context, "incr rule:id"); 
	redisGetReply(context,(void **)&r);
	rule_id=r->integer;
	printf("rule id is :%lld\n",rule_id);
	freeReplyObject(r);
	
	for(;i<argc;)
	{
		
		if (!strncasecmp(argv[i], "device:", 6))
		{
			long long event_id;
			//form device:id:stateslist to judge whether a event need to be added
			redisAppendCommand(context, "HEXISTS %s:stateslist %s",argv[i],argv[i+1]);
			redisGetReply(context,(void **)&r);
			int res=r->integer;
			freeReplyObject(r);
			if (res==0)
			{
			
				redisAppendCommand(context, "incr events:id");
				redisGetReply(context,(void **)&r);
				event_id=r->integer;
				freeReplyObject(r);
				
				redisAppendCommand(context, "hset %s:stateslist %s %lld",argv[i],argv[i+1],event_id);
				redisAppendCommand(context, "hset events:%lld %s %s",event_id,argv[i],argv[i+1]);
				
				redisGetReply(context,(void **)&r);
				freeReplyObject(r);
				redisGetReply(context,(void **)&r);
				freeReplyObject(r);
			}
			else if (res==1)
			{
				redisAppendCommand(context, "hget %s:stateslist %s",argv[i],argv[i+1]);
				redisGetReply(context,(void **)&r);
				event_id=atoi(r->str);
				freeReplyObject(r);
			}				
			redisAppendCommand(context, "sadd events:%lld:rules rule:%lld",event_id,rule_id);
			redisAppendCommand(context, "sadd rule:%lld:events events:%lld",rule_id,event_id);
			redisAppendCommand(context, "sadd %s:events events:%lld",argv[i],event_id);
			redisGetReply(context,(void **)&r);
			freeReplyObject(r);
			redisGetReply(context,(void **)&r);
			freeReplyObject(r);
			redisGetReply(context,(void **)&r);
			freeReplyObject(r);
			i=i+2;
			continue;
		}

	}	
	
	
	
}

void rule_detect(const char* event_id){
	redisReply* r;

        //get from event:id:rules
    	r = (redisReply*)redisCommand(context, "SMEMBERS %s:rules",event_id); 
    	int i=0;
	for(;i<r->elements;i++)
	{
		//get rules:id:events set and compare to the active pool
		//use pipeline would be better
		redisReply *reply;
		reply= (redisReply*)redisCommand(context, "SDIFF %s:events active:events",r->element[i]->str);
		if(reply->elements==0)
			printf("results: %s\n",r->element[i]->str);		
		freeReplyObject(reply);
		
	}
    	freeReplyObject(r);
    	
}

void device_state_income(const char* uid,const char* state)
{
	printf("uid is %s, state is %s\n\n\n\n",uid,state);
	const char* command1;
	char* event_id=NULL;
	int pass=0;//pass the repeat check;
	redisReply* r;

	//judge if it is a new dievice,eg. "device:001"
	redisAppendCommand(context, "keys %s",uid);
	redisAppendCommand(context, "hget %s state",uid);
	redisAppendCommand(context, "hset %s state %s",uid,state);
	redisGetReply(context,(void **)&r);
	
	////r = (redisReply*)redisCommand(context, "keys %s",uid); 
	//there exist a wrong need be checked that if device:100 is not hash,then comes the error need delete!
	if(r->type==REDIS_REPLY_ARRAY&&r->elements==0)
	{
		printf("first discovered device!!!\n");
		pass=1;
	}
	freeReplyObject(r);
	
	//search table device:ID:events
	redisGetReply(context,(void **)&r);
	////r = (redisReply*)redisCommand(context, "hget %s state",uid); 
	if (!pass&&!strcmp(r->str,state))
	{
		printf("nothing changed!!\n");
		freeReplyObject(r);
		return;
	}
	freeReplyObject(r);
	
	redisGetReply(context,(void **)&r);
	freeReplyObject(r);
	
    	////r = (redisReply*)redisCommand(context, "SMEMBERS %s:events",uid); 
	redisAppendCommand(context, "SDIFFSTORE active:events active:events %s:events",uid);
	redisAppendCommand(context, "SMEMBERS %s:events",uid);
	
	redisGetReply(context,(void **)&r);
	freeReplyObject(r);
	
	redisGetReply(context,(void **)&r);
	int i=0;
	int res=0;
	for(;i<r->elements;i++)
	{
		redisReply *reply= (redisReply*)redisCommand(context, "hget %s %s ",r->element[i]->str,uid);
		//delete untrigged and add triggered event
		if (res==0&&!strcmp(reply->str,state))
		{
			//now we find the triggered event,		
			//printf("%s\n",r->element[1]->element[i]->str);

			event_id=(char *)malloc(sizeof(char)*(strlen(r->element[i]->str)+1));
			strcpy(event_id,r->element[i]->str);
			res=1;
		}

		freeReplyObject(reply);
	}
	freeReplyObject(r);
	
	if (event_id!=NULL)	
	{
		redisReply *reply=(redisReply*)redisCommand(context, "SADD active:events %s",event_id);
		freeReplyObject(reply);
		rule_detect(event_id);
	}
	
	free(event_id);

}

int createConnect()
{
	if (context==NULL)
	{
		printf("a new connection!!\n");
		context = redisConnect("127.0.0.1", 6379);  
		if ( context->err)  
		{  
			redisFree(context);  
			printf("Connect to redisServer faile\n");  
			return 0;  
		}
		return 0;  
	}  
	else 
	{
		printf("use old connection!!\n");
		return 1;
	}
	
}

int closeConnect()
{
	redisFree(context);
	context=NULL;
	return 1;
}

int copestring(char **dest,char *ori)
{
	(*dest)=(char *)malloc(sizeof(char *)*(1+strlen(ori)));
	strcpy(*dest,ori);
	return 1;
}

int main(int argc,char *argv[])  
{  

	createConnect();
	
	int ret;  
        int msg_id;  
  	
        msg_id = msgget((key_t)1002,0666|IPC_CREAT);  
        if(msg_id == -1){  
                printf("msgget failed\n");  
                closeConnect();
                return -1;  
        }  
        
        printf("msg_id:%d\n",msg_id);  
        msg_buf mb;  
        /**msg_stat:
        
        0: origin
        
        1: after origin when use rule server
        2: after 1 and set device uid 
        3: after 2 and set status then set to 0
        
        5: after origin when use add rule
        6: after 5 and add device uid
        7: after 6 and add device status then set to 5
        8: after 5 and mean close add then set to 0 
        
        10: close
        **/
        int msg_stat=0;
        char *device_uid=NULL;
        char *status=NULL;
        char **devstate=NULL;
        int devnum=0;
  	while(1)
  	{
		ret = msgrcv(msg_id,(void *)&mb, sizeof(((msg_buf *)0)->msg),0,0);  
		if(ret == -1){  
		        printf("msgrcv failed:%d\n",errno);  
		        closeConnect();
		        return -1;  
		}  
		if (msg_stat==0)
		{
			if(mb.msg_type==3)
			{	
				msg_stat=1;
				if(device_uid!=NULL)	
					free(device_uid);
				copestring(&device_uid,mb.msg);
				continue;
			}
			else if (mb.msg_type==4)
			{	
				msg_stat=4;	
			}
			
			
		}
		if (msg_stat==1)
		{
			if(mb.msg_type==3)
			{	
				msg_stat=0;	
				if(status!=NULL)	
					free(status);
				copestring(&status,mb.msg);
				device_state_income(device_uid,status);	
				free(status);
				free(device_uid);
				device_uid=NULL;
				status=NULL;

			}
			continue;
		}
		if (msg_stat==4)
		{
			if(mb.msg_type==4)
			{	
				if(devstate==NULL)
					devstate=(char **)malloc(sizeof(char *)*MAX_DEV_LEN );
				msg_stat=5;
				copestring(&devstate[devnum],mb.msg);
				devnum++;
				
			}
			else if(mb.msg_type==5)
			{
				add_rules(devnum,devstate);
				int local_count=0;
				for(;local_count<devnum;local_count++)
				{
					
					free(devstate[local_count]);
				}
				free(devstate);
				devnum=0;
				devstate=NULL;
				msg_stat=0;
				break;

			}
			continue;
		}
		if (msg_stat==5)
		{
			if(mb.msg_type==4)
			{
				msg_stat=4;
				copestring(&devstate[devnum],mb.msg);
				devnum++;
			}
			continue;
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



