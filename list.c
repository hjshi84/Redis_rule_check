#include "list.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int add_action_node(action_list **head,char *val)
{
	char *temp=(char *)malloc((strlen(val)+1)*sizeof(char));
	strcpy(temp,val);
	action_list *temp_list=(action_list *)malloc(sizeof(action_list));
	temp_list->value=temp;
	temp_list->next=NULL;
	if ((*head)==NULL)
	{
		*head=temp_list;
		return 1;
	}
	action_list *last=(*head);
	while(last->next!=NULL)
		last=last->next;
	last->next=temp_list;
	return 1;
}  

int add_event_node(event_list **head,char *val){
	char *temp=(char *)malloc((strlen(val)+1)*sizeof(char));
	strcpy(temp,val);
	event_list *temp_list=(event_list *)malloc(sizeof(event_list));
	temp_list->value=temp;
	temp_list->next=NULL;
	if ((*head)==NULL)
	{
		*head=temp_list;
		return 1;
	}
	event_list *last=(*head);
	while(last->next!=NULL)
		last=last->next;
	last->next=temp_list;
	return 1;
}

int delete_action_list(action_list *head)
{
	if (head==NULL)
		return 1;
	action_list *temp;
	do
	{
		temp=head;
		head=head->next;
		free(temp->value);
		free(temp);
	}while((head!=NULL));
	return 1;
}

int delete_event_list(event_list *head)
{
	if (head==NULL)
		return 1;
	event_list *temp;
	do
	{
		temp=head;
		head=head->next;
		free(temp->value);
		free(temp);
	}while((head!=NULL));
	return 1;
}
