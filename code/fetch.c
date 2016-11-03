#include "myfs.h"

int main(int argc, char** argv){
	int rc;

	// Initialise the log file.
	init_log_file();

	init_store();
	
	if(!root_is_empty){
	
		uuid_t *dataid = &(root_object.id);
	
		unqlite_int64 nBytes;  // Data length
		rc = unqlite_kv_fetch(pDb,dataid,KEY_SIZE,NULL,&nBytes);
		if( rc != UNQLITE_OK ){
		  error_handler(rc);
		}
		if(nBytes!=sizeof(struct myfcb)){
			printf("Data object has unexpected size. Doing nothing.\n");
			exit(-1);
		}

		struct myfcb the_root_fcb;
	
		// Fetch the fcb.
		unqlite_kv_fetch(pDb,dataid,KEY_SIZE,&the_root_fcb,&nBytes);

		printf("key is:\n");
		print_id(dataid);	
		
		printf("\npath: %s\nperm: %04o\nsize: %i\n", the_root_fcb.path,the_root_fcb.mode & (S_IRWXU|S_IRWXG|S_IRWXO), the_root_fcb.size);
		
		// Fetch the data.
		uint8_t data_block[MY_MAX_FILE_SIZE];
		memset(&data_block, 0, MY_MAX_FILE_SIZE);
		nBytes = MY_MAX_FILE_SIZE;
		unqlite_kv_fetch(pDb,&(the_root_fcb.file_data_id),KEY_SIZE,&data_block,&nBytes);
		// Write the data.
		printf("\ndata: %s\n", data_block);
	}else{
		printf("Root object was empty. Nothing to fetch.");
	}
	
	//Close our database handle
	unqlite_close(pDb);
}


