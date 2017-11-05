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
	new->inode = inode; //printf("INSERE >> inode %i ", inode);
	new->block = block; //printf("block %i \n", block);
	new->prev = rA->head;
	new->next = rA->head->next;
	
	rA->head->next = new;

	//rA->head->next = new;
	rA->size ++;
}

/* ---------------------------------------------------------------------*/
int removeTHIS(list* rA, int x){
	cell* aux = rA->head->next;
	while(aux != NULL){
		if (aux->inode == x)
		{
			if (aux->next != NULL)
			{
				aux->next->prev = aux->prev;
			}
			aux->prev->next = aux->next;
			free(aux);
			rA->size --;
			return 1;
		}
		aux = aux->next;
	}
	return 0;
}

void removeInode(list* rA, int inode){
	cell* aux = rA->head->next;
	while(removeTHIS(rA, inode)){}
	
	
}

/* ---------------------------------------------------------------------*/
int isBlockUsed(list* rA, int block){

	cell* aux = rA->head->next;
	while(aux != NULL){
		if (aux->block == block)
		{
			//printf("jimabalaia + %i %i \n" ,aux->inode , block);
			//printf("\n----%i %i----\n", aux->inode, block);
			return aux->inode;
		}
		//printf("yupi %i\n", aux->block);
		aux = aux->next;
	}
	//printf("not on list %i \n", block);
	return 0;
}