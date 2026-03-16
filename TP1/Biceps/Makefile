biceps-debug: biceps.c
	cc -o biceps-debug biceps.c -Wall -Werror -lreadline -g

biceps-valgrind: biceps-debug
	valgrind --leak-check=full ./biceps-debug
