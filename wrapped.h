//
// Created by raffaele on 21/02/23.
//

#ifndef GREEN_PASS_WRAPPED_H
#define GREEN_PASS_WRAPPED_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>

ssize_t FullWrite(int, void *, size_t);
ssize_t FullRead(int, void *, size_t);

ssize_t FullWrite(int fd, void *buf, size_t count) {
    size_t nleft;
    ssize_t nwritten;
    nleft = count;
    while(nleft > 0) { // finchè non ci sono elementi a sinistra
        if((nwritten = write(fd, buf, count)) < 0) {
            if(errno == EINTR) // se interrotto tramite una system call
                continue; // ripeti il loop
            else {
                perror("write"); // altrimenti esci con l'errore
                exit(nwritten);
            }
        }
        nleft -= nwritten;
        buf += nwritten;
    }
    return nleft;
}

ssize_t FullRead(int fd, void *buf, size_t count) {
    size_t nleft;
    ssize_t nread;
    nleft = count;
    while(nleft > 0) { // finchè non ci sono elementi a sinistra
        if((nread = read(fd, buf, count)) < 0) {
            if(errno == EINTR)  // se interrotto tramite una system call
                continue; // ripeti il loop
            else {
                perror("read"); // altrimenti esci con l'errore
                exit(1);
            }
        } else if(nread == 0) // EOF
            break; // interrompi il loop
        nleft -= nread; // settiamo il left su read
        buf += nread; // settiamo il puntatore
    }
    buf = 0;
    return nleft;
}


#endif //GREEN_PASS_WRAPPED_H
