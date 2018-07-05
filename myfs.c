#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "myfs.h"

// Global Variables
char disk_name[128];   // name of virtual disk file
int  disk_size;        // size in bytes - a power of 2
int  disk_fd;          // disk file handle
int  disk_blockcount;  // block count on disk
int databegin;

struct superblock{
	char directory[20];
	int bitmap_start;
	int files_start;
	int inode_start;

	int openfiles;
};

struct file{
	char filename[MAXFILENAMESIZE];

	int double_indirect;
	int single_point;
	int double_point;

	int bytes_on_last_datablock;
	int read_pos;
	int write_pos;
};

struct superblock s;
char bitmap[DISKSIZE/BLOCKSIZE];
char files[MAXFILECOUNT];
char inodes[DISKSIZE/(BLOCKSIZE*4)];
int file_count;



/* 
   Reads block blocknum into buffer buf.
   You will not modify the getblock() function. 
   Returns -1 if error. Should not happen.
*/
int getblock (int blocknum, void *buf)
{      
	int offset, n; 
	//struct superblock* sp = (struct superblock*) buf;
	//printf("lalala %s %d\n", sp->directory,sp->bitmap_start);
	
	if (blocknum >= disk_blockcount) 
		return (-1); //error

	offset = lseek (disk_fd, blocknum * BLOCKSIZE, SEEK_SET); 
	if (offset == -1) {
		//printf ("lseek error\n"); 
		exit(0); 

	}

	n = read (disk_fd, buf, BLOCKSIZE); 
	if (n != BLOCKSIZE)
		return (-1); 	

	return (0); 
}


/*  
    Puts buffer buf into block blocknum.  
    You will not modify the putblock() function
    Returns -1 if error. Should not happen. 
*/
int putblock (int blocknum, void *buf)
{
	int offset, n;
	
	if (blocknum >= disk_blockcount) 
		return (-1); //error

	offset = lseek (disk_fd, blocknum * BLOCKSIZE, SEEK_SET);
	if (offset == -1) {
		//printf ("lseek error\n"); 
		exit (1); 
	}
	
	n = write (disk_fd, buf, BLOCKSIZE); 
	if (n != BLOCKSIZE) 
		return (-1); 

	return (0); 
}

void updateBitmap(){
	char tmp[BLOCKSIZE];

	for (int i=1; i < 1+ disk_blockcount/BLOCKSIZE; i++) {
		memcpy(tmp,bitmap+(i-1)*BLOCKSIZE, sizeof(tmp));	
		//printf ("bitmap block=%d\n", i);
		putblock(i, tmp); 
	}	
}

/* 
   IMPLEMENT THE FUNCTIONS BELOW - You can implement additional 
   internal functions. 
 */


int myfs_diskcreate (char *vdisk)
{
	strcpy (disk_name, vdisk);
	char buf[BLOCKSIZE];  

	int size = DISKSIZE; 
	disk_blockcount = DISKSIZE / BLOCKSIZE; 

	//printf ("diskname=%s size=%d blocks=%d\n", disk_name, size, disk_blockcount); 
       
	int ret = open (disk_name,  O_CREAT | O_RDWR, 0666); 	
	if (ret == -1) {
		//printf ("could not create disk\n"); 
		exit(1); 
	}
	
	bzero ((void *)buf, BLOCKSIZE); 
	int fd = open (disk_name, O_RDWR); 
	for (int i=0; i < (size / BLOCKSIZE); ++i) {
		//printf ("block=%d\n", i); 
		int n = write (fd, buf, BLOCKSIZE); 
		if (n != BLOCKSIZE) {
			//printf ("write error\n"); 
			exit (1); 
		}
	}	
	close (fd); 
	
	//printf ("created a virtual disk=%s of size=%d\n", disk_name, size);	


	//printf("block size:%d\n", disk_blockcount);
  	return(0); 
}


