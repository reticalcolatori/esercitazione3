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
	int  listen_sd, conn_sd;
    
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
	if(listen_sd <0)
	{perror("creazione socket "); exit(1);}
	printf("Server: creata la socket d'ascolto, fd=%d\n", listen_sd);

    //Imposto REUSE_ADDR
	if(setsockopt(listen_sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))<0)
	{perror("set opzioni socket d'ascolto"); exit(1);}
	printf("Server: set opzioni socket d'ascolto ok\n");

	if(bind(listen_sd,(struct sockaddr *) &servaddr, sizeof(servaddr))<0)
	{perror("bind socket d'ascolto"); exit(1);}
	printf("Server: bind socket d'ascolto ok\n");

	if (listen(listen_sd, 5)<0) //creazione coda d'ascolto (5 elementi)
	{perror("listen"); exit(1);}
	printf("Server: listen ok\n");

	/* AGGANCIO GESTORE PER EVITARE FIGLI ZOMBIE */
	signal(SIGCHLD, SIG_IGN);

	/* CICLO DI RICEZIONE RICHIESTE --------------------------------------------- */
	for(;;){ 
        
	  	lenAddress=sizeof(cliaddr);

		if((conn_sd=accept(listen_sd,(struct sockaddr *)&cliaddr,&lenAddress))<0){
		/* La accept puo' essere interrotta dai segnali inviati dai figli alla loro
		* teminazione. Tale situazione va gestita opportunamente. Vedere nel man a cosa 
		* corrisponde la costante EINTR!*/
			if (errno==EINTR){
				perror("Forzo la continuazione della accept");
				continue;
			}
			else exit(EXIT_SOCKET);
		}

		if (fork()==0){ // figlio
			/*Chiusura FileDescr non utilizzati*/
			close(listen_sd);
			

            //Recupero informazioni client per stampa.
			host=gethostbyaddr( (char *) &cliaddr.sin_addr, sizeof(cliaddr.sin_addr), AF_INET);
			if (host == NULL){
				printf("client host information not found\n"); continue;
			}
			else printf("Server (figlio): host client e' %s \n", host->h_name);

            //Leggo il numero della linea da rimuovere.
            if(read(conn_sd, &deleteLine, sizeof(int)) != sizeof(int)){
                //Non riesco a leggere la linea da eliminare esco.
                perror("read numero linea da eliminare fallita");
                exit(EXIT_IO);
            }
			printf("Linea da cancellare: %d\n", deleteLine);
			
            //Posso procedere:
            //Inizio a leggere il file
            //Mentre leggo conto le righe.
            //Se la riga è quella da eliminare non invio indietro.
            
            //Contatore dei caratteri
            //currentLine = 0;
            num = 0;
            while((nread = read(conn_sd, &tmpChar, sizeof(char))) > 0){
				
                //Salvo il carattere in un buffer temporaneo.
                buff[num] = tmpChar;

                //Se il carattere è il fine linea:
                if(buff[num] == '\n'){
                    //Se consideriamo come prima linea la 1 allora faccio prima:
                    currentLine++;

                    //Controllo linea da saltare
                    if(currentLine != deleteLine){
                        //Scrivo indietro la riga con \n finale e la stampo!
                        write(conn_sd, buff, num);
						write(conn_sd, "\n", sizeof(char));
                    }

                    //Resetto il contatore.
                    num = 0;
                } else 
					num++;
            }

            if(nread < 0){
                perror("Errore lettura file");
                exit(EXIT_IO);
            }
            

		} // figlio

        //PADRE
		close(conn_sd);  // padre chiude socket di connessione non di ascolto
	} // ciclo for infinito
}

