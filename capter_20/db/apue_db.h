#ifndef _APUE_DB_H
#define _APUE_DB_H

typedef 	void * 	DBHANDLE;

//函数接口声明
DBHANDLE db_open(const char * pathname,int oflag,...);
void	 db_close(DBHANDLE db);
char 	*db_fetch(DBHANDLE db,const char * key);
int 		 db_store(DBHANDLE db,const char * key,const char * ,int);
int 		 db_delete(DBHANDLE db,const char *);
int 		 db_rewind(DBHANDLE db);
char 	*db_nextrec(DBHANDLE db,char *);

//flags for db_store
#define 	DB_INSERT 	1
#define 	DB_REPLACE	2
#define	DB_STORE 	3



//数据库限制
#define	IDXLEN_MIN 	6	//索引长度，基本包含1字节键，1字节分隔符，1字节起始偏移量，1字节分隔符，1字节长度和终止换行符
#define 	IDXLEN_MAX 	1024
#define 	DATLEN_MIN 	2	//一字节数据和一字节换行
#define 	DATLEN_MAX 	1024



#endif