//
// Created by raffaele on 21/02/23.
//

#include "header.h"

volatile sig_atomic_t running = 1;

void *handle_connection(void *);
void sigint_handler(int);

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    socklen_t client_address_len;
    pthread_t thread_pool[THREAD_POOL_SIZE];

    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    // create server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket");
        exit(1);
    }

    // bind server socket to a port
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); // listen on all available network interfaces
    server_address.sin_port = htons(CV_PORT); // listen on port 1026
    if (bind(server_socket, (SA *) &server_address, sizeof(server_address)) == -1) {
        perror("bind");
        exit(2);
    }

    // listen for connections
    if (listen(server_socket, SOMAXCONN) == -1) {
        perror("listen");
        exit(3);
    }

    printf("CV listening on port %d\n", CV_PORT);

    // initialize thread pool
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        if (pthread_create(&thread_pool[i], NULL, handle_connection, NULL) != 0) {
            perror("pthread_create");
            exit(1);
        }
    }

    // wait for connections and assign to threads in pool
    while (running) {
        client_address_len = sizeof(client_address);
        client_socket = accept(server_socket, (struct sockaddr *) &client_address, &client_address_len);
        if (client_socket == -1) {
            perror("accept");
            continue;
        }

        // find first available thread in pool
        int thread_index = -1;
        for (int i = 0; i < THREAD_POOL_SIZE; i++) {
            if (pthread_kill(thread_pool[i], 0) == ESRCH) {
                // thread is available
                thread_index = i;
                break;
            }
        }

        if (thread_index == -1) {
            // no threads available, close connection
            close(client_socket);
        } else {
            // pass client socket to thread
            int *client_socket_ptr = malloc(sizeof(int));
            *client_socket_ptr = client_socket;
            if (pthread_create(&thread_pool[thread_index], NULL, handle_connection, client_socket_ptr) != 0) {
                perror("pthread_create");
                free(client_socket_ptr);
                break;
            } else {
                // detach thread so it can run in the background
                if (pthread_detach(thread_pool[thread_index]) != 0) {
                    free(client_socket_ptr);
                    perror("pthread_detach");
                    break;
                }
            }
        }
    }


    printf("handled\n");
    close(client_socket);
    return 0;
}


void *handle_connection(void *arg) {
    // Get client socket
    int client_socket;
    if (arg == NULL) {
        // handle error
        return NULL;
    } else {
        client_socket = *((int *) arg);
        free(arg);
    }

    // Receive data from client
    char card[TESSERA_SANITARIA_LEN + 1];
    if(recv(client_socket, card, TESSERA_SANITARIA_LEN, 0) == -1 ){
        perror("recv");
        close(client_socket);
        return NULL;
    }

    printf("code received: %s\n", card);

    //generate green pass
    struct green_pass gp;
    stpcpy(gp.card, card);
    gp.expiration_date = SIX_MONTHS;
    gp.service = 0;

    // Connect to server_v
    int serverV_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverV_socket == -1) {
        perror("socket");
        close(client_socket);
        return NULL;
    }

    struct sockaddr_in server_address;
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    server_address.sin_port = htons(SERVER_V_PORT);

    if (connect(serverV_socket, (SA *)&server_address, sizeof(server_address)) == -1) {
        perror("connect");
        close(client_socket);
        close(serverV_socket);
        return NULL;
    }

    // Send data to server
    if (send(serverV_socket, &gp, sizeof(gp), 0) == -1) {
        perror("send");
        close(client_socket);
        close(serverV_socket);
        return NULL;
    }

    printf("green pass sent\n");

    int response;
    if (recv(serverV_socket, &response, sizeof(int), 0) == -1) {
        perror("recv");
        close(client_socket);
        close(serverV_socket);
        return NULL;
    }

    printf("response : %d\n", response);

    if(response) {
        // Send response to client
        if (send(client_socket, "success", sizeof("success"), 0) == -1) {
            perror("send");
            close(client_socket);
            close(serverV_socket);
            return NULL;
        }
    } else {
        if (send(client_socket, "already exists", sizeof("already exists"), 0) == -1) {
            perror("send");
            close(client_socket);
            close(serverV_socket);
            return NULL;
        }
    }

    // Close sockets
    close(client_socket);
    close(serverV_socket);

    return NULL;
}

void sigint_handler(int sig) {
    running = 0;
}