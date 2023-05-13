/* Program: producer.c
* A simple TCP server using sockets.
* Server is executed before Client.
ICSI 412 Operating Systems Spring 2023
* Port number is to be passed as an argument.
*
* To test: Open a terminal window.
* At the prompt ($ is my prompt symbol) you may
* type the following as a test:
*
* $ ./producer 54554
* Run client by providing host and port
*
*
*/
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close
#include <ctype.h>
void error(const char *msg)
{
	perror(msg);
	exit(1);
}

sem_t r;
sem_t cR;
sem_t tU;
sem_t w;

int retReader;
int retCharReplace;
int retToUpper;
int retWriter;

int readerFinished = 0;
int charReplaceFinished = 0;
int toUpperFinished = 0;
int writerFinished = 0;

void *readFile(void *);
void *charReplace(void *);
void *toUpper(void *);
void *writer(void *);

struct Node
{
	char value;
	struct Node * next;
};

struct Buffer
{
	int count;
	char b[10];
	struct Node * head;
	struct Node * tail;
}r_b,c_b,t_b;

void iterate(struct Buffer * buffer)
{
	struct Node * cur = buffer->head;
	while(cur != NULL)
	{
		printf("Node val: %c\n",(char)cur->value);
		cur = cur->next;
	}
	printf("\n");
}

int tryPush(char val, struct Buffer * buffer)
{
	int success = -1;
	if(buffer->count < 10)
	{
		//printf("ADDING %c TO BUFFER\n",val);
		//int i = 0;
		//while(buffer->b[i] != '\0')
		//{
		//	i++;
		//}
		//buffer->b[i] = val;
		//buffer->count++;
		//success = 1;
		// Linked list
		struct Node * newChar = (struct Node*)malloc(sizeof(struct Node));
		newChar->value = val;
		newChar->next = NULL;
		if(buffer->head == NULL && buffer->tail == NULL)
		{
			buffer->head = newChar;
			buffer->tail = newChar;
		}
		else
		{
			struct Node * curTail = buffer->tail;
			curTail->next = newChar;
			buffer->tail = newChar;
		}
		//iterate(buffer);
		buffer->count++;
		success = 1;
	}
	//printf("BUFFER CONTENTS: \n");
	for(int i = 0; i < 10; i++)
	{
		//printf("[%d] %c\n", i, buffer->b[i]);
	}
	return success;
}

int pop(struct Buffer * buffer)
{
	int val;
	if(buffer->count != 0)
	{
		//printf("REMOVING FIRST VALUE FROM BUFFER\n");
		//val = buffer->b[0];
		//for(int i = 0; i < 9; i++)
		//{
		//	char next = buffer->b[i+1];
		//	buffer->b[i] = next;
		//}
		//buffer->b[9] = '\0';
		//buffer->count--;
		//Linked List
		val = buffer->head->value;
		struct Node * newHead = buffer->head->next;
		//free(buffer->head);
		buffer->head = newHead;
		//iterate(buffer);
		buffer->count--;
	}
	//printf("BUFFER CONTENTS: \n");
	for(int i = 0; i < 10; i++)
	{
		//printf("[%d] %c\n", i, buffer->b[i]);
	}
	//printf("VAL: %c\n", (char)val);
	return val;
}

void *readFile(void *filePath)
{
	int input;
	FILE *fp = fopen(filePath, "r");
	
	char line[255];
	
	//printf("r:BEGIN READING FILE\n");
	
	while(fgets(line, sizeof(line), fp) != NULL)
	{
		//printf("%s\n", line);
		int i = 0;
		while(line[i] != '\0')
		{
			//printf("r:ADDING CHAR %c TO BUFFER r_b\n",line[i]);
			//If we try to add to the buffer but the buffer is full			
			while((tryPush(line[i], &r_b) != 1))
			{	
				//printf("r:BUFFER r_b FULL... TELLING charReplace OK to RUN\n");
				//Tell the charReplace Thread it is ok to grab from the queue
				sem_post(&cR);
				//printf("r:WAITING for OK to RUN\n");
				//And wait for charReplace to tell us its ok to resume
				sem_wait(&r);
				//charReplace says its safe to try again so we block charReplace so that the buffer is safe.
			}
			//printf("r:CHAR %c ADDED TO BUFFER r_b\n",line[i]);
			i++;
		}
	}
	
	//printf("r:EXITING\n");
	//While cR is still blocked raise the flag saying we are finished
	readerFinished = 1;
	//Tell the charReplace Thread it is ok to grab from the queue
	sem_post(&cR);
}

