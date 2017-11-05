#include "dcc_os_fsck.h"
#include "list.h"

int block_size;
int group_size;

/* location of the super-block in the first group */
#define BASE_OFFSET 1024
#define BLOCK_OFFSET(block) (BASE_OFFSET + (block-1)*block_size)

// calcula onde está o início do grupo i
#define GROUP_OFFSET(i) (BASE_OFFSET + i*group_size)

#define INODE_NO(i,j) ((i*8 + j) + 1)

// ----------------------------------------------------

int check_sb(struct ext2_super_block sb){
	return (sb.s_inode_size == 0) || (sb.s_blocks_count == 0);
}
// ----------------------------------------------------


struct ext2_super_block* 
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
		
		block_size <<= 1;  
	}
	
	printf("Reading super-block from device :\n Inodes count: %u \n Blocks count : %u \n Free Inodes count : %u \n Reserved blocks count   : %u \n Free blocks count: %u\n Creator OS : %u \n First non-reserved inode: %u \n Size of inode structure : %hu \n Inodes per group : %u \n",
		   super->s_inodes_count,  
	       super->s_blocks_count,
	       super->s_r_blocks_count,     // reserved blocks count 
	       super->s_free_inodes_count,
	       super->s_free_blocks_count,
	       super->s_creator_os,
	       super->s_first_ino,          // first non-reserved inode 
	       super->s_inode_size,
	       super->s_inodes_per_group);

	close(img_fd);

	return super;
}
// ----------------------------------------------------


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


/*
 * Passeia pelos descritores de grupo para recuperar os endereços 
 * das tabelas de inodes. 
 */

int** 
find_inode_paths(const char *file_path, int group_count){

	int img_fd;
	if ((img_fd = open(file_path, O_RDONLY)) < 0){
        printf("Falha ao Abrir o arquivo %s.\n", file_path);
        return NULL;
    }

    struct ext2_group_desc gd;

	int **inode_paths = malloc(2 * sizeof(int*));
	inode_paths[0] = malloc(group_count * sizeof(int)); // inode bitmaps
	inode_paths[1] = malloc(group_count * sizeof(int)); // inode tables

    for (int i = 0; i < group_count; ++i)
	{
		int flag = 0;
		lseek(img_fd, GROUP_OFFSET(i) + block_size, SEEK_SET); // vai ao início do grupo e pula a cópia do superblock
		read(img_fd, &gd, sizeof(struct ext2_group_desc)); // lê o descritor do grupo
		
		for (int j = 0; j < group_count; ++j){
			if (inode_paths[0][j] == gd.bg_inode_bitmap) {
				flag = 1; break;
			}
		}

		if(flag == 1) {
			inode_paths[0][i] = 0; // adiciona o endereço da bitmap de inodes ao vetor
			inode_paths[1][i] = 0;
		} else{
			inode_paths[0][i] = gd.bg_inode_bitmap; // adiciona o endereço da bitmap de inodes ao vetor
			inode_paths[1][i] = gd.bg_inode_table; // adiciona o endereço do tabela de inodes ao vetor
		}

	}
    
    return inode_paths;
}
// ----------------------------------------------------


void delete_inode(int inode_no, list* inode_list, unsigned char *bitmap){
	
	removeInode(inode_list, (inode_no)); // removes inode from list
	int i = inode_no/8;
	int j = inode_no%8;

	bitmap[i] &= !(1 << j);

	printf("> > Inode %i REMOVIDO\n\n", inode_no);

}