/* format disk of size dsize */
int myfs_makefs(char *vdisk)
{
	strcpy (disk_name, vdisk); 
	disk_size = DISKSIZE; 
	disk_blockcount = disk_size / BLOCKSIZE; 

	databegin = disk_blockcount/4;

	disk_fd = open (disk_name, O_RDWR); 
	if (disk_fd == -1) {
		//printf ("disk open error %s\n", vdisk); 
		exit(1); 
	}

	char tmp[BLOCKSIZE];
	struct superblock sp;
	strcpy(sp.directory, "root");
	sp.bitmap_start = 1;
	sp.files_start = 9;
	sp.openfiles = 0;
	sp.inode_start = 139;
	snprintf(tmp,sizeof(tmp),"%s\n%d\n%d\n%d\n%d\n",sp.directory,sp.bitmap_start,sp.files_start,sp.inode_start,sp.openfiles);

	putblock(0,&tmp);

	
	char bitmap[disk_blockcount];
	for(int i=0;i<disk_blockcount;i++)
		bitmap[i] = '0';

	bitmap[0] = '1';

	for (int i=1; i < 1+ disk_blockcount/BLOCKSIZE; i++)
		bitmap[i] = '1';
	
	for (int i=1; i < 1+ disk_blockcount/BLOCKSIZE; i++) {
		memcpy(tmp,bitmap+(i-1)*BLOCKSIZE, sizeof(tmp));	
		//printf ("bitmap block=%d\n", i);
		putblock(i, tmp); 
	}	

	file_count = 0;

	/*struct file f1;
	strcpy(f1.filename,"name1");
	f1.double_indirect = 139;

	putblock(10,&f1);

	int doublein[1024];

	
	for(int i=0;i<1024;i++)
		doublein[i] = 0;

	doublein[0] = 140;

	int singlein[1024];
	for(int i=0;i<1024;i++)
		singlein[i] = disk_blockcount / 4 +1 + i;

	
	putblock(139,&doublein);
	putblock(140,&singlein);*/

	

	//printf ("formatting disk=%s, size=%d\n", vdisk, disk_size); 

	fsync (disk_fd); 
	close (disk_fd); 

	return (0); 
}

/* 
   Mount disk and its file system. This is not the same mount
   operation we use for real file systems:  in that the new filesystem
   is attached to a mount point in the default file system. Here we do
   not do that. We just prepare the file system in the disk to be used
   by the application. For example, we get FAT into memory, initialize
   an open file table, get superblock into into memory, etc.
*/

int myfs_mount (char *vdisk)
{
	struct stat finfo; 

	strcpy (disk_name, vdisk);
	disk_fd = open (disk_name, O_RDWR); 
	if (disk_fd == -1) {
		//printf ("myfs_mount: disk open error %s\n", disk_name); 
		exit(1); 
	}
	
	fstat (disk_fd, &finfo); 

	//printf ("myfs_mount: mounting %s, size=%d\n", disk_name, 
	//	(int) finfo.st_size);  
	disk_size = (int) finfo.st_size; 
	disk_blockcount = disk_size / BLOCKSIZE; 

	char tmp[BLOCKSIZE];
	//int temp[1024];
	//struct superblock s1;
	getblock(0,&tmp);
	char * token;
	char* rest = tmp;
	token = strtok_r(rest, "\n", &rest);
	strcpy(s.directory,token);
	token = strtok_r(rest, "\n", &rest);
	s.bitmap_start = atoi(token);
	token = strtok_r(rest, "\n", &rest);
	s.files_start = atoi(token);
	token = strtok_r(rest, "\n", &rest);
	s.inode_start = atoi(token);
	token = strtok_r(rest, "\n", &rest);
	s.openfiles = atoi(token);
	//printf("%s %d\n", s.directory, s.bitmap_start);


	for (int i=1; i < 1+disk_blockcount/BLOCKSIZE; i++) {
		getblock(i,&tmp);
		memcpy(bitmap+(i-1)*BLOCKSIZE, tmp, sizeof(tmp));	
	}	

	//printf("%s\n", bitmap );
	for(int i=10;i<138;i++)
		if(bitmap[i] == '1')
			files[i-10]='1';
		else
			files[i-10]='0';

	//printf("files: %s\n", files);

	for(int i=139;i<139+disk_blockcount/4;i++)
		if(bitmap[i] == '1')
			inodes[i-139]='1';
		else
			inodes[i-139]='0';
	
	

	//printf("inodes: %s\n", inodes);

	/*getblock(139,&temp);
	for(int i=0;i<1024;i++)
		printf("%d ", temp[i]);

	printf("\n");
	getblock(140,&temp);
	for(int i=0;i<1024;i++)
		printf("%d ", temp[i]);
	printf("\n");*/


	
	/* you can place these returns wherever you want. Below
	   we put them at the end of functions so that compiler will not
	   complain.
        */
  	return (0); 
}


int myfs_umount()
{
	// perform your unmount operations here

    // write your code
    size_t mylen = strlen(bitmap);
    for(int i = 0; i < mylen; i++) {
        bitmap[i] = '0';
    }
    
    mylen = strlen(inodes);
    for(int i = 0; i < mylen; i++) {
        inodes[i] = '0';
    }

    mylen = strlen(files);
    for(int i = 0; i < mylen; i++) {
        files[i] = '0';
    }
    fsync (disk_fd); 
    close (disk_fd); 
    return (0);  
}


