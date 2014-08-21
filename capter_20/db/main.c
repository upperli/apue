#include "apue.h"
#include "apue_db.h"

int main(int agrc,char *agrv[])
{
	DBHANDLE db ;
	char key[1000],data[1000];
	char  * result;
	int mode;
	int open_mode = O_RDWR  | O_CREAT;
	if(agrc == 2)
		if(agrv[1][0] == '1' )
			open_mode |= O_TRUNC;
	if((db = db_open("db", open_mode , FILE_MODE)) == NULL)
		err_sys("db_open error");

	//	fflush(stdout);
	//	sync();

	while(1){
		printf("0:store  1:fetch  2:delete  3:rewind  	4：next 	5: all 6:close 7:store again and again\n");
		int i,x = 1000000;
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
					err_msg("db_store error for %s", key);
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
					err_ret("db_store error for %s", key);
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
			case 7:
				while(x -- ){
					//sleep(1);
					sprintf(key,"%d",x);
					//strcpy(key,"template-XXXXXX");
					//mktemp(key);
					
					if(db_store(db,key,key,3) != 0)
						err_msg("db_store error for %s", key);
				}
		
				return 0;
		}
	}

	return 0;
}