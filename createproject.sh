#!/bin/bash


path=capter_$1/$2

mkdir -p $path
cp libs/copy/* $path
echo "all:libapue.a main.c" >> $path/Makefile
echo "\tgcc -o main main.c -L. -lapue" >> $path/Makefile