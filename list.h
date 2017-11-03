#ifndef FUNC_H_INCLUDED
#define FUNC_H_INCLUDED

#include <stdlib.h>

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

int findBlock(list* rA, int block);


#endif