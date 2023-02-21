#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "green_pass.h"
#include "addresses.h"

int main(int argc, char * argv[]) {
    if(argc != 2) {
        printf("Parametri in input insufficienti.\nEseguire seguendo la sintassi: ./client <CODICE_TESSERA>\n");
        exit(EXIT_FAILURE);
    }

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
    center.sin_addr.s_addr = inet_addr(LOCAL_HOST); /* Assegna all'indirizzo IP del server (`sin_addr`)
    * il valore specificato nella costante `LOCAL_HOST`,
    * dopo averlo convertito in un formato numerico con la funzione `inet_addr` */
    center.sin_port = htons(CENTER_PORT); // Con `htons` convertiamo la costante `CENTER_PORT` in un formato a 16 bit

    /* Connessione con il server utilizzando la funzione `connect`. In caso di errore, la funzione restituisce un
     * valore negativo. Il secondo argomento richiesto è un puntatore a una struttura `sockaddr`.
     * Nel nostro caso, avendo una struttura `sockaddr_in`, andiamo a fare un casting; ciò è possibile poiché
     * i primi campi delle due strutture coincidono. */
    if (connect(sock, (struct sockaddr *) &center, sizeof(center)) < 0) {
        perror("Errore durante la connessione al server");
        exit(EXIT_FAILURE);
    }
    struct GreenPass green_pass;
    strncpy(green_pass.tessera_sanitaria, argv[1], TESSERA_LENGTH);
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
    // Controlla se il Green Pass è valido o meno
    if (response == 0) {
        printf("Green pass non valido\n");
    } else {
        printf("Green pass valido\n");
    }
    // Chiude la connessione con la socket
    close(sock);
    return 0;
}
