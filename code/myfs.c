#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

#include "myfs.h"

#define INODE_SIZE sizeof(my_inode)
#define DIR_DATA_SIZE ((unqlite_int64) sizeof(dir_data_fcb))

//fcb of root directiory
my_inode the_root_fcb;

char UUID_BUF[100];

char* get_uuid(uuid_t uuid)
{
	uuid_unparse(uuid, UUID_BUF);
	return UUID_BUF;
}


void error_handle(int rc) 
{
	if(rc != UNQLITE_OK)
	{
		error_handler(rc);
	}
}

//INEFFICIENT, FIX LATER
char* get_file_name(const char* path)
{
	char* pathP = strdup(path);
	
	char* p;
	char* r;

	while((p = strsep(&pathP, "/")) != NULL)
	{
		r = p;
	}

	return r;
}


/**
 * Fetches the given 'id' from the db into the given 'memory_address'
 * 'size' is the expected size of what is fetched.
 */
int fetch_from_db(uuid_t id, void* memory_address, size_t size)
{
	//error checking
	int rc;
	unqlite_int64 nBytes = size;  //Data length.

	
	rc = unqlite_kv_fetch(pDb, id, KEY_SIZE, NULL, &nBytes);
	if (rc != UNQLITE_OK)
	{
		write_log("not found in db\n");
		return -1;
	}
	// error_handle(rc);
	// write_log("nbyets: %d | fcb: %d\n", nBytes, size);

	//error check we fetched the right thing
	if(nBytes!=size)
	{
		write_log("Data object has unexpected size. Doing nothing.\n");
		exit(-1);
	}

	//Fetch the fcb that the root object points at. We will probably need it.
	unqlite_kv_fetch(pDb, id, KEY_SIZE, memory_address, &nBytes);

	return nBytes;
}

int store_to_db(uuid_t id, void* memory_address, size_t size)
{
	int rc = unqlite_kv_store(pDb, id, KEY_SIZE, memory_address, size);
	// error_handle(rc);
	return rc;
}


/**
 * 
 * Returns NULL if path not found.
 */
int get_inode(const char* path, int get_parent, my_inode* inode_buf)
{

	write_log("get_inode for path '%s'\n", path);
	char* str = strdup(path);

	//remove path after last '/' because we want the directory before it
	if(get_parent)
	{
		char* r = strrchr(str, '/');
		*r = 0;
	}

	char* partial_path;
	my_inode current_inode;

	while((partial_path = strsep(&str, "/")) != NULL)
	{
		//root directory
		if (strcmp(partial_path, "") == 0)
		{
			current_inode = the_root_fcb;
		} 
		else 
		{
			dir_data_fcb dir_fcb;
			int rc = fetch_from_db(current_inode.data_id, &dir_fcb, sizeof(dir_data_fcb)); //FIX NEED TO FAIL IF NOT DIRECTORY MEANINGFUL ERROR

			if (rc < 0)
			{
				write_log("not found in db\n");
				return -ENOENT;
			}

			//loop through current inode
			int found = 0;
			for (int i = 0; i<MY_MAX_DIR_FILES; i++)
			{
				dir_entry entry = dir_fcb.entries[i];
				if (strcmp(entry.filename, partial_path) == 0)
				{
					//fetch next inode
					found = 1;
					write_log("FOUND\n");

					fetch_from_db(entry.inode_id, &current_inode, sizeof(my_inode));

					break;
				}
			}

			if (!found)
			{
				write_log("not found in fs\n");
				return -ENOENT;
			}
		}
	}
	memcpy(inode_buf, &current_inode, sizeof(my_inode));
	return 0;
}

void update_parent(my_inode* parent_fcb, uuid_t new_inode_id, const char* path)
{
	dir_data_fcb parent_data;

	fetch_from_db(parent_fcb->data_id, &parent_data, sizeof(dir_data_fcb));

	//add to parent
	for (int i = 0; i<MY_MAX_DIR_FILES; i++) 
	{
		dir_entry entry = parent_data.entries[i];

		//empty entry
		if(strcmp(entry.filename, "") == 0)
		{
			uuid_copy(entry.inode_id, new_inode_id);
			strcpy(entry.filename, get_file_name(path));
		
			write_log("making new inode %s\n", entry.filename);

			parent_data.entries[i] = entry;
			break;
		}
	}

	store_to_db(parent_fcb->data_id, &parent_data, sizeof(dir_data_fcb));
}


