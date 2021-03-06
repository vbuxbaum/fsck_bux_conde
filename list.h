#ifndef LIST_H_INCLUDED
#define LIST_H_INCLUDED

#include <stdlib.h>
#include <stdio.h>

typedef struct cell /* celula da lista*/
{
	int block;    			/* página que está no frame da memoria */
	int inode;				/* contador de frequencia de chamadas da pagina */
	struct cell* next; 	/* apontador para o proximo  */
	struct cell* prev; 	/* apontador para o anterior */
}cell;

typedef struct list 	/* lista */
{
	int size; 		/* tamanho da lista */
	cell* head; 	/* aponta para celula cabeça */
}list;

void newList(list* rA);

void freeList(list* rA);

int emptyList(list* rA);

void insert(list* rA, int inode, int block);

void removeInode(list* rA, int inode);

int isBlockUsed(list* rA, int block);

int isInodeAlive(list* rA, int inode);

int printList(list* rA);


#endif