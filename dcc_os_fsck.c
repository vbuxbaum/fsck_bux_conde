#include "dcc_os_fsck.h"
#include "list.h"

int _block_size;
int _group_size;

/* location of the super-block in the first group */
#define BASE_OFFSET 1024
#define BLOCK_OFFSET(block) (BASE_OFFSET + (block-1)*_block_size)

// calcula onde está o início do grupo i
#define GROUP_OFFSET(i) (BASE_OFFSET + i*_group_size)


int INODE_NO(int i, int j){
	return (i*8 + j) +1;
}

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

    lseek(img_fd, BLOCK_OFFSET(0), SEEK_SET);
    read(img_fd, super, sizeof(struct ext2_super_block));

    if (super->s_blocks_count == 0 || super->s_inode_size == 0) return NULL;
   
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


/*

					~~~~~~~~~~~~~~~~~
					S O M E T H I N G 
					~~~~~~~~~~~~~~~~~
*/

int main(int argc, char const *argv[])
{
	_block_size = 1024;

	printf("\n> > > Conferindo Superbloco . . .\n\n");

	struct ext2_super_block *superblock = check_superblock(argv[1]);

	if ( superblock == NULL ) {
		printf("Erro na leitura do superbloco. \nDeseja recuperar backup? ('s' para sim)");
		char c_opt; scanf(" %c",&c_opt);
		if (c_opt == 's'){
			superblock = retrieve_sb_backup(argv[1]);
			fix_superblock(argv[1],superblock);

			printf("> > Superbloco RECUPERADO com sucesso!\n");
		} else {
			printf(	"> > Superbloco corrompido MANTIDO!\n O FSCK não pode fazer mais nada... Adeus! \n.. Só love Só love ..\n\n");
			free(superblock);
			return 0;
		} // 
	}
	
	int group_count = 1 + (superblock->s_blocks_count-1) / superblock->s_blocks_per_group;
	//_group_size = _block_size * superblock->s_blocks_per_group;

	printf("\n> > > Lendo Tabela de Descritores de Grupo . . .\n\n");

	struct ext2_group_desc *gd_table = read_gdtable(argv[1],group_count);

	for (int i = 0; i < group_count; ++i)
	{
		struct ext2_group_desc gd = gd_table[i];

		printf("%i\n%i\n%i\n%hu\n%hu\n%hu\n%hu\n ---------- \n",gd.bg_block_bitmap, gd.bg_inode_bitmap, gd.bg_inode_table, gd.bg_free_blocks_count,gd.bg_free_inodes_count,gd.bg_used_dirs_count, gd.bg_pad );
	}

	/*if (gd_table == NULL) {
		printf(	"Erro na leitura da tabela de descritores de grupo. 
				\n O FSCK não pode fazer mais nada... Adeus! \n.. Só love Só love ..\n\n");
		free(superblock); free(gd_table);
		return 0;
	} else {
		
		for (int i_n = super->s_first_ino; i < superblock->s_inodes_count; ++i_n) {

			int grp = 

			if (check_i_bitmap(i_n, &gd_table[grp])) {
				struct ext2_inode *inode = 
			}

		}
	}
*/
	printf("\n> > > Conferindo INodes . . .\n\n");



	return 0;
}
