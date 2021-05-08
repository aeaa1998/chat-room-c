#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include "payload.pb.h"

#include <atomic>
#define _Atomic(X) std::atomic<X>
using namespace std;
#define LENGTH 2048
#define MAX_CLIENTS 100
#define BUFFER_SZ 2048
#define IP "127.0.0.1"
#define ACTIVO 1
#define OCUPADO 2
#define INACTIVO 3

atomic<long> cli_count(0);
int uid = 10;

/* Client structure */
typedef struct
{
    struct sockaddr_in address;
    int sockfd;
    int uid;
    char name[32];
    int status;
} client_t;

client_t *clients[MAX_CLIENTS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void str_overwrite_stdout()
{
    printf("\r%s", "> ");
    fflush(stdout);
}

void str_trim_lf(char *arr, int length)
{
    int i;
    for (i = 0; i < length; i++)
    { // trim \n
        if (arr[i] == '\n')
        {
            arr[i] = '\0';
            break;
        }
    }
}

void print_client_addr(struct sockaddr_in addr)
{
    printf("%d.%d.%d.%d",
           addr.sin_addr.s_addr & 0xff,
           (addr.sin_addr.s_addr & 0xff00) >> 8,
           (addr.sin_addr.s_addr & 0xff0000) >> 16,
           (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

/* Add clients to queue */
void queue_add(client_t *cl)
{
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (!clients[i])
        {
            clients[i] = cl;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

/* Remove clients to queue */
void queue_remove(int uid)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i])
        {
            if (clients[i]->uid == uid)
            {
                clients[i] = NULL;
                break;
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

/* Print requested client */
client_t *return_client(int uid)
{
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i])
        {
            if (clients[i]->uid == uid)
            {
                return clients[i];
            }
        }
    }
}

void send_message_to_chat_group(Payload payload, int uid)
{
    string message_priv = payload.sender() + ": " + payload.message();
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i])
        {
            if (clients[i]->uid != uid)
            {
                if (write(clients[i]->sockfd, message_priv.c_str(), strlen(message_priv.c_str())) < 0)
                {
                    perror("ERROR: write to descriptor failed");
                    break;
                }
            }
        }
    }
}

string getStatusString(int code)
{
    if (code == 1)
    {
        return "ACTIVO";
    }
    else if (code == 2)
    {
        return "OCUPADO";
    }
    return "INACTIVO";
}

string return_list(Payload payload)
{
    string message_list = "Lista de mensajes para " + payload.sender() + "\n";
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i])
        {
            if (strcmp(clients[i]->name, payload.sender().c_str()) != 0)

            {
                message_list = message_list + "Usuario: " + clients[i]->name = "\n";
            }
        }
    }
    return message_list;
}

/* Manages what message to send private or not */
void send_message(char *mess, int uid)
{
    pthread_mutex_lock(&clients_mutex);
    string message(mess);
    Payload payload;
    payload.ParseFromString(message);
    if (payload.flag().compare("list") == 0)
    {

        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            if (clients[i])
            {
                if (strcmp(clients[i]->name, payload.sender().c_str()) == 0)
                {
                    printf("ACa");
                    string message_list = return_list(payload);

                    printf("%s\n", message_list.c_str());
                    if (write(clients[i]->sockfd, message_list.c_str(), strlen(message_list.c_str())) < 0)
                    {
                        perror("ERROR: write to descriptor failed");
                        break;
                    }
                }
            }
        }
    }
    else if (payload.flag().compare("info") == 0)
    {
        client_t *clientToSend;
        client_t *clientToGetInfo;
        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            if (clients[i])
            {
                string message_priv = payload.sender() + " (private): " + payload.message();
                if (strcmp(clients[i]->name, payload.sender().c_str()) == 0)
                {
                    clientToSend = clients[i];
                }
                else if (strcmp(clients[i]->name, payload.extra().c_str()) == 0)
                {
                    clientToGetInfo = clients[i];
                }
            }
        }
        string username(clientToGetInfo->name);
        string uuid_s = to_string(clientToGetInfo->uid);
        char address_arr[LENGTH];
        inet_ntop(AF_INET, &clientToGetInfo->address.sin_addr, address_arr, LENGTH);
        string address(address_arr);
        string info_of_user = "Usuario: " + username + "\n" + "Status: " + getStatusString(clientToGetInfo->status) + "\nCodigo unico: " + uuid_s + "\nIp asociada: " + address;
        if (write(clientToSend->sockfd, info_of_user.c_str(), strlen(info_of_user.c_str())) < 0)
        {
            perror("ERROR: write to descriptor failed");
        }
        delete clientToSend;
        delete clientToGetInfo;
    }
    else if (payload.flag().compare("private") == 0)
    {
        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            if (clients[i])
            {
                string message_priv = payload.sender() + " (private): " + payload.message();
                if (strcmp(clients[i]->name, payload.extra().c_str()) == 0)
                {
                    if (write(clients[i]->sockfd, message_priv.c_str(), strlen(message_priv.c_str())) < 0)
                    {
                        perror("ERROR: write to descriptor failed");
                        break;
                    }
                    break;
                }
            }
        }
    }
    else if (payload.flag().compare("status") == 0)
    {
        int new_status;
        if (payload.message().compare("1") == 0)
        {
            new_status = ACTIVO;
        }
        else if (payload.message().compare("2") == 0)
        {
            new_status = OCUPADO;
        }
        else
        {
            new_status = INACTIVO;
        }
        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            if (clients[i])
            {

                if (strcmp(clients[i]->name, payload.sender().c_str()) == 0)
                {
                    string message_update_status = "Status actualizado " + getStatusString(clients[i]->status) + " -> " + getStatusString(new_status);
                    clients[i]->status = new_status;
                    if (write(clients[i]->sockfd, message_update_status.c_str(), strlen(message_update_status.c_str())) < 0)
                    {
                        perror("ERROR: write to descriptor failed");
                        break;
                    }
                    break;
                }
            }
        }
    }
    else
    {
        send_message_to_chat_group(payload, uid);
    }

    pthread_mutex_unlock(&clients_mutex);
}

