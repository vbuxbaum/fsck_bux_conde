#include "dcc_os_fsck.h"

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
	
	/*printf("Reading super-block from device :\n Inodes count: %u \n Blocks count : %u \n Reserved blocks count   : %u \n Free blocks count: %u\n Creator OS : %u \n First non-reserved inode: %u \n Size of inode structure : %hu \n",
		   super->s_inodes_count,  
	       super->s_blocks_count,
	       super->s_r_blocks_count,     // reserved blocks count 
	       super->s_free_blocks_count,
	       super->s_creator_os,
	       super->s_first_ino,          // first non-reserved inode 
	       super->s_inode_size);
 	*/
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
 * Passeia pelos descritores de grupo para tentar recuperar os endereços 
 * das tabelas de inodes. 
 */

int* 
find_inode_tables(const char *file_path, int group_count){
	int img_fd;
	if ((img_fd = open(file_path, O_RDONLY)) < 0){
        printf("Falha ao Abrir o arquivo %s.\n", file_path);
        return NULL;
    }

    struct ext2_group_desc gd;
	//int *gd_blocks = malloc(group_count * sizeof(int));
	int *inode_tables = malloc(group_count * sizeof(int));

    for (int i = 0; i < group_count; ++i)
	{
		lseek(img_fd, GROUP_OFFSET(i) + block_size, SEEK_SET); // vai ao início do grupo e pula a cópia do superblock
		read(img_fd, &gd, sizeof(struct ext2_group_desc)); // lê o descritor do grupo
		inode_tables[i] = gd.bg_inode_table; // adiciona o endereço da tabela de inodes ao vetor
		
		// printf pra ter noção do que tá rolando :P
		printf("%i %i %i %i\n", gd.bg_block_bitmap, gd.bg_inode_bitmap, gd.bg_inode_table, gd.bg_free_blocks_count);
	}
    
    return inode_tables;
}

int*
check_inodes(const char *file_path, int *inode_tables, int group_count){
	int img_fd;
	if ((img_fd = open(file_path, O_RDONLY)) < 0){
        printf("Falha ao Abrir o arquivo %s.\n", file_path);
        return NULL;
    }

    for (int i = 0; i < group_count; ++i)
    {
    	/* code */
    }


    close(img_fd);

    return NULL;
    // quando tiver os endereços das tabelas de inodes, passar por elas checkando falhas nos inodes
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

	find_inode_tables(argv[1], group_count);

	free(sb_backup);

	return 0;
}
