#include <stdio.h>  
#include <stdlib.h>  
#include <stddef.h>  
#include <stdarg.h>  
#include <string.h>  
#include <assert.h>  
#include <hiredis/hiredis.h>  
#include <time.h>  
#include "list.h"
static redisContext* context;
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

void add_rules(redisContext *context,int argc,char *argv[])
{
	long long rule_id=0l;
	//get events and actions and add them to redis
	int i=1;//the first argument is ./redis
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
			//if not defined yet
			redisAppendCommand(context, "incr events:id");
			redisGetReply(context,(void **)&r);
			event_id=r->integer;
			freeReplyObject(r);
			
			redisAppendCommand(context, "hset events:%lld %s %s",event_id,argv[i],argv[i+1]);
			redisAppendCommand(context, "sadd events:%lld:rules rule:%lld",event_id,rule_id);
			redisAppendCommand(context, "sadd rule:%lld:events events:%lld",rule_id,event_id);
			redisAppendCommand(context, "sadd %s:events events:%lld",argv[i],event_id);
			redisGetReply(context,(void **)&r);
			freeReplyObject(r);
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

void rule_detect(redisContext *context,event_list *events){
	redisReply* r;
	event_list *now=events;
	do
	{	
		//get from event:id:rules
	    	r = (redisReply*)redisCommand(context, "SMEMBERS %s:rules",now->value); 
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
	    	now=now->next;
	 }while(now!=NULL);
    	
}

void device_state_income(redisContext *context,const char* uid,const char* state)
{
	event_list *events=NULL;
	const char* command1;
	int pass=0;//pass the repeat check;
	redisReply* r;
        printf("Connect to redisServer Success\n");  
	//judge if it is a new dievice,eg. "device:001"
	redisAppendCommand(context, "keys %s",uid);
	redisAppendCommand(context, "hget %s state",uid);
	redisAppendCommand(context, "hset %s state %s",uid,state);
	redisGetReply(context,(void **)&r);
	

	//there exist a wrong need be checked that if device:100 is not hash,then comes the error need delete!
	if(r->type==REDIS_REPLY_ARRAY&&r->elements==0)
	{
		printf("first discovered device!!!\n");
		pass=1;
	}
	freeReplyObject(r);
	
	//search table device:ID:events
	redisGetReply(context,(void **)&r);

	if (!pass&&!strcmp(r->str,state))
	{
		printf("nothing changed!!\n");
		freeReplyObject(r);
		return;
	}
	freeReplyObject(r);
	
	redisGetReply(context,(void **)&r);
	freeReplyObject(r);
	
	////redisAppendCommand(context, "SDIFFSTORE active:events active:events %s:events",uid);
	redisAppendCommand(context, "SMEMBERS %s:events",uid);
	
	////redisGetReply(context,(void **)&r);
	////freeReplyObject(r);
	
	redisGetReply(context,(void **)&r);
	int i=0;

	for(;i<r->elements;i++)
	{
		redisReply *reply= (redisReply*)redisCommand(context, "hget %s %s ",r->element[i]->str,uid);
		//delete untrigged and add triggered event
		if (!strcmp(reply->str,state))
		{
			//now we find the triggered event,		
			//printf("%s\n",r->element[1]->element[i]->str);

				
			add_event_node(&events,r->element[i]->str);
			
			redisReply *otherreply=(redisReply*)redisCommand(context, "SADD active:events %s",r->element[i]->str);
			freeReplyObject(otherreply);

		}
		else
		{
			redisReply *otherreply=(redisReply*)redisCommand(context, "SREM active:events %s",r->element[i]->str);
			freeReplyObject(otherreply);
		}

		freeReplyObject(reply);
	}
	freeReplyObject(r);
	if (events!=NULL)
	{
		rule_detect(context,events);

	}
	delete_event_list(events);


}
  
int main(int argc,char *argv[])  
{  
	/*
	action_list *a=NULL;
	add_action_node(&a,"123");
	add_action_node(&a,"456");
	delete_action_list(a);
	event_list *b=NULL;
	add_event_node(&b,"123");
	add_event_node(&b,"456");
	delete_event_list(b);
	*/
	
	redisContext* context = redisConnect("127.0.0.1", 6379);  
	if ( context->err)  
	{  
		redisFree(context);  
		printf("Connect to redisServer faile\n");  
		return ;  
	}  


    	double t_start,t_end;
    	
    	//rule trigger logic
    	t_start=clock();
    	if(argc==3)
		device_state_income(context,argv[1],argv[2]);    
	t_end=clock();
	/**/
	
	/*add_rules(context,argc,argv);
	/**/
	
	
 	printf("%lf\n", (double)(t_end-t_start)/CLOCKS_PER_SEC);
 	redisFree(context);
	return 0;  
}  
