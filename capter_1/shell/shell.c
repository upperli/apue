/*************************************************************************
	> File Name: shell.c
	> Author: lcy
	> Mail: 710640803@qq.com 
	> Created Time: 2014年08月02日 星期六 10时39分15秒
 ************************************************************************/

#include<stdio.h>
#include <sys/wait.h>
#include <string.h>
#define MAXLINE 100
int main()
{
    char buf[MAXLINE];
    pid_t pid;
    int status;


    printf("%%");
    while(fgets(buf,MAXLINE,stdin) != NULL)
    {
        if(buf[strlen(buf)-1] == '\n')
            buf[strlen(buf)-1] = 0;
        if((pid = fork()) < 0)
            perror("error");
        else if(pid == 0){
            execlp(buf,buf,(char*)0);
            //err_ret("couldn't execute:%s",buf);
            exit(127);
        }

        if((pid = waitpid(pid,&status,0))<0)
            perror("wait error");
        printf("%%");
    }

}
