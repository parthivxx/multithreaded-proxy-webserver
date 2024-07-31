#include "ext/proxy_parse.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#define MAX_CLIENTS 100
#define MAX_BYTES 4096

typedef struct cache_element cache_element;

struct cache_element
{
    char* data;
    int len;
    char* url;
    time_t lru_time_track;
    cache_element* next_elemnt;
};

cache_element* find(char* url);

int add_chache_element(char* data , int len , char* url);

void remove_cache_element();

int port_number = 8080;

int proxy_socket_id;

pthread_t tid[MAX_CLIENTS];

sem_t semaphore;

pthread_mutex_t lock;

cache_element* head;

int cache_size;


void* thread_fn(void* socket_new){
    sem_wait(&semaphore);
    int p;
    sem_getvalue(&semaphore , &p);
    printf("Semaphore value is %d\n" , p);
    int* t = (int *) socket_new;
    int socket = *t;
    int byte_send_client = 0, len;

    char* buffer = (char*) calloc(MAX_BYTES , sizeof(char));
    bzero(buffer , MAX_BYTES);

    byte_send_client = recv(socket , buffer , MAX_BYTES , 0);

    while(byte_send_client > 0){
        len = strlen(buffer);
        if(strstr(buffer , "\r\n\r\n") == NULL){
            byte_send_client = recv(socket , buffer + len , MAX_BYTES - len , 0);
        }else break;
    }

    char* temp_request = (char *) malloc(strlen(buffer)*sizeof(char) + 1);

    for(int i=0;i<strlen(buffer);i++){
        temp_request[i] = buffer[i];
    }

    cache_element* temp = find(temp_request);

    if(temp != NULL){
        int sz = temp->len / sizeof(char);
        int pos = 0;
        char response[MAX_BYTES];

        while(pos < sz){
            bzero(response , sizeof(response));
            for(int i=0;i< MAX_BYTES;i++){
                response[i] = temp->data[i];
                pos++;
            }
            send(socket , response , MAX_BYTES , 0);
        }

        printf("Data received from the cache\n\n");
        printf("%s\n\n" , response);
    }else if(byte_send_client > 0){
        len = strlen(buffer);
        struct ParsedRequest* request = ParsedRequest_create();

        if(ParsedRequest_parse(request , buffer , len) < 0){
            printf("Parsing Failed\n");
        }else{
            bzero(buffer , sizeof(buffer));
            if(!strcmp(request->method , "GET")){
                if(request->host && request->path && checkHTTPversion(request->version) == 1){
                    // byte_send_client = handle_request(socket , )
                }
            }
        }
    }
}


int main(int argc , char* argv[]){
    int client_socketid , client_len;
    struct sockaddr_in server_addr , client_addr;
    sem_init(&semaphore , 0 ,MAX_CLIENTS);
    pthread_mutex_init(&lock , NULL);

    if(argc ==  2){
        port_number = atoi(argv[1]);
    }else{
        printf("Too few arguments\n");
        exit(1);
    }   

    printf("Starting proxy server at port number %d\n" , port_number);
    proxy_socket_id = socket(AF_INET , SOCK_STREAM , 0);

    if(proxy_socket_id < 0){
        perror("Failed to create a socket\n");
        exit(1);
    }

    int reuse = 1;

    if(setsockopt(proxy_socket_id , SOL_SOCKET , SO_REUSEADDR , (const char*) &reuse , sizeof(reuse)) < 0){
        perror("SetSockOpt failed\n");
        exit(1);
    }

    bzero((char*) &server_addr , sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_number);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(proxy_socket_id , (const struct sockaddr*) &server_addr , sizeof(server_addr)) < 0){
        perror("Port is not available\n");
        exit(1);
    }
    printf("Binding on port %d\n" , port_number);

    int listen_status = listen(proxy_socket_id , MAX_CLIENTS);

    if(listen_status < 0){
        perror("Error in listening\n");
        exit(1);
    }
     
    int i=0;

    int connected_socket_id[MAX_CLIENTS];

    while(1){
        bzero((char*) &client_addr , sizeof(client_addr));
         client_len = sizeof(client_addr);
         client_socketid = accept(proxy_socket_id , (struct sockaddr*) &client_addr , (socklen_t*) &client_len);
         if(client_socketid < 0){
            printf("Failed to connect with server\n");
            exit(1);
         }
         else connected_socket_id[i] = client_socketid;

         struct sockaddr_in* client_ptr = (struct sockaddr_in*) &client_addr;
         struct in_addr ip_addr = client_ptr->sin_addr;
         char str[INET_ADDRSTRLEN];
         inet_ntop(AF_INET , &ip_addr , str , INET6_ADDRSTRLEN);
         printf("Client is connected with port %d and ip address %s" , ntohs(client_addr.sin_port) , str);

         pthread_create(&tid[i] , NULL , thread_fn , (void *) &connected_socket_id[i]);
         i++;
    }   

    close(proxy_socket_id);
    return 0;
}