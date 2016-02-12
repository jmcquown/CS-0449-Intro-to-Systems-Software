#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

void *threadFcn(void *ptr);

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

//This struct holds the information that I want that is initialized in main
//but that needs to be accessed outside of main
struct stats_struct {
	int cfd;
	unsigned short port;
	char *ipstr;
};

int main() {
	pthread_t thread;
	int threadID;

	//Create socket
	int sfd = socket(PF_INET, SOCK_STREAM, 0);
	if (sfd == -1) {
		printf("Socket error");
		exit(0);
	}

	int len, connfd;
	unsigned short port;
	char *ipstring;

	struct sockaddr_in addr;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(50023);
	addr.sin_addr.s_addr = INADDR_ANY; //automatically find IP

	//Bind
	int bindReturn = bind(sfd, (struct sockaddr *)&addr, sizeof(addr));
	if (bindReturn == -1) {
	  printf("Bind Error\n");
	  exit(0);
	}

	len = sizeof(addr);
	port = ntohs(addr.sin_port);  // ntohs converts from network endian order to host endian order

	int count = 0;
	//Accept only 15 simultaneous connections
	while (count < 15) {
		//Listen
		int listenReturn = listen(sfd, 10);
		if (listenReturn == -1) {
		  printf("Listen Error\n");
		  exit(0);
		}

		//Accept
		connfd = accept(sfd, (struct sockaddr *)&addr, &len);
		if (connfd == -1) {
		  printf("Accept Error\n");
		  exit(0);
		}
		ipstring = inet_ntoa(addr.sin_addr);  // inet_ntoa returns dotted string notation of 32-bit IP address

		//Initialize the elements of the struct to the current connection, port, and ip address
		struct stats_struct stats;
		stats.cfd = connfd;
		stats.port = port;
		stats.ipstr = ipstring;

		threadID = pthread_create(&thread, NULL, threadFcn, (void *) &stats);
		pthread_join(thread, NULL);

		count++;
	}
	
	close(connfd);
	close(sfd);
	
}


void *threadFcn(void *ptr) {
	//Set all the struct elements to their approriate values
	struct stats_struct *stats = ptr;
	int connfd = stats->cfd;
	char *ipstring = stats->ipstr;
	unsigned short port = stats->port;

	//Get the message that the user sends and store it in a char array getMessage
	char getMessage[50], sendMessage[10000];
	int recvLength = recv(connfd, getMessage, sizeof(getMessage), 0);
	getMessage[recvLength] = '\0';
	// getMessage[strlen(getMessage) + 1] = "\0";
	printf("%s\n", getMessage);

	//Create three new char arrays to hold the inputted GET command information
	char html[25], http[25], get[3];
	char *token;

	//Tokenize the user input and store it into the three char arrays
	token = strtok(getMessage, " ");
	strcpy(get, getMessage);
	if (strcmp(get, "GET") != 0)
		exit(0);
	printf("GET check: %s\n", get);
	

	token = strtok(NULL, " ");
	int i;
	//I use a for loop to get rid of the '/' character at the beginnning of the file name
	strcpy(html, token + 1);
	printf("html: %s\n", html);

	//Check if the .html file is in the local directory
	if (access(html, F_OK) == -1) {
		printf("HTTP/1.1 404 Not Found\n");
		exit(0);	//If not exit program
	}
	//Else reply with this message
	else 
		strcpy(sendMessage, "HTTP/1.1 200 OK\n");


	token = strtok(NULL, " ");
	strcpy(http, token);
	printf("HTTP: %s\n", http);

	//After the get request, recv the host message
	memset(getMessage, '\0', sizeof(getMessage));
	recvLength = recv(connfd, getMessage, sizeof(getMessage), 0);
	getMessage[recvLength] = '\0';
	printf("%s\n", getMessage);

	//host will hold the HOST command, url will hold the url that comes after the HOST command
	char host[5], url[100];
	token = strtok(getMessage, " ");
	strcpy(host, getMessage);
	//Check if inputt starts with Host:
	if (strcmp(host, getMessage) != 0) {
		printf("Host not found\n");
		exit(0);	//if not exit program
	}

	token = strtok(NULL, " ");
	strcpy(url, token);
	printf("url check: %s\n", url);

	//Send the HTTP ok message
	send(connfd, sendMessage, strlen(sendMessage), 0);

	//Time stuff
	time_t currTime;
	struct tm *locTime;
	char timeBuffer[40];

	//Current Time
	currTime = time (NULL);
	//Local Time
	locTime = localtime (&currTime);
	
	//Send the time in the specified format
	strftime(timeBuffer, sizeof(timeBuffer), "Date: %a, %d %b %Y %X\n", locTime);
	send(connfd, timeBuffer, strlen(timeBuffer), 0);
	

	//Open the .html file and seek to the end
	FILE *file;
	file = fopen(html, "r");
	fseek(file, 0L, SEEK_END);
	int sizeOfFile = ftell(file);	//Get the length (in bytes) of the file
	fseek(file, 0, SEEK_SET);	//Seek back to the beginning
	

	//Send the length of the file
	memset(sendMessage, '\0', sizeof(sendMessage));
	strcpy(sendMessage, "Content-Length: ");
	char temp[5];
	sprintf(temp, "%d \n", sizeOfFile);
	strcat(sendMessage, temp);
	// strcat(sendMessage, " \n");
	send(connfd, sendMessage, sizeof(sendMessage), 0);

	memset(sendMessage, '\0', sizeof(sendMessage));
	strcpy(sendMessage, "Connection: close\nContent-Type: text/html\n");
	send(connfd, sendMessage, sizeof(sendMessage), 0);

	//Read the file and send the contents to the user
	memset(sendMessage, '\0', sizeof(sendMessage));
	char *fileContents = malloc(sizeOfFile + 1);
	fread(fileContents, sizeOfFile, 1, file);
	// printf("%s\n", fileContents);
	strcpy(sendMessage, fileContents);
	strcat(sendMessage, "\n");
	send(connfd, sendMessage, sizeof(sendMessage), 0);

	fclose(file);

	FILE *statsFile = fopen("stats.txt", "a");
	//This is directly from the FAQ

	//Lock when writing to the file
	pthread_mutex_lock(&mutex);

	fprintf(statsFile, "GET /%s %s", html, http);
	fprintf(statsFile, "Host: %s", url);
	fprintf(statsFile, "Client: %s:%u\n", ipstring, port);

	//Unlock when we are finished writing to the file

	pthread_mutex_unlock(&mutex);
	fclose(statsFile);


}