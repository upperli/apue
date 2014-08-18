#include "apue.h"
#include "apue_db.h"
#include <stdarg.h>
#include <errno.h>
#include <sys/uio.h>   //使用iovec


#define 	IDXLEN_SZ 	4	//用四个字节来储存一条索引记录的长度
#define 	SEP 			':'	//索引记录分隔符
#define 	SPACE 		' '	//空格
#define	NEWLINE 		'\n' //换行符



#define PTR_SZ		6		//哈希链表中指针域的大小
#define	PTR_MAX 		999999	//最大文件偏移量为 10^PTR_SZ-1
#define	NHASH_DEF	137 		//默认哈希表大小
#define 	HASH_OFF		PTR_SZ 	//索引文件中哈希表的偏移量
#define 	FREE_OFF 	0


typedef	unsigned long DBHASH;
typedef unsigned long COUNT;

typedef struct {

	char * name;		//db的名字

/*
*索引文件相关
*/
	int 	idxfd;		//索引文件的fd

	
	char *idxbuf;	//索引文件缓存
	off_t 	idxoff;	//索引文件记录的偏移量，遍历时用

	off_t	hashoff;//索引文件中散列表的偏移
	DBHASH	nhash;	//当前散列表大小

	off_t	chainoff;//链表的位置
	off_t 	ptroff;//上条记录位置

	off_t 	ptrval;//下一条索引记录偏移量
	off_t	idxlen;//索引记录长度

/*
* 	数据文件相关
*/
	int datfd;		//数据文件的fd
	char *datbuf;	//数据文件缓存
	off_t 	datoff;	//数据文件记录的偏移量
	off_t 	datlen; //数据长度

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
static	int 		 _db_find_and_lock(DB *db, const char *key/*关键字*/, int writelock/*非0是写锁，0是读锁*/);
static	DBHASH 	 _db_hash(DB *db, const char *key);
static	off_t 	 _db_readptr(DB *db, off_t offset/*哈希表上的位置*/);
static 	off_t	 _db_readidx(DB *db, off_t offset/*索引记录的偏移量*/);
static 	char 	*_db_readdat(DB * db);	//读取数据
static 	int 		 _db_findfree(DB *db, int keylen, int datlen);
static	void 	 _db_writeptr(DB *db, off_t offset, off_t ptrval);
static 	void 	 _db_writedat(DB *db, const char *data, off_t offset, int whence);

static 	void 	 _db_writeidx(DB *db, const char *key, off_t offset, int whence, off_t ptrval);
static 	void 	 _db_dodelete(DB *db);

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

		if(writew_lock(db->idxfd , 0 , SEEK_SET, 0) < 0)//加锁   len=0 表示一直到EOF，相当于从当前位置锁住后面所有的
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

			i = strlen(hash);  //i变成了hash字符串长度
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
static void _db_free(DB *db)
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




/*fetch*/
char * db_fetch(DBHANDLE	h, const char * key)
{
	DB *db = h;
	char * ptr;
	if(_db_find_and_lock(db, key, 0) < 0)//这个函数会给记录加锁
	{//记录未找到
		ptr = NULL;
		++db->cnt_fetcherr;
	}else{
		ptr = _db_readdat(db);
		++db->cnt_fetchok;
	}

	if(un_lock(db->idxfd, db->chainoff, SEEK_SET, 1) < 0)
		err_dump("db_fetch: unlock error");
	return ptr;
}

/*
* 	加锁查找
*	chainoff更新成这个关键字所在哈希表位置 hash（key)
*	ptroff 是记录地址的位置
* 	返回值表示是否找到，0表示找到，-1表示找不到
*/
static int _db_find_and_lock(DB *db, const char *key, int writelock)
{
	off_t	offset, nextoffset;

	db->chainoff = (_db_hash(db, key) * PTR_SZ) + db->hashoff;
	db->ptroff = db->chainoff;

	if(writelock)//写锁
	{
		if(writew_lock(db->idxfd, db->chainoff, SEEK_SET, 1) < 0)
			err_dump("_db_find_and_lock: writew_lock error");
	}else{//读锁
		if(readw_lock(db->idxfd, db->chainoff, SEEK_SET, 1) < 0)
			err_dump("_db_find_and_lock: readw_lock error");
	}

	offset = _db_readptr(db, db->ptroff);//获取链表第一条记录地址

	while(offset != 0)
	{
		nextoffset = _db_readidx(db, offset);//读索引文件，获取下一条记录位置，idxbuf结构： 关键字 null 数据偏移 null 数据长度null
		if(strcmp(db->idxbuf, key) == 0)
			break;	//找到关键字匹配
		db->ptroff = offset;//ptroff是上一条地址，储存当前记录指针的位置
		offset = nextoffset;
	}

	return (offset == 0 ? -1 : 0);

}


//计算关键字的哈希值
static	DBHASH _db_hash(DB *db, const char *key)
{
	/*
	*把字符串每一位的ascii乘以它的位置相加，最后取模
	*/

	DBHASH hval = 0;
	char c;
	int i;
	for (i = 1; (c = *key++); ++i)
	{
		hval += c * i;
	}
	return (hval % db->nhash);
}

//读取链表地址，不修改db
static off_t _db_readptr(DB *db, off_t offset)
{
	char asciiptr[PTR_SZ + 1];
	
	if(lseek(db->idxfd, offset, SEEK_SET) == -1)
		err_dump("_db_readptr: lseek error to ptr field");
	if(read(db->idxfd, asciiptr, PTR_SZ) != PTR_SZ)
		err_dump("_db_readptr: read error of ptr field");
	
	asciiptr[PTR_SZ] = 0;//保证最后一位是null
	return(atol(asciiptr));//把字符串转化成数字
}

/*
*	给定记录偏移量，返回下一条记录地址，同时读取这条记录信息储存在db中
*/
static off_t	_db_readidx(DB *db, off_t offset)
{
	/*
	*一条索引记录的结构：
	*1.链表指针 2.索引记录长度（只包含后面的项） 3.键 4.分隔符 5.数据记录偏移 6.记录长度 7.\n
	*/
	ssize_t 	i;
	char 	*ptr1, *ptr2;
	char 	asciiptr[PTR_SZ + 1],asciilen[IDXLEN_SZ + 1];

	struct iovec		iov[2];



	if((db->idxoff = lseek(db->idxfd, offset, offset == 0 ? SEEK_CUR : SEEK_SET)) == -1)
		err_dump("_db_readidx: lseek error");


	//散布读操作，这样就能一次读两块内容
	iov[0].iov_base = asciiptr;//获取下一条记录指针
	iov[0].iov_len = PTR_SZ;
	iov[1].iov_base = asciilen;//记录长度
	iov[1].iov_len = IDXLEN_SZ;

	if((i = readv(db->idxfd, &iov[0], 2)) != PTR_SZ + IDXLEN_SZ)
	{
		if(i == 0 && offset == 0)
			return -1;//EOF
		err_dump("_db_readidx: readv error of index record");
	}

	asciiptr[PTR_SZ] = 0;
	db->ptrval = atol(asciiptr);

	asciilen[IDXLEN_SZ] = 0;

	if((db->idxlen = atoi(asciilen)) < IDXLEN_MIN || db->idxlen > IDXLEN_MAX)
		err_quit("_db_readidx: invalid length");//记录长度错误

	if ((i = read(db->idxfd, db->idxbuf, db->idxlen)) != db->idxlen)
		err_dump("_db_readidx: read error of index record");

	if(db->idxbuf[db->idxlen -1] != NEWLINE)
		err_quit("_db_readidx: missing newline");

	db->idxbuf[db->idxlen -1] = 0;


	if((ptr1 = strchr(db->idxbuf, SEP)) == NULL)
		err_quit("_db_readidx: missing first separator");
	*ptr1++ = 0;//将分隔符换成了null，这样字符串idxbuf就是关键字
	/*
	* *ptr1 = 0;
	* ptr1 ++;
	*/

	if((ptr2 = strchr(ptr1 , SEP)) == NULL)
		err_quit("_db_readidx: missing second separator");
	*ptr2++ = 0;

	if(strchr(ptr2 , SEP) != NULL)
		err_quit("_db_readidx: too many separators");

	if((db->datoff = atol(ptr1)) < 0)//第一个分隔符后是数据偏移
		err_quit("_db_readidx: starting offset < 0");
	if((db->datlen = atol(ptr2)) <= 0 || db->datlen > DATLEN_MAX)//第二个分割符后是记录长度
		err_quit("_db_readidx: data record invalid length");

	return db->ptrval;//返回的是下一条记录偏移
}



//读取数据文件
static 	char *_db_readdat(DB * db)
{
	if (lseek(db->datfd, db->datoff, SEEK_SET) == -1)
		err_dump("_db_readdat: lseek error");
	if(read(db->datfd, db->datbuf, db->datlen) != db->datlen)
		err_dump("_db_readdat: read error");
	if(db->datbuf[db->datlen-1] != NEWLINE)
		err_dump("_db_readdat: missing newline");
	db->datbuf[db->datlen-1] = 0;
	return db->datbuf;
}


//增加数据
int 	db_store(DBHANDLE h, const char * key, const char *data, int flag)
{
	DB *db = h;
	int rc, keylen, datlen;
	off_t ptrval;

	if(flag != DB_INSERT && flag != DB_REPLACE && flag != DB_STORE)
	{
		errno = EINVAL;
		return -1;
	}

	keylen = strlen(key);
	datlen = strlen(data) + 1;

	if(datlen < DATLEN_MIN || datlen > DATLEN_MAX)
		err_dump("db_store: invalid data length");

	if(_db_find_and_lock(db, key, 1) < 0)//加了写锁
	{							
		/*
		*	如果数据库里没有这个关键字
		* 	可以用DB_INSERT和DB_STOR但是DB_REPLACE不合法
		*/
		if(flag == DB_REPLACE)
		{
			//因为不存在，所以替换不合法
			rc = -1;
			++db->cnt_storerr;
			errno = ENOENT;
			goto doreturn;
		}

		//因为没有找到，索引ptroff的值无意义
		ptrval = _db_readptr(db, db->chainoff);
		//ptrval 是链表的偏移


		if(_db_findfree(db, keylen, datlen) < 0)//使用最佳适配原则找空间，指其他记录删除释放的空间
		{
			//没有找到，记录写到文件结尾
			_db_writedat(db, data, 0, SEEK_END);
			_db_writeidx(db, key, 0, SEEK_END, ptrval);
			_db_writeptr(db, db->chainoff, db->idxoff);
			++db->cnt_stor1;

		}else{
			//找到了 写道相应位置
			_db_writedat(db, data, db->datoff, SEEK_SET);
			_db_writeidx(db, key, db->idxoff, SEEK_SET,ptrval);
			_db_writeptr(db, db->chainoff, db->idxoff);
			++db->cnt_stor2;
		}
	}else{//数据库里存在这个关键字

		if(flag == DB_INSERT)//不能插入，只能替换
		{
			rc = 1;
			++db->cnt_storerr;
			goto doreturn;
		}

		if(datlen != db->datlen)//判断数据长度是否相等
		{
			//不相等，要删除原先的记录
			_db_dodelete(db);
			ptrval = _db_readptr(db, db->chainoff);
			_db_writedat(db, data, 0, SEEK_END);
			_db_writeidx(db, key, 0, SEEK_END, ptrval);
			_db_writeptr(db, db->chainoff, db->idxoff);

			++db->cnt_stor3;

		}else{
			//相等的情况，直接改数据文件
			_db_writedat(db, data, db->datoff,SEEK_SET);
			++db->cnt_stor4;
		}
	}

	rc = 0;
doreturn:
	if(un_lock(db->idxfd, db->chainoff, SEEK_SET, 1) < 0)
		err_dump("db_store: un_lock error");
	return rc;
}

//找空闲位置
static int _db_findfree(DB *db, int keylen, int datlen)
{
	int rc;
	off_t offset, nextoffset, saveoffset;

	if(writew_lock(db->idxfd, FREE_OFF, SEEK_SET, 1) < 0)//加写锁
		err_dump("_db_findfree: writew_lock error");
	
	saveoffset = FREE_OFF;//FREE_OFF 就是空闲地址指针的位置

	offset = _db_readptr(db, saveoffset);//offset是空闲地址指针

	//最佳适配原则找位置,结果保存在offset中，saveoffset是前一个地址
	while(offset != 0)
	{
		nextoffset = _db_readptr(db, offset);//空闲地址上读取下一个空闲地址的指针
		if(strlen(db->idxbuf) == keylen && db->datlen == datlen)//关键字长度和数据长度是否匹配
			break;

		saveoffset = offset;
		offset = nextoffset;
	}


	if(offset == 0)
	{
		rc = -1;
	}else{
		_db_writeptr(db, saveoffset, db->ptrval);//在将offset从链表中除掉
		rc = 0;
	}

	if(un_lock(db->idxfd, FREE_OFF, SEEK_SET, 1) < 0)//解锁
		err_dump("_db_findfree： un_lock error");

	return rc;

}

//将数据写入索引记录的指针域，将parval写到offset位置
static void _db_writeptr(DB *db, off_t offset, off_t ptrval)
{
	char asciiptr[PTR_SZ + 1];

	if(ptrval < 0 || ptrval > PTR_MAX)
		err_quit("_db_writeptr: invalid ptr: %d", ptrval);//指针不合法，退出
	
	sprintf(asciiptr, "%*ld", PTR_SZ, ptrval);
 
	if(lseek(db->idxfd, offset, SEEK_SET) == -1)
		err_dump("_db_writeptr: lseek error to ptr field");
	if(write(db->idxfd, asciiptr, PTR_SZ) != PTR_SZ)
		err_dump("_db_writeptr: write error of ptr field");
}


/*
*	加锁部分有疑问，为何往非结尾部分写不用加锁？因为不能被访问到？
* 	对文件加锁不影响其他用户读，因为读是在索引记录上加锁，其他用户写记录的时候，如果不是往文件末尾添加也不会有冲突，
* 	但是同时的话会有冲突，因为lseek与write之间不是原子操作，必须要加锁，以后尝试用pwrite试试，貌似有pwritev操作
*/
static void _db_writedat(DB *db, const char *data, off_t offset, int whence)
{
	struct iovec		iov[2];

	static char newline = NEWLINE;

	if(whence == SEEK_END)//如果往最后追加时，加锁，否则不用加锁，
		if(writew_lock(db->datfd, 0, SEEK_SET, 0) < 0)//锁住整个文件
			err_dump("_db_writedat: writew_lock error");

	if((db->datoff = lseek(db->datfd, offset, whence)) == -1)
		err_dump("_db_writedat: lseek error");
	db->datlen = strlen(data) + 1;

	iov[0].iov_base = (char *) data;
	iov[0].iov_len = db->datlen - 1;
	iov[1].iov_base = &newline;
	iov[1].iov_len = 1;

	if(writev(db->datfd, &iov[0], 2) != db->datlen)
		err_dump("_db_writedat: writev error of data record");

	if(whence == SEEK_END)
		if(un_lock(db->datfd, 0, SEEK_SET, 0) < 0)
			err_dump("_db_writedat： un_lock error");
}

/*
*   需要db提供正确的数据地址偏移，正确的数据空间长度
*/
static void _db_writeidx(DB *db, const char *key, off_t offset, int whence, off_t ptrval)
{
	struct iovec 	iov[2];
	char 	asciiptr[PTR_SZ + IDXLEN_SZ + 1];
	int len;
	char 	*fmt;
	if((db->ptrval = ptrval) < 0 || ptrval > PTR_MAX)
		err_quit("_db_writeidx: invalid ptr: %d", ptrval);
	
	//生成记录
	if(sizeof(off_t) == sizeof(long long))
		fmt = "%s%c%lld%c%d\n";
	else
		fmt = "%s%c%ld%c%d\n";
	sprintf(db->idxbuf, fmt, key, SEP, db->datoff, SEP, db->datlen);

	//计算长度
	if((len = strlen(db->idxbuf)) < IDXLEN_MIN || len > IDXLEN_MAX)
		err_quit("_db_writeidx: invalid length");
	sprintf(asciiptr, "%*ld%*d", PTR_SZ, ptrval, IDXLEN_SZ, len);

	if(whence == SEEK_END)//加锁，原因见_db_writedat
		if(writew_lock(db->idxfd, ((db->nhash + 1)*PTR_SZ) + 1, SEEK_SET, 0) < 0)
			err_dump("_db_writeidx: writew_lock error");



	if((db->idxoff = lseek(db->idxfd, offset, whence)) == -1)
		err_dump("_db_writeidx: lseek error");
	iov[0].iov_base = asciiptr;
	iov[0].iov_len = PTR_SZ + IDXLEN_SZ;
	iov[1].iov_base = db->idxbuf;
	iov[1].iov_len = len;

	if(writev(db->idxfd, &iov[0], 2) != PTR_SZ + IDXLEN_SZ + len)
		err_dump("_db_writeidx: writev error");

	if(whence == SEEK_END)
		if(un_lock(db->idxfd, ((db->nhash + 1)*PTR_SZ) + 1, SEEK_SET, 0) < 0)
			err_dump("_db_writeidx: writew_lock error");
}

/*
*	db: datoff是要删除数据的地址，idxoff是当前记录的偏移，idxbuf是索引记录中间用null间隔，ptrval是下一条索引记录的偏移
*/
static void  _db_dodelete(DB *db)
{
	int i;
	char *ptr;
	off_t freeptr, saveptr;

	for(ptr = db->datbuf, i = 0; i < db->datlen - 1; ++i)
		*ptr++ = SPACE;
	*ptr = 0;
	ptr = db->idxbuf;
	while(*ptr)
		*ptr++ = SPACE;

	if(writew_lock(db->idxfd, FREE_OFF, SEEK_SET, 1) < 0)
		err_dump("_db_dodelete: writew_lock error");

	_db_writedat(db, db->datbuf, db->datoff, SEEK_SET);//将数据部分全部写成空格

	freeptr = _db_readptr(db, FREE_OFF);//freeptr是空闲地址链表位置
	saveptr = db->ptrval;//保存链表指针

	/*
	*当前记录改成：freeptr，idxlen，原先key长度的空格，：，数据偏移，：数据长度，\n
	*/
	_db_writeidx(db, db->idxbuf, db->idxoff, SEEK_SET, freeptr);

	_db_writeptr(db, FREE_OFF, db->idxoff);//将该记录假如空闲链表
	
	_db_writeptr(db, db->ptroff, saveptr);//上条记录接上下条记录



	if(un_lock(db->idxfd, FREE_OFF, SEEK_SET, 1) < 0)
		err_dump("_db_dodelete: un_lock error");
}


//删除记录
int db_delete(DBHANDLE h, const char *key)
{
	DB *db = h;
	int rc = 0;
	if(_db_find_and_lock(db, key, 1) == 0)
	{
		_db_dodelete(db);
		++db->cnt_delok;
	}else{
		rc = -1;
		++db->cnt_delerr;
	}

	if(un_lock(db->idxfd, db->chainoff, SEEK_SET, 1) < 0)
		err_dump("db_delete: unlock error");
	return rc;
}
//下一条记录
char * db_nextrec(DBHANDLE h, char *key)
{
	DB *db = h;
	char c;
	char *ptr;
	if(readw_lock(db->idxfd, FREE_OFF, SEEK_SET, 1) < 0)
		err_dump("db_nextrec: readw_lock error");
	do{
		if(_db_readidx(db, 0) < 0)
		{
			ptr = NULL;
			goto doreturn;
		}
		ptr = db->idxbuf;
		while((c = *ptr++) != 0 && c == SPACE);//可否改成while(*ptr++ == SPACE);
	}while(c == 0);

	if(key != NULL)
		strcpy(key, db->idxbuf);
	ptr = _db_readdat(db);
	++db->cnt_nextrec;
doreturn:
	if(un_lock(db->idxfd, FREE_OFF, SEEK_SET, 1) < 0)
		err_dump("db_nextrec: un_lock error");
	return ptr;
}