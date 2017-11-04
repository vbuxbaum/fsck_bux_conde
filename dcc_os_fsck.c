#include "dcc_os_fsck.h"
#include "list.h"

int block_size;
int group_size;

/* location of the super-block in the first group */
#define BASE_OFFSET 1024
#define BLOCK_OFFSET(block) (BASE_OFFSET + (block-1)*block_size)

// calcula onde está o início do grupo i
#define GROUP_OFFSET(i) (BASE_OFFSET + i*group_size)


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
	
	printf("Reading super-block from device :\n Inodes count: %u \n Blocks count : %u \n Reserved blocks count   : %u \n Free blocks count: %u\n Creator OS : %u \n First non-reserved inode: %u \n Size of inode structure : %hu \n Inodes per group : %u \n",
		   super->s_inodes_count,  
	       super->s_blocks_count,
	       super->s_r_blocks_count,     // reserved blocks count 
	       super->s_free_blocks_count,
	       super->s_creator_os,
	       super->s_first_ino,          // first non-reserved inode 
	       super->s_inode_size,
	       super->s_inodes_per_group);

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
	//int *gd_blocks = malloc(group_count * sizeof(int));

	int **inode_paths = malloc(2 * sizeof(int*));
	inode_paths[0] = malloc(group_count * sizeof(int)); // inode bitmaps
	inode_paths[1] = malloc(group_count * sizeof(int)); // inode tables

    for (int i = 0; i < group_count; ++i)
	{
		lseek(img_fd, GROUP_OFFSET(i) + block_size, SEEK_SET); // vai ao início do grupo e pula a cópia do superblock
		read(img_fd, &gd, sizeof(struct ext2_group_desc)); // lê o descritor do grupo
		
		if (gd.bg_block_bitmap != 0)
		{
			inode_paths[0][i] = gd.bg_inode_bitmap; // adiciona o endereço da bitmap de inodes ao vetor
			inode_paths[1][i] = gd.bg_inode_table; // adiciona o endereço do tabela de inodes ao vetor
		} else {
			inode_paths[0][i] = -1;
			inode_paths[1][i] = -1;
		}
	
		// printf pra ter noção do que tá rolando :P
		printf("%i %i\n", inode_paths[0][i], inode_paths[1][i]);
	}
    
    return inode_paths;
}

int*
check_inodes(const char *file_path, int **inode_paths, int group_count, int inode_size){

	int img_fd;
	if ((img_fd = open(file_path, O_RDONLY)) < 0){
        printf("Falha ao Abrir o arquivo %s.\n", file_path);
        return NULL;
    }

    //int i_block[EXT2_N_BLOCKS]; // container p/ blocos do inode repetido
    int bitmap[block_size];	// o bitmap tem block_size linhas de 32 bits
    list inode_list;
    newList(&inode_list); 

    struct ext2_inode aux_inode;

    for (int h = 0; h < group_count; ++h)
    {
    	if (inode_paths[0][h] > 0) lseek(img_fd, inode_paths[0][h], SEEK_SET); // encontra o bitmap
		else continue; 
		
		read(img_fd, bitmap, block_size * sizeof(int)); // copia o bitmap

		for(int i = 0; i < block_size; ++i) //percorre cada linha do bitmap
		{
			for(int j = 0; j < 32; ++j) // verifica cada bit
			{
				if (!(bitmap[i] & (1 << j))) // se for diferente de zero = valido
					continue;
				else
				{
					lseek(img_fd, BLOCK_OFFSET(inode_paths[1][h])+ (i*31 + j)*inode_size, SEEK_SET); // encontra a tabela de inodes
					read(img_fd, &aux_inode, sizeof(struct ext2_inode)); // copia os blocos do inode

					//check if any of the EXT2_N_BLOCKS (plus double, triple tables)
					//addresses are already on the list, if not, add them
					for(int k = 0; k < EXT2_N_BLOCKS; ++k)
					{
						if (isBlockUsed(&inode_list, aux_inode.i_block[k]))
						{
							removeInode(&inode_list, i*31 + j); // removes inode from list
							bitmap[i] &= !(1 << j);
							break;
							//remove_inode(j) //this function has to remove the jth entry from the inode table
						}
						else{
							printf("%i\t%i\n", i*31 + j, aux_inode.i_block[k] );
							insert(&inode_list, aux_inode.i_block[k], i*31 + j);
						}
					}

				}
			}
		}

    }

    freeList(&inode_list);
    close(img_fd);

    return NULL;
    // quando tiver os endereços das tabelas de inodes, passar por elas checkando falhas nos inodes
}

void interface(const char *file_path){
	//
}

int main(int argc, char const *argv[])
{
	/* calculate number of block groups on the disk */
	//unsigned int group_count = 1 + (super.s_blocks_count-1) / super.s_blocks_per_group;

	/* calculate size of the group descriptor list in bytes */
	//unsigned int descr_list_size = group_count * sizeof(struct ext2_group_descr);

	block_size = 1024;

	/*  SOLVE FIRST ATTACK  */
	struct ext2_super_block *sb_backup = retrieve_sb_backup(argv[1]);
	fix_superblock(argv[1],sb_backup);


	int group_count = 1 + (sb_backup->s_blocks_count-1) / sb_backup->s_blocks_per_group;
	
	group_size = block_size * sb_backup->s_blocks_per_group;



	check_inodes(argv[1], find_inode_paths(argv[1], group_count), group_count, sb_backup->s_inode_size);

	free(sb_backup);

	return 0;
}
