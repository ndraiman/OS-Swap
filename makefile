CC = gcc
CFLAGS = -c
OBJECTS = mem_sim.o main.o

app: $(OBJECTS)
	$(CC) $(OBJECTS) -o app
	
clean:
	rm $(OBJECTS)
	
mem_sim.o: mem_sim.c mem_sim.h
	$(CC) $(CFLAGS) mem_sim.c mem_sim.h

main.o: main.c
	$(CC) $(CFLAGS) main.c

