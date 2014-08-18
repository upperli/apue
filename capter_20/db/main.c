#include "apue.h"
#include "apue_db.h"

int main()
{
	DBHANDLE db ;
	char key[1000],data[1000];
	char  * result;
	int mode;
	if((db = db_open("db", O_RDWR  | O_CREAT  , FILE_MODE)) == NULL)
		err_sys("db_open error");

	while(1){
		printf("0:store  1:fetch  2:delete  3:rewind  	4：next 		5: all 6:close\n");
		int i;
		scanf("%d",&i);
		switch(i){
			case 0:
				printf("key:\n");
				scanf(" %s",key);
				printf("data:\n");
				scanf(" %s",data);
				printf("mode:\n");
				scanf("%d",&mode);
				if(db_store(db,key,data,mode) != 0)
					err_quit("db_store error for %s", key);
				break;
			case 1:
				printf("key:\n");
				scanf(" %s",key);
				if((result = db_fetch(db,key)) == NULL)
					printf("NULL\n");
				else
					printf("%s\n", result);
				break;			
			case 2:
				printf("key:\n");
				scanf(" %s",key);
				if(db_delete(db,key) != 0)
					err_quit("db_store error for %s", key);
				break;	
			case 3:
				db_rewind(db);
				break;
			case 4:
				if((result = db_nextrec(db, key))  == NULL)
					printf("NULL\n");
				else
					printf("%s   %s\n", key, result);
				break;	
			case 5:
				db_rewind(db);
				while((result = db_nextrec(db, key))  != NULL)
					printf("%s   %s\n", key, result);
				break;	
			case 6:
				db_close(db);
				return 0;
		}
	}

	return 0;
}