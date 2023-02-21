#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "green_pass.h"
#include "header.h"

#define SERVERV_ADDRESS "127.0.0.1"
#define SERVERV_PORT 8890
#define CENTER_PORT 8888




int main() {
    /* Crea il file descriptor di un nuovo socket utilizzando il protocollo TCP (`SOCK_STREAM`).
     * In caso di errore, la funzione socket restituisce -1 */
    int centers_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (centers_sock == -1) {
        perror("Errore durante la creazione del socket del centro vaccinale");
        exit(EXIT_FAILURE);
    }

    // Crea un indirizzo per il socket del centro vaccinale
    struct sockaddr_in centers_addr;
    centers_addr.sin_family = AF_INET; // Il tipo di connessione, `AF_INET` indica l'utilizzo del protocollo IPv4
    centers_addr.sin_addr.s_addr = INADDR_ANY; /* Si imposta l'indirizzo IP del socket in modo da accettare
    * le connessioni da qualsiasi indirizzo IP, sia interno che esterno alla rete locale */
    centers_addr.sin_port = htons(CENTER_PORT); /* Con `htons` convertiamo la costante `CENTER_PORT`
    * in un formato a 16 bit */

    /* `bind` imposta le proprietà del socket del centro vaccinale, come l'indirizzo IP
     * e il numero di porta, in modo che il socket sia pronto ad accettare le connessioni
     * in entrata dai client che utilizzano lo stesso indirizzo IP e lo stesso numero di porta */
    if (bind(centers_sock, (struct sockaddr *) &centers_addr, sizeof(centers_addr)) < 0) {
        perror("Errore durante il binding del socket del centro vaccinale");
        exit(EXIT_FAILURE);
    }

    // Con la `listen()`, il socket `centers_sock` è pronto per accettare connessioni in arrivo da altri socket
    listen(centers_sock, 1024);

    /* Crea il file descriptor di un nuovo socket utilizzando il protocollo TCP (`SOCK_STREAM`).
     * In caso di errore, la funzione socket restituisce -1 */
    int servv_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (servv_sock == -1) {
        perror("Errore durante la creazione del socket per ServerV");
        exit(EXIT_FAILURE);
    }

    /* Inizializzazione di una struttura `sockaddr_in` con i dati necessari
     * per stabilire la connessione con il serverV */
    struct sockaddr_in servv_addr;
    servv_addr.sin_family = AF_INET;
    servv_addr.sin_addr.s_addr = inet_addr(SERVERV_ADDRESS);
    servv_addr.sin_port = htons(SERVERV_PORT);

    /* Connessione con il server utilizzando la funzione `connect`. In caso di errore, la funzione restituisce un
     * valore negativo. Il secondo argomento richiesto è un puntatore a una struttura `sockaddr`.
     * Nel nostro caso, avendo una struttura `sockaddr_in`, andiamo a fare un casting; ciò è possibile poiché
     * i primi campi delle due strutture coincidono. */
    if (connect(servv_sock, (struct sockaddr *) &servv_addr, sizeof(servv_addr)) < 0) {
        perror("Errore durante la connessione a ServerV");
        exit(EXIT_FAILURE);
    }

    /* Viene utilizzata una struttura `sockaddr_in` per contenere l'indirizzo del client e viene
     * impostata una variabile `client_len` con la lunghezza di tale struttura */
    struct sockaddr_in client_addr;
    int client_len = sizeof(client_addr);

    while (1) {
         /* La funzione `accept()` blocca l'esecuzione del programma fino a quando non arriva
          * una nuova richiesta di connessione da parte di un client. `accept()` restituisce
          * un nuovo socket tramite il quale il server può comunicare con il client */
        int client_sock = accept(centers_sock, (struct sockaddr *) &client_addr, (socklen_t *) &client_len);
        if (client_sock < 0) {
            perror("Errore durante la connessione al client");
            exit(EXIT_FAILURE);
        }

        struct GreenPass green_pass; // Conterrà i dati relativi al green pass del client
        /* Si riceve il pacchetto inviato dal client tramite la funzione `recv()`,
        * che memorizza i dati ricevuti nella struttura `GreenPass` */
        if (recv(client_sock, &green_pass, sizeof(green_pass), 0) < 0) {
            perror("Errore durante la ricezione del codice della tessera sanitaria dal client");
            close(client_sock);
            continue;
        }

        /* Se il campo `tessera_sanitaria` è vuoto, significa che il client ha inviato un
         * segnale di terminazione della connessione */
        if (green_pass.tessera_sanitaria[0] == '\0') {
            close(client_sock);
            break;
        }

        green_pass.valid_from = time(NULL);
        green_pass.valid_until = (time(NULL) + 30 * 24 * 60 * 60);
        struct tm *from_ptr = localtime(&green_pass.valid_from);
        struct tm *until_ptr = localtime(&green_pass.valid_until);


        char valid_from[11];
        strftime(valid_from, sizeof(valid_from), "%d/%m/%Y", from_ptr);
        char valid_until[11];
        strftime(valid_until, sizeof(valid_until), "%d/%m/%Y", until_ptr);
        printf("%s : %s : %s\n", green_pass.tessera_sanitaria, valid_from, valid_until);

        if (send(servv_sock, &green_pass, sizeof(green_pass), 0) < 0) {
            perror("Errore durante l'invio del codice della tessera sanitaria a ServerV");
            close(client_sock);
            continue;
        }

        close(client_sock);
    }

    close(servv_sock);

    return 0;
}