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
#include <sys/ioctl.h> 
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

#define DIM_BUFFER 256
#define DIM_WIND 128

#define PARAM_ERR 1
#define NETW_ERR 2
#define TX_NETW_ERR 3
#define RX_NETW_ERR 4
#define IO_ERR 5
#define FORK_ERR 6
#define EXEC_ERR 7

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
    long nCurrLinee = 0;

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
        
        //una volta che ho terminato LSEEK!!!! Devo rispostare I/O pointer all'inizio del file.
        lseek(fdCurrFile, 0, SEEK_SET);

        //il file passato presente in nomeFile è corretto: esiste e ho i permessi di rd/wr
        printf("Inserisci il numero di linea da rimuovere dal file, %s:\n", nomeFile);
        
        long nRmLinea = 0;       //linea da rimuovere
        int res;                 //valore di ritorno di scanf
        char c;                  //char per consumo buffer fino a EOL
        char okstr[DIM_BUFFER];

        //leggo ciclicamente linea da rimuovere fino a quando non è corretta.
        while ((res = scanf("%ld", &nRmLinea)) != EOF) {

            if(res == 1){ //ho effettivamente letto un intero

                if(nRmLinea <= 0){ //verifico se linea da rimuovere è negativa
                    printf("Il numero della linea da rimuovere deve essere > 0!\n");
                    continue;
                } else if(nRmLinea > nCurrLinee) { //verifico se linea da rimuovere è outofbound
                    printf("Il numero della linea da rimuovere non esiste nel file %s, numero di linee = %ld\n", nomeFile, nCurrLinee);
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
        if((write(fdSocket, &nRmLinea, sizeof(long))) < 0){
            perror("Errore durante la scrittura del numero di linea da rimuovere al server.");
            //qua sarebbe contro protocollo proseguire dall'altro lato ho il server che si aspetta proprio il numero della 
            //riga da rimuovere! --> chiudo così dopo un po' anche il server si accorgerà che qualcosa non è andato e chiuderà
            //la connessione con il corrispettivo client per il quale si è verificato l'errore.
            close(fdSocket);
            close(fdCurrFile);
            exit(TX_NETW_ERR);
        }
        printf("CLIENT: inviato al server il numero di linea da eliminare %ld.\n", nRmLinea);


        //creo un figlio a cui delego il compito di consumare tutto input
        int pid;
        pid = fork();

        if (pid == 0) { //sono il figlio

            char currCh;

            //vado a leggere nel frattempo la risposta dal server.
            //se scrivo su un altro file non ci sono problemi, evito si saturi la buffer di ricezione

            int fdFileTemp = -1;
            
            if((fdFileTemp = open("tmp.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644)) < 0){
                perror("Impossibile aprire il file temporaneo.");
                exit(IO_ERR);
            }
            
            //ricevo il file in risposta dal server lo salvo sia sul file sia stampo a console
            while((read(fdSocket, &currCh, sizeof(char))) > 0){

                //istruzioni per verificare i dati nel buffer ancora da consumare!
                //int dataReceiveBufferClient;
                //ioctl(fdSocket, FIONREAD, &dataReceiveBufferClient);
                
                printf("%c", currCh);
                if((write(fdFileTemp, &currCh, sizeof(char))) < 0){
                    perror("Errore scrittura sul file temporaneo.");
                    close(fdSocket);
                    close(fdFileTemp);
                    exit(RX_NETW_ERR);
                }
            }
            //giunto qua ho ricevuto, il file dal server e lo ho salvato sul file temporaneo lo sposto sull'originale.

            // Chiusura socket in ricezione -> non ho più intenzione di ricevere nulla ho consumato tutto l'input!
            shutdown(fdSocket, SHUT_RD);

            //chiudo il file tmp
            close(fdFileTemp);

            execlp("mv", "mv", "tmp.txt", nomeFile, NULL);
            perror("Errore esecuzione exec da parte del figlio");
            close(fdSocket);
            exit(EXEC_ERR);
        } 
        
        //sono il padre
        
        //invio il file dall'altra parte su fdSocket un carattere alla volta
        char currCh;
        
        printf("Client: invio il contenuto del file al server.\n");
        while((read(fdCurrFile, &currCh, sizeof(char))) > 0){
            if((write(fdSocket, &currCh, sizeof(char))) < 0){
                perror("Errore trasferimento file sul server!");
                close(fdSocket);
                close(fdCurrFile);
            }
        }
        printf("CLIENT: inviato file %s al server, voglio rimuovere la linea %ld.\n", nomeFile, nRmLinea);
        
        // Chiusura socket in spedizione -> invio dell'EOF
        shutdown(fdSocket, SHUT_WR);
        close(fdCurrFile);
        
        //A questo punto sono riuscito ad inviare tutto il file dall'altra parte e ora divento un filtro ben fatto,
        //per questo ho già creato un figlio che ha il compito di consumare tutto l'input che mi ritorna dal server,
        //per evitare che si saturi la buffer di ricezione del client.        
        close(fdSocket);

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