/* create a file with name filename */
int myfs_create(char *filename)
{	
	
	if(file_count >= 64)
	return -1;

	int used;
	for(used = 10;used<138; used++){
		if(files[used-10] == '1'){
			char y[BLOCKSIZE];
			getblock(used,&y);

			char *token;
    		char *rest = y;
			token = strtok_r(rest, "\n", &rest);
			//printf("used: %s\n", token);
			if(strcmp(token,filename) == 0)
			{
				return -1;
			}
		}
	}

	
	for(used = 10;used<138 &&files[used-10] == '1';used++);


	struct file f1;
	strcpy(f1.filename,filename);
	f1.double_indirect = -1;
	f1.double_point = 0;
	f1.single_point = 0;
	f1.bytes_on_last_datablock = 0;
	f1.read_pos = 0;
	f1.write_pos = 0;

	char tmp[BLOCKSIZE];
	snprintf(tmp,sizeof(tmp),"%s\n%d\n%d\n%d\n%d\n%d\n%d\n",f1.filename,f1.double_indirect,f1.double_point,
			f1.single_point,f1.bytes_on_last_datablock,f1.read_pos,f1.write_pos);
	file_count++;

	putblock(used,&tmp);
	bitmap[used] = '1';
	files[used-10] = '1';
	//printf("file inserted at:%d\n", used);
	updateBitmap();

	return used; 
}


/* open file filename */
int myfs_open(char *filename)
{
	if(file_count >= 64)
	return -1;

	int index = -1; 
	
	int used;
	//printf("file open\n");
	for(used = 10;used<138; used++){
		if(files[used-10] == '1'){
			char y[BLOCKSIZE];
			getblock(used,&y);

			char *token;
    		char *rest = y;
			token = strtok_r(rest, "\n", &rest);
			//printf("used: %s\n", token);
			if(strcmp(token,filename) == 0)
			{
				index = used;
				char y[BLOCKSIZE];
				getblock(used,&y);
				char *token;
				char *rest = y;
				token = strtok_r(rest, "\n", &rest);	
				struct file f1;
				strcpy(f1.filename,token);
				token = strtok_r(rest, "\n", &rest);
				f1.double_indirect = atoi(token);
				token = strtok_r(rest, "\n", &rest);
				f1.double_point = atoi(token);
				token = strtok_r(rest, "\n", &rest);
				f1.single_point = atoi(token);
				token = strtok_r(rest, "\n", &rest);
				f1.bytes_on_last_datablock = atoi(token);
				token = strtok_r(rest, "\n", &rest);
				f1.read_pos = atoi(token);
				token = strtok_r(rest, "\n", &rest);
				f1.write_pos = atoi(token);

				f1.read_pos = 0;
				f1.write_pos = 0;

				char tmp[BLOCKSIZE];
				snprintf(tmp,sizeof(tmp),"%s\n%d\n%d\n%d\n%d\n%d\n%d\n",f1.filename,f1.double_indirect,f1.double_point,
					f1.single_point,f1.bytes_on_last_datablock,f1.read_pos,f1.write_pos);


				putblock(used,&tmp);
			
			}
		}			
	}

	//printf("file found at:%d\n", index);       
	return (index); 
}

/* close file filename */
int myfs_close(int fd)
{
	char y[BLOCKSIZE];
	getblock(fd,&y);
	char *token;
    char *rest = y;
	token = strtok_r(rest, "\n", &rest);	
	struct file f1;
	strcpy(f1.filename,token);
	token = strtok_r(rest, "\n", &rest);
	f1.double_indirect = atoi(token);
	token = strtok_r(rest, "\n", &rest);
	f1.double_point = atoi(token);
	token = strtok_r(rest, "\n", &rest);
	f1.single_point = atoi(token);
	token = strtok_r(rest, "\n", &rest);
	f1.bytes_on_last_datablock = atoi(token);
	token = strtok_r(rest, "\n", &rest);
	f1.read_pos = atoi(token);
	token = strtok_r(rest, "\n", &rest);
	f1.write_pos = atoi(token);

	f1.read_pos = -1;

	char tmp[BLOCKSIZE];
	snprintf(tmp,sizeof(tmp),"%s\n%d\n%d\n%d\n%d\n%d\n%d\n",f1.filename,f1.double_indirect,f1.double_point,
					f1.single_point,f1.bytes_on_last_datablock,f1.read_pos,f1.write_pos);


	putblock(fd,&tmp);
	file_count--;


	return (0); 
}

