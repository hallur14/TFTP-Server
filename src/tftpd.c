#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

enum opCode
{
    RRQ = 1,
    WRQ,
    DATA,
    ACK,
    ERROR
};

unsigned short serverBlockNumber = 1;
char path[4096];
FILE *filePtr;
unsigned char packageToSend[516];

// Method declaration
void processRequest(int opCode, ssize_t *dataLength);

int main(int argc, char *argv[])
{
    int sockfd;
    struct sockaddr_in server, client;
    unsigned char message[512];    
    char filename[4096];
    int myPort = atoi(argv[1]);
    char *folderName = argv[2];
    char mode[10]; // Keep track of the TransferMode of the data.
    int clientOpcode;
    unsigned short clientBlockNumber; // The block number from the client
    ssize_t dataLength; // Keep track of the size of the data portion of packages.    
    char fullPath[4096];

   //Get the path to where your files are kept
    getcwd(path, 4096);   
    
    // Check if we get port as parameter
    if(argc < 2 ) 
    {
	    printf("Port and data folder needed as parameters");
		exit(0);
    }

    // Create and bind a UDP socket.
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;

    // Network functions need arguments in network byte order instead
    // of host byte order. The macros htonl, htons convert the values.
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(myPort);
    bind(sockfd, (struct sockaddr *) &server, (socklen_t) sizeof(server));

    for (;;) 
    {
	    memset(fullPath, 0, sizeof(fullPath));
        memset(message, 0, sizeof(message));       
 
        // be NUL-terminated later.
        socklen_t len = (socklen_t) sizeof(client);
        ssize_t n = recvfrom(sockfd, message, sizeof(message) - 1, 0, (struct sockaddr *) &client, &len);

        message[n] = '\0';
	    
        // Set the cliet opcode
        clientOpcode = message[1];

        // Checks if we recieved ERRROR opCode
	    if(clientOpcode == ERROR)
        {
            printf("Revieved error opCode, abort transfer");
            exit(0);
        }
        
        // Checks if we recieved ACK, RRQ or WRQ opCode
        if(clientOpcode == ACK)
	    {        
	        clientBlockNumber = (((short int)message[2]) << 8 ) | message[3]; 
        }
        else if(clientOpcode == RRQ || clientOpcode == WRQ)
        {
            // Copy the rest of the message into filename 
	        strncpy((char*)filename, (char*)&message[2], sizeof(filename) - 1);
            
            // Get the transfer mode of the message
	        strncpy((char*)mode, (char*)&message[strlen(filename)+3], sizeof(mode));
            
            // Create a path to our file
            strcpy(fullPath, path);
            strncat(fullPath, "/", sizeof(fullPath)-1);
            strncat(fullPath, folderName, sizeof(fullPath)-1);
            strncat(fullPath, "/", sizeof(fullPath)-1);
            strncat(fullPath, filename, sizeof(fullPath)-1); 
	    }

	    if(clientOpcode == RRQ)
	    { 
            if(!strcmp("netascii", mode))
    	    {
        	    filePtr = fopen(fullPath, "r");
    	    }
    	    else
    	    {
            	filePtr = fopen(fullPath, "rb"); 
    	    }
	        
            if(filePtr == NULL)
    	    {	
		        exit(0);
    	    }	
	        
            processRequest(DATA, &dataLength);
	    }   

	    if(clientOpcode == ACK)
	    {
            if(serverBlockNumber == clientBlockNumber)
	        {
	            serverBlockNumber++;        
                processRequest(DATA, &dataLength);
	        }     
	    }

        fflush(stdout);

        sendto(sockfd, packageToSend,dataLength + 4, 0,
               (struct sockaddr *) &client, len);
        
        if(dataLength < 512)
	    {
	        fclose(filePtr);
            printf("Transfer complete\n");
            exit(0);
	    }
    }
}

// A method that creates the package to be sent to the client,
void processRequest(int opCode, ssize_t *dataLength)
{             
    memset(packageToSend, 0, sizeof(packageToSend));	
    unsigned char fileBuffer[516];
    char tempBuffer[512];

    // Fills the fileBuffer with necessary opCode and server blocknumber
    fileBuffer[0] = (unsigned char)0;
    fileBuffer[1] = (unsigned char)opCode;
    fileBuffer[2] = (unsigned char) ((serverBlockNumber >> 8 ) & 0xFF);	     
    fileBuffer[3] = (unsigned char) ((serverBlockNumber & 0x00FF));

    //printf("Loading data into buffer \n");
    *dataLength = fread(tempBuffer, 1, 512, filePtr);
    
    // Loads the fileBuffer with the data from the file
    for(int i = 4; i < 516; i++)
    {
        fileBuffer[i] = tempBuffer[i-4];
    }
    
    // Adds 0 at the end of the char array
    packageToSend[515] = '\0';
    
    // packageToSend global variable is filled with opcode, blocknumber and data package
    for(int i = 0; i < 516; i++)
    {
    	packageToSend[i] = fileBuffer[i];
    }
}