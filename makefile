CC = gcc
CFLAGS = -c
OBJECTS = mem_sim.o main.o

app: $(OBJECTS)
	$(CC) $(OBJECTS) -Wall -o app
	
clean:
	rm $(OBJECTS)
	
mem_sim.o: mem_sim.c mem_sim.h
	$(CC) $(CFLAGS) mem_sim.c mem_sim.h

main.o: MAIN.c
	$(CC) $(CFLAGS) MAIN.c

