#include <stdio.h>
#include <time.h>
#include "redis.h"

int main(int argc,char *argv[])  
{  
	
	createConnect();



    	double t_start,t_end;
    	
    	t_start=clock();
    	if(argc==3)
		device_state_income(argv[1],argv[2]);    
	t_end=clock();
	/**/
	
	
	/*t_start=clock();
	add_rules(argc,argv);
	t_end=clock();
	/**/
	
	
 	printf("%lf\n", (double)(t_end-t_start)/CLOCKS_PER_SEC);
 	//closeConnect();
	return 0;  
}  