int myfs_delete(char *filename)
{
	int used;
	//printf("file open\n");
	for(used = 10;used<138; used++){
		if(files[used-10] == '1'){
			char y[BLOCKSIZE];
			getblock(used,&y);

			char *token;
    		char *rest = y;
			token = strtok_r(rest, "\n", &rest);
			
			if(strcmp(token,filename) == 0)
			{	
				//printf("in delete of : %s\n", token);
				struct file f1;
				strcpy(f1.filename,token);
				token = strtok_r(rest, "\n", &rest);
				f1.double_indirect = atoi(token);
				token = strtok_r(rest, "\n", &rest);
				f1.double_point = atoi(token);
				token = strtok_r(rest, "\n", &rest);
				f1.single_point = atoi(token);
				token = strtok_r(rest, "\n", &rest);
				f1.bytes_on_last_datablock = atoi(token);
				token = strtok_r(rest, "\n", &rest);
				f1.read_pos = atoi(token);
				token = strtok_r(rest, "\n", &rest);
				f1.write_pos = atoi(token);
				if(f1.read_pos == -1){
					int temp[1024];
					if(f1.double_indirect != -1){
						getblock(f1.double_indirect,&temp);
						bitmap[f1.double_indirect] = '0';
						for(int i = 0; i <= f1.double_point;i++){
							int single = temp[i];
							bitmap[single] = '0';
							inodes[single-139] = '0'; 

							
							int tmp[1024];
							getblock(single,tmp);

							int sing_max = 1023;
							if(i == f1.double_point)	
								sing_max = f1.single_point;				

							for(int j=0; j <= sing_max; j++){						
								char t[BLOCKSIZE];
								//printf("in delete data: %d\n", tmp[j]); 
								getblock(tmp[j],t);
								bzero(t, sizeof(t));
								putblock(tmp[j],t);

								bitmap[tmp[j]] = '0';
							}
						}
					}

					files[used-10] = '0';
					bitmap[used] = '0';
					char z[BLOCKSIZE];
					bzero(z, sizeof(z));
					putblock(used,&z);
				}
				else
					return -1;
				
			}
		}			
	}
	updateBitmap();
	return (0); 
}

