#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <libgen.h>

#include "myfs.h"

//fcb of root directiory
my_inode the_root_fcb;

//global for printing uuid_t
//see char* get_uuid() below
char UUID_BUF[50];

/**
 * Function for debugging purposes to print a uuid_t
 */
char* get_uuid(uuid_t uuid)
{
	uuid_unparse(uuid, UUID_BUF);
	return UUID_BUF;
}


void error_handle(int rc) 
{
	if (rc != UNQLITE_OK)
	{
		error_handler(rc);
	}
}


char* get_file_name(const char* path)
{
	return basename(path);
}


/**
 * Fetches the given 'id' from the database into the given 'data' pointer
 * 'size' is the expected size of what is fetched.
 *
 * Returns the number of bytes fetched (which is equal to size) on success.
 * Otherwise returns -ENOENT if the item was not found in the database.
 */
int fetch_from_db(uuid_t id, void* data, size_t size)
{
	//error checking
	int rc;
	unqlite_int64 nBytes = size;  //Data length.

	rc = unqlite_kv_fetch(pDb, id, KEY_SIZE, NULL, &nBytes);
	if (rc != UNQLITE_OK)
	{
		return -ENOENT;
	}
	error_handle(rc);

	//error check we fetched the right thing
	if(nBytes!=size)
	{
		write_log("[DB] fetch: Data object has unexpected size. Expected %d, got %d\n", size, nBytes);
		exit(-1);
	}

	//Fetch the fcb that the root object points at. We will probably need it.
	unqlite_kv_fetch(pDb, id, KEY_SIZE, data, &nBytes);

	return nBytes;
}

/**
 * Function to store back to the database.
 * Stores with key 'id' and value 'data'.
 * 'size' is the size of 'data'.
 *
 * Returns 0 on success.
 */
int store_to_db(uuid_t id, void* data, size_t size)
{
	if (uuid_compare(id, root_object.id) == 0)
	{
		the_root_fcb = *((my_inode*)data);
	}
	int rc = unqlite_kv_store(pDb, id, KEY_SIZE, data, size);
	error_handle(rc);
	return rc;
}


/**
 * Gets the inode at the end of the given path and puts it in the pointer 'inode'
 * If the 'get_parent' flag is set to be greater than 0, gets the inode one before the end
 *
 * Example:
 * 	get_inode("/a/b/c", &inode, 0) gets the inode of "/a/b"
 * 	get_inode("/a/b/c", &inode, 1) gets the inode of "/a/b/c"
 *
 * Returns 0 on success and -ENOENT if an inode was not found at the given path.
 */
int get_inode(const char* path, my_inode* inode, int get_parent)
{
	write_log("[FUNC] get_inode: path='%s' get_parent='%d'\n", path, get_parent);
	char* str = strdup(path);

	//remove path after last '/' because we want the directory before it
	if(get_parent)
	{
		char* r = strrchr(str, '/');
		*r = 0;
	}

	char* partial_path;

	while((partial_path = strsep(&str, "/")) != NULL)
	{
		//root directory
		if (strcmp(partial_path, "") == 0)
		{
			inode = memcpy(inode, &the_root_fcb, sizeof(my_inode));
		}
		else 
		{
			dir_data_fcb dir_fcb;
			int rc = fetch_from_db(inode->data_id, &dir_fcb, sizeof(dir_data_fcb)); 
			if (rc < 0)
			{
				write_log("[FUNC] get_inode: Not found in db\n");
				return -ENOENT;
			}

			//loop through current inode
			int found = 0;
			for (int i = 0; i<MY_MAX_DIR_FILES; i++)
			{
				dir_entry entry = dir_fcb.entries[i];
				if (strcmp(entry.filename, partial_path) == 0)
				{
					found = 1;

					//fetch next inode
					fetch_from_db(entry.inode_id, inode, sizeof(my_inode));

					break;
				}
			}
			if(!found)
			{
				write_log("[FUNC] get_inode: Not found in fs\n");
				return -ENOENT;
			}
		}
	}
	return 0;
}

/**
 * Function to update the parent directory with a new inode
 * Returns 0 on success, an error code on error.
 */
