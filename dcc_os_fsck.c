#include "dcc_os_fsck.h"
#include "list.h"

int _block_size;
int _group_size;

/* location of the super-block in the first group */
#define BASE_OFFSET 1024
#define BLOCK_OFFSET(block) (BASE_OFFSET + (block-1)*_block_size)

// calcula onde está o início do grupo i
#define GROUP_OFFSET(i) (BASE_OFFSET + i*_group_size)

// ----------------------------------------------------


struct ext2_super_block * 
retrieve_sb_backup(const char* file_path) {

	int img_fd;

	if ((img_fd = open(file_path, O_RDONLY)) < 0){
        printf("Falha ao Abrir o arquivo %s.\n", file_path);
        return NULL;
    }

	int backups[3] = { SB_BACKUP_1K , SB_BACKUP_2K , SB_BACKUP_4K };

	struct ext2_super_block *super = malloc(sizeof(struct ext2_super_block)); 
	
	for (int i = 0; i < 3; ++i)
	{
		lseek(img_fd, BLOCK_OFFSET(backups[i]), SEEK_SET);
		read(img_fd, super, sizeof(struct ext2_super_block));

		if (i == super->s_log_block_size) break;
		
		_block_size <<= 1;  
	}
	
	/*printf("Reading super-block from device :\n Inodes count: %u \n Blocks count : %u \n Free Inodes count : %u \n Reserved blocks count   : %u \n Free blocks count: %u\n Creator OS : %u \n First non-reserved inode: %u \n Size of inode structure : %hu \n Inodes per group : %u \n",
		   super->s_inodes_count,  
	       super->s_blocks_count,
	       super->s_r_blocks_count,     // reserved blocks count 
	       super->s_free_inodes_count,
	       super->s_free_blocks_count,
	       super->s_creator_os,
	       super->s_first_ino,          // first non-reserved inode 
	       super->s_inode_size,
	       super->s_inodes_per_group);*/

	close(img_fd);

	return super;
}

int 
fix_superblock(const char *file_path, struct ext2_super_block *sb_backup) {
	
	int img_fd;
	if ((img_fd = open(file_path, O_WRONLY)) < 0){
        printf("Falha ao Abrir o arquivo %s.\n", file_path);
        return -1;
    }

    lseek(img_fd, BASE_OFFSET, SEEK_SET);
	write(img_fd, sb_backup, sizeof(struct ext2_super_block));

	close(img_fd);

	return 0;
}

struct ext2_super_block *
check_superblock(const char *file_path){
	int img_fd;
	if ((img_fd = open(file_path, O_RDONLY)) < 0){
        printf("Falha ao Abrir o arquivo %s.\n", file_path);
        return NULL;
    }

    struct ext2_super_block *super = malloc(sizeof(struct ext2_super_block));

    lseek(img_fd, BASE_OFFSET, SEEK_SET);
    read(img_fd, super, sizeof(struct ext2_super_block));

    if (super->s_blocks_count == 0 || super->s_inode_size == 0) {return NULL;}
   
    return super;
}

struct ext2_group_desc * 
read_gdtable(const char *file_path, int group_count){

	int img_fd;
	if ((img_fd = open(file_path, O_RDONLY)) < 0){
        printf("Falha ao Abrir o arquivo %s.\n", file_path);
        return NULL;
    }

    struct ext2_group_desc *gd_table = malloc(group_count * sizeof(struct ext2_group_desc));

    lseek(img_fd, BLOCK_OFFSET(2), SEEK_SET); // vai ao início do grupo e pula a cópia do superblock
    read(img_fd, gd_table, group_count*sizeof(struct ext2_group_desc)); // lê a tabela de descritores de grupo

    return gd_table;
}

char *
read_i_bitmap(int img_fd, int i_bitmap_blk){
	unsigned char *bitmap = malloc(_block_size);

	lseek(img_fd, BLOCK_OFFSET(i_bitmap_blk), SEEK_SET);
	read(img_fd, bitmap, _block_size);

	return bitmap;
}


char *
write_i_bitmap(int img_fd, unsigned char *bitmap, int i_bitmap_blk){
	lseek(img_fd, BLOCK_OFFSET(i_bitmap_blk), SEEK_SET);
	write(img_fd, bitmap, _block_size);
}
/* -------------------------------------------------------
check_i_bitmap :
	> retorna o INDICE do inode no bitmap, caso esteja ATIVO
	> retorna ZERO se inode NÃO ESTIVER ATIVO 
*/
unsigned int 
check_i_bitmap(int i_n, unsigned char *bitmap, int i_per_grp) {
	unsigned int index = (i_n-1)%i_per_grp;

	int i = index/8;
	int j = index%8;

	//printf("%i\n", bitmap[i] & (1 << j));

	//printf("%i\n", bitmap[i]);

	return (bitmap[i] & (1 << j)) > 0 ? index : 0;
}