int myfs_read(int fd, void *buf, int n)
{
	int bytes_read = -1; 
	char x[BLOCKSIZE];
	struct file f1;
	getblock(fd,&x);

	if(files[fd-10] == '0')
		return -1;
	
	char * token;
	char* rest = x;
	token = strtok_r(rest, "\n", &rest);
	strcpy(f1.filename,token);
	token = strtok_r(rest, "\n", &rest);
	f1.double_indirect = atoi(token);
	token = strtok_r(rest, "\n", &rest);
	f1.double_point = atoi(token);
	token = strtok_r(rest, "\n", &rest);
	f1.single_point = atoi(token);
	token = strtok_r(rest, "\n", &rest);
	f1.bytes_on_last_datablock = atoi(token);
	token = strtok_r(rest, "\n", &rest);
	f1.read_pos = atoi(token);
	token = strtok_r(rest, "\n", &rest);
	f1.write_pos = atoi(token);

	if(n>MAXREADWRITE)
		n = MAXREADWRITE;

	int size = myfs_filesize(fd);
	bytes_read = 0;
	
	if(f1.double_indirect != -1 && f1.read_pos+ n <= size){
		int tmp = f1.double_indirect;
		//printf("in read bitmap:%d\n",bitmap[tmp] );
		//printf("in read read pos:%d\n",f1.read_pos);
		if(bitmap[tmp] == '1' && f1.read_pos != -1){
			int temp[1024];
			int double_read = f1.read_pos / (BLOCKSIZE*BLOCKSIZE/4);
			int single_read = (f1.read_pos / (BLOCKSIZE)) % (BLOCKSIZE/4);
			 
			getblock(f1.double_indirect,temp);
			 
			int single = temp[double_read];
			getblock(single ,temp);
			int datablock = temp[single_read];
			//printf("in read data:%d\n",single_read);
			char t[BLOCKSIZE];
			getblock(datablock,t);

			char result[n+1];
			bzero(result,n+1);
			if(n + f1.read_pos % BLOCKSIZE < BLOCKSIZE){
				char x[n];
				bzero(x,n);
				memcpy(x,t +(f1.read_pos % BLOCKSIZE),n);
				memcpy(result, t + (f1.read_pos % BLOCKSIZE), n);	
				f1.read_pos += n;	
					
				memcpy(buf,result,n+1);
				int d=0;
				for(int b=0;b<MAXREADWRITE+1 &&d== 0;b++){
					if(result[b] == '\0'){
					d = b;
					}						
				}
				if(d>MAXREADWRITE)
					d = MAXREADWRITE;
				bytes_read = MAXREADWRITE;
				//printf("bytes read1:%d\n",bytes_read);
				//printf("%d\n",sizeof(result));
			}
			else{
				int dif = -f1.read_pos % BLOCKSIZE + BLOCKSIZE;
				memcpy(result, t + f1.read_pos % BLOCKSIZE, dif);	
				f1.read_pos += dif;	

				double_read = f1.read_pos / (BLOCKSIZE*BLOCKSIZE/4);
				single_read = f1.read_pos / (BLOCKSIZE)  % (BLOCKSIZE/4);
				
				getblock(f1.double_indirect,temp);				
				single = temp[double_read];
				getblock(single ,temp);
				datablock = temp[single_read];

				memcpy(result+dif, t + f1.read_pos % BLOCKSIZE, n -dif);	
				f1.read_pos += n -dif;

				//printf("in reading: %d\n",double_read);
				memcpy(buf,result,n+1);
				//printf("%d\n",sizeof(result));
				int d=0;
				for(int b=0;b<MAXREADWRITE+1 &&d== 0;b++){
					if(result[b] == '\0'){
					d = b;
					}						
				}
				if(d>MAXREADWRITE)
					d = MAXREADWRITE;
				bytes_read = MAXREADWRITE;
				//printf("bytes read2:%d\n",bytes_read);
			}
			
			

			char tmp[BLOCKSIZE];
			snprintf(tmp,sizeof(tmp),"%s\n%d\n%d\n%d\n%d\n%d\n%d\n",f1.filename,f1.double_indirect,f1.double_point,
					f1.single_point,f1.bytes_on_last_datablock,f1.read_pos,f1.write_pos);

			putblock(fd,&tmp);
			 
		 }
	}
	else if( size - f1.read_pos > 0 &&  size - f1.read_pos < BLOCKSIZE){
		int a =  size - f1.read_pos;

		int temp[1024];
		int double_read = f1.read_pos / (BLOCKSIZE*BLOCKSIZE/4);
		int single_read = (f1.read_pos / (BLOCKSIZE)) % (BLOCKSIZE/4);
			 
		getblock(f1.double_indirect,temp);
			 
		int single = temp[double_read];
		getblock(single ,temp);
		int datablock = temp[single_read];
			//printf("in read data:%d\n",single_read);
		getblock(datablock,x);

		char result[a+1];
		bzero(result,a+1);
		memcpy(result, x + f1.read_pos % BLOCKSIZE, a);	
		f1.read_pos += a;	

		bzero(buf,n);
		memcpy(buf,result,a+1);
		
		bytes_read += a;
		//printf("bytes read3:%d\n",bytes_read);

		char tmp[BLOCKSIZE];
		snprintf(tmp,sizeof(tmp),"%s\n%d\n%d\n%d\n%d\n%d\n%d\n",f1.filename,f1.double_indirect,f1.double_point,
					f1.single_point,f1.bytes_on_last_datablock,f1.read_pos,f1.write_pos);

		putblock(fd,&tmp);


		
	}
	else if(f1.read_pos >= size){
		bzero(buf,n);
	}
	//printf("bytes read:%d\n",bytes_read);
	// write your code
	
	return (bytes_read); 

}

