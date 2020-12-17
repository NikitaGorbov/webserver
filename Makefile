all: bin bin/client bin/server
	
bin:
	mkdir bin
	gcc resource/cgi-source/hello.c -o resource/cgi-bin/hello

bin/client: src/client.c
	gcc src/client.c -o bin/client

bin/server: src/server.c
	gcc src/server.c -o bin/server

clean:
	rm bin/client
	rm bin/server
	rm resource/cgi-bin/*
	rmdir bin
