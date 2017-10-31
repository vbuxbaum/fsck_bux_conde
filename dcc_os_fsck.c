#include "dcc_os_fsck.h"

int block_size;

/* location of the super-block in the first group */
#define BASE_OFFSET 1024
#define BLOCK_OFFSET(block) (BASE_OFFSET + (block-1)*block_size)


struct ext2_super_block* 
save_first_attack(const char* file_path){

	int img_fd;

	if ((img_fd = open(file_path, O_RDONLY)) < 0){
        printf("Falha ao Abrir o arquivo %s.\n", file_path);
        return NULL;
    }

	int init_b_size = block_size;

	int backups[3] = { SB_BACKUP_1K , SB_BACKUP_2K , SB_BACKUP_4K };

	struct ext2_super_block *super = malloc(sizeof(struct ext2_super_block)); 
	
	for (int i = 0; i < 3; ++i)
	{
		lseek(img_fd, BLOCK_OFFSET(backups[i]), SEEK_SET);
		read(img_fd, super, sizeof(struct ext2_super_block));

		if (i == super->s_log_block_size) break;
		
		block_size <<= 1;  
	}
	
	printf("Reading super-block from device :\n Inodes count: %u \n Blocks count : %u \n Reserved blocks count   : %u \n Free blocks count: %u\n Creator OS : %u \n First non-reserved inode: %u \n Size of inode structure : %hu \n",
		   super->s_inodes_count,  
	       super->s_blocks_count,
	       super->s_r_blocks_count,     // reserved blocks count 
	       super->s_free_blocks_count,
	       super->s_creator_os,
	       super->s_first_ino,          // first non-reserved inode 
	       super->s_inode_size);
 
	close(img_fd);
	return super;
}

int main(int argc, char const *argv[])
{
	/* calculate number of block groups on the disk */
	//unsigned int group_count = 1 + (super.s_blocks_count-1) / super.s_blocks_per_group;

	/* calculate size of the group descriptor list in bytes */
	//unsigned int descr_list_size = group_count * sizeof(struct ext2_group_descr);

	block_size = 1024;

	save_first_attack(argv[1]);


	//lseek(img_fd, 1024 + block_size, SEEK_SET);
	//read(img_fd, &gd, sizeof(struct ext2_group_desc));



	return 0;
}
