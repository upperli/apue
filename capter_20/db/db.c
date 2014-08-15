#include "apue.h"
#include "apue_db.h"
#include <stdarg.h>
#include <errno.h>
#include <sys/uio.h>   //使用iovec



#define PTR_SZ	6	//哈希链表中指针域的大小
#define	NHASH_DEF	999999 	//最大文件偏移量为 10^PTR_SZ-1
#define 	HASH_OFF		PTR_SZ 	//索引文件中哈希表的偏移量

typedef	unsigned long DBHASH;
typedef unsigned long COUNT;

typedef struct {
	int 	idxfd;		//索引文件的fd
	int datfd;		//数据文件的fd
	
	char *idxbuf;	//索引文件缓存
	char *datbuf;	//数据文件缓存

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
	char 	asciiptr[PTR_SZ + 1] , hash[(NHASH_DEF + 1) * PTR_SZ + 2];//+2是为了换行和null
	

	struct stat 	statbuff;	//存储文件信息的数据结构

	len	= strlen(pathname);
	if((db = _db_alloc(len)) == NULL)
	{
		err_dump("db_open: _db_alloc error for DB");//错误退出，会产生 core 文件
	}

	db->nhash = NHASH_DEF;	//散列表大小
	db->hashoff = HASH_OFF;	//散列表偏移量

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

	if(db->idxfd < 0 || db->datfd < 0)//打开出错处理
	{	_db_free(db);
		return NULL;
	}

	if((oflag & (O_CREAT | O_TRUNC)) == (O_CREAT | O_TRUNC))//创建并截断为0
	{
		/*
		*创建数据库时进行初始化，这里要进行加锁，假如两个进程同时创建一个数据库，不加锁会出错
		*/

		if(writew_lock(db->idxfd , 0 , SEEK_SET, 0) < 0)//加锁   
			err_dump("db_open: write_lock error");
			/*
			 *#define writew_lock(fd,offset,whence,len)\
			 	lock_reg((fd),F_SETLK,F_WRLCK,(offset),(whence),(len)),定义在apue.h中
			 */

		if(fstat(db->idxfd , &statbuff) < 0)
			err_sys("db_open: fstat error");	//为什么这里使用err_sys?更严重的错误采用err_dump，有待于进一步思考
		if(statbuff.st_size == 0)
		{
			sprintf(asciiptr , "%*d" , PTR_SZ , 0);
			hash[0] = 0;
			for (i = 0; i < NHASH_DEF + 1; ++i)
			{
				strcat(hash,asciiptr);
			}
			strcat(hash,"\n");
			i = strlen(hash);
			if(write(db->idxfd, hash, i) != i)
				err_dump("db_open: index file init write error");

		}
		if(un_lock(db->idxfd , 0 , SEEK_SET , 0)  < 0)//define see apue.h
			err_dump("db_open: unlock error");

	}

	db_rewind(db);
	return db;

}
void db_close(DBHANDLE h)
{
	_db_free((DB *)h);	//关闭数据库就是释放
}


static DB 	*_db_alloc(int namelen)
{
	DB 	*db;
//为DB分配空间
	if((db = calloc(1,sizeof(DB))) == NULL)//calloc与malloc只是参数不同 
		err_dump("_db_alloc: calloc error for DB");
	db->idxfd = db->datfd = -1;
//为name分配空间
	if((db->name = malloc(namelen + 5)) == NULL)
		err_dump("_db_alloc： malloc error for name");

	if((db->idxbuf = malloc(IDXLEN_MAX + 2)) == NULL)
		err_dump("_db_alloc： malloc error for index buffer");

	if((db->datbuf = malloc(DATLEN_MAX + 2)) == NULL)
		err_dump("_db_alloc： malloc error for data buffer");

	return db;
}

/*
*关闭文件描述符，释放申请的动态空间
*/
static	_db_free(DB *db)
{

	if(db->idxfd >= 0)
		close(db->idxfd);

	if(db->datfd >= 0)
		close(db->datfd);

	if(db->idxbuf != NULL)
		free(db->idxbuf);
	
	if(db->datbuf != NULL)
		free(db->datbuf);
	if(db->name != NULL)
		free(db->name);

	free(db);

}




//遍历记录初始化
void db_rewind(DBHANDLE h)
{
	DB 	*db = h;
	off_t offset = (db->nhash + 1) * PTR_SZ; 	//计算位置
	if((db->idxoff = lseek(db->idxfd, offset + 1, SEEK_SET)) == -1)
		err_dump("db_rewind: 	lseek error");
}