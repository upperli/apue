#include "apue.h"
#include "apue_db.h"
#include <stdarg.h>
#include <errno.h>
#include <sys/uio.h>   //使用iovec


typedef	unsigned long DBHASH;
typedef unsigned long COUNT;

typedef struct {
	int 	idxfd;		//索引文件的fd
	int datfd;		//数据文件的fd
	char * name;		//db的名字
	off_t 	idxoff;	//索引文件记录的偏移量
	off_t 	datoff;	//数据文件记录的偏移量

	off_t	hashoff;//索引文件中散列表的偏移
	DBHASH	nhash;	//当前散列表大小



/*
 *	关于数据库操作的统计计数	
*/
	COUNT	cnt_delok;
	COUNT 	cnt_delerr;
	COUNT 	cnt_fetchok;
	COUNT 	cnt_fetcherr;
	COUNT 	cnt_nextrec;
	COUNT 	cnt_stor1;	//DB_INSERT,非空添加
	COUNT 	cnt_stor2;	//DB_INSERT，找到空间，重利用
	COUNT 	cnt_stor3;	//DB_REPLACE	，不同长度，添加
	COUNT 	cnt_stor4;	//DB_REPLACE，相同长度，重写
	COUNT 	cnt_storerr;
}DB;

//内部隐藏函数
static	DB 		*_db_alloc(int);
static 	void 	 _db_dodelete(DB *);
static 	void 	 _db_free(DB *);



//打开或者创建一个数据库，参数和open系统调用一样
DBHANDLE	db_open(const char * pathname , int oflag , ...)
{
	DB 	*db;
	int 	len,mode;
	size_t 	i;
	char 	asciiptr[PTR_SZ+1] , hash[(NHASH_DEF + 1) * PTR_SZ + 2];//+2是为了换行和null
	

	struct stat 	statbuff;	//存储文件信息的数据结构

	len	= strlen(pathname);
	if((db = _db_alloc(len)) == NULL)
	{
		err_dump("db_open: _db_alloc error for DB");//错误退出，会产生 core 文件
	}

	db->nhash = NHASH_DEF;
	db->hashoff = HASH_OFF;

	strcpy(db->name,pathname);	//保存数据库名

	strcat(db->name, ".idx");	//db->name变成了索引文件

	if(oflag & O_CREAT)
	{
		va_list 	ap;
		//处理可变参数
		va_start(ap,oflag);
		mode = va_arg(ap,int);
		va_end(ap);


		db->idxfd = open(db->name,oflag,mode);
		strcpy(db->name + len,".dat");
		db->datfd = open(db->name,oflag,mode);
	}else{
		//打开文件，但是因为不需要创建，所以不需要mode
		db->idxfd = open(db->name,oflag);
		strcpy(db->name + len,".dat");
		db->datfd = open(db->name,oflag);
	}

	if(db->idxfd < 0 || db->datfd < 0)
	{	_db_free(db);
		return NULL;
	}



}