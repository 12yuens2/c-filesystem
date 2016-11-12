#include "fs.h"

#define MY_MAX_PATH 100

// #define MY_MAX_FILE_SIZE 1000
#define MY_MAX_DIR_FILES 32
#define MY_MAX_FILE_NAME 255

#define MY_MAX_DATA_SIZE 4
#define MY_MAX_DIRECT_BLOCKS 8

#define MY_DATA_SIZE_PER_BLOCK MY_MAX_DATA_SIZE * MY_MAX_DIRECT_BLOCKS
#define MY_MAX_FILE_SIZE MY_MAX_DIRECT_BLOCKS * MY_DATA_SIZE_PER_BLOCK

#define FLOOR(x,y) ((x > y) ? y : x)


/* Inode struct */
typedef struct my_inode
{
	uuid_t id;

	//file_fcb if file, dir_fcb if directory
	uuid_t data_id;

	uid_t uid;
	gid_t gid;
	mode_t mode;
	time_t mtime;	
	time_t ctime;
	off_t size;

} my_inode;


/**
 * File data structs 
 */
typedef struct data_block
{
	uuid_t id;
	size_t size;
	uint8_t data[MY_MAX_DATA_SIZE];

} data_block;

typedef struct direct_block
{
	uuid_t id; 

	//data blocks
	uuid_t blocks[MY_MAX_DIRECT_BLOCKS];

} direct_block;

typedef struct file_data_fcb
{
	uuid_t id; 

	//direct data blocks
	uuid_t direct_data_id;

	//indirect level 1 index
	uuid_t index_ids[MY_MAX_DIRECT_BLOCKS];

} file_data_fcb;



/**
 * Directory structs
 */
typedef struct dir_entry 
{
	uuid_t inode_id;
	char filename[MY_MAX_FILE_NAME];
} dir_entry;


typedef struct dir_data_fcb
{
	//own id
	uuid_t id;

	//array of dir_entries
	dir_entry* entries;

} dir_data_fcb;


