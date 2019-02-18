all:
	gcc -Wall -Wextra shell.c -o 550shell -lreadline

run: all
	./shell
