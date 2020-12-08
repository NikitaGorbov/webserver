all: bin bin/client bin/server
	
bin:
	mkdir bin
	mkdir resource

bin/client: src/client.c
	gcc src/client.c -o bin/client

bin/server: src/server.c
	gcc src/server.c -o bin/server

clean:
	rm bin/client
	rm bin/server
	rmdir bin
	rmdir resource
