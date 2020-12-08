#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>

enum errors {
    OK,
    ERR_INCORRECT_ARGS,
    ERR_SOCKET,
    ERR_SETSOCKETOPT,
    ERR_BIND,
    ERR_LISTEN
};

enum typesOfFiles {
    FILE_BINARY,
    FILE_MULTIMEDIA,
    FILE_TEXT
};

int init_socket(int port) {
    //open socket, return socket descriptor
    int server_socket = socket(PF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) { perror("Fail: open socket");
        _exit(ERR_SOCKET);
    }

    //set socket option
    int socket_option = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &socket_option, (socklen_t) sizeof(socket_option));
    if (server_socket < 0) {
        perror("Fail: set socket options");
        _exit(ERR_SETSOCKETOPT);
    }

    //set socket address
    struct sockaddr_in server_address; server_address.sin_family = AF_INET; server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_socket, (struct sockaddr *) &server_address, (socklen_t) sizeof(server_address)) < 0) {
        perror("Fail: bind socket address");
        _exit(ERR_BIND);
    }

    //listen mode start
    if (listen(server_socket, 5) < 0) {
        perror("Fail: bind socket address");
        _exit(ERR_LISTEN);
    }
    return server_socket;
}

char *get_word(int socket, int *endOfLineFlag) {
    char *word = NULL;
    char c;
    int i = 1;

    do {
        //c = getchar();
        if (read(socket, &c, 1) <= 0) {
            printf("Client is unreachable\n");
            free(word);
            exit(1);
        }
        
        word = realloc(word, sizeof(char *) * (i + 1));
        if (!word) {
            printf("Realloc failed\n");
            free(word);
            exit(1);
        }
            word[i - 1] = c;
        
        i++;
    } while (c != ' ' && c != '\n' && c != 13);
    // Set end of line flag if the last character was "\n"
    *endOfLineFlag = (word[i - 2] == '\n');
    // Null terminate the word
    word[i - 2] = 0;
    return word;
}

char **get_list(int socket) {
    char **list = NULL;
    int i = 1, flag = 0;

    do {
        list = realloc(list, sizeof(char *) * (i + 1));
        if (!list) {
            printf("Realloc failed\n");
            free(list);
            exit(1);
        }
        list[i - 1] = get_word(socket, &flag);
        i++;
    } while (!flag);
    // Add NULL pointer at the end of the command for further execution
    list[i - 1] = NULL;
    return list;
}

int file_exists(char *path) {
    FILE *fd = fopen(path, "r");
    if (!fd) {
        return 0;
    }
    fclose(fd);
    printf("exists!\n");
    return 1;
}

int get_file_size(char *path) {
    char buf;
    int size = 0;
    FILE *file;
    file = fopen(path, "r");
    if (!file) {
        printf("Could not open file\n");
        return -1;
    }
    while (fgetc(file) != EOF) {
        size++;
    }
    fclose(file);
    return size;
}

int filetype(char *filename) {
    if (!filename ) {
        return -1;
    }
    int i;
    for (i = 0; filename[i]; i++) {
        if (filename[i] == '.') {
            if (!strcmp(filename + i, ".png")) {
                printf("multimedia\n");
                return FILE_MULTIMEDIA;
            }
            if (!strcmp(filename + i, ".html") ||
                !strcmp(filename + i, ".txt")) {
                printf("html/txt\n");
                return FILE_TEXT;
            }
        }
    }
    return FILE_BINARY;
}

void send_text(char *pathToFile, int socket) {
    int filesize = get_file_size(pathToFile);
    char buf;

    char charFilesize[11];
    snprintf(charFilesize, 10, "%d", filesize);

    FILE *fd = fopen(pathToFile, "r");
    printf("HTTP/1.1 200\n\n");
    write(socket, "HTTP/1.1 200\nContent-Length: ", 29);
    write(socket, charFilesize, strlen(charFilesize));
    write(socket, "\n\n", 2);

    buf = fgetc(fd);
    while (buf != EOF) {
        write(socket, &buf, 1);
        buf = fgetc(fd);
    }
    fclose(fd);
}