struct ext2_inode *
read_inode(int img_fd, unsigned int i_index, int i_table_blk ){
	struct ext2_inode *inode = malloc(sizeof(struct ext2_inode)) ;

	lseek(img_fd, BLOCK_OFFSET(i_table_blk) + i_index*sizeof(struct ext2_inode), SEEK_SET);
	read(img_fd, inode, sizeof(struct ext2_inode));

	return inode;
} 

void
write_inode(int img_fd, struct ext2_inode *inode, unsigned int i_index, int i_table_blk ){
	lseek(img_fd, BLOCK_OFFSET(i_table_blk) + i_index*sizeof(struct ext2_inode), SEEK_SET);
	write(img_fd, inode, sizeof(struct ext2_inode));
} 

void 
delete_inode(int img_fd, unsigned int index, unsigned char *bitmap, int i_bitmap_blk){
	unsigned int i = index/8;
	unsigned int j = index%8;

 	//printf("\nJ%i %i\n", bitmap[i], (int)pow(2,j));

	bitmap[i] -= (int)pow(2,j);

	//printf("k%i\n", bitmap[i]);
	
	lseek(img_fd, BLOCK_OFFSET(i_bitmap_blk), SEEK_SET);     	// acha local do bitmap
	write(img_fd, bitmap, _block_size); // escreve novo bitmap no arquivo

}

/* -------------------------------------------------------
check_ind_blocks :
	> retorna o numero do outro Inode, caso um bloco já esteja usado
	> retorna ZERO se estiver TUDO OK 
*/
int 
check_ind_blocks(int img_fd, int i_n, int level, int ind_block_addr, list *i_b_list, int *blk_cnt){
	int links_per_block = _block_size/sizeof(int);
	int *ind_block = malloc(_block_size);

	lseek(img_fd, BLOCK_OFFSET(ind_block_addr),SEEK_SET);
	read(img_fd, ind_block, _block_size);

	for (int i = 0; i < links_per_block; ++i)
	{
		if (ind_block[i] == 0) { continue; }

		if (level > 1) {
			int result = check_ind_blocks(img_fd, i_n, level-1, ind_block[i], i_b_list, blk_cnt);
			if (result)	{ return result; }
				else	{ continue;	}
		}

		int blk_used = isBlockUsed(i_b_list, ind_block[i]);
		if ((blk_used != (i_n)) && (blk_used != 0)) {// quando inode usa bloco já utilizado por outro inode
			return blk_used;
		} 

		*blk_cnt ++;
		insert(i_b_list, i_n, ind_block[i]);
	}

	return 0;
}

/* -------------------------------------------------------
check_inode : 
	> retorna o numero do inode se estiver tudo ok;
	> retorna o numero do outro Inode, caso um bloco já esteja usado
	> retorna ZERO se estiver sem permissões
*/
int
check_inode(int img_fd, int i_n, struct ext2_inode *inode, list *i_b_list){
	int blk_cnt = 0; 
	int result = -1;

	for (int i = 0; i < EXT2_N_BLOCKS && blk_cnt < inode->i_blocks ; ++i) {
		
		if (inode->i_block[i] == 0) { continue; }
		
		if (i == EXT2_IND_BLOCK) { 
			result = check_ind_blocks(img_fd, i_n, 1, inode->i_block[i], i_b_list, &blk_cnt); 
			continue;
		} if (i == EXT2_DIND_BLOCK){
			result = check_ind_blocks(img_fd, i_n, 2, inode->i_block[i], i_b_list, &blk_cnt);
			continue;
		} if (i == EXT2_TIND_BLOCK) {
			result = check_ind_blocks(img_fd, i_n, 3, inode->i_block[i], i_b_list, &blk_cnt);
			continue;
		} 

		int blk_used = isBlockUsed(i_b_list, inode->i_block[i]);
			
		if (blk_used != 0 && blk_used != i_n) { return blk_used; }
			
		blk_cnt ++;
		insert(i_b_list, i_n, inode->i_block[i]);
	}

	if (result >= 0) { 
		return (result > 0) ? result : i_n; 
	}

	return (inode->i_mode == 0) ? 0 : i_n;
}

