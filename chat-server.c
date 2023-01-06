#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define BACKLOG 10
#define BUF_SIZE 4096
#define MAXNAME 16

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

struct conn_node{
    int socket;
    int conn_id;
    char *remote_ip;
    char *nickname;
    uint16_t remote_port;
    struct conn_node *next;
};

void delete(struct conn_node* ptr);
void *conn_handler(void *dt);

struct conn_node *head = (void *)NULL;

int main(int argc, char *argv[])
{
    char *listen_port;
    int listen_fd, conn_fd;
    struct addrinfo hints, *res;
    int rc;
    struct sockaddr_in remote_sa;
    socklen_t addrlen;
    int numthreads = 0;

    // Create socket
    listen_port = argv[1];
    if((listen_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1){
        perror("client: socket");
        exit(EXIT_FAILURE);
    }

    // Establish hints
    memset(&hints, 0 , sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if((rc = getaddrinfo((void *)NULL, listen_port, &hints,&res)) != 0){
        printf("getaddrinfo failed: %s\n", gai_strerror(rc));
        exit(1);
    }

    // Bind 
    if((bind(listen_fd, res -> ai_addr, res -> ai_addrlen)) == -1){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // Checks if listen_fd socket is listening for connections
    if(listen(listen_fd, BACKLOG)==0){
        puts("Listening\n");
    }
    else{
        puts("Overflow\n");
    }
    
    /* infinite loop of accepting new connections and handling them */
    while(1){
        /* accept a new connection (will block until one appears) */
        addrlen = sizeof(remote_sa);
        if((conn_fd = accept(listen_fd, (struct sockaddr *) &remote_sa, &addrlen)) != -1){
            
            // create new node
            struct conn_node *node = malloc(sizeof(struct conn_node));
            // if this is the first connection
            if(head == (void *)NULL){ 
                head = node;
                node->next = (void *)NULL;
            }else{ //find last node, make the last node point to the new node;
                struct conn_node *curNode = head;
                while(curNode->next != (void *)NULL){
                    curNode = curNode->next;
                }
                curNode->next = node;
                node->next = (void *)NULL;
            }
            // Declare connection struct variables
            numthreads++;
            node->nickname = (char *)malloc(MAXNAME);
            strncpy(node->nickname, "unknown", MAXNAME);
            node->conn_id = numthreads;
            node->socket = conn_fd;
            node->remote_ip = inet_ntoa(remote_sa.sin_addr);
            node->remote_port = ntohs(remote_sa.sin_port);

            printf("\nNew connection #%d from %s: %d\n", node->conn_id, node->remote_ip, node->remote_port);
           
            //create a new thread
            pthread_t newthread;
            if(pthread_create(&newthread, (void *)NULL, conn_handler, (void *)node)!=0){  // what if we passed the node here instead of conn_fd??
                perror("pthread_create");
                exit(EXIT_FAILURE);
            }
            
        }
    }
}

// Thread function which runs as a connection is made
// Listens for assigned client and sends messages to all clients
void *conn_handler(void *dt){
    
    int bytes_received;
    char buf[BUF_SIZE];
    struct conn_node *node = dt;
    
    int break_loop = 0;
    char returnStr[BUF_SIZE];


    // Listens for messages received from client
    while((bytes_received = recv(node->socket, buf, BUF_SIZE, 0)) > 0){

        buf[bytes_received] = '\0';
        returnStr[0] = '\0';

        time_t t = time((void *)NULL);
        struct tm curTime = *localtime(&t);
        int total_msg_len = 10; // Every time string will be length 10
        
        // Display date
        sprintf(returnStr, "%02d:%02d:%02d: ", curTime.tm_hour, curTime.tm_min, curTime.tm_sec);


        // Sees if there is a nickname change
        char subbuf[7];
        memcpy(subbuf, buf, 6);
        if(strcmp("/nick ", subbuf)==0){
            // Print old nickname / remote_ip and new nickname
            char prevnick[strlen(node->nickname)];
            strncpy(prevnick, node->nickname, strlen(node->nickname)+1);

            // Store new nickname in node struct variable
            strtok(buf, " ");
            char *name = strtok((void *)NULL, " ");
            strncpy(node->nickname, name, strlen(name)+1);

            // Concatenates new nickname announcement to the end of message string
            char messagestr[BUF_SIZE]; 
            sprintf(messagestr, "User %s (%s) is now known as %s", prevnick, node ->remote_ip, node->nickname);
            strncat(returnStr, messagestr, strlen(messagestr));
            total_msg_len += strlen(messagestr);
        }
        else
        {
            // Add Nickname: message to the sending string
            int name_len = strlen(node->nickname);
            strncat(returnStr, node->nickname, name_len+1);

            char *semicolon = ": ";
            strncat(returnStr, semicolon, 3);

            strncat(returnStr, buf, bytes_received+1);
            total_msg_len = strlen(returnStr);
        }
        
        // Sending message to all clients
        // Need to add time and client's sendings nickname
        struct conn_node *sendnode = head;
        while(sendnode != (void *)NULL){
            send(sendnode->socket, returnStr, total_msg_len+1, 0);
            sendnode = sendnode->next;
        }

        if(break_loop){
            break;
        }
    }

    // Delete node and close thread after client disconnects
    delete(node);
    char exit_msg[BUF_SIZE];

    time_t t = time((void *)NULL);
    struct tm curTime = *localtime(&t);

    sprintf(exit_msg, "%02d:%02d:%02d: User %s (%s) disconnected", curTime.tm_hour, curTime.tm_min, curTime.tm_sec, node->nickname, node->remote_ip);
    int total_msg_len = strlen(exit_msg);

    // Notifies all clients of disconnected client
    struct conn_node *sendnode = head;
    while(sendnode != (void *)NULL){
        send(sendnode->socket, exit_msg, total_msg_len+1, 0);
        sendnode = sendnode->next;
    }
    
    // Close
    close(node->socket);
    free(node->nickname);
    free(node);
    puts("Closing Socket and Exiting Thread\n");
    pthread_exit((void *)NULL);

}       

//delete node function from LL
void delete(struct conn_node* ptr){ 
    //Delete the head
    if(head->conn_id == ptr->conn_id){
        head = ptr->next;
        return;
    }

    //Iterate through list until we find the ptr node
    struct conn_node *curNode = head;
    while(curNode->next != ptr){
        curNode = curNode->next;
    }
    curNode->next = ptr->next;
}