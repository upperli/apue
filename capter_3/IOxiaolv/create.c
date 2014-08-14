#include "apue.h"
#define BUFFSIZE 4096
int main(int agrc,char * agrv[])
{
	if(agrc < 2)
		return 0;
	int fd;
	if((fd = creat("testfile",FILE_MODE)) < 0)
		err_sys("create error\n");
	int n = atoi(agrv[1]);
	char buf[BUFFSIZE];
	for(int i = 0;i < BUFFSIZE;i++)
		buf[i] = 'n';
	while(n)
	{
		int i;
		if(n > BUFFSIZE)
			i = BUFFSIZE;
		else
			i = n;
		n = n-i;
		if(write(fd,buf,i) < 0)
			err_sys("write error\n");
	}
	close(fd);
	return 0;

}