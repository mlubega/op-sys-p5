CC   = gcc
OPTS = -Wall

all: server client

# this generates the target executables
server: server.o udp.o
	$(CC) -o server server.o udp.o -g 

client: client.o udp.o libmfs.so 
	$(CC) -lmfs -L. -o client client.o udp.o -g 


libmfs.so: mfs.o udp.o 
	$(CC) -c -fpic mfs.c udp.c -Wall -Werror -g
	$(CC) -shared -o libmfs.so mfs.o udp.o -g


# this is a generic rule for .o files 
%.o: %.c 
	$(CC) $(OPTS) -c $< -o $@ -g

clean:
	rm -f *.o *.so server client