// Get file and directory attributes (meta-data).
// Read 'man 2 stat' and 'man 2 chmod'.
static int myfs_getattr(const char *path, struct stat *stbuf)
{
	write_log("myfs_getattr(path=\"%s\", statbuf=0x%08x)\n", path, stbuf);

	memset(stbuf, 0, sizeof(struct stat));


	//is root
	if (strcmp(path, "/") == 0)
	{
		stbuf->st_mode = the_root_fcb.mode;
		stbuf->st_nlink = 2;
		stbuf->st_uid = the_root_fcb.uid;
		stbuf->st_gid = the_root_fcb.gid;
	}
	else
	{
		dir_data_fcb root_data;
		int bytes = fetch_from_db(the_root_fcb.data_id, &root_data, sizeof(dir_data_fcb));

		if (strstr(path, "/") != NULL)
		{
			//loop through directory files starting from the root
			my_inode* inode;

			int rc = get_inode(path, 0, inode);

			if (rc < 0)
			{
				write_log("myfs_getattr - ENOENT\n");
				return rc;
			}
			else 
			{
				stbuf->st_mode = inode->mode;
				stbuf->st_nlink = 1;
				stbuf->st_mtime = inode->mtime;
				stbuf->st_uid = inode->uid;
				stbuf->st_gid = inode->gid;
				stbuf->st_size = inode->size;
			}
		}
		else
		{
			write_log("myfs_getattr - ENOENT\n");
			return -ENOENT;
		}
	}
	return 0;
}

// Read a directory.
// Read 'man 2 readdir'.
static int myfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;

	write_log("write_readdir(path=\"%s\", buf=0x%08x, filler=0x%08x, offset=%lld, fi=0x%08x)\n", path, buf, filler, offset, fi);

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	//path is root
	// if (strstr(path, "/") == 0)
	// {
	unqlite_int64 nBytes;
	my_inode* inode;
	int rc = get_inode(path, 0, inode);

	if(rc < 0)
	{
		write_log("returned readdir without checking dir contents\n");
		return 0;
	}
	

	dir_data_fcb dir_data;

	fetch_from_db(inode->data_id, &dir_data, sizeof(dir_data_fcb));

	for (int i = 0; i<MY_MAX_DIR_FILES; i++)
	{
		char* filename = dir_data.entries[i].filename;
		// write_log("filename: %s\n", filename);

		//there is a filename
		if (strcmp(filename, get_file_name(path)) != 0)
		{
			filler(buf, dir_data.entries[i].filename, NULL, 0);
		}
		
	}
	// }

	// char *pathP = (char*)&(the_root_fcb.path);
	// if (*pathP != '\0')
	// {
	// 	// drop the leading '/';
	// 	pathP++;
	// 	filler(buf, pathP, NULL, 0);
	// }

	write_log("end read.\n");

	return 0;
}

