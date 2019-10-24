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
        write(fdSocket, nRmLinea, sizeof(int));


        //chiudo file e risetto a zero il n delle linee
        close(fdCurrFile);
        nCurrLinee = 0;
        
        printf("Inserisci un nome di file di cui vuoi rimuovere una linea (EOF per terminare):\n");
        gets(okstr);
    }

    if(connected) //solo se la connessione è stata creata chiudo il socket descriptor
        close(fdSocket);

    /*

    serverSock.sin_addr.s_addr= ((struct in_addr *)(host->h_addr_list[0]))->s_addr;

    int fdSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(fdSocket < 0){
        perror("Impossibile creare la socket.");
        exit(1);
    }

    if((connect(fdSocket,(struct sockaddr *) &serverSock, sizeof(serverSock))) < 0){
        perror("Impossibile instaurare la connessione.");
        exit(1);
    }
    
    //a connessione instaurata mando il nome del file e aspetto un ACK
    write(fdSocket, argv[2], strlen(argv[2]));

    char risp;
    read(fdSocket, &risp, 1);

    printf("CLIENT: ho ricevuto dal server %c\n", risp);

    //inizio ad inviare il file fino alla fine!
    int fdFile;
    if((fdFile = open(argv[2], O_RDONLY)) < 0){
        perror("Impossibile creare il file!");
        exit(1);
    }

    //in fdFile ho il file aperto!
    /*-----------------------------------------------------------------------------------------------------------
    TEST!!!
    Scopriamo che con un file da 80000 byte, il client impiega per la sua esecuzione:
     - 0.051145 microsec --> se legge da file e invia su socket un char alla volta! (10^-8)
     - 0.052781 --> se legge 256 byte alla volta e li scrive allo stesso modo
     - 0.000232 --> se legge 1024 byte alla volta e li scrive allo stesso modo

    char tmp;
    while((read(fdFile, &tmp, sizeof(tmp))) > 0){
        write(fdSocket, &tmp, sizeof(char));
    }
    ----------------------------------------------------------------------------------------------------------*/

    /*
    char tmp[DIM_BUFFER];
    while((read(fdFile, &tmp, sizeof(tmp))) > 0){
        write(fdSocket, &tmp, strlen(tmp));
    }
    close(fdSocket);
    close(fdFile);


//    clock_t end = clock();
//    double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;

//    printf("CLIENT: tempo di esecuzione %f\n", time_spent);

    return 0;

}
