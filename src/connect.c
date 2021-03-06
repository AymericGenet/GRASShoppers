/*
 * connect.c : Connections handler.
 *
 *  Created on: Apr 22, 2019
 *      Author: Aymeric Genêt
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "connect.h"
#include "grass.h"



int accept_sock(int port) {
    int server_fd = 0, new_socket = 0;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    /* Creates socket */
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation error");
        return -1;
    }

    /* Configures socket */
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                   &opt, sizeof(opt)) != 0) {
        perror("Socket option failed");
        return -1;
    }

    /* Configures address and binds socket to it */
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
        perror("Socket bind failed");
        return -1;
    }

    /* Listens for socket connections and limits to 16 connections */
    if (listen(server_fd, 16) < 0) {
        perror("Socket listen failed");
        return -1;
    }

    /* Accepts new connection */
    if ((new_socket = accept(server_fd, (struct sockaddr *) &address,
                             (socklen_t *) &addrlen)) < 0) {
        perror("Socket accept failed");
        return -1;
    }

    return new_socket;
}

int connect_sock(char const *ip_addr, int port) {
    int sock = 0;
    struct sockaddr_in serv_addr;

    /* Creates a socket. */
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error.");
        return -1;
    }

    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr. sin_family = AF_INET;
    serv_addr. sin_port = htons(port);

    /* Converts IPv4 and IPv6 addresses from text to binary form */
    if (inet_pton(AF_INET, ip_addr, &serv_addr.sin_addr) <= 0) {
        perror("Socket address translation error.");
        return -1;
    }

    /* Connects to the server address */
    if (connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("Socket connection error.");
        return -1;
    }

    return sock;
}

void get_ip(char *ip_addr, int n_ip, int sock) {
    struct sockaddr_in addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);

    if (getpeername(sock, (struct sockaddr*) &addr, &addr_size) == 0) {
        strncpy(ip_addr, inet_ntoa(addr.sin_addr), n_ip - 1);
    }
    ip_addr[n_ip - 1] = '\0';
}

int recv_file(struct FileLoading *fload) {
    char buffer[SIZE_BUFFER] = {0};
    int valread = 1;
    unsigned long total = 0;
    FILE *fp = NULL;

    /* Opens received file */
    if ((fp = fopen(fload->filename, "w")) == NULL) {
        return 0; /* False */
    }

    /* Reads all file */
    for (total = 0; valread > 0 && total < fload->size; total += valread) {
        valread = read(fload->sock, buffer, SIZE_BUFFER-1);
        valread = fwrite(buffer, 1, valread, fp);
    }

    /* Cleans up */
    fclose(fp);

    return valread > 0;
}

int send_file(struct FileLoading *fload) {
    char buffer[SIZE_BUFFER] = {0};
    int valread = 1;
    unsigned long total = 0;
    FILE *fp = NULL;

    /* Opens received file */
    if ((fp = fopen(fload->filename, "r")) == NULL) {
        return 0; /* False */
    }

    /* Sends all file */
    for (total = 0; valread > 0 && total < fload->size; total += valread) {
        valread = fread(buffer, 1, SIZE_BUFFER-1, fp);
        valread = send(fload->sock, buffer, valread, 0);
    }

    /* Cleans up */
    fclose(fp);

    return valread > 0;
}
