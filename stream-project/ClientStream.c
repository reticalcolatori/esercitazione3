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
#define TX_NETW_ERR 3
#define RX_NETW_ERR 4
#define IO_ERR 5

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

    //predispongo strutture per la socket e inizializzo
    struct sockaddr_in serverSock;
    int fdSocket = -1;
    serverSock.sin_family = AF_INET;
    serverSock.sin_port = htons(12345);
    serverSock.sin_addr.s_addr= ((struct in_addr *)(host->h_addr_list[0]))->s_addr;

    //Preparo variabili per nome e numero linee
    char nomeFile[DIM_BUFFER];
    int nCurrLinee = 0;

    //chiedo ciclicamente il nome del file da rimuovere la linea fino ad EOF!
    printf("Inserisci un nome di file di cui vuoi rimuovere una linea:\n");

    while((fgets(nomeFile, DIM_BUFFER, stdin)) != NULL){

        //prendo tempo di start
        clock_t begin = clock();


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
        
        //una volta che ho terminato LSEEK!!!! Devo rispostare I/O pointer in cima al file.
        lseek(fdCurrFile, 0, SEEK_SET);

        //il file passato presente in nomeFile è corretto: esiste e ho i permessi di rd/wr
        printf("Inserisci il numero di linea da rimuovere dal file, %s:\n", nomeFile);
        
        int nRmLinea = 0;       //linea da rimuovere
        int res;                //valore di ritorno di scanf
        char c;                 //char per consumo buffer fino a EOL
        char okstr[DIM_BUFFER];

        //leggo ciclicamente linea da rimuovere fino a quando non è corretta.
        while ((res = scanf("%i", &nRmLinea)) != EOF) {
            printf("Leggo il numero della linea da eliminare = %d.\n", nRmLinea);
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

        //creo la socket
        if((fdSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0){
            perror("Impossibile creare la socket.");
            exit(NETW_ERR);
        }

        //eseguo la connect, la BIND verrà effettuata automaticamente
        if((connect(fdSocket, (struct sockaddr *) &serverSock, sizeof(serverSock))) < 0){
            perror("Impossibile instaurare la connessione con il server.");
            exit(NETW_ERR);
        }
        printf("Client: correttamente connesso al server!\n");
        

        //invio il numero della riga da rimuovere
        if((write(fdSocket, &nRmLinea, sizeof(int))) < 0){
            perror("Errore durante la scrittura del numero di linea da rimuovere al server.");
            //qua sarebbe contro protocollo proseguire dall'altro lato ho il server che si aspetta proprio il numero della 
            //riga da rimuovere! --> chiudo così dopo un po' anche il server si accorgerà che qualcosa non è andato e chiuderà
            //la connessione con il corrispettivo client per il quale si è verificato l'errore.
            close(fdSocket);
            close(fdCurrFile);
            exit(TX_NETW_ERR);
        }
        printf("CLIENT: inviato al server il numero di linea da eliminare %d.\n", nRmLinea);

        //invio il file dall'altra parte su fdSocket un carattere alla volta
        char currCh;
        
        while((read(fdCurrFile, &currCh, sizeof(char))) > 0){
            if((write(fdSocket, &currCh, sizeof(char))) < 0){
                perror("Errore trasferimento file sul server!");
                close(fdSocket);
                close(fdCurrFile);
            }
        }
        printf("CLIENT: inviato file %s al server, voglio rimuovere la linea %d.\n", nomeFile, nRmLinea);

        // Chiusura socket in spedizione -> invio dell'EOF
		shutdown(fdSocket, 1);
        
        close(fdCurrFile);
        //A questo punto sono riuscito ad inviare tutto il file dall'altra parte e ora divento un filtro ben fatto,
        //fino a quando il server mi invia qualcosa io vado a sovrascrivere il file che ho appena inviato.

        //---------------------------- 2 STRADE -------------------------------------------//

        //1) UN SOLO FILEDESCRIPTOR (1 OPEN)
        //Mi devo ricordare innanzitutto di riposizionare I/O pointer in cima al file, e tramite la memset impostare il
        //contenuto del file a 0, dato che quello che vado a sovrascrivere avrà contenuto più corto.

        //2) DUE APERTURE! --> quella adottata in questa soluzione!
        //Una prima apertura per fare tutte le op. ed inviare al server il file.
        //Una seconda con mod O_TRUNC per sovrascrivere il contenuto che mi arriva dal server.

        if((fdCurrFile = open(nomeFile, O_WRONLY | O_TRUNC)) < 0){
            perror("Impossibile aprire il file per sovrascriverne il contenuto.");
            exit(IO_ERR);
        }
        printf("Riaperto il file in sovrascittura.\n");

        //ricevo il file in risposta dal server lo salvo sia sul file sia stampo a console
        while((read(fdSocket, &currCh, sizeof(char))) > 0){
            //se leggo EOF esci 
            if(currCh == EOF)
                break;

            printf("%c", currCh);
            if((write(fdCurrFile, &currCh, sizeof(char))) < 0){
                perror("Errore sovrascrittura sul file.");
                close(fdSocket);
                close(fdCurrFile);
                exit(RX_NETW_ERR);
            }
        }
        printf("CLIENT: ricevuto file dal server senza la linea che andava eliminata!\n");

        // Chiusura socket in ricezione -> non ho più intenzione di ricevere nulla ho consumato tutto l'input!
		shutdown(fdSocket, 0);
        close(fdSocket);

        //chiudo file e risetto a zero il numero delle linee
        close(fdCurrFile);
        nCurrLinee = 0;
        
        //prendo il tempo finale
        clock_t end = clock();
        double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
        printf("CLIENT: tempo di esecuzione %f sec\n", time_spent);

        printf("Inserisci un nome di file di cui vuoi rimuovere una linea (EOF per terminare):\n");
        gets(okstr); 
        //consumo il restante della linea (\n compreso), altrimenti alla prossima iterazione la fgets avrebbe già
        //il resto della linea da leggere
    }

    return 0;

}