void send_bin(char *pathToFile, char *filename, int socket) {
    char *binOutput = NULL;
    char buf;
    char charFilesize[11];
    int filesize, i = 0;
    int fd[2];

    pipe(fd);
    if (fork() == 0) {
        dup2(fd[1], 1);
        close(fd[0]);
        close(fd[1]);
        if (execlp(pathToFile, filename, NULL) < 0) {
            printf("Could not execute file\n");
            exit(1);
        }
    }
    close(fd[1]);

    printf("Output:\n");
    while (read(fd[0], &buf, 1) > 0) {
        putchar(buf);
        binOutput = realloc(binOutput, sizeof(char *) * (i + 2));
        binOutput[i] = buf;
        i++;
    }
    binOutput[i] = 0;

    filesize = strlen(binOutput);
    snprintf(charFilesize, 10, "%d", filesize);

    printf("\nsize of output: %d\n", filesize);
    putchar('\n');

    printf("HTTP/1.1 200\n\n");
    write(socket, "HTTP/1.1 200\nContent-Length: ", 29);
    write(socket, charFilesize, strlen(charFilesize));
    write(socket, "\n\n", 2);
    write(socket, binOutput, filesize);
    
    printf("File execution finished\n");

    wait(NULL);
    close(fd[0]);
    if (binOutput) {
        free(binOutput);
    }
}

void analyze_request(char ***request, int socket) {
    char buf = 0;
    char *pathToFile = NULL;

    if (!request[0][0] ||
        strcmp(request[0][0], "GET") ||
        !request[0][2] ||
        strcmp(request[0][2], "HTTP/1.1")) {
        printf("HTTP/1.1 400 Bad Request\n");
        write(socket, "HTTP/1.1 400 Bad Request\n", 25);
        return;
    }
    
    int fileType = filetype(request[0][1]);
    switch (fileType) {
        case FILE_BINARY:
            // ./resource/cgi-bin/
            pathToFile = malloc(sizeof(char) * (20 + strlen(request[0][1])));
            strncpy(pathToFile, "./resource/cgi-bin/", 20);
            strcat(pathToFile, request[0][1]);
            break;
        case FILE_TEXT:
            // ./resource/html/
            pathToFile = malloc(sizeof(char) * (17 + strlen(request[0][1])));
            strncpy(pathToFile, "./resource/html/", 17);
            strcat(pathToFile, request[0][1]);
            break;
    }
    printf("%s\n", pathToFile);

    if (!file_exists(pathToFile)) {
        printf("HTTP/1.1 404\n");
        write(socket, "HTTP/1.1 404\n", 13);
        write(socket, "Content-Length: 38\n\n", 20);
        write(socket, "<html><h1>Page not found!</h1></html>\n", 38);
        if (pathToFile) {
            free(pathToFile);
        }
        return;
    }

    if (fileType == FILE_BINARY) {
        printf("Executing binary file\n");
        send_bin(pathToFile, request[0][1], socket);
        free(pathToFile);
        return;
    }

    if(fileType == FILE_TEXT) {
        send_text(pathToFile, socket);
    }

    free(pathToFile);
}

// Free a string of words
void free_cmd(char **cmd) {
    int i = 0;
    while(cmd[i]) {
        free(cmd[i]);
        i++;
    }
    free(cmd);
}

int main(int argc, char** argv) {
    if (argc != 3) {
        puts("Incorrect args.");
        puts("./server <port> <n>");
        puts("Example:"); puts("./web-server 5000 1");
        return ERR_INCORRECT_ARGS;
    }
    int n = atoi(argv[2]);
    int strings, j;
    int port = atoi(argv[1]);
    int server_socket = init_socket(port);
    struct sockaddr_in client_address;
    socklen_t size = sizeof(client_address);
    int client_socket[n];
    char ***request = NULL;
    puts("Wait for connection");
    for (int i = 0; i < n; i++) {
        client_socket[i] = accept(server_socket, (struct sockaddr *) &client_address, &size);
        printf("connected: %s %d\n", inet_ntoa(client_address.sin_addr),
                ntohs(client_address.sin_port));

        if (fork() == 0) {
            while (1) {
                strings = 0;
                while (1) {
                    request = realloc(request, sizeof(char**) * (strings + 1));
                    request[strings] = get_list(client_socket[i]);
                    if (strlen(request[strings][0]) == 0 ||
                            request[strings][0][0] == '\r') {
                        break;
                    }
                    strings++;
                }

                analyze_request(request, client_socket[i]);

                j = 0;
                while (j <= strings) {
                    free_cmd(request[j]);
                    j++;
                }
            }
        }
    }
    for (int i = 0; i < n; i++) {
        wait(NULL);
        close(client_socket[i]);
    }
    free(request);
    return OK;
}
