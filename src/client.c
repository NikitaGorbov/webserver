#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wait.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>

enum errors {
    OK,
    ERR_INCORRECT_ARGS,
    ERR_SOCKET,
    ERR_CONNECT
};

int init_socket(const char *ip, int port) {
    printf("Connecting...\n");
    //open socket, result is socket descriptor
    int server_socket = socket(PF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Fail: open socket");
        _exit(ERR_SOCKET);
    }

    //prepare server address
    struct hostent *host = gethostbyname(ip);
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    memcpy(&server_address.sin_addr, host -> h_addr_list[0], sizeof(server_address.sin_addr));

    //connection
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    memcpy(&sin.sin_addr, host->h_addr_list[0], sizeof(sin.sin_addr));
    if (connect(server_socket, (struct sockaddr*) &sin, (socklen_t) sizeof(sin)) < 0) {
        perror("Fail: connect");
        _exit(ERR_CONNECT);
    }
    printf("Connected\n");
    return server_socket;
}

char *read_word() {
    char *word = NULL;
    char c;
    int i = 1;
    while(1) {
        c = getchar();
        if (c != '\n') {
            word = realloc(word, sizeof(char) * (i + 1));
            word[i - 1] = c;
            i++;
        } else {
            break;
        }
    }
    if (i > 1) {
        word[i - 1] = 0;
    }
    return word;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        puts("Incorrect args.");
        puts("./telnet <ip> <port>");
        puts("Example:");
        puts("./telnet 127.0.0.1 80");
        return ERR_INCORRECT_ARGS;
    }
    int port = atoi(argv[2]);
    int server = init_socket(argv[1], port);    // (ip, port)
    char *input = NULL;
    char *request = NULL;
    char c;
    int i = 0;

    if (fork() == 0) {
        while(1) {
            if (read(server, &c, 1) <= 0) {
                printf("Server is down\n");
                exit(1);
            }
            putchar(c);
            fflush(stdout);
        }
    }
    while(1) {
        input = read_word();
        if (!input) {
            printf("Error occured\n");
            break;
        } 

        // 23 is the constant length of other characters in the GET request
        request = realloc(request, sizeof(char) * (23 + strlen(argv[1]) + strlen(input)));
        strncpy(request, "GET ", 5);
        strncat(request, input, strlen(input));
        strncat(request, " HTTP/1.1\nHost: ", 17);
        strncat(request, argv[1], strlen(argv[1]));
        strncat(request, "\n\n", 3);

        write(server, request, strlen(request));

        free(input);
    }
    wait(NULL);
    close(server);
    return OK;
}