// Read a file.
// Read 'man 2 read'.
static int myfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	size_t len;
	(void) fi;

	write_log("myfs_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);


	my_inode* inode;
	int rc = get_inode(path, 0, inode);

	if (rc < 0)
	{
		return rc;
	}
	else 
	{
		len = inode->size;
		uint8_t data_block[MY_MAX_FILE_SIZE];
		memset(&data_block, 0, MY_MAX_FILE_SIZE);

		//there is a data block
		if (uuid_compare(zero_uuid, inode->data_id) != 0)
		{
			fetch_from_db(inode->data_id, &data_block, MY_MAX_FILE_SIZE);
		}

		if (offset < len)
		{
			if (offset + size > len)
			{
				size = len - offset;
			}

			memcpy(buf, &data_block + offset, size);
		}
		else
		{
			size = 0;
		}
		return size;
	}

	// if(strcmp(path, the_root_fcb.path) != 0)
	// {
	// 	write_log("myfs_read - ENOENT");
	// 	return -ENOENT;
	// }

	// len = the_root_fcb.size;

	// uint8_t data_block[MY_MAX_FILE_SIZE];

	// memset(&data_block, 0, MY_MAX_FILE_SIZE);
	// uuid_t *data_id = &(the_root_fcb.file_data_id);

	// // Is there a data block?
	// if(uuid_compare(zero_uuid,*data_id)!=0)
	// {
	// 	unqlite_int64 nBytes;  //Data length.
	// 	int rc = unqlite_kv_fetch(pDb,data_id,KEY_SIZE,NULL,&nBytes);

	// 	if( rc != UNQLITE_OK )
	// 	{
	// 	  error_handler(rc);
	// 	}

	// 	if(nBytes!=MY_MAX_FILE_SIZE)
	// 	{
	// 		write_log("myfs_read - EIO");
	// 		return -EIO;
	// 	}

	// 	// Fetch the fcb the root data block from the store.
	// 	unqlite_kv_fetch(pDb,data_id,KEY_SIZE,&data_block,&nBytes);
	// }

	// if (offset < len) 
	// {
	// 	if (offset + size > len) 
	// 	{
	// 		size = len - offset;
	// 	}

	// 	memcpy(buf, &data_block + offset, size);
	// } 
	// else 
	// {
	// 	size = 0;
	// }

	// return size;
}

// This file system only supports one file. Create should fail if a file has been created. Path must be '/<something>'.
// Read 'man 2 creat'.
static int myfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    write_log("myfs_create(path=\"%s\", mode=0%03o, fi=0x%08x)\n", path, mode, fi);

    int pathlen = strlen(path);
    if (pathlen >= MY_MAX_PATH)
    {
    	write_log("myfs_create - ENAMETOOLONG");
    	return -ENAMETOOLONG;
    }

    my_inode* parent_fcb;
    int rc = get_inode(path, 1, parent_fcb);

    if (rc < 0)
    {
    	parent_fcb = &the_root_fcb;
    }

    my_inode new_inode;
    memset(&new_inode, 0, sizeof(my_inode));

    uuid_generate(new_inode.id);

    struct fuse_context* context = fuse_get_context();

    new_inode.uid = context->uid;
    new_inode.gid = context->gid;
    new_inode.mode = mode | S_IFREG;
    new_inode.size = 0;

    store_to_db(new_inode.id, &new_inode, sizeof(my_inode));

    //store to parent
    update_parent(parent_fcb, new_inode.id, path);

	return 0;

 //    if(the_root_fcb.path[0] != '\0')
 //    {
	// 	write_log("myfs_create - ENOSPC");
	// 	return -ENOSPC;
	// }

	// int pathlen = strlen(path);
	// if(pathlen >= MY_MAX_PATH)
	// {
	// 	write_log("myfs_create - ENAMETOOLONG");
	// 	return -ENAMETOOLONG;
	// }

	// sprintf(the_root_fcb.path,path);
	// struct fuse_context *context = fuse_get_context();
	// the_root_fcb.uid=context->uid;
	// the_root_fcb.gid=context->gid;
	// the_root_fcb.mode=mode|S_IFREG;

	// int rc = unqlite_kv_store(pDb,&(root_object.id),KEY_SIZE,&the_root_fcb,sizeof(struct dir_fcb));
	// if( rc != UNQLITE_OK )
	// {
	// 	write_log("myfs_create - EIO");
	// 	return -EIO;
	// }

 //    return 0;
}

// Set update the times (actime, modtime) for a file. This FS only supports modtime.
// Read 'man 2 utime'.
static int myfs_utime(const char *path, struct utimbuf *ubuf)
{
    write_log("myfs_utime(path=\"%s\", ubuf=0x%08x)\n", path, ubuf);


    my_inode* inode;
    int rc = get_inode(path, 0, inode);

    if (rc < 0)
    {
    	return rc;
    }
    else 
    {
    	inode->mtime = ubuf->modtime;

    	//write back to store
    	int rc = store_to_db(inode->id, inode, sizeof(my_inode));
    	if (rc != UNQLITE_OK)
    	{
    		write_log("myfs_write - EIO\n");
    		return -EIO;
    	}

    }

    return 0;

	// if(strcmp(path, the_root_fcb.path) != 0)
	// {
	// 	write_log("myfs_utime - ENOENT");
	// 	return -ENOENT;
	// }

	// the_root_fcb.mtime = ubuf->modtime;

	// // Write the fcb to the store.
 //    int rc = unqlite_kv_store(pDb, &(root_object.id), KEY_SIZE, &the_root_fcb, sizeof(struct dir_fcb));
	// if( rc != UNQLITE_OK )
	// {
	// 	write_log("myfs_write - EIO");
	// 	return -EIO;
	// }

 //    return 0;
}