/* -------------------------------------------------------
check_ind_dir entry :
	> retorna o numero do outro Inode, caso um bloco já esteja usado
	> retorna ZERO se estiver TUDO OK 
*/
/*int 
check_ind_dir_entry(int img_fd, int i_n, int level, int ind_block_addr, list *i_b_list, int *blk_cnt){
	int links_per_block = _block_size/sizeof(struct ext2_dir_entry_2);
	struct ext2_dir_entry_2 *ind_block = malloc(_block_size);

	lseek(img_fd, BLOCK_OFFSET(ind_block_addr),SEEK_SET);
	read(img_fd, ind_block, _block_size);

	for (int i = 0; i < links_per_block; ++i)
	{
		if (ind_block[i].inode == 0) { continue; }

		if (level > 1) {
			int result = check_ind_blocks(img_fd, i_n, level-1, ind_block[i], i_b_list, blk_cnt);
			if (result)	{ return result; }
				else	{ continue;	}
		}

		int blk_used = isInodeAlive(i_b_list, ind_block[i]->inode);
		if ((blk_used != (i_n)) && (blk_used != 0)) {// quando inode usa bloco já utilizado por outro inode
			return blk_used;
		} 

		*blk_cnt ++;
		insert(i_b_list, i_n, ind_block[i]);
	}

	return 0;
}*/


int 
check_dir(int img_fd, int i_n, list *i_found_list, struct ext2_group_desc *gd_table, struct ext2_super_block *super) {
	int index = (i_n-1)%super->s_inodes_per_group;
	int grp   = (i_n-1)/super->s_inodes_per_group;

	

	struct ext2_inode *inode = read_inode(img_fd, index, gd_table[grp].bg_inode_table);

	//int dir_ents*;
	
	for(int i = 0; i < EXT2_N_BLOCKS; ++i) {	// passeia pelas entradas do diretório					
		
		if (inode->i_block[i] == 0) continue;

		// if (i == EXT2_IND_BLOCK) { 
		// 	dir_ents = check_ind_dir_entry(img_fd, i_n, 1, inode->i_block[i], i_b_list, &blk_cnt); 
		// 	continue;
		// } if (i == EXT2_DIND_BLOCK){
		// 	dir_ents = check_ind_dir_entry(img_fd, i_n, 2, inode->i_block[i], i_b_list, &blk_cnt);
		// 	continue;
		// } if (i == EXT2_TIND_BLOCK) {
		// 	dir_ents = check_ind_dir_entry(img_fd, i_n, 3, inode->i_block[i], i_b_list, &blk_cnt);
		// 	continue;
		// } 

		struct ext2_dir_entry_2 *dir_entry;
		unsigned int size = 0;
		unsigned char block[_block_size];

		lseek(img_fd, BLOCK_OFFSET(inode->i_block[i]), SEEK_SET); // encontra a entrada do diretório
		read(img_fd, block, _block_size); // copia os dados da entrada do diretório
		
		dir_entry = (struct ext2_dir_entry_2 *) block;           // first entry in the directory 

		int count = 0;
		
		while(size < inode->i_size && dir_entry->inode > 0) {

	        char file_name[EXT2_NAME_LEN+1];
	        memcpy(file_name, dir_entry->name, dir_entry->name_len);
	        file_name[dir_entry->name_len] = 0;              /* append null char to the file name */
	        printf("%10u %s %i\n", dir_entry->inode, file_name, inode->i_size);

	        grp = (dir_entry->inode-1)/super->s_inodes_per_group;
	        unsigned char *bitmap = read_i_bitmap(img_fd, gd_table[grp].bg_inode_bitmap);

	        if (!check_i_bitmap(dir_entry->inode, bitmap, super->s_inodes_per_group)) {
	        	printf("SAFADO\n");
	        	break;
	        }

	        if (isInodeAlive(i_found_list, dir_entry->inode) == -1) { // inode foi deletado no caminho
	        	dir_entry->name_len = 0;

	        	lseek(img_fd, BLOCK_OFFSET(inode->i_block[i]) + size, SEEK_SET); // encontra a entrada do diretório
				write(img_fd, dir_entry, dir_entry->rec_len); // copia os dados para entrada do diretório

				printf("DEFUNTO\n");

	        	removeInode(i_found_list, dir_entry->inode);
	        } else if (isInodeAlive(i_found_list, dir_entry->inode) == 1)
	        {
	        	/* code */
	        }

	        if ((dir_entry->file_type & 2) && count > 4)
	        {
	        	//check_dir(img_fd, dir_entry->inode, i_found_list, )
	        }
	        dir_entry = (void*) dir_entry + dir_entry->rec_len;      /* move to the next entry */
	        size += dir_entry->rec_len; 
	        count ++;
		}
	}
}



