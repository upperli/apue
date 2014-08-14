#include "apue.h"

int main(int agrc,char * agrv[])
{
	int BUFFSIZE,n;
	if(agrc < 2)
		BUFFSIZE = 4096;
	else
		BUFFSIZE = atoi(agrv[1]);
	char * buf = (char * )malloc(BUFFSIZE);
	printf("%d\n",BUFFSIZE);
	while((n = read(STDIN_FILENO,buf,BUFFSIZE)) > 0)
		if(write(STDOUT_FILENO,buf,n) != n)
			err_sys("write error\n");
	if(n < 0)
		err_sys("read error");

	return 0;
}
