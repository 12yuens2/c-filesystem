#include <uuid/uuid.h>
#include <unqlite.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <fuse.h>

extern unqlite_int64 root_object_size_value;
#define ROOT_OBJECT_KEY "root"
#define ROOT_OBJECT_KEY_SIZE 4
#define ROOT_OBJECT_SIZE_P (unqlite_int64 *)&root_object_size_value
#define ROOT_OBJECT_SIZE root_object_size_value
#define ROOT_OBJECT_ID root_object.id
#define ROOT_OBJECT_ID_P &(root_object.id)

#define KEY_SIZE 16

#define DATABASE_NAME "myfs.db"

typedef struct rootS{
	uuid_t id;
}*root;

extern unqlite *pDb;
extern struct rootS root_object;
extern int root_is_empty;

extern void error_handler(int);
extern int read_root();
extern int write_root();
void print_id(uuid_t *);
void init_store();
int update_root();

extern FILE* init_log_file();
extern void write_log(const char *, ...);

extern uuid_t zero_uuid;

struct myfs_state {
    FILE *logfile;
};
#define NEWFS_PRIVATE_DATA ((struct myfs_state *) fuse_get_context()->private_data)

// For use in example code only. Do not use this structure in your submission!



