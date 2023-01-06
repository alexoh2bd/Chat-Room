/*
 * echo-client.c
 */

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>

#define BUF_SIZE 4096

void *client_recv_handler(void *dt);
void *client_send_handler(void *dt);


int main(int argc, char *argv[])
{
    char *dest_hostname, *dest_port;
    struct addrinfo hints, *res;
    int conn_fd;
    int rc;

    dest_hostname = argv[1];
    dest_port     = argv[2];


    //create a socket
    if((conn_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1){ 
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /* but we do need to find the IP address of the server */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if((rc = getaddrinfo(dest_hostname, dest_port, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(rc));
        exit(EXIT_FAILURE);
    }

    /* connect to the server */
    if(connect(conn_fd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    printf("Connected\n");

    // Create thread for receiving messages from the server
    pthread_t recv_thread;
    if(pthread_create(&recv_thread, (void *)NULL, client_recv_handler, &conn_fd)!=0){  
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    // Create thread for sending messages to the server
    pthread_t send_thread;
    if(pthread_create(&send_thread, (void *)NULL, client_send_handler, &conn_fd)!=0){  
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }


    void **retval = (void *)NULL;
    pthread_join(send_thread, retval);
    printf("The send thread has returned.\n");

    close(conn_fd);
    printf("Closing Client.");
}

// Thread for handling messages from the server
void *client_recv_handler(void *dt){
    
    int bytes_received;
    char buf[BUF_SIZE];
    int *ptr = dt;
    int conn_fd = *ptr;

    // Listens to the incoming message and prints 
    while((bytes_received = recv(conn_fd, buf, BUF_SIZE, 0)) > 0){

        printf("%s\n",buf);
        fflush(stdout);

    }

    printf("Server Disconnected.\n");
    exit(EXIT_SUCCESS);
    pthread_exit((void *)NULL);
}

void *client_send_handler(void *dt){

    int bytes_read;
    char buf[BUF_SIZE];
    int *ptr = dt;
    int conn_fd = *ptr;

    // Sends client's message to server
    while((bytes_read = read(0, buf, BUF_SIZE)) > 0) {

        buf[bytes_read-1] = '\0';
        send(conn_fd, buf, bytes_read, 0);

    }

    // Exit the recv handler thread
    printf("Server has disconnected.\n");
    pthread_exit((void *)NULL);
}