// Write to a file.
// Read 'man 2 write'
static int myfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    write_log("myfs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);

    my_inode* inode;
    int rc = get_inode(path, 0, inode);

    if (rc < 0)
    {
    	return rc;
    }
    else 
    {
    	uint8_t data_block[MY_MAX_FILE_SIZE];

    	memset(&data_block, 0, MY_MAX_FILE_SIZE);

    	uuid_t data_id;
    	uuid_copy(data_id, inode->data_id);
    	if (uuid_compare(zero_uuid, data_id) == 0)
    	{
    		uuid_generate(data_id);
    	}
    	else
    	{
    		fetch_from_db(data_id, &data_block, MY_MAX_FILE_SIZE);
    	}

    	int written = snprintf(data_block, MY_MAX_FILE_SIZE, buf);

    	//write data back to db
    	store_to_db(data_id, &data_block, MY_MAX_FILE_SIZE);

    	time_t now = time(0);

    	uuid_copy(inode->data_id, data_id);
    	inode->size = written;
    	inode->mtime = now;
    	inode->ctime = now;


    	//write inode back to db
    	store_to_db(inode->id, inode, sizeof(my_inode));

    	return written;
    }

	// if(strcmp(path, the_root_fcb.path) != 0)
	// {
	// 	write_log("myfs_write - ENOENT");
	// 	return -ENOENT;
 //    }

	// if(size >= MY_MAX_FILE_SIZE)
	// {
	// 	write_log("myfs_write - EFBIG");
	// 	return -EFBIG;
	// }

	// uint8_t data_block[MY_MAX_FILE_SIZE];

	// memset(&data_block, 0, MY_MAX_FILE_SIZE);
	// uuid_t *data_id = &(the_root_fcb.file_data_id);
	// // Is there a data block?
	// if(uuid_compare(zero_uuid,*data_id)==0)
	// {
	// 	// Generate a UUID for the data block. We'll write the block itself later.
	// 	uuid_generate(the_root_fcb.file_data_id);
	// }
	// else
	// {
	// 	// First we will check the size of the obejct in the store to ensure that we won't overflow the buffer.
	// 	unqlite_int64 nBytes;  // Data length.
	// 	int rc = unqlite_kv_fetch(pDb,data_id,KEY_SIZE,NULL,&nBytes);

	// 	if( rc!=UNQLITE_OK || nBytes!=MY_MAX_FILE_SIZE)
	// 	{
	// 		write_log("myfs_write - EIO");
	// 		return -EIO;
	// 	}

	// 	// Fetch the data block from the store.
	// 	unqlite_kv_fetch(pDb,data_id,KEY_SIZE,&data_block,&nBytes);
	// 	// Error handling?
	// }

	// // Write the data in-memory.
 //    int written = snprintf(data_block, MY_MAX_FILE_SIZE, buf);

	// // Write the data block to the store.
	// int rc = unqlite_kv_store(pDb,data_id,KEY_SIZE,&data_block,MY_MAX_FILE_SIZE);
	// if( rc != UNQLITE_OK )
	// {
	// 	write_log("myfs_write - EIO");
	// 	return -EIO;
	// }

	// // Update the fcb in-memory.
	// the_root_fcb.size=written;
	// time_t now = time(NULL);
	// the_root_fcb.mtime=now;
	// the_root_fcb.ctime=now;

	// // Write the fcb to the store.
 //    rc = unqlite_kv_store(pDb,&(root_object.id),KEY_SIZE,&the_root_fcb,sizeof(struct dir_fcb));
	// if( rc != UNQLITE_OK )
	// {
	// 	write_log("myfs_write - EIO");
	// 	return -EIO;
	// }

 //    return written;
}

