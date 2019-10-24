#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <time.h>

#define DIM_BUFFER 256

#define PARAM_ERR 1
#define NETW_ERR 2

int main(int argc, char const *argv[]) {
    //Client nodoServer

    //Verifica argomenti di invocazione
    if(argc != 2){
        perror("Usage: client nodoServer");
        exit(PARAM_ERR);
    }

    //Controllo subito se il nodoServer è corretto, altrimenti esco
    struct hostent *host;
    host = gethostbyname(argv[1]);

    if(host == NULL){
        perror("Impossibile risalire all'indirizzo del server.");
        exit(PARAM_ERR);
    }

//    clock_t begin = clock();

    //Preparo variabili per nome e numero linee
    char nomeFile[DIM_BUFFER];
    int nCurrLinee = 0;

    //boolean sentinella che tiene traccia dello stato della connessione
    int connected = 0;

    //predispongo strutture per la socket
    struct sockaddr_in serverSock;
    int fdSocket = -1;

    //chiedo ciclicamente il nome del file da rimuovere la linea fino ad EOF!
    printf("Inserisci un nome di file di cui vuoi rimuovere una linea:\n");

    while((fgets(nomeFile, DIM_BUFFER, stdin)) != NULL){
        //Aggiungo terminatore di stringa!
        nomeFile[strlen(nomeFile)-1] = '\0';

        //ho il nome del file nuovo: verifico se esiste e se possiedo diritti di rd/wr
        int fdCurrFile = open(nomeFile, O_RDWR);
        //In caso di esito negativo continuo a richiedere un file da stdin all'utente
        if(fdCurrFile < 0){
            //file passato non corretto lo richiedo!
            printf("File non presente o non possiedo i diritti di rd/wr.\n");
            //passo a richiederne uno nuovo
            printf("Inserisci un nuovo nome di file di cui vuoi rimuovere una linea:\n");
            continue;
        }
        
        //conto già il numero delle linee del file
        char ch;
        while((read(fdCurrFile, &ch, sizeof(char))) > 0) {
            if(ch == '\n')
                nCurrLinee++;
        }

        //il file passato presente in nomeFile è corretto: esiste e ho i permessi di rd/wr
        printf("Inserisci il numero di linea da rimuovere dal file, %s:\n", nomeFile);
        
        int nRmLinea = 0;       //linea da rimuovere
        int res;                //valore di ritorno di scanf
        char c;                 //char per consumo buffer fino a EOL
        char okstr[DIM_BUFFER];

        while ((res = scanf("%i", &nRmLinea)) != EOF) {
            if(res == 1){ //ho effettivamente letto un intero
                if(nRmLinea <= 0){ //verifico se linea da rimuovere è negativa
                    printf("Il numero della linea da rimuovere deve essere > 0!\n");
                    continue;
                } else if(nRmLinea > nCurrLinee) { //verifico se linea da rimuovere è outofbound
                    printf("Il numero della linea da rimuovere non esiste nel file %s, numero di linee = %d\n", nomeFile, nCurrLinee);
                    continue;
                }
                break; //se invece la linea è accettabile esco dal ciclo
            } 

            /* ATTENZIONE! Se l'input contiene prima dell'intero altri caratteri la scanf
            * si blocca sul primo carattere non intero letto.
            * Quindi consumo tutto il buffer in modo da sbloccare la scanf.
            */
            do {
                c = getchar();
            } while (c != '\n');
            printf("Numero linea non valida! Inserisci un nuovo numero intero (>0):\n");
            continue;
        
            gets(okstr); //consumo il restante della linea \n compreso!
        }

        /*ho già eseguito tutti i controlli del caso, posso procedere ad inviare al server:
        * 1) il numero della linea da rimuovere (che so per certo esistere)
        * 2) il contenuto del file
        */

        //Per massimizzare l'efficienza, creo una sola connessione la prima volta che invio un file
        //Le volte successive sarà già aperta
        if(!connected){ 
            //creo la stream socket
            if((fdSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0){
                perror("Impossibile creare la socket.");
                exit(NETW_ERR);
            }

            //preparo la struttura per la connessione al server (supponiamo che la porta sia nota: 12345)
            serverSock.sin_family = AF_INET;
            serverSock.sin_port = htons(12345);
            serverSock.sin_addr.s_addr= ((struct in_addr *)(host->h_addr_list[0]))->s_addr;

            //eseguo la connect, la BIND verrà effettuata automaticamente
            if((connect(fdSocket, (struct sockaddr *) &serverSock, sizeof(serverSock))) < 0){
                perror("Impossibile instaurare la connessione con il server.");
                exit(NETW_ERR);
            }

            //setto la variabile sentinella così alle prossime iterazioni questo blocco verrà saltato
            connected = 1;
        }
    
        //sono connesso (lato client) e posso comunicare tramite fdSocket
    
        //invio il numero della riga da rimuovere
        write(fdSocket, &nRmLinea, sizeof(int));

        //leggo un char alla volta e lo scrivo sulla socket
        char tmp;
        lseek(fdCurrFile, 0, SEEK_SET);
        while((read(fdCurrFile, &tmp, sizeof(tmp))) > 0){
            write(fdSocket, &tmp, sizeof(char));
        }

        //Chiudo il file e lo riapro troncandolo per sovrascriverne il contenuto
        //con ciò che ricevo dal server
        close(fdCurrFile);
        fdCurrFile = open(nomeFile, O_WRONLY | O_TRUNC);

        while((read(fdSocket, &tmp, sizeof(tmp))) > 0){
            write(fdCurrFile, &tmp, sizeof(char));
            printf("%c", tmp);
        }

        //chiudo file e risetto a zero il numero delle linee
        close(fdCurrFile);
        nCurrLinee = 0;
        
        printf("Inserisci un nome di file di cui vuoi rimuovere una linea (EOF per terminare):\n");
        gets(okstr); 
        //consumo il restante della linea (\n compreso), altrimenti alla prossima iterazione la fgets avrebbe già
        //il resto della linea da leggere
    }

    if(connected) //solo se la connessione è stata creata chiudo il socket descriptor
        close(fdSocket);

    //clock_t end = clock();
    //double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
    //printf("CLIENT: tempo di esecuzione %f\n", time_spent);

    return 0;

}
