#include "list.h"

/* ---------------------------------------------------------------------*/
void newList(list* rA){
	rA->head = (cell*)malloc(sizeof(cell));
	rA->head->next = NULL;
	rA->head->prev = NULL;
	rA->size = 0;
}

/* ---------------------------------------------------------------------*/
void removeFirst(list* rA){
	cell* aux = rA->head->next;
	rA->head->next = aux->next;
	if (emptyList(rA))
	{
		/* DO NOTHING */
	}else{
		aux->next->prev = rA->head;
	}
	free(aux);
	rA->size --;
}

/* ---------------------------------------------------------------------*/
void freeList(list* rA){
	while(!emptyList(rA)){
		removeFirst(rA);
	}
	free(rA->head);
}

/* ---------------------------------------------------------------------*/
int emptyList(list* rA){
	if (rA->head->next == NULL){
		return 1;
	}
	return 0;
}

/* ---------------------------------------------------------------------*/
void insert(list* rA, int inode, int block){
	cell* new = (cell*)malloc(sizeof(cell));
	new->inode = inode;
	new->block = block;
	new->prev = rA->head;
	new->next = rA->head->next;

	if (emptyList(rA) == 1)
	{
		rA->head->prev = new;
	}else{
		rA->head->next->prev = new;	
	}
	rA->head->next = new;
	rA->size ++;
}

/* ---------------------------------------------------------------------*/
void removeInode(list* rA, int inode){
	cell* aux = rA->head->next;
	while(aux != NULL){
		if (aux->inode == inode)
		{
			if (aux->next != NULL)
			{
				aux->next->prev = aux->prev;
			}
			aux->prev->next = aux->next;
			free(aux);
			rA->size --;
		}
		aux = aux->next;
	}
}

/* ---------------------------------------------------------------------*/
int isBlockUsed(list* rA, int block){
	cell* aux = rA->head->next;
	while(aux != NULL){
		if (aux->block == block)
		{
			return 1;
		}
		aux = aux->next;
	}
	return 0;
}