void *charReplace(void * c)
{
	//printf("cR:	WAITING FOR OK TO BEGIN\n");
	//Wait for permission to run because readFile needs to begin filling the buffer
	sem_wait(&cR);
	//printf("cR:	RUNNING\n");
	while(r_b.count > 0)
	{
		//Grab character from buffer
		//printf("cR:	GRABBING VAL FROM BUFFER r_b\n");
		char val = (char)pop(&r_b);
		if(val == ' ')
		{
			val = (char)c;
		}
		//printf("cR:	TELLING reader OK to RUN\n");
		// Tell the reader thread it is ok to run
		sem_post(&r);
		//printf("cR:	ADDING %c TO BUFFER c_b\n", val);
		//Push the new character into the charReplace Buffer
		while((tryPush(val, &c_b) != 1))
		{
			//printf("cR:	c_b FULL\n");
			//printf("cR:	TELLING toUpper OK to RUN\n");
			// Tell the toUpper thread it is ok to run
			sem_post(&tU);
			//printf("cR:	WAITING FOR OK TO RESUME\n");
			//Wait to try again
			sem_wait(&cR);
		}
		//printf("cR:	%c ADDED TO BUFFER c_b\n", val);
		// if the reader finished executing only wait on the writer
		//if(readerFinished == 0 )
		if(readerFinished == 0)
		{
			//printf("cR:	WAITING FOR OK TO RESUME\n");
			sem_wait(&cR);
		}	
	}
	//printf("cR:	EXITING\n");
	charReplaceFinished = 1;
	sem_post(&tU);
}

void *toUpper(void *fileLine)
{
	//printf("tU:		WAITING FOR OK TO BEGIN\n");
	//wait until someone says we can run
	sem_wait(&tU);
	while(c_b.count > 0)
	{	
		//printf("tU:		GRABBING VAL FROM BUFFER c_b\n");
		//Grab character from buffer
		char val = (char)pop(&c_b);
		if(isalpha(val))
		{
			val = toupper(val);
		}
		//printf("tU:		TELLING charReplace OK to RUN\n");
		// Tell the characterReplace thread it is ok to run
		sem_post(&cR);
		//printf("tU:		ADDING %c to BUFFER t_b\n",val);
		//Push the new character into the charReplace Buffer
		while((tryPush(val, &t_b) != 1))
		{
			//printf("tU:		BUFFER t_b FULL\n");
			//printf("tU:		TELLING writer OK to RUN\n");
			// Tell the writer thread it is ok to run
			sem_post(&w);
			//printf("tU:		WAITING FOR OK TO RESUME\n");
			sem_wait(&tU);
		}
		//printf("tU:		%c ADDED to BUFFER t_b\n",val);
		if(charReplaceFinished == 0)
		{
			//printf("tU:		WAITING FOR OK TO RESUME\n");
			sem_wait(&tU);
		}
	}
	//printf("tU:		EXITING\n");
	toUpperFinished = 1;
	// Tell the writer thread it is ok to run
	sem_post(&w);
}

void *writer(void *buffer)
{
	
	//File Setup
	int outFile;
	outFile = open("Output.txt", O_TRUNC | O_CREAT | O_RDWR, S_IRWXU);
	//File Buffer Setup
	char outBuffer[512];
	int i = 0;
	//printf("w:			WAITING FOR OK TO BEGIN\n");
	//wait until someone says we can run
	sem_wait(&w);
	while(t_b.count > 0)
	{	
		//printf("w:			GRABBING VAL FROM BUFFER c_b\n");
		//Grab character from buffer
		char val = (char)pop(&t_b);
		outBuffer[i++] = val;
		//printf("w:			TELLING toUpper OK to RUN\n");
		// Tell the toUpper thread it is ok to run
		sem_post(&tU);
		if(toUpperFinished == 0)
		{
			//printf("w:			WAITING FOR OK TO RESUME\n");
			// Wait until its ok to run again
			sem_wait(&w);
		}
	}
	
	// Before we exit write the contents of our outbuffer to the outfile
	write(outFile, outBuffer, strlen(outBuffer));
	close(outFile);
	//printf("w:			EXITING\n");
	writerFinished = 1;
}

