/* Client is a client for both Node and Hub. */
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netinet/ip.h>
#include<arpa/inet.h>
#include<errno.h>
#include<stdio.h>
#include<string.h>
#include<sys/time.h>
#include<stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>


struct node {
     struct sockaddr_in address;
     long port;
     int del;
};

struct node nodeList[10];

/* Client to hub -> Request List */
struct hubmsg_t {
    int mtype;
    long port;
};

/* Hub to client -> Node List */
struct listmsg_t {
    int mtype;
    struct node nodeList[10];
};
struct listmsg_t listmsg;

/* Message to/from node */
struct nodemsg_t {
    int mtype;
    char name[4000]; // name or data
    long from; // pos or length
};

int num = 0;
char filename[128];
long from;
int get;

pthread_mutex_t mutexFrom = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexGet = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condGet = PTHREAD_COND_INITIALIZER;

/* Subprogram to download the file */

void *handleFile (void* arg)
{
	int no = *((int *)arg);
	struct nodemsg_t nodemsg;
	struct nodemsg_t recm;
	int socknode, len, i, j, f;	
	
	while(1)
	{
		socknode = socket(AF_INET, SOCK_STREAM, 0); // client's
		if (socknode<0) {
			perror("[Client] Error creating:");
	}	

	printf("Created sock...\n");
	
	/* Manipulate! */
	listmsg.nodeList[no].address.sin_port = htons(listmsg.nodeList[no].port);

		if (connect(socknode, (struct sockaddr*) &listmsg.nodeList[no].address, sizeof(struct sockaddr)) < 0) {
			printf("[Client] Connect error at %d, %s :: %d :\n",no, inet_ntoa(listmsg.nodeList[no].address.sin_addr), listmsg.nodeList[no].address.sin_port);
			perror(":");
			break;
		}

	printf("Connected to [%d] %s : %ld\n", no, inet_ntoa(listmsg.nodeList[no].address.sin_addr),listmsg.nodeList[no].port);

	nodemsg.mtype = 4;
	nodemsg.from = -1;
	strcpy(nodemsg.name,filename);

	/* Set the position to read from. */
	pthread_mutex_lock(&mutexFrom);
	if (from >= 0)
	{
		nodemsg.from = from;
		from = from + 100;
	}
	pthread_mutex_unlock(&mutexFrom);

	if (nodemsg.from < 0)
	{
		close(socknode);
		return;
	}	

        i = send(socknode, &nodemsg, sizeof(struct nodemsg_t), 0);
	if (i<0) { perror("[Client] can't send to node :"); break; }

	printf("[Client] Sent to node.\n");

	i = recv(socknode, &recm, sizeof(struct nodemsg_t), 0);

	if (i<0)
	{
		printf("Error when downloading from %d. >:(\n",i);
		break;
	}

	printf("[Client] Got from node.\n");
        
	if (recm.from == 0)
	{
		/* Downloaded full file. */
		pthread_mutex_lock(&mutexFrom);
		from = -1;
		pthread_mutex_unlock(&mutexFrom);
		close(socknode);
		return;
	}
	
	pthread_mutex_lock(&mutexGet);
	while (get > 0)
		pthread_cond_wait(&condGet, &mutexGet);
	get = 1;
	/* Access to the file */
	f = open(filename, O_WRONLY);
	if (f < 0)
	{
		printf("Cannot open file. %d\n", no);
		break;
	}
	
	if (lseek(f, nodemsg.from, SEEK_SET) < 0)
	{
		printf("Can't seek in file.%d\n",no);
		break;
	}

	if (write(f, recm.name, recm.from) != recm.from)
	{
		printf("Can't write properly in file.%d\n",no);
		break;
	}

	close(f);
	
	get = 0;
	pthread_cond_broadcast(&condGet);
	pthread_mutex_unlock(&mutexGet);

	close(socknode);
	sleep(1);
	}
	
}

int main()
{
        printf("START WITH THE NAME OF ALLAH\n");
	struct sockaddr_in hub_addr;
	int sockhub;
	int len, i;

	struct hubmsg_t hubmsg;
	struct nodemsg_t nodemsg;

	/* First, the client requests the list of nodes from the Hub.*/

	sockhub = socket(AF_INET, SOCK_STREAM, 0);
	if (sockhub<0) {
		perror("[Client] Error creating socket:");
	}
	
	hub_addr.sin_family = AF_INET;
	hub_addr.sin_addr.s_addr = INADDR_ANY;
	hub_addr.sin_port = htons(1026); // hub port

	if (connect(sockhub, (struct sockaddr*) &hub_addr, sizeof(struct sockaddr))<0) {
		perror("[Client] Connect error:");
	}


	hubmsg.mtype = 2;
	hubmsg.port = -1;

	printf("Created message to send...\n");

        i = send(sockhub, &hubmsg, sizeof(struct hubmsg_t), 0);
	if (i<0) perror("[Node] can't send to hub :");

	printf("Sent.\n");

	i = recv(sockhub, &listmsg, sizeof(struct listmsg_t), 0);
	if (i<0) perror("[Node] can't recv from hub :");
	
	printf("Got message.\n");

	*((struct node*)nodeList) = *((struct node*)listmsg.nodeList);

	printf("Stored list locally.\n");

	i = 0;
	printf("List of available nodes:\n");	
	while (listmsg.nodeList[i].address.sin_port != 0)
	{	
		printf("[%d] %s : %ld\n", i+1, inet_ntoa(listmsg.nodeList[i].address.sin_addr),listmsg.nodeList[i].port);
		i = i + 1;
	}
	num = i;
 
	/* Then, the client must connect to all of the available nodes. */

	/* Client enters the name of the file to be downloaded...*/
	printf("%d nodes. \nPlease enter the name of the file you want to download:\n", num);
	gets(filename);

	int f;
	/* Create file */
	f = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0777);

	if (f < 0)
	{
		printf("Couldn't create %s\n",filename);
		return 1;
	}	
	close(f);

	/* That's right, we need num threads. How awesome is that?*/

	struct sockaddr_in address;
	int sock;

	pthread_t thr[num];
	i = 0;
	while (i < num && listmsg.nodeList[i].address.sin_port != 0)
	{
		if (pthread_create(&thr[i], 0, handleFile, &i) != 0)
		{
			printf("Error creating thread %d.\n",i);
		}
	sleep(1);
	i = i + 1;
	}

	i = 0;	
	while (i < num && listmsg.nodeList[i].address.sin_port != 0)
	{
		pthread_join(thr[i], NULL);
	i=i + 1;
	}

	printf("Your File is 100 downloaded.\n");
	

char s[1];
gets(s);	

i = 0;

close(sockhub);
return 0;
}
