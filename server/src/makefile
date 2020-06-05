server:  server.o socket.o filer.o
	gcc server.o socket.o filer.o -lpthread -o server


server.o: server.c socket.h filer.h
	gcc -c -lpthread server.c

socket.o: socket.c socket.h
	gcc -c socket.c

filer.o: filer.c filer.h
	gcc -c filer.c


.PHONY:clean
clean:
	rm *.o server
