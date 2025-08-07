main: main.c
	cc -Wall -Wextra -g -o main main.c -lm -lraylib
run: main
	st -e ./main