int update_parent(my_inode* parent_inode, uuid_t new_inode_id, const char* path)
{
	dir_data_fcb parent_data;
	fetch_from_db(parent_inode->data_id, &parent_data, sizeof(dir_data_fcb));

	parent_inode->size = parent_inode->size + 1;
	parent_inode->mtime = time(NULL);

	int found = 0;

	//add new inode to parent
	for (int i = 0; i<MY_MAX_DIR_FILES; i++)
	{
		dir_entry entry = parent_data.entries[i];

		//found empty entry
		if(strcmp(entry.filename, "") == 0)
		{
			found = 1;

			uuid_copy(entry.inode_id, new_inode_id);
			strcpy(entry.filename, get_file_name(path));

			write_log("[FUNC] Updated parent with new inode (name='%s')\n", entry.filename);

			parent_data.entries[i] = entry;
			break;
		}
	}

	if (!found)
	{
		write_log("[FUNC] Update parent directory no more space\n.");
		return -ENOENT;
	}

	store_to_db(parent_inode->id, parent_inode, sizeof(my_inode));
	store_to_db(parent_inode->data_id, &parent_data, sizeof(dir_data_fcb));

	return 0;
}


// Get file and directory attributes (meta-data).
// Read 'man 2 stat' and 'man 2 chmod'.
static int myfs_getattr(const char *path, struct stat *stbuf)
{
	write_log("\n[SYST] getattr: (path=\"%s\", statbuf=0x%08x)\n", path, stbuf);

	memset(stbuf, 0, sizeof(struct stat));

	my_inode inode;
	int rc = get_inode(path, &inode, 0);

	if (rc < 0)
	{
		return -ENOENT;
	}
	else 
	{
		stbuf->st_mode = inode.mode;
		stbuf->st_nlink = 1; 
		stbuf->st_mtime = inode.mtime;
		stbuf->st_atime = inode.atime;
		stbuf->st_ctime = inode.ctime;
		stbuf->st_uid = inode.uid;
		stbuf->st_gid = inode.gid;
		stbuf->st_size = inode.size;
	}

	return 0;
}

/**
 * Read a directory.
 * Read 'man 2 readdir'.
 */
static int myfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;

	write_log("\n[SYST] readdir: (path=\"%s\", buf=0x%08x, filler=0x%08x, offset=%lld, fi=0x%08x)\n", path, buf, filler, offset, fi);

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	//get the inode
	my_inode inode;
	int rc = get_inode(path, &inode, 0);
	if(rc < 0)
	{
		write_log("[SYST] readdir: Returned without checking dir contents\n");
		return 0;
	}
	
	//read directory data for that inode
	dir_data_fcb dir_data;
	fetch_from_db(inode.data_id, &dir_data, sizeof(dir_data_fcb));

	//list the files in the directory
	for (int i = 0; i<MY_MAX_DIR_FILES; i++)
	{
		char* filename = dir_data.entries[i].filename;

		//there is a filename
		if (strcmp(filename, "") != 0)
		{
			filler(buf, filename, NULL, 0);
		}
	}

	write_log("[SYST] readdir: End read. \n");
	return 0;
}

/**
 * Assume 'id' given points to a data_block in the db
 * 'data_offset' is an int between 0 and MY_MAX_DATA_SIZE - 1
 * 'size' is the amount of bytes to read, between 1 and MY_MAX_DATA_SIZE
 */
int read_data_block(uuid_t id, char** buf, int data_offset, int size)
{
	data_block block;
	fetch_from_db(id, &block, sizeof(data_block));
	write_log("[SYST] read: Read to buffer: '%s'\n", block.data);

	memcpy(*buf, &(block.data[data_offset]), size);
	*buf += size;

	return size;
}

/** 
 * 'id' is the uuid_t of a direct block
 * 'block_offset' is which block to start from from 0 to MY_MAX_DIRECT_BLOCKS
 * 'data_offset' is which byte of the block to start from from 0 to MY_MAX_DATA_SIZE - 1
 */
int read_direct_block(uuid_t id, char** buf, int block_offset, int data_offset, int size)
{
	direct_block block;
	fetch_from_db(id, &block, sizeof(direct_block));

	int read = size;
	int i = block_offset;

	//read with offset
	read -= read_data_block(block.blocks[i], buf, data_offset, FLOOR(read, MY_MAX_DATA_SIZE));
	i++;

	//subsequent reads have no offset
	while (read > 0 && i < MY_MAX_DIRECT_BLOCKS)
	{
		read -= read_data_block(block.blocks[i], buf, 0, FLOOR(read, MY_MAX_DATA_SIZE));
		i++;
	}

	if (read > 0)
	{
		//read only part of size
		return size - read;
	}
	else 
	{
		//read all of size
		return size;
	}

}

