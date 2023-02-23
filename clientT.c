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
    /* Controllo dei parametri presi da riga di comando */
    if (argc != 2) {
        printf("Parametri in input insufficienti.\nEseguire seguendo la sintassi: ./client <CODICE_TESSERA>\n");
        exit(EXIT_FAILURE);
    }

    /* Crea il file descriptor di un nuovo socket utilizzando il protocollo TCP (`SOCK_STREAM`).
     * In caso di errore, la funzione socket restituisce -1 */
    int client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock == -1) {
        perror("Errore durante la creazione del socket");
        exit(EXIT_FAILURE);
    }

    /* Crea la struct `sockaddr_in` per stabilire la connessione */
    struct sockaddr_in serverG_address;
    /* Inizializza tutti i campi della struct `sockaddr_in` */
    serverG_address.sin_family = AF_INET; // utilizza un protocollo IPv4
    serverG_address.sin_addr.s_addr = inet_addr(LOCAL_HOST); // ascolta sull'interfaccia di rete locale
    serverG_address.sin_port = htons(SERVERG_PORT); // si mette in ascolto sulla porta 8100
    /* Esegue la connessione al server G */
    if (connect(client_sock, (struct sockaddr *) &serverG_address, sizeof(serverG_address)) == -1) {
        perror("Errore durante la connessione al server");
        close(client_sock); // chiude la socket del client
        exit(EXIT_FAILURE);
    }

    /* Converte la stringa della tessera sanitaria in maiuscola */
    struct GreenPass green_pass;
    strncpy(green_pass.tessera_sanitaria, argv[1], TESSERA_LENGTH);
    for (size_t i = 0; green_pass.tessera_sanitaria[i] != '\0'; ++i) {
        green_pass.tessera_sanitaria[i] = toupper(green_pass.tessera_sanitaria[i]);
    }
    /* Imposta il tipo di servizio che richiede il client,
     * nel caso del clientT viene richiesta la validazione del green pass */
    green_pass.service = VALIDATION_GP;

    /* Invia il green pass al centro vaccinale */
    if (send(client_sock, &green_pass, sizeof(green_pass), 0) == -1) {
        perror("Errore durante l'invio del codice della tessera sanitaria al centro vaccinale");
        close(client_sock);
        exit(EXIT_FAILURE);
    }

    /* Attende la risposta da parte del socket */
    int response;
    if (recv(client_sock, &response, sizeof(response), 0) < 0) {
        perror("Errore durante la ricezione della risposta dal centro vaccinale");
        exit(EXIT_FAILURE);
    }

    /* Controlla se il green pass è valido o meno in base al valore
     * della variabile response */
    if (response == 0) {
        printf("Green pass non valido\n");
    } else {
        printf("Green pass valido\n");
    }

    /* Chiude la connessione con la socket */
    close(client_sock);

    return 0;
}