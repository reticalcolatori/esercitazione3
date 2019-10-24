/* Server che fornisce la valutazione di un'operazione tra due interi */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>

#define LINE_LENGTH 256

#define EXIT_ARGS 1
#define EXIT_HOST_ERR 2
#define EXIT_SOCKET 3
#define EXIT_FILE 4

/*Struttura di una richiesta*/
/********************************************************/
typedef struct{
	char nomefile[LINE_LENGTH];
}Request;
/********************************************************/
/********************************************************/

int main(int argc, char **argv){
	//Variabile per settare l'opzione REUSE_ADDR
	const int on = 1;

	//Roba di rete
	int sd;
	struct sockaddr_in cliaddr, servaddr;
	struct hostent *clienthost;
	int lenAddress;
	int port;

	//Campi di richiesta risposta
	Request req;
	int ris;

	//Contatore
	int i = 0;

	//Roba per file
	//File Descriptor
	int fd;
	int nread;

	//Contiene il numero di caratteri della parola più lunga
	int maxWordLen = 0;

	//Contiene il numero di caratteri della parola corrente
	int currentWordLen = 0;

	char tmpChar;

	/* CONTROLLO ARGOMENTI ---------------------------------- */
	if(argc!=2){
		printf("Error: %s port\n", argv[0]);
		exit(1);
	}
	
	//Controllo sulla porta
	while( argv[1][i]!= '\0' ){
		if((argv[1][i] < '0') || (argv[1][i] > '9')){
			printf("Secondo argomento non intero\n");
			printf("Error: %s port\n", argv[0]);
			exit(EXIT_ARGS);
		}
		i++;
	}  	

	 	port = atoi(argv[1]);
		  
  	if (port < 1024 || port > 65535){
	      printf("Error: %s port\n", argv[0]);
	      printf("1024 <= port <= 65535\n");
	      exit(EXIT_ARGS);  	
  	}

	/* INIZIALIZZAZIONE INDIRIZZO SERVER ---------------------------------- */
	memset ((char *)&servaddr, 0, sizeof(struct sockaddr_in));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = INADDR_ANY;  
	servaddr.sin_port = htons(port);  

	/* CREAZIONE, SETAGGIO OPZIONI E CONNESSIONE SOCKET -------------------- */
	sd=socket(AF_INET, SOCK_DGRAM, 0);

	if(sd <0){perror("creazione socket "); exit(EXIT_SOCKET);}
	printf("Server: creata la socket, sd=%d\n", sd);

	if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))<0)
	{perror("set opzioni socket "); exit(EXIT_SOCKET);}
	printf("Server: set opzioni socket ok\n");

	if(bind(sd,(struct sockaddr *) &servaddr, sizeof(servaddr))<0)
	{perror("bind socket "); exit(EXIT_SOCKET);}
	printf("Server: bind socket ok\n");

	/* CICLO DI RICEZIONE RICHIESTE ------------------------------------------ */
	for(;;){
		//Indica la dimensione della struttra socket address.
		//Viene modificata da recvfrom
		lenAddress=sizeof(struct sockaddr_in);/* valore di ritorno */ 

		if (recvfrom(sd, &req, sizeof(Request), 0, (struct sockaddr *)&cliaddr, &lenAddress)<0)
		{perror("recvfrom "); continue;}

		//Qui cerco di ricavare info sul client per dirlo su stampa.
		clienthost=gethostbyaddr( (char *) &cliaddr.sin_addr, sizeof(cliaddr.sin_addr), AF_INET);

		if (clienthost == NULL) printf("client host information not found\n");
		else printf("Operazione richiesta da: %s %i\n", clienthost->h_name,(unsigned)ntohs(cliaddr.sin_port)); 

		/*EXEC*/
		//Cosa dobbiamo fare:
		//1)Verifica che esista il file.
		//2)In caso positivo apro il file e applico l'algoritmo.

		if ( ( fd = open(req.nomefile, O_RDONLY) ) < 0 ) {
        	//non riesco ad aprire il file
			//Mando come risposta l'errore della open.
			ris = fd;
    	}else{
			//File aperto

			while((nread = read(fd, &tmpChar, sizeof(char))) > 0){
				if(tmpChar != ' ' && tmpChar != '\n'){
					currentWordLen++;
				}else{
					if(currentWordLen > maxWordLen){
						maxWordLen = currentWordLen;
					}
					currentWordLen = 0;
				}
			}

			if(nread < 0 ){
				//non riesco a leggere il file
				//Mando come risposta l'errore della read.
				ris = nread;
			}else{
				//Sono riuscito a leggere, mando maxWordLen
				ris = maxWordLen;
			}

			close(fd);

		}

		ris=htons(ris);

		//Invio risposta.
		if (sendto(sd, &ris, sizeof(ris), 0, (struct sockaddr *)&cliaddr, lenAddress)<0)
		{perror("sendto "); continue;}

	} //for
}
