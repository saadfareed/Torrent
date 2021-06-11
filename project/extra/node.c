/* Node is a client for Hub and a server for Client. */

#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netinet/ip.h>
#include<arpa/inet.h>
#include<pthread.h>
#include<errno.h>
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>

/* Message sent to hub */
struct hubmsg_t {
    int mtype;
    long port;
};

/* Message to/from client */
struct clientmsg_t {
    int mtype;
    char name[4000];
    long from; // pos or length
};

struct sockaddr_in address, client_addr, hub_addr;

void* handleClient(void* arg) 
{
	printf("Accepted connection...\n");	

	int sock = *((int*) arg);
	int i, len, f;
	char file[256];

	struct clientmsg_t clientmsg;
	struct clientmsg_t sent;

	struct sockaddr_in address; // client
	len = sizeof(address);	
	
	getpeername(sock, (struct sockaddr*)&address, &len);
	// new socket structure

	i = recv(sock, &clientmsg, sizeof(struct clientmsg_t), 0);
	if (i < 0) 
	{
		perror("[Node] recv from client error:");
		return NULL;
	}
	else printf("Received message...\n");

	if (clientmsg.mtype == 4)
	{
		/* Client requested stuff */

		strcpy(file, "source/");
		strcat(file,clientmsg.name);

		printf("Sending file %s...\n", file);

		/* Open file, bin mode, read-only. */
		f = open(file, O_RDONLY);

		if (f < 0)
		{
			printf("[Node] Error opening the file '%s'\n", file);
			perror(":");
			return NULL;
		}

		if (lseek(f, clientmsg.from, SEEK_SET) < 0)
		{
			printf("Can't seek in %s.\n",file);
			return NULL;
		}
		
		sent.mtype = 5;
		sent.from = read(f, sent.name, 100);

		if (sent.from < 0)
		{
			printf("Error reading file %s.\n", file);
			return NULL;
		}

		int ok;
		ok = send(sock, &sent, sizeof(struct clientmsg_t), 0);

		if (ok<0) 
		{
			perror("[Node] Send to client error:"); 
			return NULL;
		}
		
	}
	else
	{
		/* Unrecognized message (0 = Client finished) */
	}

return NULL;
}


int main() {
        printf("START WITH THE NAME OF ALLAH\n");
	int sock, sockhub, sock_client;
	int i, len;

	struct hubmsg_t hubmsg;
	struct clientmsg_t clientmsg;

	pthread_t thr;

	printf("Starting...\n");

	sockhub = socket(AF_INET, SOCK_STREAM, 0);
	if (sock<0) {
		perror("[Node] Error creating:");
	}	

	printf("Created sock...\n");

	/* First of all, the node must connect, i.e. send message to the hub. */

	hub_addr.sin_family = AF_INET;
	hub_addr.sin_addr.s_addr = INADDR_ANY;
	hub_addr.sin_port = htons(1026);

	if (connect(sockhub, (struct sockaddr*) &hub_addr, sizeof(struct sockaddr))<0) {
		perror("[Node] Connect error:");
	}

	struct timeval tv;
	struct timezone tz;

	gettimeofday(&tv, &tz);

	srand(tv.tv_usec);

	hubmsg.mtype = 1;
	hubmsg.port = 1026 + rand()%1000;

	printf("Created message to send...\n");

        i = send(sockhub, &hubmsg, sizeof(struct hubmsg_t), 0);
	if (i<0) perror("[Node] can't send to hub :");

	printf("Sent.\n");
	
	/* Second of all, the node must be able to accept connections from clients.
	I.e. create a new socket, as the socket used for the communication with the hub cannot be reused.
	(as it already acts like a client, and we need one to act like a server) */

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock<0) {
		perror("[Node] Error creating:");
	}

	printf("Created second socket...\n");
	
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(hubmsg.port);

	if (bind(sock, (struct sockaddr*) &address, sizeof(struct sockaddr))<0) {
		perror("[Node] Error binding");
	}

	printf("Binded...\n");

	if (listen(sock, 20) < 0)
		perror("[Node] won't listen");

	printf("Listening...\n");
	
	while(1) {
		len = sizeof(struct sockaddr_in);
		sock_client = accept(sock, (struct sockaddr*) &client_addr, &len);
		pthread_create(&thr, 0, handleClient, &sock_client);
	sleep(1);
	} 
close(sock_client);
close(sockhub);
close(sock);
return 0;
}

