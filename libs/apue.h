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
void err_dump(const char * fmt,...);//dump core and terminal,会产生core文件 一般用于更严重错误
void err_msg(const char * fmt,...);
void err_quit(const char * fmt,...);



//记录锁
int lock_reg(int fd, int cmd, int type, off_t offset, int whence, off_t len);

//读锁
#define 	read_lock(fd,offset,whence,len)	\
			lock_reg((fd), F_SETLK, F_RDLCK, (offset), (whence), (len))
//读锁阻塞版本
#define 	readw_lock(fd,offset,whence,len)	\
			lock_reg((fd), F_SETLKW, F_RDLCK, (offset), (whence), (len))
//写锁
#define 	write_lock(fd,offset,whence,len)	\
			lock_reg((fd), F_SETLK, F_WRLCK, (offset), (whence), (len))
//写锁阻塞版本
#define 	writew_lock(fd,offset,whence,len)	\
			lock_reg((fd), F_SETLKW, F_RDLCK, (offset), (whence), (len))
//解锁
#define 	un_lock(fd,offset,whence,len)	\
			lock_reg((fd), F_SETLK, F_UNLCK, (offset), (whence), (len))



#endif
