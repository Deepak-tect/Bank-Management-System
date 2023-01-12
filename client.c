#include <stdio.h> 
#include <sys/types.h>           
#include <fcntl.h>      
#include <sys/socket.h> 
#include <unistd.h>      
#include <string.h> 
#include <errno.h>     
#include <netinet/ip.h> 

void connection_handler(int sockFD); 

void main()
{
    int socketFD, connect_Status;
    struct sockaddr_in serverAddress;
    struct sockaddr server;

    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD == -1)
    {
        perror("Error while creating server socket!");
        _exit(0);
    }

    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);                // Binds the socket to all interfaces
    serverAddress.sin_family = AF_INET;                               // IPv4
    serverAddress.sin_port = htons(8081);                             // Server will listen to port 8080
    

    connect_Status = connect(socketFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
    if (connect_Status == -1)
    {
        perror("Error while connecting to server!");
        close(socketFD);
        _exit(0);
    }

    connection_handler(socketFD);

    close(socketFD);
}



void connection_handler(int sockFD)
{
    char temp_buffer[1000];
    char readBuffer[1000], writeBuffer[1000];              // A buffer used for reading from / writting to the server
    ssize_t readBytes, writeBytes;                         // Number of bytes read from / written to the socket

    do
    {
        bzero(temp_buffer, sizeof(temp_buffer));
        bzero(readBuffer, sizeof(readBuffer));                              // Empty the read buffer
        
        readBytes = read(sockFD, readBuffer, sizeof(readBuffer));
        if (readBytes == -1)
            perror("Error while reading from client socket!");
        else if (readBytes == 0)
            printf("No error received from server! Closing the connection to the server now!\n");
        else if (strchr(readBuffer, '^') != NULL)
        {
                                                                            // Skip read from client
            strncpy(temp_buffer, readBuffer, strlen(readBuffer) - 1);
            printf("%s\n", temp_buffer);
            writeBytes = write(sockFD, "^", strlen("^"));
            if (writeBytes == -1)
            {
                perror("Error while writing to client socket!");
                break;
            }
        }
        else if (strchr(readBuffer, '$') != NULL)
        {
                                              // Server sent an error message and is now closing it's end of the connection
            strncpy(temp_buffer, readBuffer, strlen(readBuffer) - 2);
            printf("%s\n", temp_buffer);
            printf("Closing the connection to the server now!\n");
            break;
        }
        else
        {
            bzero(writeBuffer, sizeof(writeBuffer));                        // Empty the write buffer

            if (strchr(readBuffer, '#') != NULL)
                strcpy(writeBuffer, getpass(readBuffer));
            else
            {
                printf("%s\n", readBuffer);
                scanf("%[^\n]%*c", writeBuffer);                            // Take user input!
            }

            writeBytes = write(sockFD, writeBuffer, strlen(writeBuffer));
            if (writeBytes == -1)
            {
                perror("Error while writing to client socket!");
                printf("Closing the connection to the server now!\n");
                break;
            }
        }
    } while (readBytes > 0);

    close(sockFD);
}