/**
 * Reads the entire data of the file.
 * 'first_block_to_read' can be any uuid_t for a block to read from 'data_fcb'
 */
int read_block(uuid_t first_block_to_read, char** read_buf, file_data_fcb data_fcb, int size, int offset, int indrect_offset)
{
	int read = size;

	//start from direct blocks
	int block_index = offset / MY_MAX_DATA_SIZE;
	int data_index = offset % MY_MAX_DATA_SIZE;

	//read first block
	read -= read_direct_block(first_block_to_read, read_buf, block_index, data_index, size);

	// read next indirect blocks
	int i = indrect_offset;
	while (read > 0 && i < MY_MAX_DIRECT_BLOCKS)
	{
		read -= read_direct_block(data_fcb.index_ids[i], read_buf, 0, 0, read);
		i++;
	}

	return read;
}

// Read a file.
// Read 'man 2 read'.
static int myfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	size_t len;
	(void) fi;

	write_log("\n[SYST] read: (path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);

	my_inode inode;
	int rc = get_inode(path, &inode, 0);
	if (rc < 0)
	{
		return -ENOENT;
	}
	else 
	{
		inode.atime = time(NULL);
		store_to_db(inode.id, &inode, sizeof(my_inode));

		len = inode.size;
		int read = len;

		char** read_buf = &buf;

		file_data_fcb data_fcb;
		fetch_from_db(inode.data_id, &data_fcb, sizeof(file_data_fcb));

		//go straight to indirect blocks
		if (offset < len)
		{
			if (offset > MY_DATA_SIZE_PER_BLOCK)
			{
				//get which indirect block to start from
				int index_offset = (offset / MY_DATA_SIZE_PER_BLOCK);

				//change offset to start at the current indirect block
				offset = offset - (index_offset * MY_DATA_SIZE_PER_BLOCK);

				//start reading from indirect blocks
				read_block(data_fcb.index_ids[index_offset], read_buf, data_fcb, len, offset, index_offset);
			}
			else 
			{
				//start reading from direct blocks
				read_block(data_fcb.direct_data_id, read_buf, data_fcb, len, offset % MY_MAX_DATA_SIZE, 0);
			}
		}
		else 
		{
			size = 0;
		}

		return size;
	}
}


// Create a file.
// Read 'man 2 creat'.
static int myfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    write_log("\n[SYST] create: (path=\"%s\", mode=0%03o, fi=0x%08x)\n", path, mode, fi);

    int pathlen = strlen(path);
    if (pathlen >= MY_MAX_PATH)
    {
    	write_log("[SYST] create: - ENAMETOOLONG\n");
    	return -ENAMETOOLONG;
    }

    my_inode parent_fcb;
    int rc = get_inode(path, &parent_fcb, 1);
    if (rc < 0)
    {
    	parent_fcb = the_root_fcb;
    }

    //create new inode
    my_inode new_inode;
    memset(&new_inode, 0, sizeof(my_inode));

    uuid_generate(new_inode.id);

    struct fuse_context* context = fuse_get_context();

    time_t now = time(NULL);
    new_inode.mtime = now;
    new_inode.atime = now;
    new_inode.ctime = now;

    new_inode.uid = context->uid;
    new_inode.gid = context->gid;
    new_inode.mode = mode | S_IFREG;
    new_inode.size = 0;

    //store new inode
    store_to_db(new_inode.id, &new_inode, sizeof(my_inode));

    //store to parent
    rc = update_parent(&parent_fcb, new_inode.id, path);
    if (rc < 0)
    {
    	return rc;
    }

	return 0;
}

// Set update the times (actime, modtime) for a file. This FS only supports modtime.
// Read 'man 2 utime'.
static int myfs_utime(const char *path, struct utimbuf *ubuf)
{
    write_log("\n[SYST] utime: (path=\"%s\", ubuf=0x%08x)\n", path, ubuf);

    my_inode inode;
    int rc = get_inode(path, &inode, 0);
    if (rc < 0)
    {
    	return -ENOENT;
    }

    else 
    {
    	inode.mtime = ubuf->modtime;

    	//write back to store
    	rc = store_to_db(inode.id, &inode, sizeof(my_inode));
    	if (rc != UNQLITE_OK)
    	{
    		write_log("[SYST] utime: - EIO\n");
    		return -EIO;
    	}

    }

    return 0;

}


