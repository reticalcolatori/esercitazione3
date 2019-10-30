/* OpDatagram_client: richiede la valutazione di un'operazione tra due interi */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>

#define LINE_LENGTH 256

//Codici errore
#define EXIT_ARGS 1
#define EXIT_HOST_ERR 2

// client serverAddress portAddress

int main(int argc, char **argv)
{
	struct hostent *host;
	struct sockaddr_in clientaddr, servaddr;
	int  port;
	int sd;
	int lenNomeFile;
	int lenAddress;
	int ris;
	char nomefile[LINE_LENGTH];

	int i = 0;

	/* CONTROLLO ARGOMENTI ---------------------------------- */
	if(argc!=3){
		printf("Error:%s serverAddress serverPort\n", argv[0]);
		exit(EXIT_ARGS);
	}

	//Controllo sulla porta
	while( argv[2][i]!= '\0' ){
		if((argv[2][i] < '0') || (argv[2][i] > '9')){
			printf("Secondo argomento non intero\n");
			printf("Error:%s serverAddress serverPort\n", argv[0]);
			exit(EXIT_ARGS);
		}
		i++;
	} 

	//Conversione port a intero
	port = atoi(argv[2]);

	/* VERIFICA PORT e HOST */
	if (port < 1024 || port > 65535){
		printf("%s = porta scorretta...\n", argv[2]);
		exit(EXIT_ARGS);
	}

	/* INIZIALIZZAZIONE INDIRIZZO CLIENT E SERVER --------------------- */
	memset((char *)&clientaddr, 0, sizeof(struct sockaddr_in));
	clientaddr.sin_family = AF_INET;
	clientaddr.sin_addr.s_addr = INADDR_ANY;
	
	// Passando 0 ci leghiamo ad un qualsiasi indirizzo libero
	clientaddr.sin_port = 0;

	//Recupero l'indirizzo del server remoto attraverso la funzione gethostbyname.
	host = gethostbyname (argv[1]); 

	//Verifica host
	if (host == NULL){
		printf("%s not found in /etc/hosts\n", argv[1]);
		exit(EXIT_HOST_ERR);
	}else{
		//Inizializzazione server address
		memset((char *)&servaddr, 0, sizeof(struct sockaddr_in));
		servaddr.sin_family = AF_INET;
		servaddr.sin_addr.s_addr=((struct in_addr *)(host->h_addr_list[0]))->s_addr;
		servaddr.sin_port = htons(port);
	}

	/* CREAZIONE (e controllo) SOCKET ---------------------------------- */

	if((sd=socket(AF_INET, SOCK_DGRAM, 0)) < 0) { 
		perror("apertura socket");
		exit(1);
	}
	printf("Client: creata la socket sd=%d\n", sd);

	/* BIND SOCKET, a una porta scelta dal sistema --------------- */
	if(bind(sd,(struct sockaddr *) &clientaddr, sizeof(clientaddr))<0) {
		perror("bind socket ");
		exit(1);
	}
	printf("Client: bind socket ok, alla porta %i\n", clientaddr.sin_port);

	/* CORPO DEL CLIENT: ciclo di accettazione di richieste da utente */
	printf("nome file, EOF per terminare: ");

	while (gets(nomefile) != NULL )
	{
    	
		printf("Nome file: %s\n", nomefile);

		/* richiesta operazione */

		//lunghezza nome file:
		//Più 1 per il terminatore.
		lenNomeFile = strlen(nomefile)+1;

		//Lunghezza struttura indirizzo.
		lenAddress=sizeof(servaddr);

		if(sendto(sd, nomefile, sizeof(char)*lenNomeFile, 0, (struct sockaddr *)&servaddr, lenAddress)<0){
			perror("sendto");
			printf("nome file, EOF per terminare: ");
			continue;
		}

		/* ricezione del risultato */
		printf("Attesa del risultato...\n");
		
		if (recvfrom(sd, &ris, sizeof(ris), 0, (struct sockaddr *)&servaddr, &lenAddress)<0){
			perror("recvfrom");
			printf("nome file, EOF per terminare: ");
			continue;
		}

		//Conversione in formato locale.
		ris = (int) ntohs(ris);

		if(ris > 0){
			printf("Numero dei caratteri della parola più lunga : %i\n", ris);
		}else{
			printf("Errore dal server : %i\n", ris);
		}

		printf("nome file, EOF per terminare: ");

	} // while gets
	
	//CLEAN OUT
	close(sd);
	printf("\nClient: termino...\n");  

	return 0;
}
