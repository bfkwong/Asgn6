all: mush clean 

mush: mush.o parseline.o
	gcc -Wall -g -o mush mush.o parseline.o
mush.o: mush.c 
	gcc -Wall -g -c mush.c
parseline.o: parseline.c
	gcc -Wall -g -c parseline.c
clean: 
	rm parseline.o mush.o
