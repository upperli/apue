#include "apue.h"
#include "apue_db.h"

int main()
{
	DBHANDLE db ;
	if((db = db_open("db", O_RDWR  | O_CREAT | O_TRUNC , FILE_MODE)) == NULL)
		err_sys("db_open error");
	db_store(db,"Alpha","data1",DB_INSERT);
	db_store(db,"beta","Data for beta",DB_INSERT);
	db_store(db,"gamma","record3",DB_INSERT);
	char * ptr = db_fetch(db,"beta");
	printf(ptr);
	db_close(db);
	return 0;
}