void check_inode(int img_fd, int inode_no, int *inode_path, list *inode_list, unsigned char *bitmap){ //int *junk_inodes
	struct ext2_inode aux_inode;

	lseek(img_fd, BLOCK_OFFSET(inode_path[1]) + (inode_no)*sizeof(struct ext2_inode), SEEK_SET); // encontra o inode na tabela de inodes
	read(img_fd, &aux_inode, sizeof(struct ext2_inode)); // copia os blocos do inode

	if (S_ISDIR(aux_inode.i_mode)) // CONFERE SE É DIRETÓRIO
	{
		for(int k = 0; k < EXT2_N_BLOCKS; ++k) {	// passeia pelos blocos das entradas do diretório					
			struct ext2_dir_entry_2 dir_entry;
			lseek(img_fd, BLOCK_OFFSET(aux_inode.i_block[k]), SEEK_SET); // encontra a tabela de inodes
			read(img_fd, &dir_entry, sizeof(struct ext2_dir_entry_2)); // copia os blocos do inode
			if (dir_entry.inode != 0 && strlen(dir_entry.name) == 0){  // caso a entrada seja usada e não tenha nome
				printf("opssssssssssssssssssssss\n");
				break;
			} else if (dir_entry.inode == 0) continue; // caso entrada não seja usada
			
			if (dir_entry.name[0] == '.')
			{
				printf("PASTINHAAA\n");
			}

			check_inode(img_fd, dir_entry.inode, inode_path, inode_list, bitmap);

		}
	} else {
		for(int k = 0; k < EXT2_N_BLOCKS; ++k)
		{						
			if (aux_inode.i_block[k] == 0) return;
			int blk_used = isBlockUsed(inode_list, aux_inode.i_block[k]);
			if ((blk_used != (inode_no)) && (blk_used != 0)) // quando inode usa bloco já utilizado por outro inode
			{	
				printf("OOOPSIE!! Inode %i utiliza um bloco já apontado por outro Inode. \nDeseja apagar este Inode? ('s' para 'sim')", (inode_no));
				char c_opt; scanf(" %c",&c_opt);
				if (c_opt == 's'){ // remove inode da lista e do bitmap
					delete_inode(inode_no,inode_list,bitmap);
					lseek(img_fd, BLOCK_OFFSET(inode_path[0]), SEEK_SET);
					write(img_fd, bitmap, block_size * sizeof(unsigned char));
					return;
				} else { printf("> > Inode mantido\n");}
				break;
			}
			else{	// insere INODE na lista
				insert(inode_list, inode_no, aux_inode.i_block[k]);
			}
		}
	}


	if (aux_inode.i_mode == 0)
	{
		printf("JINZU!! inode %i não tem permissões. \nQual permissão deseja aplicar?", (inode_no));
		short int mode_opt; scanf(" %hu",&mode_opt);

		if (mode_opt != 0){
			aux_inode.i_mode = mode_opt;
		}
	}
	return;
}



int*
check_inode_table(const char *file_path, int **inode_paths, int group_count, int i_per_group){

	int img_fd;
	if ((img_fd = open(file_path, O_RDWR)) < 0){
        printf("Falha ao Abrir o arquivo %s.\n", file_path);
        return NULL;
    }

    //int i_block[EXT2_N_BLOCKS]; // container p/ blocos do inode repetido
    unsigned char bitmap[block_size];	// o bitmap tem block_size linhas de 32 bits
    list inode_list;
    newList(&inode_list); 

    printf("\n> > > Conferindo inodes . . .\n\n");

    for (int h = 0; h < group_count; ++h)
    {
    	if (inode_paths[0][h] > 0) lseek(img_fd, BLOCK_OFFSET(inode_paths[0][h]), SEEK_SET); // encontra o bitmap
		else continue; 
		
		read(img_fd, bitmap, block_size * sizeof(unsigned char)); // copia o bitmap

		for(int i = 0; i < block_size; ++i) //percorre cada linha do bitmap
		{
			for(int j = 0; j < 8; ++j) // verifica cada bit
			{
				if (!(bitmap[i] & (1 << j)) || (INODE_NO(i,j)) < 10) // se for diferente de zero = valido
					continue;
				else /*if ((INODE_NO(i,j)) > i_per_group){
					//break;
				} else */{
					int group_paths[2] = {inode_paths[0][h] , inode_paths[1][h]};
					check_inode(img_fd, INODE_NO(i,j), group_paths, &inode_list, bitmap);
				}
			}
		}
    }

    freeList(&inode_list);
    close(img_fd);

    return NULL;
    // quando tiver os endereços das tabelas de inodes, passar por elas checkando falhas nos inodes
}

// ----------------------------------------------------


int main(int argc, char const *argv[])
{
	/* calculate number of block groups on the disk */
	//unsigned int group_count = 1 + (super.s_blocks_count-1) / super.s_blocks_per_group;

	/* calculate size of the group descriptor list in bytes */
	//unsigned int descr_list_size = group_count * sizeof(struct ext2_group_descr);

	block_size = 1024;


	struct ext2_super_block *sb_backup = retrieve_sb_backup(argv[1]);
	fix_superblock(argv[1],sb_backup);


	int group_count = 1 + (sb_backup->s_blocks_count-1) / sb_backup->s_blocks_per_group;
	
	group_size = block_size * sb_backup->s_blocks_per_group;



	check_inode_table(argv[1], find_inode_paths(argv[1], group_count), group_count, sb_backup->s_inodes_per_group);

	free(sb_backup);

	return 0;
}
