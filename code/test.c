#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define FILE "/cs/scratch/sy35/mnt/afile.txt"

int main(int argc, char** argv){
	int res=0;
	int fd2=open(FILE, O_RDWR|O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
	if (fd2!=-1){
		int wr=write(fd2,"1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 3\n",100);
		if(wr==-1){
			res=errno;
			perror("write");
		}
		struct stat statbuf;
		int sr=stat(FILE, &statbuf);
		if(sr!=0){
			res=errno;
			perror("stat");
		}


		lseek(fd2, 15, SEEK_SET);

		// char* buf = malloc(100);
		// int rd = read(fd2, buf, 100);
		// printf("%s\n", buf);
		int wr2 = write(fd2, "a", 1);

	}else{
		res=errno;
		perror("open");
	}
	return res;
}
