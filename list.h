#ifndef _LIST_H
#define _LIST_H

typedef struct action_list
{
	char *value;
	struct action_list *next;
} action_list;

typedef struct event_list
{
	char *value;
	struct event_list *next;
} event_list;

typedef struct rule_list
{
	action_list *actions_list;
	event_list *events_list;
	struct rule_list *next;
} rule_list;

int delete_event_list(event_list *head);
int delete_action_list(action_list *head);
int add_action_node(action_list **head,char *val);
int add_event_node(event_list **head,char *val);

#endif
