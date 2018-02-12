#define _GNU_SOURCE             
#include <pthread.h>
#include "SortedList.h"
#include <string.h>
#include <stdio.h>

void SortedList_insert(SortedList_t *list, SortedListElement_t *element){
	if(element == NULL || list == NULL){
		fprintf(stderr,"Invalid input pointer\n");
		return;
	}
	
	SortedListElement_t temp = list->next;
	while(temp->key != NULL && strcmp(temp->key, element->key) < 0)
		if(opt_yield & INSERT_YIELD){
				if(!pthread_yield()){
					perror("Fail to yield.\n");
				}
		}
		temp = temp->next;
	}
	if(opt_yield & INSERT_YIELD){
				if(!pthread_yield()){
					perror("Fail to yield.\n");
				}
	}
	temp->prev->next = element;
	element->prev = temp->prev;
	element->next = temp;
	temp->prev = element;	

}

int SortedList_delete( SortedListElement_t *element){
	if(element == NULL){
		fprintf(stderr,"Invalid input pointer\n");
		return 1;
	}
	
	if(element->next->prev == element && element->prev->next ==element){
		if(opt_yield & DELETE_YIELD){
				if(!pthread_yield()){
					perror("Fail to yield.\n");
				}
		}
		element->prev->next = element->next;
		element->next->prev = element->prev;
		free(element);
		return 0;
	}
	
	return 1;
}

SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key){
	if(key == NULL || list == NULL){
		fprintf(stderr,"Invalid input pointer\n");
		exit(1);
	}
	SortedListElement_t temp = list->next;
	while(temp->key != NULL && strcmp(temp->key, element->key) != 0){
		if(opt_yield & LOOKUP_YIELD){
				if(!pthread_yield()){
					perror("Fail to yield.\n");
				}
		}
		temp = temp->next;
	}
	if(temp->key == NULL) return NULL;
	else return temp;
}

int SortedList_length(SortedList_t *list){
	if(list == NULL) return -1;
	int counter = 0;
	SortedListElement_t temp = list->next;
	while(temp != list){
		if(temp->next->prev == temp && temp->prev->next == temp){
			__sync_fetch_and_add(&counter,1);
			if(opt_yield & LOOKUP_YIELD){
				if(!pthread_yield()){
					perror("Fail to yield.\n");
				}
			}
			temp = temp->next;
		}
		else return -1;
	}
	return counter;
}