// Assume buf_ptr passed here is <= MY_MAX_DATA_SIZE * MY_MAX_DIRECT_BLOCKS
int write_to_direct_block(char** buf_ptr, direct_block* block_buf, size_t* size, int block_index, int data_index)
{
	int written = 0;

	for (int i = block_index; i<MY_MAX_DIRECT_BLOCKS; i++)
	{
		int size_left = *size;
		int write_index = MY_MAX_DATA_SIZE - data_index;
		data_block block;

		if (uuid_compare(zero_uuid, block_buf->blocks[i]) == 0)
		{
			uuid_generate(block.id);
			uuid_copy(block_buf->blocks[i], block.id);

			memset(&(block.data), 0, MY_MAX_DATA_SIZE);
		}
		else
		{
			fetch_from_db(block_buf->blocks[i], &block, sizeof(data_block));
		}

		//do checking
		int write = size_left;
		if (write > write_index)
		{
			write = write_index;
		}
		snprintf(&(block.data[data_index]), write + 1, *buf_ptr);

		//still have leftover to write
		if (write == write_index)
		{
			written += write_index;
			size_left -= write_index;

			block.size = MY_MAX_DATA_SIZE;

			//move data pointer along to next bytes
			*buf_ptr += write_index;
		}
		else if (write > size_left)
		{	
			written += size_left;
			size_left = -1;

			block.size += size_left;
		}
		else 
		{	
			//no leftover to write		
			written += write;
			size_left -= write;

			block.size = write + data_index;
		}

		write_log("[SYST] write: Wrote %s with size %d with offset %d\n", block.data, written, data_index);
		store_to_db(block_buf->blocks[i], &block, sizeof(data_block));

		if (size_left < 0)
		{
			*buf_ptr += write;
			return written;
		}

		*size = size_left;
		data_index = 0;
	}

	return written;
}

int write_block(uuid_t* first_block, char** buf, file_data_fcb* data_fcb, size_t size, off_t offset, int indirect_offset)
{
	int block_index = offset / MY_MAX_DATA_SIZE;
	int data_index = offset % MY_MAX_DATA_SIZE;

	//first block
	direct_block block;
	memset(&block, 0, sizeof(direct_block));
	if (uuid_compare(zero_uuid, *first_block) == 0)
	{
		uuid_generate(block.id);
		uuid_copy(*first_block, block.id);
	}
	else
	{
		fetch_from_db(*first_block, &block, sizeof(direct_block));
	}

	int written = write_to_direct_block(buf, &block, &size, block_index, data_index);
	store_to_db(block.id, &block, sizeof(direct_block));

	//next indirect blocks
	int i = indirect_offset;
	while(strlen(*buf) > 0 && i < MY_MAX_DIRECT_BLOCKS && size > 0)
	{
		direct_block next_block;
		memset(&next_block, 0, sizeof(direct_block));
		if (uuid_compare(zero_uuid, data_fcb->index_ids[i]) == 0)
		{
			uuid_generate(next_block.id);
			uuid_copy(data_fcb->index_ids[i], next_block.id);
		}
		else
		{
			fetch_from_db(data_fcb->index_ids[i], &next_block, sizeof(direct_block));
		}

		written += write_to_direct_block(buf, &next_block, &size, 0, 0);
		store_to_db(next_block.id, &next_block, sizeof(direct_block));

		i++;
	}

	return written;
}

