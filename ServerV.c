//
// Created by raffaele on 21/02/23.
//

#include "header.h"

volatile sig_atomic_t running = 1;

void *handle_connection(void *);
void sigint_handler(int);
void delete_row(const char*, const char*);

pthread_mutex_t mutex;


int main(int argc, char *argv[]) {
    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    socklen_t client_address_len;
    pthread_t thread_pool[THREAD_POOL_SIZE];

    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);


    pthread_mutex_init(&mutex, NULL);

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
    server_address.sin_port = htons(SERVER_V_PORT); // listen on port 1024
    if (bind(server_socket, (SA *) &server_address, sizeof(server_address)) == -1) {
        perror("bind");
        exit(2);
    }

    // listen for connections
    if (listen(server_socket, SOMAXCONN) == -1) {
        perror("listen");
        exit(3);
    }

    printf("server V listening on port %d\n", SERVER_V_PORT);

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
                break;
            } else {
                // detach thread so it can run in the background
                if (pthread_detach(thread_pool[thread_index]) != 0) {
                    perror("pthread_detach");
                    break;
                }
            }
        }
    }

    pthread_mutex_destroy(&mutex);
    close(client_socket);

    return 0;
}


void *handle_connection(void *arg) {
    int client_socket;
    if (arg == NULL) {
        // handle error
        return NULL;
    } else {
        client_socket = *((int *) arg);
        free(arg);
    }

    // Receive the struct from the client
    struct green_pass gp;

    int bytes_received = recv(client_socket, &gp, sizeof(gp), 0);
    if (bytes_received == -1) {
        perror("recv");
        close(client_socket);
        return NULL;
    } else if (bytes_received != sizeof(gp)) {
        close(client_socket);
        return NULL;
    }

    //lock mutex
    if (pthread_mutex_lock(&mutex) != 0) {
        perror("pthread_mutex_lock");
        close(client_socket);
        return NULL;
    }

    // Open data file
    FILE *fp = fopen(FILE_NAME, "rb+");
    if (fp == NULL) {
        perror("fopen");
        close(client_socket);
        pthread_mutex_unlock(&mutex);
        return NULL;
    }

    if (gp.service == SERVICE_VALIDITY_GP) {
        printf("service requested : validity\n");
        // Find the corresponding code in the file
        fseek(fp, 0, SEEK_SET);
        char line[100];
        long int offset = 0;
        while (fgets(line, sizeof(line), fp) != NULL) {
            if (strncmp(line, gp.card, TESSERA_SANITARIA_LEN) == 0) {
                // Found the corresponding code
                int validity = line[strlen(line) - 2] - '0';
                int new_validity = validity == 1 ? 0 : 1;
                offset = -2;
                fseek(fp, offset, SEEK_CUR);
                fprintf(fp, "%d\n", new_validity);
                fflush(fp);
                break;
            }
        }
    }

        //service : 1 - search the code in the file
    else if (gp.service == SERVICE_READ_GP) {
        int found = FALSE;

        printf("service requested : read\n");
        char line[TESSERA_SANITARIA_LEN + EXPIRATION_DATE_LEN + 3];
        while (fgets(line, TESSERA_SANITARIA_LEN + EXPIRATION_DATE_LEN + 3, fp) != NULL) {

            // Extract the code and expiration date from the line
            char code[TESSERA_SANITARIA_LEN + 1];
            int day, month, year;
            int n_items = sscanf(line, "%[^:/]:%d/%d/%d", code, &day, &month, &year);
            if (n_items != 4) {
                continue;  // Invalid line, skip it
            }

            if (strcmp(code, gp.card) == 0) {
                printf("Code found\n");
                found = TRUE;
                // Code found, check if it's still valid
                char* last_char = line + strlen(line) - 1;
                printf("last char: %c\n", *last_char);
                if(*last_char == '1') {
                    time_t now = time(NULL);
                    struct tm *tm_struct = localtime(&now);
                    tm_struct->tm_mday = day;
                    tm_struct->tm_mon = month - 1;
                    tm_struct->tm_year = year - 1900;
                    time_t expiry_time = mktime(tm_struct);

                    if (expiry_time < now) {
                        // Code expired, send response to client
                        if (send(client_socket, "EXPIRED", 7, 0) == -1) {
                            perror("send");
                            fclose(fp);
                            close(client_socket);
                            pthread_mutex_unlock(&mutex);
                            return NULL;
                        }
                        fclose(fp);
                        close(client_socket);
                        pthread_mutex_unlock(&mutex);
                        return NULL;
                    } else {
                        // Code still valid, send response to client
                        if (send(client_socket, "VALID", 5, 0) == -1) {
                            perror("send");
                            fclose(fp);
                            close(client_socket);
                            pthread_mutex_unlock(&mutex);
                            return NULL;
                        }
                        fclose(fp);
                        close(client_socket);
                        pthread_mutex_unlock(&mutex);
                        return NULL;
                    }
                } else {
                    if (send(client_socket, "NOT VALID", 9, 0) == -1) {
                        perror("send");
                        fclose(fp);
                        close(client_socket);
                        pthread_mutex_unlock(&mutex);
                        return NULL;
                    }
                }
            }
        }

        if(!found) {
            // Code not found, send response to client
            if (send(client_socket, "NOT FOUND", 9, 0) == -1) {
                perror("send");
                fclose(fp);
                close(client_socket);
                pthread_mutex_unlock(&mutex);
                return NULL;
            }
        }
    }

        //service : 0 - write the code in the file
    else {
        printf("service requested : write\n");

        // Check if the code already exists in the file
        rewind(fp);
        char line[TESSERA_SANITARIA_LEN + EXPIRATION_DATE_LEN + 1];
        while (fgets(line, TESSERA_SANITARIA_LEN + EXPIRATION_DATE_LEN + 1, fp) != NULL) {
            char code[TESSERA_SANITARIA_LEN + 1];
            int n_items = sscanf(line, "%[^:/]:", code);
            if (n_items != 1) {
                continue; // Invalid line, skip it
            }
            if (strcmp(code, gp.card) == 0) {
                printf("given code already exists\n");
                pthread_mutex_unlock(&mutex);
                fflush(fp);
                fclose(fp);

                int response = 0;
                if (send(client_socket, &response, sizeof(int), 0) == -1) {
                    perror("send");
                    close(client_socket);
                    close(client_socket);
                    return NULL;
                }

                close(client_socket);
                return NULL;
            }
        }

        // Compute the actual expiry date based on the validity time left
        time_t now = time(NULL);
        time_t expiry_time = now + gp.expiration_date;
        struct tm *tm_struct = localtime(&expiry_time);

        // Write the code and expiry date to the end of the file
        fseek(fp, 0, SEEK_END);
        fprintf(fp, "%s:%d/%d/%d\n", gp.card, tm_struct->tm_mday, tm_struct->tm_mon + 1, tm_struct->tm_year + 1900);
        fflush(fp);
    }

    pthread_mutex_unlock(&mutex);

    int response = 1;
    if (send(client_socket, &response, sizeof(int), 0) == -1) {
        perror("send");
        close(client_socket);
        return NULL;
    }


    fclose(fp);
    close(client_socket);
    return NULL;
}

void sigint_handler(int sig) {
    running = 0;
}