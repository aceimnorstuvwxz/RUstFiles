app: main.o payload.o
	gcc -o app main.o payload.o -levent

main.o: main.c
	gcc -c main.c

payload.o: payload.c
	gcc -c payload.c