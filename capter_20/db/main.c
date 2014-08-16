#include "apue.h"
#include "apue_db.h"

int main()
{
	DBHANDLE db ;
	if((db = db_open("db", O_RDWR  | O_CREAT , FILE_MODE)) == NULL)
		err_sys("db_open error");
	char * ptr = db_fetch(db,"beta");
	printf(ptr);
	db_close(db);
	return 0;
}