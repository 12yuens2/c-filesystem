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

// #define MY_MAX_DATA_SIZE 4

// struct myfcb
// {
// 	char path[MY_MAX_PATH];
// 	uuid_t file_data_id;
	
// 	// see 'man 2 stat' and 'man 2 chmod'
// 	//meta-data for the 'file'
// 	uid_t  uid;		/* user */
//     gid_t  gid;		/* group */
// 	mode_t mode;	/* protection */
// 	time_t mtime;	/* time of last modification */
// 	time_t ctime;	 time of last change to meta-data (status) 
// 	off_t size;		/* size */

	
// 	uuid_t data;
	
// 	//meta-data for the root thing (directory)
// 	uid_t  root_uid;		/* user */
//     gid_t  root_gid;		/* group */
// 	mode_t root_mode;	/* protection */
// 	time_t root_mtime;	/* time of last modification */
// };


//treat files and directories as the same thing (an inode)
typedef struct my_inode
{
	uuid_t id;

	//data block if file, dir_data_fcb if directory
	uuid_t data_id;

	// uuid_t direct_block;
	// uuid_t indirect_block;

	uid_t uid;
	gid_t gid;
	mode_t mode;
	time_t mtime;	
	time_t ctime;
	off_t size;


} my_inode;


// typedef struct dir_fcb
// {
// 	myinode inode;

// 	char path[MY_MAX_PATH];//?

// } dir_fcb;


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






typedef struct dir_entry 
{
	uuid_t inode_id;
	char filename[MY_MAX_FILE_NAME];
} dir_entry;


typedef struct dir_data_fcb
{
	//own id
	uuid_t id;

	dir_entry entries[MY_MAX_DIR_FILES];

	// char filenames[MY_MAX_DIR_FILES][MY_MAX_FILE_NAME];
	// uuid_t files[MY_MAX_DIR_FILES];

} dir_data_fcb;


