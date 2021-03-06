/*
 * client.c : Client main file.
 *
 *  Created on: Apr 22, 2019
 *      Author: Aymeric Genêt
 */

#include "grass.h"
#include "connect.h"
#include "client.h"

#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <poll.h>



#define PRINT_EXHAUSTIVE_READ(valread, buffer, size, pfd) do { \
    do { \
        valread = read(pfd.fd, buffer, size-1); \
        buffer[valread] = '\0'; \
        printf("%s", buffer); \
    } while (poll(&pfd, 1, 0) > 0); \
    printf("\n"); \
} while (0)

/* ============================ GLOBAL VARIABLES ============================ */

#define SIZE_CLIENT_COMMANDS 3
static const struct ClientCommand cli_cmd[SIZE_CLIENT_COMMANDS] = {
    {"get", GET},
    {"put", PUT},
    {"exit", EXIT}
};

pthread_t *cli_get_thread = NULL;
pthread_t *cli_put_thread = NULL;

/* =============================== FUNCTIONS ================================ */

static void *client_get(void *args) {
    struct FileLoading *fload = (struct FileLoading*) args;

    if ((fload->sock = connect_sock(fload->ipv4_addr, fload->port)) == -1) {
        free(fload);
        return NULL;
    }

    recv_file(fload);
    close(fload->sock);

    free(fload);
    return NULL;
}

static void *client_put(void *args) {
    struct FileLoading *fload = (struct FileLoading*) args;

    if ((fload->sock = connect_sock(fload->ipv4_addr, fload->port)) == -1) {
        free(fload);
        return NULL;
    }

    send_file(fload);
    close(fload->sock);

    free(fload);
    return NULL;
}

static int client_parse_command(char const *command, struct ClientCommand cmd, struct pollfd pfd) {
    char format[SIZE_BUFFER] = {0}, output[SIZE_BUFFER] = {0};
    int valread = 0;
    struct FileLoading *fload = NULL;

    strcat(format, cmd.keyword);
    switch (cmd.id) {
        case GET:
            /* Blocks if cli_get_thread exists. */
            if (cli_get_thread == NULL) {
                cli_get_thread = malloc(sizeof(pthread_t));
            } else {
                pthread_join(*cli_get_thread, NULL);
            }

            /* Sends command */
            valread = send(pfd.fd, command, SIZE_BUFFER, 0);
            valread = read(pfd.fd, output, SIZE_BUFFER-1);

            if (valread <= 0) {
                /* Cleans up */
                free(cli_get_thread);
                cli_get_thread = NULL;
                return valread;
            }

            /* Prepares thread */
            fload = malloc(sizeof(struct FileLoading));

            strcat(format, " %1023[A-Za-z0-9_.-]");
            sscanf(command, format, fload->filename);
            sscanf(output, "get port: %d size: %lu", &(fload->port), &(fload->size));
            get_ip(fload->ipv4_addr, 16, pfd.fd);

            pthread_create(cli_get_thread, NULL, client_get, fload);

            return valread;

        case PUT:
            /* Blocks if cli_get_thread exists. */
            if (cli_put_thread == NULL) {
                cli_put_thread = malloc(sizeof(pthread_t));
            } else {
                pthread_join(*cli_put_thread, NULL);
            }

            /* Sends command */
            valread = send(pfd.fd, command, SIZE_BUFFER, 0);
            valread = read(pfd.fd, output, SIZE_BUFFER-1);

            if (valread <= 0) {
                /* Cleans up */
                free(cli_put_thread);
                cli_put_thread = NULL;
                return valread;
            }

            /* Prepares thread */
            fload = malloc(sizeof(struct FileLoading));

            strcat(format, " %1023[A-Za-z0-9_.-] %d");
            sscanf(command, format, fload->filename, &(fload->size));
            sscanf(output, "put port: %d", &(fload->port));
            get_ip(fload->ipv4_addr, 16, pfd.fd);

            pthread_create(cli_put_thread, NULL, client_put, fload);

            return valread;
        case EXIT:
        default:
            send(pfd.fd, "exit", 6, 0);
            return -1;
    }
}

static int send_command(char *buffer, struct pollfd pfd) {
    int valread = 0;
    size_t i = 0;

    /* Handles special commands */
    for (i = 0; i < SIZE_CLIENT_COMMANDS; ++i) {
        if (strstr(buffer, cli_cmd[i].keyword)) {
            return client_parse_command(buffer, cli_cmd[i], pfd);
        }
    }

    /* Any other command */
    valread = send(pfd.fd, buffer, SIZE_BUFFER, 0);
    PRINT_EXHAUSTIVE_READ(valread, buffer, SIZE_BUFFER, pfd);

    return valread;
}

/* ================================= MAIN =================================== */

int main(int argc, char **argv) {
    char buffer[SIZE_BUFFER] = {0};
    int sock = 0, valread = 0;
    struct pollfd pfd;

    /* Handles arguments */
    if (argc != 3) {
        printf("Usage: %s [HOST] [PORT]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* Connects to server */
    if ((sock = connect_sock(argv[1], atoi(argv[2]))) == -1) {
        exit(EXIT_FAILURE);
    }
    pfd.fd = sock;
    pfd.events = POLLRDNORM;

    /* Reads welcome message */
    PRINT_EXHAUSTIVE_READ(valread, buffer, SIZE_BUFFER, pfd);

    while (valread > 0) {
        /* Main loop */
        printf("> ");

        memset(buffer, 0, SIZE_BUFFER);
        scanf(FORMAT_SCANF, buffer); getchar();

        valread = send_command(buffer, pfd);
    }

    /* Cleans up */
    if (cli_get_thread != NULL) {
        pthread_join(*cli_get_thread, NULL);
        free(cli_get_thread);
        cli_get_thread = NULL;
    }
    if (cli_put_thread != NULL) {
        pthread_join(*cli_put_thread, NULL);
        free(cli_put_thread);
        cli_put_thread = NULL;
    }
    close(sock);

    return 0;
}
