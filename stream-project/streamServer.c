/* Server che riceve un file e lo ridirige ordinato al client */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define LINE_LENGTH 256

#define EXIT_ARGS 1
#define EXIT_HOST_ERR 2
#define EXIT_SOCKET 3
#define EXIT_IO 4


/********************************************************/
void gestore(int signo){
  int stato;
  printf("esecuzione gestore di SIGCHLD\n");
  wait(&stato);
}
/********************************************************/

int main(int argc, char **argv)
{
    //Costante per definire REUSE_ADDR
    const int on = 1;

    //Socket descriptor per la listen e la connessione al client.
	int listen_sd, conn_sd;
    
	int port;
    //Lunghezza indirizzo
    int lenAddress;

    //Contatore per verifica args.
    int num;
	
    //Indirizzi client e server.
	struct sockaddr_in cliaddr, servaddr;
    //Risoluzione host client.
	struct hostent *host;

    //Roba per algoritmo

    //Linea corrente
    int currentLine = 0;

    //Linea da eliminare
    int deleteLine = -1;

    //Buffer temporaneo per salvare la linea.
    char buff[LINE_LENGTH];

    int nread;
    char tmpChar;

    //Inizializzo il buffer a zero
    memset(buff, 0, LINE_LENGTH);

	/* CONTROLLO ARGOMENTI ---------------------------------- */
	if(argc!=2){
		printf("Error: %s port\n", argv[0]);
		exit(1);
	}
	else{
		num=0;
		while( argv[1][num]!= '\0' ){
			if( (argv[1][num] < '0') || (argv[1][num] > '9') ){
				printf("Secondo argomento non intero\n");
				exit(EXIT_ARGS);
			}
			num++;
		} 	
		port = atoi(argv[1]);
		if (port < 1024 || port > 65535){
			printf("Error: %s port\n", argv[0]);
			printf("1024 <= port <= 65535\n");
			exit(EXIT_ARGS);  	
		}

	}

	/* INIZIALIZZAZIONE INDIRIZZO SERVER ----------------------------------------- */
	memset ((char *)&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(port);

	/* CREAZIONE E SETTAGGI SOCKET D'ASCOLTO --------------------------------------- */
	listen_sd=socket(AF_INET, SOCK_STREAM, 0);
	if(listen_sd < 0) {
		perror("Errore creazione socket.");
		exit(1);
	}
	printf("Server: creata la socket d'ascolto, fd=%d\n", listen_sd);

    //Imposto REUSE_ADDR
	if(setsockopt(listen_sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0){
		perror("set opzioni socket d'ascolto");
		exit(1);
	}
	printf("Server: set opzioni socket d'ascolto ok (impostato reuseaddress).\n");

	//eseguo il binding sono il server devo farlo!
	if(bind(listen_sd,(struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)	{
		perror("bind socket d'ascolto");
		exit(1);
	}
	printf("Server: eseguito binding socket d'ascolto.\n");

	//creo la coda per la ricezione delle richieste di connessione
	if (listen(listen_sd, 5) < 0) {
		perror("Errore creazione coda di ascolto."); 
		exit(1);
	}
	printf("Server: creata la coda per ricezione delle richieste di connessione.\n");

	/* AGGANCIO GESTORE PER EVITARE FIGLI ZOMBIE */
	signal(SIGCHLD, SIG_IGN);

	/* CICLO DI RICEZIONE RICHIESTE --------------------------------------------- */
	for(;;){ 
        printf("Server: in attesa di richieste di connessione...");
	  	lenAddress=sizeof(cliaddr);

		if((conn_sd = accept(listen_sd, (struct sockaddr *)&cliaddr, &lenAddress)) < 0){
		/* La accept puo' essere interrotta dai segnali inviati dai figli alla loro
		* teminazione. Tale situazione va gestita opportunamente. Vedere nel man a cosa 
		* corrisponde la costante EINTR!*/
			if (errno==EINTR){
				perror("Forzo la continuazione della accept");
				continue;
			}
			else exit(EXIT_SOCKET);
		}

		if (fork()==0){ // figlio itera e processa le richieste fino a quando non riceve EOF!
			/*Chiusura FileDescr non utilizzati*/
			close(listen_sd);
			

            //Recupero informazioni client per stampa.
			host=gethostbyaddr( (char *) &cliaddr.sin_addr, sizeof(cliaddr.sin_addr), AF_INET);
			if (host == NULL){
				perror("Impossibile risalire alle informazioni dell'host a partire dal suo indirizzo.\n");
				continue;
			}
			else 
				printf("Server (figlio): host client è %s\n", host->h_name);

            //Leggo il numero della linea da rimuovere.
            if(read(conn_sd, &deleteLine, sizeof(int)) < 0){
                //Non riesco a leggere la linea da eliminare esco.
                perror("Impossibile leggere il numero della linea da eliminare.");
                exit(EXIT_IO);
            }
			//se tutto ok non eseguo controlli sono tutti eseguiti dal client nel caso non invia nemmeno la richiesta al server.
			printf("Linea da cancellare: %d\n", deleteLine);
			
            //Posso procedere:
            //Inizio a leggere il file
            //Mentre leggo conto le righe.
            //Se la riga è quella da eliminare non invio indietro.
            
            //Contatore dei caratteri
            //currentLine = 0;
            num = 0;

			printf("Ricevo file dal client sulla socket connessa fd: %d.\n", conn_sd);
//--------------------------PROBLEMA QUA SIPIANTA--------------------------------------------------------------//
            //while((nread = read(conn_sd, &tmpChar, sizeof(char))) > 0){
			while((read(conn_sd, &tmpChar, sizeof(char))) > 0){
				
				if(tmpChar == EOF){
					//ricevuto EOF dal client
					printf("Ho ricevuto EOF dal client.\n");
					break;
				}

				printf("%c", tmpChar);
                //Salvo il carattere in un buffer temporaneo.
                buff[num] = tmpChar;

                //Se il carattere è il fine linea:
                if(buff[num] == '\n'){
                    //Se consideriamo come prima linea la 1 allora faccio prima:
                    currentLine++;

                    //Controllo linea da saltare
                    if(currentLine != deleteLine){
                        //Scrivo indietro la riga con \n finale e la stampo!
						printf("SERVER: rispedisco indietro la linea --> %s\n", buff);
                        write(conn_sd, buff, strlen(buff));
						//write(conn_sd, "\n", sizeof(char));
                    }

                    //Resetto il contatore.
                    num = 0;
                } else 
					num++;
            }
			//devo inviare un EOF per spiantare il client che altrimenti rimane in attesa perenne!!!
			tmpChar = EOF;
			if((write(conn_sd, &tmpChar, sizeof(char))) < 0){
				perror("Impossibile inviare EOF al server!");
				close(conn_sd);
			}
			printf("Inviato un EOF dall'altro lato, sblocco il client ho finito di inviare il file modificato.\n");

            if(nread < 0){
                perror("Errore lettura file");
                exit(EXIT_IO);
            }

		} // figlio

        //PADRE
		close(conn_sd);  // padre chiude socket di connessione non di ascolto
	} // ciclo for infinito
}

