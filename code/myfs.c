/*
  MyFS. One directory, one file, 1000 bytes of storage. What more do you need?

  This Fuse file system is based largely on the HelloWorld example by Miklos Szeredi <miklos@szeredi.hu> (http://fuse.sourceforge.net/helloworld.html). Additional inspiration was taken from Joseph J. Pfeiffer's "Writing a FUSE Filesystem: a Tutorial" (http://www.cs.nmsu.edu/~pfeiffer/fuse-tutorial/).
*/

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

#include "myfs.h"

#define INODE_SIZE sizeof(myinode)
#define FILE_FCB_SIZE sizeof(file_fcb)
#define DIR_FCB_SIZE ((unqlite_int64) sizeof(dir_fcb))
#define DIR_DATA_SIZE ((unqlite_int64) sizeof(dir_data_fcb))

//fcb of root directiory
struct dir_fcb the_root_fcb;

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


char* get_file_name(char* path)
{
	char* pathP = path;
	if (*pathP != '\0')
	{
		// drop the leading '/';
		pathP++;
	}
	write_log("path: %s strchr: %s\n", path, strrchr(path, '/'));
	return pathP;
}


/**
 * 
 */
int fetch_from_db(uuid_t id, void* memory_address, int size)
{
	//error checking
	int rc;
	unqlite_int64 nBytes = size;  //Data length.
	rc = unqlite_kv_fetch(pDb, id, KEY_SIZE, NULL, &nBytes);
	error_handle(rc);
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

int store_to_db(uuid_t id, void* memory_address, int size)
{
	int rc = unqlite_kv_store(pDb, id, KEY_SIZE, memory_address, size);
	error_handle(rc);
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
		stbuf->st_mode = the_root_fcb.inode.mode;
		stbuf->st_nlink = 2;
		stbuf->st_uid = the_root_fcb.inode.uid;
		stbuf->st_gid = the_root_fcb.inode.gid;
	}
	else
	{
		dir_data_fcb root_data;
		int bytes = fetch_from_db(the_root_fcb.inode.data, &root_data, sizeof(dir_data_fcb));
		int found = 0;
		if (strstr(path, "/") != NULL)
		{
			for(int i = 0; i<MY_MAX_DIR_FILES; i++)
			{
				dir_entry entry = root_data.entries[i];
				if (strcmp(entry.filename, get_file_name(path)) == 0)
				{
					found = 1;
					dir_fcb dir;
					fetch_from_db(entry.inode_id, &dir, sizeof(dir_fcb));

					stbuf->st_mode = dir.inode.mode;
					stbuf->st_nlink = 1;
					stbuf->st_mtime = dir.inode.mtime;
					stbuf->st_uid = dir.inode.uid;
					stbuf->st_gid = dir.inode.gid;

				}
			}
			if (found == 0)
			{
				return -ENOENT;
			}
			// write_log("myfs_getattr - strcmp '/' \n");	
			// stbuf->st_mode = the_root_fcb.inode.mode;
			// stbuf->st_nlink = 1;
			// stbuf->st_mtime = the_root_fcb.inode.mtime;
			// stbuf->st_uid = the_root_fcb.inode.uid;
			// stbuf->st_gid = the_root_fcb.inode.gid;
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
	if (strcmp(path, "/") == 0)
	{
		unqlite_int64 nBytes;
		dir_data_fcb root_data;
		unqlite_kv_fetch(pDb, the_root_fcb.inode.data, KEY_SIZE, &root_data, &nBytes);

		for (int i = 0; i<MY_MAX_DIR_FILES; i++)
		{
			char* filename = root_data.entries[i].filename;
			// write_log("filename: %s\n", filename);

			if (strcmp(filename, get_file_name(path)) != 0)
			{
				filler(buf, root_data.entries[i].filename, NULL, 0);
			}
			
		}
	}

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


	return -ENOENT;

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

    return -ENOENT;

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

    return -ENOENT;

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

    return -ENOENT;

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
	dir_fcb new_dir;
	myinode dir_inode = new_dir.inode;

	uuid_generate(dir_inode.id);

	dir_inode.mode = mode | S_IFDIR;//S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH;;
	dir_inode.uid = getuid();
	dir_inode.gid = getgid();
	dir_inode.mtime = time(NULL);

	//make directory data fcb
	dir_data_fcb dir_data;
	uuid_generate(dir_data.id);
	
	uuid_copy(dir_inode.data, dir_data.id);

	//store directory fcb
	new_dir.inode = dir_inode;
	int rc = unqlite_kv_store(pDb, dir_inode.id, KEY_SIZE, &new_dir, DIR_FCB_SIZE);
	error_handle(rc);

	//store directory data
	rc = unqlite_kv_store(pDb, dir_data.id, KEY_SIZE, &dir_data, DIR_DATA_SIZE);
	error_handle(rc);

	write_log("made dir%s\n", path);


	// unqlite_int64 bytes;
	// unqlite_kv_fetch(pDb, the_root_fcb.inode.data, KEY_SIZE, NULL, &bytes);
	// write_log("got root_data of size %d from %s\n", bytes, get_uuid(the_root_fcb.inode.data));


	//root data
	dir_data_fcb parent_data;
	// unqlite_int64 nBytes = sizeof(dir_data_fcb);
	// int bytes = unqlite_kv_fetch(pDb, the_root_fcb.inode.data, KEY_SIZE, &parent_data, &nBytes);
	int bytes = fetch_from_db(the_root_fcb.inode.data, &parent_data, sizeof(dir_data_fcb));

	char* name;

	//add to parent
	for (int i = 0; i<MY_MAX_DIR_FILES; i++) 
	{
		dir_entry entry = parent_data.entries[i];

		//empty entry
		if(strcmp(entry.filename, "") == 0)
		{
			uuid_copy(entry.inode_id, dir_inode.id);
			strcpy(entry.filename, get_file_name(path));
		
			write_log("making new dir %s\n", entry.filename);
			name = entry.filename;

			parent_data.entries[i] = entry;
			break;
		}
	}

	store_to_db(the_root_fcb.inode.data, &parent_data, sizeof(dir_data_fcb));
	// rc = unqlite_kv_store(pDb, the_root_fcb.inode.data, KEY_SIZE, &parent_data, nBytes);

	error_handle(rc);

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
	if (strcmp(path, the_root_fcb.path) != 0)
		return -ENOENT;

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

		if(nBytes!=sizeof(dir_fcb))
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
		memset(&the_root_fcb, 0, sizeof(dir_fcb));

		//See 'man 2 stat' and 'man 2 chmod'.

		//mkdir the root directory
		// myfs_mkdir("/", S_IFDIR|S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);

		the_root_fcb.inode.mode |= S_IFDIR|S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH;
		the_root_fcb.inode.mtime = time(0);
		the_root_fcb.inode.uid = getuid();
		the_root_fcb.inode.gid = getgid();

		//create directory data of root
		dir_data_fcb dir_data;
		// memset(&dir_data, 0, sizeof(dir_data_fcb));

		uuid_generate(dir_data.id);

		for (int i = 0; i<MY_MAX_DIR_FILES; i++) 
		{
			memset(&dir_data.entries[i], 0, sizeof(dir_entry));
		}

		uuid_copy(the_root_fcb.inode.data, dir_data.id);

		//write root data to db
		rc = unqlite_kv_store(pDb, the_root_fcb.inode.data, KEY_SIZE, &dir_data, sizeof(dir_data_fcb));
		error_handle(rc);

		dir_data_fcb* data;
		unqlite_int64 bytes;
		unqlite_kv_fetch(pDb, the_root_fcb.inode.data, KEY_SIZE, data, &bytes);
		printf("wrote root_data with id %s and size: %d\n", get_uuid(the_root_fcb.inode.data), bytes);

		//Generate a key for the_root_fcb and update the root object.
		uuid_generate(root_object.id);

		printf("init_fs: writing root fcb\n");
		//write root fcb to db
		rc = unqlite_kv_store(pDb, root_object.id, KEY_SIZE, &the_root_fcb, sizeof(dir_fcb));
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
