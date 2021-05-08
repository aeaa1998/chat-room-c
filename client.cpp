#include <iostream>
#include <string>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "payload.pb.h"

#define LENGTH 2048
#define IP "127.0.0.1"
#define ACTIVO = 1
#define OCUPADO = 2
#define INACTIVO = 3
// #define PRIVATE_FLAG "private"
// Global variables
using namespace std;

volatile sig_atomic_t flag = 0;
int sockfd = 0;
char name[32];

void str_overwrite_stdout()
{
    printf("%s", "> ");
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

void catch_ctrl_c_and_exit(int sig)
{
    flag = 1;
}

void *send_msg_handler(void *arg)
{
    string message;

    char buffer[LENGTH + 32] = {};
    cout << "What's your name? ";

    while (1)
    {
        getline(cin, message);
        printf("HERE1");
        Payload payload;
        printf("HERE2");
        payload.set_sender(&name);
        payload.set_message(&message);
        payload.set_flag("message");
        // payload->set_message(std::string s(message));
        if (strcmp(message, "exit") == 0)
        {
            break;
        }
        else
        {
            // if (check_is_private(message) == 1)
            // {
            //     int i;
            //     int offset = 0;
            //     int end = offset + 3;
            //     char new_message[LENGTH] = {};
            //     char username[LENGTH] = {};
            //     int goOn = 0;
            //     int extraOffset = 0;

            //     for (i = end; i < strlen(message); i++)
            //     {
            //         if (message[i] == ' ' && goOn == 0)
            //         {
            //             goOn = 1;
            //         }
            //         else if (goOn == 1)
            //         {
            //             new_message[i - (end + extraOffset) - 1] = message[i];
            //         }
            //         else
            //         {
            //             username[i - end] = message[i];
            //             extraOffset++;
            //         }
            //     }
            //     sprintf(buffer, "-p %s %s (private) -> %s: %s\n", username, name, username, new_message);
            //     bzero(new_message, LENGTH);
            //     bzero(username, LENGTH);
            // }
            // else
            // {
            printf("HERE");
            string out;
            payload.SerializeToString(&out);
            sprintf(buffer, "%s", out);
            // }

            send(sockfd, buffer, strlen(buffer), 0);
        }

        bzero(message, LENGTH);

        bzero(buffer, LENGTH + 32);
    }
    catch_ctrl_c_and_exit(2);
}

void *recv_msg_handler(void *arg)
{
    char message[LENGTH] = {};
    while (1)
    {
        int receive = recv(sockfd, message, LENGTH, 0);
        if (receive > 0)
        {
            printf("%s \n", message);
            str_overwrite_stdout();
        }
        else if (receive == 0)
        {
            break;
        }
        else
        {
            // -1
        }
        memset(message, 0, sizeof(message));
    }
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        printf("Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int port = atoi(argv[1]);
    // char *n = argv[2];

    char *ip = IP;

    signal(SIGINT, catch_ctrl_c_and_exit);

    // printf("Please enter your name: ");
    strcpy(name, argv[2]);
    // fgets(name, 32, stdin);
    str_trim_lf(name, strlen(name));

    if (strlen(name) > 32 || strlen(name) < 2)
    {
        printf("Name must be less than 30 and more than 2 characters.\n");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr;

    /* Socket settings */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    // Connect to Server
    int err = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err == -1)
    {
        printf("ERROR: connect\n");
        return EXIT_FAILURE;
    }

    // Send name
    send(sockfd, name, 32, 0);

    printf("=== WELCOME TO THE CHATROOM ===\n");

    pthread_t send_msg_thread;
    if (pthread_create(&send_msg_thread, NULL, &send_msg_handler, NULL) != 0)
    {
        printf("ERROR: pthread\n");
        return EXIT_FAILURE;
    }

    pthread_t recv_msg_thread;
    if (pthread_create(&recv_msg_thread, NULL, &recv_msg_handler, NULL) != 0)
    {
        printf("ERROR: pthread\n");
        return EXIT_FAILURE;
    }

    while (1)
    {
        if (flag)
        {
            printf("\nBye\n");
            break;
        }
    }

    close(sockfd);

    return EXIT_SUCCESS;
}
