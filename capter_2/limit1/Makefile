all: main.c libapue.a
	gcc -o limit main.c -L. -lapue

install:getC
	awk -f getC > main.c
