#ifndef _APUE_H
#define _APUE_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>

#include <unistd.h>
#include <signal.h>

#include <stddef.h>

#include <fcntl.h>//与 FILE_MODE有关，与IO有关
#define FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)   // 用于创建文件时指定文件模式

#define MAXLINE 4096

void err_ret(const char * fmt,...);
void err_sys(const char * fmt,...);
void err_exit(int error,const char * fmt,...);
void err_dump(const char * fmt,...);
void err_msg(const char * fmt,...);
void err_quit(const char * fmt,...);

#endif