// Write to a file.
// Read 'man 2 write'
static int myfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    write_log("\n[SYST] write: (path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);

    my_inode inode;
    int rc = get_inode(path, &inode, 0);
    if (rc < 0)
    {
    	return -ENOENT;
    }
    else if (size > MY_MAX_FILE_SIZE)
    {
    	return -EFBIG;
    }

   	char** buf_ptr = &buf;
	int written = 0;

	file_data_fcb data;
	memset(&data, 0, sizeof(file_data_fcb));
	
	//check if there is already a data block
	if (uuid_compare(zero_uuid, inode.data_id) == 0)
	{
		//if not generate an id
		uuid_generate(data.id);
	}
	else 
	{
		//if there is, fetch it from the db
		fetch_from_db(inode.data_id, &data, sizeof(file_data_fcb));
	}


	if (offset > MY_DATA_SIZE_PER_BLOCK)
	{
		//start from indirect blocks
		int index_offset = (offset / MY_DATA_SIZE_PER_BLOCK);
		offset = offset - (index_offset * MY_DATA_SIZE_PER_BLOCK);

		written += write_block(&(data.index_ids[index_offset - 1]), buf_ptr, &data, size, offset, index_offset);
	}
	else 
	{
		written += write_block(&(data.direct_data_id), buf_ptr, &data, size, offset, 0);
		//start from direct blocks
	}

	uuid_copy(inode.data_id, data.id);

	//store file data fcb
	store_to_db(data.id, &data, sizeof(file_data_fcb));

    if (offset == 0) 
    {
    	inode.size = written;
    }
    else 
    {
    	inode.size = inode.size + written;
    }

	time_t now = time(NULL);
    inode.mtime = now;
    inode.atime = now;

    //store file inode
    store_to_db(inode.id, &inode, sizeof(my_inode));

	write_log("[SYST] write: end write, wrote %d bytes\n", written);

    return size;

}


// Set the size of a file.
// Read 'man 2 truncate'.
int myfs_truncate(const char *path, off_t newsize)
{
    write_log("\n[SYST] truncate: (path=\"%s\", newsize=%lld)\n", path, newsize);

    if (newsize >= MY_MAX_FILE_SIZE)
    {
    	write_log("[SYST] truncate: - EFBIG\n");
    	return -EFBIG;
    }

    my_inode inode;
    int rc = get_inode(path, &inode, 0);
    if (rc < 0)
    {
    	write_log("[SYST] truncate: -ENOENT\n");
    	return -ENOENT;
    }

    if (newsize > inode.size)
    {
    	char buf[newsize - inode.size];
    	memset(buf, 0, newsize - inode.size);

    	myfs_write(path, buf, newsize - inode.size, inode.size, NULL);
    } 

    inode.size = newsize;
    rc = store_to_db(inode.id, &inode, sizeof(my_inode));
    if (rc != UNQLITE_OK)
    {
    	write_log("[SYST] truncate: - EIO\n");
    	return -EIO;
    }

    write_log("[SYST] truncate: End.\n");
    return 0;
}

// Set permissions.
// Read 'man 2 chmod'.
int myfs_chmod(const char *path, mode_t mode)
{
    write_log("\n[SYST] chmod: (path=\"%s\", mode=0%03o)\n", path, mode);

    my_inode inode;
    int rc = get_inode(path, &inode, 0);
    if (rc < 0)
    {
    	write_log("[SYST] chmod: - ENOENT\n");
    	return -ENOENT;
    }

    inode.mode = mode;
    store_to_db(inode.id, &inode, sizeof(my_inode));

    write_log("[SYST] chmod: End.\n");
    return 0;
}

// Set ownership.
// Read 'man 2 chown'.
int myfs_chown(const char *path, uid_t uid, gid_t gid)
{
    write_log("\n[SYST] chown: (path=\"%s\", uid=%d, gid=%d)\n", path, uid, gid);

    my_inode inode;
    int rc = get_inode(path, &inode, 0);
    if (rc < 0)
    {
    	write_log("[SYST] chown: - ENOENT\n");
    	return -ENOENT;
    }

    inode.uid = uid;
    inode.gid = gid;

    store_to_db(inode.id, &inode, sizeof(my_inode));

    write_log("[SYST] chown: End.\n");
    return 0;
}

/** 
 * Create a directory.
 * Read 'man 2 mkdir'.
 */
int myfs_mkdir(const char *path, mode_t mode)
{
	write_log("\n[SYST] mkdir: path='%s', name='%s'\n",path, get_file_name(path));

	//make directory fcb
	my_inode new_inode;
	memset(&new_inode, 0, sizeof(my_inode));

	uuid_generate(new_inode.id);

	new_inode.mode = mode | S_IFDIR;
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

	write_log("[SYST] mkdir: Made new directory '%s'\n", path);


	//get parent fcb
	my_inode parent_fcb;
	rc = get_inode(path, &parent_fcb, 1);
	if (rc < 0)
	{
		parent_fcb = the_root_fcb;
	}

	//update parent
	rc = update_parent(&parent_fcb, new_inode.id, path);
	if (rc < 0)
	{
		return rc;
	}

	write_log("[SYST] mkdir: End.\n");
    return 0;
}