int myfs_write(int fd, void *buf, int n)
{
	int bytes_written = -1; 
	char t[BLOCKSIZE];
	struct file f1;
	getblock(fd,t);	
	char * token;
	char* rest = t;

	token = strtok_r(rest, "\n", &rest);
	strcpy(f1.filename,token);
	token = strtok_r(rest, "\n", &rest);
	f1.double_indirect = atoi(token);
	token = strtok_r(rest, "\n", &rest);
	f1.double_point = atoi(token);
	token = strtok_r(rest, "\n", &rest);
	f1.single_point = atoi(token);
	token = strtok_r(rest, "\n", &rest);
	f1.bytes_on_last_datablock = atoi(token);	
	token = strtok_r(rest, "\n", &rest);	
	f1.read_pos = atoi(token);
	token = strtok_r(rest, "\n", &rest);	
	f1.write_pos = atoi(token);

	if(n>MAXREADWRITE)
		n = MAXREADWRITE;

	int used;
	//printf("%s\n",bitmap);
	if(f1.read_pos == -1)
		return -1;

	if(f1.double_indirect == -1){
		
		for(used = 138;used<disk_blockcount/4 &&bitmap[used] == '1';used++);
		bitmap[used] = '1';
		inodes[used-139] = '1';
		f1.double_indirect = used;

		int single;
		for(single = 138;single<disk_blockcount/4 && bitmap[single] == '1';single++);
		bitmap[single] = '1';
		inodes[single-139] = '1';

		int temp[1024];
		getblock(used,&temp);
		temp[f1.double_point] = single;
		putblock(used,&temp);
		//printf("double_indirect in %d\n",used);

		int datablock;
		for(datablock = disk_blockcount/4 +1;datablock<disk_blockcount  &&bitmap[datablock] == '1';datablock++);
		getblock(single,&temp);
		temp[f1.single_point] = datablock;
		putblock(single,&temp);
		//printf("single_indirect in %d\n",single);
		bitmap[datablock] = '1';

		char arr[BLOCKSIZE];
		bzero(arr,BLOCKSIZE);
		memcpy(arr,buf,n);
		putblock(datablock, arr);
		f1.bytes_on_last_datablock = n;
		f1.write_pos = n;
		
		//printf("%d",f1.write_pos);

		char tmp[BLOCKSIZE];
		snprintf(tmp,sizeof(tmp),"%s\n%d\n%d\n%d\n%d\n%d\n%d\n",f1.filename,f1.double_indirect,f1.double_point,
					f1.single_point,f1.bytes_on_last_datablock,f1.read_pos,f1.write_pos);

		putblock(fd,&tmp);
		updateBitmap();
	}
	else{
		used = f1.double_indirect;

		int single;
		int temp[1024];
		getblock(used,&temp);
		single = temp[f1.double_point];
		//printf("double_indirect in %d\n",used);

		int datablock;
		getblock(single,&temp);
		datablock = temp[f1.single_point];
		//printf("single_indirect in %d\n",single);

		getblock(datablock,&t);
		//printf("%s\n",t);
		//printf("sizeof buf : %d\n",sizeof(buf) );
		int size = myfs_filesize(fd);
		if(f1.write_pos + n <= size){
			int double_po = f1.write_pos / (BLOCKSIZE*BLOCKSIZE/4);
			int single_po = f1.write_pos / BLOCKSIZE;
			int bytes_on = f1.write_pos % BLOCKSIZE;

			//printf("d:%d s:%d by:%d\n", double_po, single_po, bytes_on);

			used = f1.double_indirect;
			getblock(used,&temp);
			single = temp[double_po];
			//printf("double_indirect in %d\n",used);

			getblock(single,&temp);
			datablock = temp[single_po];
			//printf("single_indirect in %d\n",single);
			getblock(datablock,t);

			if( bytes_on + n <= BLOCKSIZE){
				memcpy(t + bytes_on,buf,n);
				putblock(datablock, t);
				f1.write_pos += n;
				//printf("%s\n",t);
			}
			else{
				int to_write = BLOCKSIZE - bytes_on;
				char tmp_buf[to_write];
				memcpy(tmp_buf,buf,to_write);
				memcpy(t+bytes_on,tmp_buf,to_write);
				putblock(datablock, t);
				

				char new_t[n-to_write];
				
				memcpy(new_t,buf+to_write,n-to_write);

				single_po++;

				if(single_po >= 1024){
					single_po = 0;
					double_po++;
					used = f1.double_indirect;
										
					int temp[1024];
					getblock(used,&temp);
					single = temp[double_po];
				}
				
				getblock(single,&temp);
				datablock = temp[single_po];
				char x[BLOCKSIZE];
				getblock(datablock,&x);
				memcpy(x,new_t,n-to_write);
				bytes_on  = n - to_write;
				//printf("%s with size:%d\n",new_t,n-to_write);
				putblock(datablock, &x);
				//printf("new datablock: %d\n",datablock);
				f1.write_pos += n;


			}

		}
		else if( f1.bytes_on_last_datablock  + n < BLOCKSIZE){
			memcpy(t + f1.bytes_on_last_datablock,buf,n);
			putblock(datablock, t);
			//printf("%s\n",t);
			f1.bytes_on_last_datablock += n;
			f1.write_pos += n;
		}
		else{
			int to_write = BLOCKSIZE - f1.bytes_on_last_datablock;
			char tmp_buf[to_write];
			memcpy(tmp_buf,buf,to_write);
			memcpy(t+f1.bytes_on_last_datablock,tmp_buf,to_write);
			putblock(datablock, t);


			char new_t[n-to_write];
			
			memcpy(new_t,buf+to_write,n-to_write);

			f1.single_point++;
			//printf("yeni datablock for : %d\n",f1.bytes_on_last_datablock);

			if(f1.single_point >= 1024){
				f1.single_point = 0;
				f1.double_point++;
				used = f1.double_indirect;
				
				for(single = 138;single<disk_blockcount/4 && bitmap[single] == '1';single++);
				bitmap[single] = '1';
				inodes[single-139] = '1';
				//printf("new single:%d\n",single);
				
				int temp[1024];
				getblock(used,&temp);
				temp[f1.double_point] = single;
				putblock(used,&temp);
			}

			for(datablock = disk_blockcount/4 +1;datablock<disk_blockcount  &&bitmap[datablock] == '1';datablock++);
			getblock(single,&temp);
			temp[f1.single_point] = datablock;
			putblock(single,&temp);
			f1.bytes_on_last_datablock  = n - to_write;
			bitmap[datablock] = '1';
			//printf("%s with size:%d\n",new_t,n-to_write);
			putblock(datablock, new_t);
			//printf("%s\n",new_t);
			//printf("new datablock: %d\n",datablock);
			f1.write_pos += n;
		}		
		char tmp[BLOCKSIZE];
		snprintf(tmp,sizeof(tmp),"%s\n%d\n%d\n%d\n%d\n%d\n%d\n",f1.filename,f1.double_indirect,f1.double_point,
					f1.single_point,f1.bytes_on_last_datablock,f1.read_pos,f1.write_pos);
		
		putblock(fd,&tmp);
		updateBitmap();
	}
	bytes_written = n;	

	return (bytes_written); 
} 

