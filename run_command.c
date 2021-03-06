#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>  
#include <sys/ipc.h>  
#include <sys/msg.h>  
#include <string.h>  
#include <sys/stat.h>  
#include <unistd.h>  
#include <errno.h>  
#define LEN 10
typedef struct {  
        long int my_msg_type; //64位机器必须设置为long  
        char msg[512];  
}msg_buf;  

int main(int argc,char *argv[])  
{  
	int i;  
        int ret;  
        int msg_id;  
  	
        msg_id = msgget((key_t)1002,0666|IPC_CREAT);  
        //msg_id=524290;
        if(msg_id == -1){  
                printf("msgget failed\n");  
                return -1;  
        }  
        printf("msg_id=%d\n",msg_id);  
  	int time=0;
  	while(time<1)
 	{ 	
	  	for(i=0;i<argc;i++)
	  	{
	  		msg_buf mb;  
	  		if(i<argc-1)
			{

				mb.my_msg_type = 4;  
				strcpy(mb.msg,argv[i+1]);  
			}
			else
			{

				mb.my_msg_type = 5;  
				strcpy(mb.msg,"bye!");  
			}
		  
			ret = msgsnd(msg_id,(void *)&mb,sizeof(((msg_buf *)0)->msg),0);  
			if(ret == -1){  
				printf("msgsnd failed\n");  
				return -1;  
			}  
			printf("message sent:%d\n",ret);  
		}
		time++;
  	}
        return 0;  

}  