// Set the size of a file.
// Read 'man 2 truncate'.
int myfs_truncate(const char *path, off_t newsize)
{
    write_log("myfs_truncate(path=\"%s\", newsize=%lld)\n", path, newsize);

    return -ENOENT;

	// if(newsize >= MY_MAX_FILE_SIZE)
	// {
	// 	write_log("myfs_truncate - EFBIG");
	// 	return -EFBIG;
	// }

	// the_root_fcb.size = newsize;

	// // Write the fcb to the store.
 //    int rc = unqlite_kv_store(pDb,&(root_object.id),KEY_SIZE,&the_root_fcb,sizeof(struct dir_fcb));
	// if( rc != UNQLITE_OK )
	// {
	// 	write_log("myfs_write - EIO");
	// 	return -EIO;
	// }

	// return 0;
}

// Set permissions.
// Read 'man 2 chmod'.
int myfs_chmod(const char *path, mode_t mode)
{
    write_log("myfs_chmod(fpath=\"%s\", mode=0%03o)\n", path, mode);

    return 0;
}

// Set ownership.
// Read 'man 2 chown'.
int myfs_chown(const char *path, uid_t uid, gid_t gid)
{
    write_log("myfs_chown(path=\"%s\", uid=%d, gid=%d)\n", path, uid, gid);

    return 0;
}

/** 
 * Create a directory.
 * Read 'man 2 mkdir'.
 */
int myfs_mkdir(const char *path, mode_t mode)
{
	write_log("myfs_mkdir: %s\n",path);

	write_log("fname; %s\n", get_file_name(path));

	//make directory fcb
	my_inode new_inode;
	memset(&new_inode, 0, sizeof(my_inode));

	uuid_generate(new_inode.id);

	new_inode.mode = mode | S_IFDIR;//S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH;;
	new_inode.uid = getuid();
	new_inode.gid = getgid();
	new_inode.mtime = time(NULL);

	//make directory data fcb
	dir_data_fcb dir_data;
	memset(&dir_data, 0, sizeof(dir_data_fcb));
	uuid_generate(dir_data.id);
	
	uuid_copy(new_inode.data_id, dir_data.id);

	//store directory fcb
	int rc = unqlite_kv_store(pDb, new_inode.id, KEY_SIZE, &new_inode, sizeof(my_inode));
	error_handle(rc);

	//store directory data
	rc = unqlite_kv_store(pDb, dir_data.id, KEY_SIZE, &dir_data, sizeof(dir_data_fcb));
	error_handle(rc);

	write_log("made dir%s\n", path);


	//update parent
	my_inode* parent_fcb;
	rc = get_inode(path, 1, parent_fcb);

	if (rc < 0)
	{
		parent_fcb = &the_root_fcb;
	}

	update_parent(parent_fcb, new_inode.id, path);

	// error_handle(rc);

    return 0;
}

// Delete a file.
// Read 'man 2 unlink'.
// int myfs_unlink(const char *path)
// {
// 	write_log("myfs_unlink: %s\n",path);

//     return 0;
// }

// // Delete a directory.
// // Read 'man 2 rmdir'.
// int myfs_rmdir(const char *path)
// {
//     write_log("myfs_rmdir: %s\n",path);

//     return 0;
// }

// // OPTIONAL - included as an example
// // Flush any cached data.
// int myfs_flush(const char *path, struct fuse_file_info *fi)
// {
//     int retstat = 0;

//     write_log("myfs_flush(path=\"%s\", fi=0x%08x)\n", path, fi);

//     return retstat;
// }

// // OPTIONAL - included as an example
// // Release the file. There will be one call to release for each call to open.
// int myfs_release(const char *path, struct fuse_file_info *fi)
// {
//     int retstat = 0;

//     write_log("myfs_release(path=\"%s\", fi=0x%08x)\n", path, fi);

//     return retstat;
// }

// OPTIONAL - included as an example
// Open a file. Open should check if the operation is permitted for the given flags (fi->flags).
// Read 'man 2 open'.
static int myfs_open(const char *path, struct fuse_file_info *fi)
{
	// if (strcmp(path, the_root_fcb.path) != 0)
	

	write_log("myfs_open(path\"%s\", fi=0x%08x)\n", path, fi);

	//return -EACCES if the access is not permitted.

	return 0;
}

