#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include "cJSON.h"


int main(int argc,char *argv[])  
{
	char *content="{\"name\": \"1\",\"events\": [{\"uid\":\"001\",\"id\":\"1\",\"name\":\"1\"},{\"uid\":\"002\",\"id\":\"2\",\"name\":\"2\"}],\"actions\": [{\"id\":\"1\", \"name\":\"action_a\"},{\"id\":5, \"name\":\"action_b\"}] ,\"enable\": \"0\",\"repeatable\": 0, \"time_constr\":  \"this\",\"admin\": \"admin@inesa.com\"}";
	
	char *out;
	cJSON *json1;
	int json0;
	cJSON *json;
	json1=cJSON_Parse(content);
	json0=4;
	int i=0;
	char *aa=cJSON_PrintUnformatted(cJSON_GetArrayItem(json1,1));
	printf("\nllalal:   %s   :llll\n",aa);
	/*for(;i<json0;i++)
	{
		int p=0;
		cJSON *rule_content=cJSON_GetArrayItem(json1,i);
		printf("haha\n%s\ngetit\n",cJSON_Print(rule_content));
		out=cJSON_Print(json);
		cJSON_Delete(json);
		printf("%s\n",out);
		free(out);
	}*/

	return 0;
}
