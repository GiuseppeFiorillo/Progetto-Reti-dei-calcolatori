#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "green_pass.h"
#include "header.h"

#define CENTER_ADDRESS "127.0.0.1"
//#define CENTER_PORT 8888
#define CENTER_PORT 1026

int main() {
    /* Crea il file descriptor di un nuovo socket utilizzando il protocollo TCP (`SOCK_STREAM`).
     * In caso di errore, la funzione socket restituisce -1 */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Errore durante la creazione del socket");
        exit(EXIT_FAILURE);
    }

    /* Inizializzazione di una struttura `sockaddr_in` con i dati necessari
     * per stabilire la connessione con il centro vaccinale */
    struct sockaddr_in center;
    center.sin_family = AF_INET; // Il tipo di connessione, `AF_INET` indica l'utilizzo del protocollo IPv4
    center.sin_addr.s_addr = inet_addr(CENTER_ADDRESS); /* Assegna all'indirizzo IP del server (`sin_addr`)
    * il valore specificato nella costante `CENTER_ADDRESS`,
    * dopo averlo convertito in un formato numerico con la funzione `inet_addr` */
    center.sin_port = htons(CV_PORT); // Con `htons` convertiamo la costante `CENTER_PORT` in un formato a 16 bit

    /* Connessione con il server utilizzando la funzione `connect`. In caso di errore, la funzione restituisce un
     * valore negativo. Il secondo argomento richiesto è un puntatore a una struttura `sockaddr`.
     * Nel nostro caso, avendo una struttura `sockaddr_in`, andiamo a fare un casting; ciò è possibile poiché
     * i primi campi delle due strutture coincidono. */
    if (connect(sock, (struct sockaddr *) &center, sizeof(center)) < 0) {
        perror("Errore durante la connessione al server");
        exit(EXIT_FAILURE);
    }

    struct GreenPass green_pass;
    printf("Inserire il codice della tesseria sanitaria (16 caratteri): ");
    scanf("%s", green_pass.tessera_sanitaria);
    getchar();

    // Convertiamo la stringa ricevuta in una maiuscola
    for (size_t i = 0; green_pass.tessera_sanitaria[i] != '\0'; i++) {
        green_pass.tessera_sanitaria[i] = toupper(green_pass.tessera_sanitaria[i]);
    }

    /* Invia il codice della tessera sanitaria al centro vaccinale. Lo zero come ultimo argomento specifica
     * di non usare opzioni particolari per l'invio. */
    if (send(sock, &green_pass, sizeof(green_pass), 0) < 0) {
        perror("Errore durante l'invio del codice della tessera sanitaria al centro vaccinale");
        exit(EXIT_FAILURE);
    }

    // Riceve la risposta dal centro vaccinale
    int response;
    if (recv(sock, &response, sizeof(response), 0) < 0) {
        perror("Errore durante la ricezione della risposta dal centro vaccinale");
        exit(EXIT_FAILURE);
    }

    if (response == 0) {
        printf("Green pass non valido\n");
    } else {
        printf("Green pass valido\n");
    }

    close(sock);

    return 0;
}