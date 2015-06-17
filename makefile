CC = gcc
CFLAGS = -c
OBJECTS = mem_sim.o MAIN.o

app: $(OBJECTS)
	$(CC) $(OBJECTS) -Wall -o app
	
clean:
	rm $(OBJECTS)
	
mem_sim.o: mem_sim.c mem_sim.h
	$(CC) $(CFLAGS) mem_sim.c mem_sim.h

MAIN.o: MAIN.c
	$(CC) $(CFLAGS) MAIN.c

