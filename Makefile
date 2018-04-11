all:
	clang-3.5 -std=c99 -Wall -pedantic *.c -L. -lruntime