int myfs_truncate(int fd, int size)
{
	if(files[fd-10] == '0')
		return -1;

	int prev_size = myfs_filesize(fd);
	if(size >= prev_size)
		return 0;

	int dif = prev_size - size;

	char y[BLOCKSIZE];
	getblock(fd,&y);
	char *token;
    char *rest = y;
	token = strtok_r(rest, "\n", &rest);	
	struct file f1;
	strcpy(f1.filename,token);
	token = strtok_r(rest, "\n", &rest);
	f1.double_indirect = atoi(token);
	token = strtok_r(rest, "\n", &rest);
	f1.double_point = atoi(token);
	token = strtok_r(rest, "\n", &rest);
	f1.single_point = atoi(token);
	token = strtok_r(rest, "\n", &rest);
	f1.bytes_on_last_datablock = atoi(token);
	token = strtok_r(rest, "\n", &rest);
	f1.read_pos = atoi(token);
	token = strtok_r(rest, "\n", &rest);
	f1.write_pos = atoi(token);

	if(f1.double_indirect == -1)
		return -1;

	int temp[1024];
	getblock(f1.double_indirect, &temp);

	int single = temp[f1.double_point];
	getblock(single,&temp);

	int datablock  = temp[f1.single_point];
	getblock(datablock,&y);

	if(dif < f1.bytes_on_last_datablock){
		//printf("to delete %d\n",dif);
		char tmp[BLOCKSIZE];
		bzero(tmp, sizeof(tmp));
		memcpy(tmp,y,f1.bytes_on_last_datablock -dif);
		putblock(datablock,&tmp);
		f1.bytes_on_last_datablock -= dif;
	}
	else{
		dif = dif -f1.bytes_on_last_datablock;
		char tmp[BLOCKSIZE];
		bzero(tmp, sizeof(tmp));
		putblock(datablock,&tmp);
		bitmap[datablock] = '0';
		

		getblock(single, &temp);

		temp[f1.single_point] = 0;
		f1.single_point--;

		getblock(f1.double_indirect, &temp);
		single = temp[f1.double_point];
		int i = dif / BLOCKSIZE;
		int count = dif % BLOCKSIZE;
		while(i>0){
			if(f1.single_point == 0){
				getblock(f1.double_indirect, &temp);
				temp[f1.double_point] = 0;
				f1.double_point--;
				single = temp[f1.double_point];
				putblock(f1.double_indirect, &temp);
				
				f1.single_point = 1024;
			}

			getblock(single, &temp);

			temp[f1.single_point] = 0;
			f1.single_point--;
			putblock(single,&temp);

			
			i--;
		}

		getblock(f1.double_indirect, &temp);
		single = temp[f1.double_point];
		getblock(single,&temp);

		datablock  = temp[f1.single_point];
		getblock(datablock,&y);

		if(count < BLOCKSIZE){
			char tm[BLOCKSIZE];
			bzero(tm, sizeof(tm));
			memcpy(tm,y,BLOCKSIZE-count);
			putblock(datablock,&tm);
			//printf("to delete 4 \n");
			f1.bytes_on_last_datablock = BLOCKSIZE-count;
		}


	}
	int size_now = (f1.double_point)*BLOCKSIZE*BLOCKSIZE/4 +(f1.single_point)*BLOCKSIZE + f1.bytes_on_last_datablock;
	if(f1.read_pos > size_now)
		f1.read_pos =size_now;
	if(f1.write_pos > size_now)
		f1.write_pos =size_now;

	char tmp[BLOCKSIZE];
	snprintf(tmp,sizeof(tmp),"%s\n%d\n%d\n%d\n%d\n%d\n%d\n",f1.filename,f1.double_indirect,f1.double_point,
					f1.single_point,f1.bytes_on_last_datablock,f1.read_pos,f1.write_pos);
		
	putblock(fd,&tmp);

	updateBitmap();
	return (0); 
} 