/* Handle all communication with the client */
void *handle_client(void *arg)
{
    char buff_out[BUFFER_SZ];
    char name[32];
    int leave_flag = 0;

    cli_count++;
    client_t *cli = (client_t *)arg;

    // Name
    if (recv(cli->sockfd, name, 32, 0) <= 0 || strlen(name) < 2 || strlen(name) >= 32 - 1)
    {
        printf("Didn't enter the name.\n");
        leave_flag = 1;
    }
    else
    {
        strcpy(cli->name, name);
        sprintf(buff_out, "%s has joined\n", cli->name);
        printf("%s", buff_out);
        send_message(buff_out, cli->uid);
    }

    bzero(buff_out, BUFFER_SZ);

    while (1)
    {
        if (leave_flag)
        {
            break;
        }

        int receive = recv(cli->sockfd, buff_out, BUFFER_SZ, 0);
        if (receive > 0)
        {
            if (strlen(buff_out) > 0)
            {
                send_message(buff_out, cli->uid);

                str_trim_lf(buff_out, strlen(buff_out));
                printf("%s -> %s\n", buff_out, cli->name);
            }
        }
        else if (receive == 0 || strcmp(buff_out, "exit") == 0)
        {
            sprintf(buff_out, "%s has left\n", cli->name);
            printf("%s", buff_out);
            send_message(buff_out, cli->uid);
            leave_flag = 1;
        }
        else
        {
            printf("ERROR: -1\n");
            leave_flag = 1;
        }

        bzero(buff_out, BUFFER_SZ);
    }

    /* Delete client from queue and yield thread */
    close(cli->sockfd);
    queue_remove(cli->uid);
    free(cli);
    cli_count--;
    pthread_detach(pthread_self());

    return NULL;
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *ip = IP;
    int port = atoi(argv[1]);
    int option = 1;
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    pthread_t tid;

    /* Socket settings */
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(port);

    /* Ignore pipe signals */
    signal(SIGPIPE, SIG_IGN);

    if (setsockopt(listenfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char *)&option, sizeof(option)) < 0)
    {
        perror("ERROR: setsockopt failed");
        return EXIT_FAILURE;
    }

    /* Bind */
    if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("ERROR: Socket binding failed");
        return EXIT_FAILURE;
    }

    /* Listen */
    if (listen(listenfd, 10) < 0)
    {
        perror("ERROR: Socket listening failed");
        return EXIT_FAILURE;
    }

    printf("=== WELCOME TO THE CHATROOM ===\n");

    while (1)
    {
        socklen_t clilen = sizeof(cli_addr);
        connfd = accept(listenfd, (struct sockaddr *)&cli_addr, &clilen);

        /* Check if max clients is reached */
        if ((cli_count + 1) == MAX_CLIENTS)
        {
            printf("Max clients reached. Rejected: ");
            print_client_addr(cli_addr);
            printf(":%d\n", cli_addr.sin_port);
            close(connfd);
            continue;
        }

        /* Client settings */
        client_t *cli = (client_t *)malloc(sizeof(client_t));
        cli->address = cli_addr;
        cli->sockfd = connfd;
        cli->uid = uid++;
        cli->status = ACTIVO;

        /* Add client to the queue and fork thread */
        queue_add(cli);
        pthread_create(&tid, NULL, &handle_client, (void *)cli);

        /* Reduce CPU usage */
        sleep(1);
    }

    return EXIT_SUCCESS;
}