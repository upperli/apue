all: apue_err.o lock.o
	ar rc libapue.a apue_err.o lock.o
	cp libapue.a copy/
	cp apue.h copy/
apue_err.o:apue_err.c
lock.o:lock.c