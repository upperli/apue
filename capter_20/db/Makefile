all:db.o main.o
	ar rc 	libdb.a 	db.o libapue.a
	gcc -o main main.o -L.  -ldb -lapue
db.o:db.c

test:main.o
	gcc -o main main.o -L.  -ldb -lapue
main.o:main.c