int main(int argc, char const *argv[])
{
	_block_size = 1024;

	printf("\n> > > Conferindo Superbloco . . .\n");

	struct ext2_super_block *superblock = check_superblock(argv[1]);

	if ( superblock == NULL ) {
		printf("> > Erro na leitura do superbloco. \nDeseja recuperar backup? ('s' para sim)");
		char c_opt; scanf(" %c",&c_opt);
		if (c_opt == 's'){
			superblock = retrieve_sb_backup(argv[1]);
			fix_superblock(argv[1],superblock);

			printf("> > Superbloco RECUPERADO com sucesso!\n");
		} else {
			printf(	"> > Superbloco corrompido MANTIDO!\n O FSCK não pode fazer mais nada... Adeus! \n.. Só love Só love ..\n\n");
			free(superblock);
			return -1;
		} // 
	} else { printf("> OK!\n"); }
	
	int group_count = 1 + (superblock->s_blocks_count-1) / superblock->s_blocks_per_group;
	//_group_size = _block_size * superblock->s_blocks_per_group;

	printf("\n> > > Lendo Tabela de Descritores de Grupo . . .\n");

	struct ext2_group_desc *gd_table = read_gdtable(argv[1],group_count);

	if (gd_table == NULL) {
		printf(	"> > Erro na leitura da tabela de descritores de grupo. \n O FSCK não pode fazer mais nada... Adeus! \n.. Só love Só love ..\n\n");
		free(superblock); free(gd_table);
		return -1; // erro na leitura da tabela de descritores
	} else { 
		int img_fd;
		if ((img_fd = open(argv[1], O_RDWR)) < 0){
	        printf("Falha ao Abrir o arquivo %s.\n", argv[1]);
	        return -1;
	    }

	    list i_b_list; // LISTA PARA AUXILIAR CHECKAGEM POR BLOCOS DE MAIS DE UM DONO
    	newList(&i_b_list);

    	list i_found_list; // LISTA PARA REGISTRAR OS INODES ENCONTRADOS NA 1ª VARREDURA
    	newList(&i_found_list);

    	printf("\n> > > Conferindo inodes . . .\n\n");

	    //printf("inode\tblocks\tsize\n");
		
		for (int i_n = superblock->s_first_ino; i_n < superblock->s_inodes_count; ++i_n) {

			int grp = (i_n - 1)/superblock->s_inodes_per_group;

			unsigned char *bitmap = read_i_bitmap(img_fd, gd_table[grp].bg_inode_bitmap);

			

			unsigned int index = check_i_bitmap(i_n, bitmap, superblock->s_inodes_per_group);

			if ( index > 0) {
				struct ext2_inode *inode = read_inode(img_fd, index, gd_table[grp].bg_inode_table);				
				//printf("%i\t%i\t%i\n", i_n, inode->i_blocks, inode->i_size);

				int check_result = check_inode(img_fd, i_n, inode, &i_b_list);

				if ( check_result == i_n ){
					//printf("i_node %i ok\n", i_n);
					insert(&i_found_list, i_n, 1);
					// ADICIONAR À LISTA DE INODES ENCONTRADOS, COM BLOCO = 1 

				} else if (check_result == 0) { 
					printf("\n> > JINZU!! i_node %i tem PERMISSÃO INVÁLIDA.\n", i_n);
					printf("    Qual permissão deseja aplicar? .. ");

					short int mode_opt; scanf(" %hu",&mode_opt);

					while (mode_opt <= 0){
						printf("> Opção inválida... tente um número maior que zero. .. ");
						scanf(" %hu", &mode_opt);
					} 

					inode->i_mode = mode_opt;

					write_inode(img_fd, inode, index, gd_table[grp].bg_inode_table);

				} else {
					printf("\n> > OOPSIE!! i_node %i utiliza BLOCO DO INODE %i. \n", i_n, check_result);
					printf("    Deseja apagar Inode %i? ('s' para 'sim')", i_n);
					char c_opt; scanf(" %c",&c_opt);

					if (c_opt == 's' || c_opt == 'S') { // remove inode da lista e do bitmap
						delete_inode(img_fd, index, bitmap, gd_table[grp].bg_inode_bitmap); 	// remove inode do bitmap e atualiza bitmap
						removeInode(&i_b_list, i_n); 					// remove inode da lista

						inode->i_links_count = 0;
						inode->i_mode = 0;
						inode->i_dtime = (int)time(NULL);
						printf("\n> > i_dtime %i. \n", inode->i_dtime);

						write_inode(img_fd, inode, index, gd_table[grp].bg_inode_table);
						//write_i_bitmap(img_fd, bitmap, )

						printf("> > Inode %i REMOVIDO\n\n", i_n);

						insert(&i_found_list, i_n, -1);
						// ADICIONAR À LISTA DE INODES ENCONTRADOS, COM BLOCO = -1 

					} else printf("> > Inode mantido\n");

				}

			} else { /* inode zerado no bitmap */ }
		} // FIM DO LOOP DE VARREDURA DE INODES

		//printList(&i_found_list);


		// LOOP PARA VARREDURA DE DIRETÓRIOS
		//check_dir(img_fd, 2, &i_found_list, gd_table, superblock); // INODE 2 RESERVADO PARA DIRETÓRIO ROOT
		printf("\n> > > FIM\n");

		close(img_fd);
		

		

	}

	return 0;
}