static struct fuse_operations myfs_oper = 
{
	.getattr	= myfs_getattr,
	.readdir	= myfs_readdir,
	.open		= myfs_open,
	.read		= myfs_read,
	.create		= myfs_create,
	.utime 		= myfs_utime,
	.write		= myfs_write,
	.truncate	= myfs_truncate,
	.mkdir 		= myfs_mkdir,
	// .flush		= myfs_flush,
	// .release	= myfs_release,
	// .rmdir 		= myfs_rmdir,
	// .unlink 	= myfs_unlink,
	.chown 		= myfs_chown,
	.chmod 		= myfs_chmod,
};


// Initialise the in-memory data structures from the store. If the root object (from the store) is empty then create a root fcb (directory)
// and write it to the store. Note that this code is executed outide of fuse. If there is a failure then we have failed toi initlaise the
// file system so exit with an error code.
void init_fs()
{
	int rc;
	printf("init_fs\n");
	//Initialise the store.
	init_store();
	if(!root_is_empty)
	{
		printf("init_fs: root is not empty\n");

		//error checking
		unqlite_int64 nBytes;  //Data length.
		rc = unqlite_kv_fetch(pDb, root_object.id, KEY_SIZE, NULL, &nBytes);
		error_handle(rc);

		if(nBytes!=sizeof(my_inode))
		{
			printf("Data object has unexpected size. Doing nothing.\n");
			exit(-1);
		}

		//Fetch the fcb that the root object points at. We will probably need it.
		unqlite_kv_fetch(pDb, root_object.id, KEY_SIZE, &the_root_fcb, &nBytes);
	}
	else
	{
		printf("init_fs: root is empty\n");
		//Initialise and store an empty root fcb.
		memset(&the_root_fcb, 0, sizeof(my_inode));

		//See 'man 2 stat' and 'man 2 chmod'.

		//mkdir the root directory
		// myfs_mkdir("/", S_IFDIR|S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);

		the_root_fcb.mode |= S_IFDIR|S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH;
		time_t now = time(0);
		the_root_fcb.mtime = now;
		the_root_fcb.ctime = now;
		the_root_fcb.uid = getuid();
		the_root_fcb.gid = getgid();

		//create directory data of root
		dir_data_fcb root_data;
		uuid_generate(root_data.id);

		for (int i = 0; i<MY_MAX_DIR_FILES; i++) 
		{
			memset(&root_data.entries[i], 0, sizeof(dir_entry));
		}

		uuid_copy(the_root_fcb.data_id, root_data.id);

		//write root data to db
		rc = unqlite_kv_store(pDb, root_data.id, KEY_SIZE, &root_data, sizeof(dir_data_fcb));
		error_handle(rc);

		dir_data_fcb* data;
		unqlite_int64 bytes;
		unqlite_kv_fetch(pDb, the_root_fcb.data_id, KEY_SIZE, data, &bytes);
		printf("wrote root_data with id %s and size: %d\n", get_uuid(the_root_fcb.data_id), bytes);

		//Generate a key for the_root_fcb and update the root object.
		uuid_generate(root_object.id);

		printf("init_fs: writing root fcb\n");
		//write root fcb to db
		rc = unqlite_kv_store(pDb, root_object.id, KEY_SIZE, &the_root_fcb, sizeof(my_inode));
		error_handle(rc);

		unqlite_kv_fetch(pDb, root_object.id, KEY_SIZE, NULL, &bytes);

		printf("wrote root fcb of size %d\n", bytes);

		printf("init_fs: writing updated root object\n");
		//Store root object
		rc = write_root();
	 	error_handle(rc);
	}
}

void shutdown_fs()
{
	unqlite_close(pDb);
}

int main(int argc, char *argv[])
{
	int fuserc;
	struct myfs_state *myfs_internal_state;

	//Setup the log file and store the FILE* in the private data object for the file system.
	myfs_internal_state = malloc(sizeof(struct myfs_state));
    myfs_internal_state->logfile = init_log_file();

	//Initialise the file system. This is being done outside of fuse for ease of debugging.
	init_fs();

	fuserc = fuse_main(argc, argv, &myfs_oper, myfs_internal_state);

	//Shutdown the file system.
	shutdown_fs();

	return fuserc;
}
