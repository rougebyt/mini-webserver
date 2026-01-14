// mini-webserver.c - Tiny epoll-based HTTP/1.1 server (~420 LOC)
// Compile: make
// Run: ./mini-webserver [port] [root_dir]

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/sendfile.h>

#define MAX_EVENTS 1024
#define BUFFER_SIZE 8192
#define DEFAULT_PORT 8080
#define DEFAULT_ROOT "public"

typedef struct {
    int fd;
    char *buffer;
    size_t len;
    size_t pos;
    int state; // 0: reading, 1: writing, 2: done
} Client;

static const char *get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".css")  == 0) return "text/css";
    if (strcmp(ext, ".js")   == 0) return "application/javascript";
    if (strcmp(ext, ".png")  == 0) return "image/png";
    if (strcmp(ext, ".jpg")  == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif")  == 0) return "image/gif";
    if (strcmp(ext, ".txt")  == 0) return "text/plain";
    return "application/octet-stream";
}

static void send_response(int fd, const char *status, const char *content_type, const char *body, size_t body_len) {
    char header[BUFFER_SIZE];
    time_t now;
    time(&now);
    char date[128];
    strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));

    snprintf(header, sizeof(header),
             "HTTP/1.1 %s\r\n"
             "Server: mini-webserver/0.1\r\n"
             "Date: %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n",
             status, date, content_type, body_len);

    send(fd, header, strlen(header), 0);
    if (body_len > 0) {
        send(fd, body, body_len, 0);
    }
}

static void handle_request(Client *client, const char *root) {
    char method[16], path[512], protocol[16];
    if (sscanf(client->buffer, "%15s %511s %15s", method, path, protocol) != 3) {
        send_response(client->fd, "400 Bad Request", "text/plain", "Bad Request", 11);
        client->state = 2;
        return;
    }

    if (strcmp(method, "GET") != 0) {
        send_response(client->fd, "405 Method Not Allowed", "text/plain", "Method Not Allowed", 19);
        client->state = 2;
        return;
    }

    // Normalize path
    char full_path[1024];
    if (strcmp(path, "/") == 0) {
        snprintf(full_path, sizeof(full_path), "%s/index.html", root);
    } else {
        snprintf(full_path, sizeof(full_path), "%s%s", root, path);
    }

    // Prevent directory traversal
    if (strstr(full_path, "../") != NULL) {
        send_response(client->fd, "403 Forbidden", "text/plain", "Forbidden", 9);
        client->state = 2;
        return;
    }

    struct stat st;
    if (stat(full_path, &st) == -1) {
        const char *not_found = "<h1>404 Not Found</h1>";
        send_response(client->fd, "404 Not Found", "text/html", not_found, strlen(not_found));
        client->state = 2;
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        const char *dir_msg = "<h1>Directory listing not supported</h1>";
        send_response(client->fd, "403 Forbidden", "text/html", dir_msg, strlen(dir_msg));
        client->state = 2;
        return;
    }

    int file_fd = open(full_path, O_RDONLY);
    if (file_fd == -1) {
        const char *err = "<h1>500 Internal Server Error</h1>";
        send_response(client->fd, "500 Internal Server Error", "text/html", err, strlen(err));
        client->state = 2;
        return;
    }

    char *content_type = (char *)get_mime_type(full_path);

    char header[BUFFER_SIZE];
    time_t now; time(&now);
    char date[128];
    strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));

    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Server: mini-webserver/0.1\r\n"
             "Date: %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %lld\r\n"
             "Connection: close\r\n"
             "\r\n",
             date, content_type, (long long)st.st_size);

    send(client->fd, header, strlen(header), 0);

    // Use sendfile for efficiency
    off_t offset = 0;
    while (offset < st.st_size) {
        ssize_t sent = sendfile(client->fd, file_fd, &offset, st.st_size - offset);
        if (sent <= 0) break;
    }

    close(file_fd);
    client->state = 2; // done
}

int main(int argc, char *argv[]) {
    int port = (argc > 1) ? atoi(argv[1]) : DEFAULT_PORT;
    const char *root = (argc > 2) ? argv[2] : DEFAULT_ROOT;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        return 1;
    }

    if (listen(server_fd, SOMAXCONN) == -1) {
        perror("listen");
        return 1;
    }

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        return 1;
    }

    struct epoll_event ev = {0};
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);

    printf("mini-webserver listening on http://0.0.0.0:%d/ (root: %s)\n", port, root);

    struct epoll_event events[MAX_EVENTS];

    while (1) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == server_fd) {
                // Accept new connection
                int client_fd = accept(server_fd, NULL, NULL);
                if (client_fd == -1) continue;

                fcntl(client_fd, F_SETFL, O_NONBLOCK);

                Client *client = calloc(1, sizeof(Client));
                client->fd = client_fd;
                client->buffer = malloc(BUFFER_SIZE);
                client->len = 0;
                client->pos = 0;
                client->state = 0;

                ev.events = EPOLLIN | EPOLLET;
                ev.data.ptr = client;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
            } else {
                Client *client = (Client*)events[i].data.ptr;
                int done = 0;

                if (events[i].events & EPOLLIN) {
                    ssize_t count;
                    while ((count = read(client->fd, client->buffer + client->len, BUFFER_SIZE - client->len - 1)) > 0) {
                        client->len += count;
                        client->buffer[client->len] = '\0';

                        if (strstr(client->buffer, "\r\n\r\n")) {
                            handle_request(client, root);
                            done = 1;
                            break;
                        }
                    }
                    if (count == 0 || (count == -1 && errno != EAGAIN)) {
                        done = 1;
                    }
                }

                if (done || (events[i].events & (EPOLLERR | EPOLLHUP))) {
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client->fd, NULL);
                    close(client->fd);
                    free(client->buffer);
                    free(client);
                }
            }
        }
    }

    return 0;
}
