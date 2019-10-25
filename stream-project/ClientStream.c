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

int main(int argc, char const *argv[]) {
    //Client nodoServer

    //Verifica argomenti di invocazione
    if(argc != 2){
        perror("Usage: client nodoServer");
        exit(PARAM_ERR);
    }

    struct hostent *host;
    host = gethostbyname(argv[1]);

    if(host == NULL){
        perror("Impossibile risalire all'indirizzo del server.");
        exit(PARAM_ERR);
    }

//    clock_t begin = clock();

    //chiedo utente nomefile da esaminare
    //int checkFileName = 0;
    char nomeFile[DIM_BUFFER];
    int nCurrLinee = 0;

    //boolean sentinella che tiene traccia dello stato della connessione
    int connected = 0;

    //supponiamo che la porta sia nota e coincida con 12345
    struct sockaddr_in serverSock;
    int fdSocket = -1;

     //chiedo ciclicamente il nome del file da rimuovere la linea fino ad EOF!
    printf("Inserisci un nome di file di cui vuoi rimuovere una linea:\n");

    while((fgets(nomeFile, DIM_BUFFER, stdin)) != NULL){
        nomeFile[strlen(nomeFile)-1] = '\0';

        //ho il nome del file vnuovoerifico se esiste e se diritti di rd/wr
        int fdCurrFile = open(nomeFile, O_RDWR);
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

        //il file passato presente in nomeFile è corretto
        printf("Inserisci il numero di linea da rimuovere dal file, %s:\n", nomeFile);
        
        int nRmLinea = 0;
        int res;
        char c;
        char okstr[DIM_BUFFER];

        while ((res = scanf("%i", &nRmLinea)) != EOF) {
            if(res == 1){
                if(nRmLinea <= 0){ //verifico se linea da rimuovere è negativa
                    printf("Il numero della linea da rimuovere deve essere > 0!\n");
                    continue;
                } else if(nRmLinea > nCurrLinee) { //outofbound
                    printf("Il numero della linea da rimuovere non esiste nel file %s, numero di linee = %d\n", nomeFile, nCurrLinee);
                    continue;
                }
                break;
            } 
            /* Problema nell'implementazione della scanf. Se l'input contiene PRIMA
            * dell'intero altri caratteri la testina di lettura si blocca sul primo carattere
            * (non intero) letto. Ad esempio: ab1292\n
            *				  ^     La testina si blocca qui
            * Bisogna quindi consumare tutto il buffer in modo da sbloccare la testina.
            */
            do {
                c = getchar();
            } while (c != '\n');
            printf("Numero linea non valida! Inserisci un nuovo numero intero (>0):\n");
            continue;
        
            gets(okstr); //consumo il restante della linea \n compreso!
        }

        //ho già eseguito tutti i controlli del caso posso passare ad inviare al server prima il numero della linea da rimuovere
        //che so per certo esistere! NON VA RICONTROLLATO DI LA, il server quando incontra la linea da rimuovere semplicemente 
        //non la reinvia.

        //scommenta dopo per la richiesta di connessione al server
        
        if(!connected){ //solo la prima volta creo la connessione
            //creo la socket
            if((fdSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0){
                perror("Non riesco a creare la socket.");
                exit(NETW_ERR);
            }

            //preparo la struttura per la connessione al server
            serverSock.sin_family = AF_INET;
            serverSock.sin_port = htons(12345);
            serverSock.sin_addr.s_addr= ((struct in_addr *)(host->h_addr_list[0]))->s_addr;

            //eseguo la connect
            if((connect(fdSocket, (struct sockaddr *) &serverSock, sizeof(serverSock))) < 0){
                perror("Impossibile instaurare la connessione con il server.");
                exit(NETW_ERR);
            }

            connected = 1;
        }
        

        //sono connesso e posso comunicare con fdSocket

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


        //invio il file dall'altra parte su fdSocket una riga alla volta
        //IPOTESI: tutte le linee sono al più lunghe DIM_BUFFER
        char currLine[DIM_BUFFER];
        int i = 0; //indice nella stringa currLine dove salvare currCh fino al fine linea prima di inviare dall'altra parte
        char currCh;
        
        while((read(fdCurrFile, &currCh, sizeof(char))) > 0){
            if(currCh == '\n'){ //letto il fine linea aggiungo terminatore e invio la linea sulla socket.
                currLine[i] = '\0';
                //debug stampo la linea letta dato che è finita!
                printf("%s\n", currLine);

                i = 0;
                
                if((write(fdSocket, currLine, strlen(currLine))) < 0){
                    perror("Errore durante il trasferimento del file verso il server.");
                    //Anche in questo caso dall'altro lato ho un server che attende tutto il file per rispettare il protocollo
                    //non posso andare avanti è giusto anche in questo caso uscire.
                    close(fdSocket);
                    close(fdCurrFile);
                    exit(TX_NETW_ERR);
                }
            } else { //letto un carattere che non è il fine linea lo devo aggiungere alla linea e incrementare i
                currLine[i] = currCh;
                i++;
            }
        }
        printf("CLIENT: inviato file %s al server, voglio rimuovere la linea %d.\n", nomeFile, nRmLinea);

        //A questo punto sono riuscito ad inviare tutto il file dall'altra parte e ora divento un filtro ben fatto,
        //fino a quando il server mi invia qualcosa io vado a sovrascrivere il file che ho appena inviato.

        //Mi devo ricordare innanzitutto di riposizionare I/O pointer in cima al file, e tramite la memset impostare il
        //contenuto del file a 0, dato che quello che vado a sovrascrivere avrà contenuto più corto.

        //dato che ero in fondo se chiedo di restituire la posizione del file so quanto è lungo già!!!
        int fileLength = lseek(fdCurrFile, 0, SEEK_END);
        printf("Posizione IO Pointer finale: %d\n", fileLength);
        printf("Dimensione file: %ld\n", fileLength*sizeof(char));
        
        //riavvolgo fino all'inizio, resetto il contenuto del file
        ftruncate(fdCurrFile, 0);
        ftruncate(fdCurrFile, fileLength*sizeof(char));
        lseek(fdCurrFile, 0, SEEK_SET);

        //prova del 9 per verificare se la dimensione coincide dopo queste operazioni mi sposto in fondo e stampo il 
        //valore deve coincidere con quello di prima
        //fileLength = lseek(fdCurrFile, 0, SEEK_END);
        //printf("Posizione IO Pointer finale dopo reset della dimensione e contenuto: %d\n", fileLength);

        //ricevo il file in risposta dal server lo salvo sia sul file sia stampo a console
        while((read(fdSocket, &currCh, sizeof(char))) > 0){
            printf("%c", currCh);
            if((write(fdCurrFile, &currCh, sizeof(char))) < 0){
                perror("Errore sovrascrittura sul file.");
                close(fdSocket);
                close(fdCurrFile);
                exit(RX_NETW_ERR);
            }
        }
        printf("CLIENT: ricevuto file dal server senza la linea che andava eliminata!");

        //chiudo file e risetto a zero il n delle linee
        close(fdCurrFile);
        nCurrLinee = 0;
        
        printf("Inserisci un nome di file di cui vuoi rimuovere una linea (EOF per terminare):\n");
        gets(okstr);
    }

    if(connected) //solo se la connessione è stata creata chiudo il socket descriptor
        close(fdSocket);

//    clock_t end = clock();
//    double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;

//    printf("CLIENT: tempo di esecuzione %f\n", time_spent);

    return 0;

}
