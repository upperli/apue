#include "apue.h"
char buf1[] = "abcdefg";
char buf2[] = "ABCDEFG";


int main()
{
	int fd;
	if((fd = creat("file.hole",FILE_MODE)) < 0)
		err_sys("create error\n");
	if(write(fd,buf1,7) != 7)
		err_sys("write error\n");
	if(lseek(fd,16384,SEEK_SET) == -1)
		err_sys("seek error\n");
	if(write(fd,buf2,7) != 7)
		err_sys("write error\n");
	return 0;
}