int main(int argc, char *argv[]) 
{
	retReader = sem_init(&r, 0, 0);
	retCharReplace = sem_init(&cR, 0, 0);
	retToUpper = sem_init(&tU, 0, 0);
	retWriter = sem_init(&w, 0, 0);
	
	pthread_t thR, thCR, thTU, thW;
	
	
	int sockfd, newsockfd, portno;
	int end = 0;
	socklen_t clilen;
	char buffer[256];
	char filename[32];
	char fileLocation[256];
	char character;
	struct sockaddr_in serv_addr, cli_addr;
	int n;
	if (argc < 2) {
		fprintf(stderr,"ERROR, not enough arguments\n");
		exit(1);
	}
	fprintf(stdout, "Run client by providing host and port\n");
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket");
	bzero((char *) &serv_addr, sizeof(serv_addr));
	portno = atoi(argv[1]);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		error("ERROR on binding");
	listen(sockfd,5);
	clilen = sizeof(cli_addr);
	while(!end)
	{
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
		if (newsockfd < 0)
			error("ERROR on accept");
		bzero(buffer,256);
		n = read(newsockfd,buffer,255);
		if (n < 0)
			error("ERROR reading from socket");
		//printf("Here is the message: %s\n",buffer);
		
		// Parse the client sent string
		int i = 0;
		//parse command
		while(i < strlen(buffer) && buffer[i] != ' ')
		{
			filename[i] = buffer[i];
			i++;
		}
		filename[i] = '\0';
		//printf("FileName: %s\n", filename);
		i++;
		//printf("Begin Parsing Arguments\n");
		int j = 0;
		//parse arguments
		while(i < strlen(buffer) && (buffer[i] != ' '))
		{
			//printf("Adding Character: %c\n", buffer[i]);
			fileLocation[j] = buffer[i];
			i++;
			j++;
		}
		fileLocation[j] = '\0';
		j = 0;
		
		
		//printf("Begin Parsing 2 Argument\n");
		i++;
		//parse arguments
		while(i < strlen(buffer) && buffer[i] != ' ' && buffer[i] != '\n')
		{
			//printf("Adding Character: %c\n", buffer[i]);
			character = buffer[i];
			i++;
			j++;
		}
		// Parse the client sent string
		//printf("Filename: %s\n", filename);
		//printf("File Location: %s\n", fileLocation);
		//printf("Char: %c\n", character);
		
		// Thread Stuff
		
		// Process File
		pthread_create( &thR, NULL, readFile, (void*)fileLocation);
		pthread_create( &thCR, NULL, charReplace, (void*)character);
		pthread_create( &thTU, NULL, toUpper, NULL);
		pthread_create( &thW, NULL, writer, NULL);
		
		pthread_join(thR,NULL);
		pthread_join(thCR, NULL);
		pthread_join(thTU,NULL);
		pthread_join(thW, NULL);
		
		//readFile("testInput.txt");
		//tryPush('H',&r_b);
		//tryPush('O',&r_b);
		//tryPush('T',&r_b);
		//tryPush('E',&r_b);
		//tryPush('L',&r_b);
		//char val;
		//val = (char)pop(&r_b);
		//End Thread Stuff
		
		bzero(filename,32);
		bzero(fileLocation,256);
		character = '\0';
		
		//Write to client
		n = write(newsockfd,"Output.txt Output.txt",21);
		if (n < 0)
			error("ERROR writing to socket");
	}
	
	close(newsockfd);
	close(sockfd);
	return 0;
}
