#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

#define MAX_CLIENTS 100
#define BUFFER_SZ 2048
#define IP "127.0.0.1"
#define ACTIVO 1
#define OCUPADO 2
#define INACTIVO 3

static _Atomic unsigned int cli_count = 0;
static int uid = 10;

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

int check_is_private(char *message)
{
    int i;
    int end = 2;
    char holder[200] = {};
    for (i = 0; i < end; i++)
    {
        holder[i] = message[i];
    }
    if (strcmp(holder, "-p") == 0)
    {
        return 1;
    }
    else
    {
        return -1;
    }
}

/* Manages what message to send private or not */
void send_message(char *mess, int uid)
{
    pthread_mutex_lock(&clients_mutex);
    int isPrivate = 0;
    if (check_is_private(mess) == 1)
    {
        isPrivate = 1;
    }
    if (isPrivate)
    {
        int i;
        int counter = 0;
        char username[30] = {};
        for (i = 2; i < 30; i++)
        {
            if (mess[i] == " ")
            {
                break;
            }
            username[i - 2] = mess[i];
            counter++;
        }
        char real_message[strlen(mess)];

        for (i = 0; i < strlen(mess) - counter - 2; i++)
        {
            printf("%d\n", 2 + counter + i);
            real_message[i] = mess[2 + counter + i];
            for (int i = 0; i < MAX_CLIENTS; ++i)
            {
                if (clients[i])
                {
                    if (strcmp(clients[i]->name, username) == 0)
                    {
                        if (write(clients[i]->sockfd, real_message, strlen(real_message)) < 0)
                        {
                            perror("ERROR: write to descriptor failed");
                            break;
                        }
                        break;
                    }
                }
            }
        }
        // strncpy(real_message, mess[2 + counter - 1], strlen(mess) - 2 + counter);
    }
    else
    {
        send_message_to_chat_group(mess);
        // char real_message[strlen(mess)];
        // strcpy(real_message, mess);
    }

    pthread_mutex_unlock(&clients_mutex);
}

void send_message_to_chat_group(char *message)
{
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i])
        {
            if (clients[i]->uid != uid)
            {
                if (write(clients[i]->sockfd, message, strlen(message)) < 0)
                {
                    perror("ERROR: write to descriptor failed");
                    break;
                }
            }
        }
    }
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