// Delete a file.
// Read 'man 2 unlink'.
int myfs_unlink(const char *path)
{
	write_log("\n[SYST] unlink: path='%s'\n",path);

	char* file_name = get_file_name(path);

	my_inode parent;
	int rc = get_inode(path, &parent, 1);
	if (rc < 0)
	{
		return -ENOENT;
	}

	dir_data_fcb parent_data;
	fetch_from_db(parent.data_id, &parent_data, sizeof(dir_data_fcb));

	int found = 0;
	for (int i = 0; i<MY_MAX_DIR_FILES; i++)
	{
		dir_entry* entry = &parent_data.entries[i];

		if (strcmp(entry->filename, file_name) == 0)
		{
			found = 1;

			//memset to remove from parent's inode
			memset(&entry->inode_id, 0, sizeof(uuid_t));
			memset(&entry->filename, 0, MY_MAX_FILE_NAME);
			break;
		}
	}

	if (found)
	{
		parent.mtime = time(NULL);
		store_to_db(parent.id, &parent, sizeof(my_inode));
		store_to_db(parent.data_id, &parent_data, sizeof(dir_data_fcb));
		return 0;
	}
	else 
	{
		return -ENOENT;
	}
}


// Delete a directory.
// Read 'man 2 rmdir'.
int myfs_rmdir(const char *path)
{
    write_log("\n[SYST] rmdir: path='%s'\n",path);

    my_inode inode;
    int rc = get_inode(path, &inode, 0);

    if (rc < 0)
    {
    	write_log("[SYST] rmdir: - ENOENT\n");
    	return -ENOENT;
    }
    else 
    {
    	//loop through directory contents
    	dir_data_fcb dir_fcb;
    	fetch_from_db(inode.data_id, &dir_fcb, sizeof(dir_data_fcb));

    	int empty = 1;
    	for (int i = 0; i < MY_MAX_DIR_FILES; i++)
    	{
    		dir_entry entry = dir_fcb.entries[i];
    		if (strcmp(entry.filename, "") != 0)
    		{
    			empty = 0;
    			break;	
    		}
    	}

    	if (empty)
    	{
    		return myfs_unlink(path);
    	}
    	else 
    	{
    		write_log("[SYST] rmdir: -ENOTEMPTY\n");
    		return -ENOTEMPTY;
    	}
    }


    return 0;
}

// OPTIONAL - included as an example
// Flush any cached data.
int myfs_flush(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;

    write_log("myfs_flush(path=\"%s\", fi=0x%08x)\n", path, fi);

    return retstat;
}

// OPTIONAL - included as an example
// Release the file. There will be one call to release for each call to open.
int myfs_release(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;

    write_log("myfs_release(path=\"%s\", fi=0x%08x)\n", path, fi);

    return retstat;
}

// OPTIONAL - included as an example
// Open a file. Open should check if the operation is permitted for the given flags (fi->flags).
// Read 'man 2 open'.
static int myfs_open(const char *path, struct fuse_file_info *fi)
{
	// if (strcmp(path, the_root_fcb.path) != 0)
	write_log("[SYST] open: (path\"%s\", fi=0x%08x)\n", path, fi);

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
	.flush		= myfs_flush,
	.release	= myfs_release,
	.rmdir 		= myfs_rmdir,
	.unlink 	= myfs_unlink,
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

		the_root_fcb.mode |= S_IFDIR|S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH;
		time_t now = time(NULL);
		the_root_fcb.mtime = now;
		the_root_fcb.ctime = now;
		the_root_fcb.uid = getuid();
		the_root_fcb.gid = getgid();
		the_root_fcb.size = 0;


		//create directory data of root
		dir_data_fcb root_data;
		memset(&root_data, 0, sizeof(dir_data_fcb));

		uuid_generate(root_data.id);
		uuid_copy(the_root_fcb.data_id, root_data.id);

		//write root data to db
		store_to_db(root_data.id, &root_data, sizeof(dir_data_fcb));

		//Generate a key for the_root_fcb and update the root object.
		uuid_generate(root_object.id);
		uuid_copy(the_root_fcb.id, root_object.id);

		printf("init_fs: writing root fcb\n");
		//write root fcb to db
		rc = unqlite_kv_store(pDb, root_object.id, KEY_SIZE, &the_root_fcb, sizeof(my_inode));
		error_handle(rc);

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