int myfs_seek(int fd, int offset)
{
	int position = -1; 
	if(files[fd-10] == '0')
		return -1;

	int size = myfs_filesize(fd);
	if(offset > size)
		offset = size;

	if(files[fd-10] == '1'){
		char y[BLOCKSIZE];
		getblock(fd,&y);
		char *token;
		char *rest = y;
		token = strtok_r(rest, "\n", &rest);	
		struct file f1;
		strcpy(f1.filename,token);
		token = strtok_r(rest, "\n", &rest);
		f1.double_indirect = atoi(token);
		token = strtok_r(rest, "\n", &rest);
		f1.double_point = atoi(token);
		token = strtok_r(rest, "\n", &rest);
		f1.single_point = atoi(token);
		token = strtok_r(rest, "\n", &rest);
		f1.bytes_on_last_datablock = atoi(token);
		token = strtok_r(rest, "\n", &rest);
		f1.read_pos = atoi(token);
		token = strtok_r(rest, "\n", &rest);
		f1.write_pos = atoi(token);

		f1.read_pos = offset;
		f1.write_pos = offset;

		char tmp[BLOCKSIZE];
		snprintf(tmp,sizeof(tmp),"%s\n%d\n%d\n%d\n%d\n%d\n%d\n",f1.filename,f1.double_indirect,f1.double_point,
					f1.single_point,f1.bytes_on_last_datablock,f1.read_pos,f1.write_pos);

		putblock(fd,&tmp);
		position = fd;
	}

	return (position); 
} 

int myfs_filesize (int fd)
{
	int size = -1; 
	
	if(files[fd-10] == '1'){
		char y[BLOCKSIZE];
		getblock(fd,&y);
		char *token;
		char *rest = y;
		token = strtok_r(rest, "\n", &rest);	
		struct file f1;
		strcpy(f1.filename,token);
		token = strtok_r(rest, "\n", &rest);
		f1.double_indirect = atoi(token);
		token = strtok_r(rest, "\n", &rest);
		f1.double_point = atoi(token);
		token = strtok_r(rest, "\n", &rest);
		f1.single_point = atoi(token);
		token = strtok_r(rest, "\n", &rest);
		f1.bytes_on_last_datablock = atoi(token);
		token = strtok_r(rest, "\n", &rest);
		f1.read_pos = atoi(token);

		//printf("%d %d %d \n",(f1.double_point),(f1.single_point),f1.bytes_on_last_datablock);
		size = (f1.double_point)*BLOCKSIZE*BLOCKSIZE/4 +(f1.single_point)*BLOCKSIZE + f1.bytes_on_last_datablock;
	}

	return (size); 
}


void myfs_print_dir ()
{
	for(int i= 10;i<138;i++ ){
		if(files[i-10] == '1'){
			char t[BLOCKSIZE];
			getblock(i,&t);
			char * token;
			char* rest = t;
			token = strtok_r(rest, "\n", &rest);
			printf("%s\n", token);
		}
	}
}


void myfs_print_blocks (char *  filename)
{
	for(int i= 10;i<138;i++ ){
		if(files[i-10] == '1'){
			char t[BLOCKSIZE];
			getblock(i,&t);
			char * token;
			char* rest = t;
			token = strtok_r(rest, "\n", &rest);
			if(strcmp(token,filename) == 0){
				struct file f1;
				strcpy(f1.filename,token);
				token = strtok_r(rest, "\n", &rest);
				f1.double_indirect = atoi(token);
				token = strtok_r(rest, "\n", &rest);
				f1.double_point = atoi(token);
				token = strtok_r(rest, "\n", &rest);
				f1.single_point = atoi(token);

				if(f1.double_indirect != -1){

					printf("%s: %d", f1.filename,f1.double_indirect);
					int single;
					int singlemax = 0;

					for(int j = 0; j <= f1.double_point; j++){
						int temp[1024];
						getblock(f1.double_indirect,&temp);
						single = temp[j];
						printf(" %d",single);
						if(single > singlemax)
							singlemax= single;
					}

					for(int j = 0; j <= f1.double_point; j++){
						int temp[1024];
						getblock(f1.double_indirect,&temp);
						single = temp[j];

						int lim = f1.single_point;
						if( single != singlemax)
							lim =1023;

						for(int k=0;k<=lim;k++){
							int datablock;
							getblock(single,&temp);
							datablock = temp[k];
							printf(" %d",datablock);
						}
					}
					printf("\n");	
				}	
				else{
					printf("%s:\n", f1.filename);
				}		

				
				
			}
		}
	}
}
