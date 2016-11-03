#include "myfs.h"

int main(int argc, char** argv){
	int rc;
	
	//initialise the log file
	init_log_file();

	init_store();
	
	if(root_is_empty){
		// Create data object id and store it in the root object.
    	uuid_generate(root_object.id);
    	
    	printf("id is:\n");
    	print_id(&(root_object.id));
		printf("\n");
		
		struct myfcb the_root_fcb;
		memset(&the_root_fcb, 0, sizeof(struct myfcb));
		
		uuid_generate(the_root_fcb.file_data_id);	
		uint8_t data_block[MY_MAX_FILE_SIZE];
		memset(&data_block, 0, MY_MAX_FILE_SIZE);
		
		sprintf(the_root_fcb.path,"/hello.txt");
		int written = sprintf(data_block, "This is the file's contents.");
		
		// see 'man 2 stat' and 'man 2 chmod'
		the_root_fcb.mode |= S_IFREG|S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH; 
		the_root_fcb.mtime = time(0);
		the_root_fcb.size = written;
		
		// Store the data object.
		uuid_t *data_id = &(the_root_fcb.file_data_id);
		int rc = unqlite_kv_store(pDb,data_id,KEY_SIZE,&data_block,MY_MAX_FILE_SIZE);
		if( rc != UNQLITE_OK ){
			error_handler(rc);
		}
	
		// Store the fcb object.
		rc = unqlite_kv_store(pDb,&(root_object.id),KEY_SIZE,&the_root_fcb,sizeof(struct myfcb));
	 	if( rc != UNQLITE_OK ){
   			error_handler(rc);
		}
		
		// Store root object.
		rc = write_root();
	 	if( rc != UNQLITE_OK ){
   			error_handler(rc);
		}
	}else{
		printf("Root object is not empty. Doing nothing.\n");
	}

	unqlite_close(pDb);

	return 0;
}
