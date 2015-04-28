all:nets

nets:main.c
	gcc main.c -o nets -std=c99
	
main.c: