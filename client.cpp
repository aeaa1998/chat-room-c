#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ifaddrs.h>
#include "payload.pb.h"

#define LENGTH 2048
#define IP "13.58.87.181"

// Global variables
using namespace std;

volatile sig_atomic_t flag = 0;
int sockfd = 0;
char name[32];
char ip[LENGTH];

string getIPAddress()
{
    string ipAddress = "error";
    struct ifaddrs *interfaces = NULL;
    struct ifaddrs *temp_addr = NULL;
    int success = 0;
    // retrieve the current interfaces - returns 0 on success
    success = getifaddrs(&interfaces);
    if (success == 0)
    {
        // Loop through linked list of interfaces
        temp_addr = interfaces;
        while (temp_addr != NULL)
        {
            if (temp_addr->ifa_addr->sa_family == AF_INET)
            {
                // Check if interface is en0 which is the wifi connection on the iPhone
                // if (strcmp(temp_addr->ifa_name, "en0") == 0)
                // {
                ipAddress = inet_ntoa(((struct sockaddr_in *)temp_addr->ifa_addr)->sin_addr);
                // }
            }
            temp_addr = temp_addr->ifa_next;
        }
    }
    // Free memory
    freeifaddrs(interfaces);
    return ipAddress;
}
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

int check_is_private(char message[])
{
    int end = 3;
    char holder[LENGTH] = {};
    for (int i = 0; i < end; i++)
    {
        holder[i] = message[i];
    }
    if (strcmp(holder, "-p ") == 0)
    {
        bzero(holder, LENGTH);
        return 1;
    }
    else
    {
        bzero(holder, LENGTH);
        return -1;
    }
}

int check_is_info_user(char message[])
{
    int end = 6;
    char holder[LENGTH] = {};
    for (int i = 0; i < end; i++)
    {
        holder[i] = message[i];
    }
    if (strcmp(holder, "-info ") == 0)
    {
        bzero(holder, LENGTH);
        return 1;
    }
    else
    {
        bzero(holder, LENGTH);
        return -1;
    }
}

int check_is_status(char message[])
{
    int counter = 0;
    int end = 3;
    char holder[LENGTH] = {};
    for (int i = 0; i < end; i++)
    {
        holder[i] = message[i];
    }
    if (strcmp(holder, "-s ") == 0)
    {
        bzero(holder, LENGTH);
    }
    else
    {
        bzero(holder, LENGTH);
        return -1;
    }
    if (strcmp(message, "-s ACTIVO") == 0)
    {
        return 1;
    }
    else if (strcmp(message, "-s INACTIVO") == 0)
    {
        return 2;
    }
    else if (strcmp(message, "-s OCUPADO") == 0)
    {
        return 3;
    }
    return -1;
}

void *send_msg_handler(void *arg)
{
    char message[LENGTH] = {};
    char buffer[LENGTH + 32];

    while (1)
    {
        str_overwrite_stdout();
        fgets(message, LENGTH, stdin);
        str_trim_lf(message, LENGTH);
        Payload payload;
        payload.set_sender(name);

        // payload->set_message(std::string s(message));
        if (strcmp(message, "exit") == 0)
        {
            break;
        }
        else
        {
            if (strcmp(message, "--list") == 0)
            {
                payload.set_flag(Payload_PayloadFlag::Payload_PayloadFlag_user_list);
                string out;
                payload.SerializeToString(&out);
                sprintf(buffer, "%s", out.c_str());
            }
            else if (check_is_info_user(message) == 1)
            {
                payload.set_flag(Payload_PayloadFlag::Payload_PayloadFlag_user_info);
                int i;
                int offset = 0;
                int end = offset + 6;
                char new_message[LENGTH] = {};
                char username[LENGTH] = {};
                int goOn = 0;
                int extraOffset = 0;
                for (i = end; i < strlen(message); i++)
                {
                    if (message[i] == ' ' && goOn == 0)
                    {
                        goOn = 1;
                    }
                    else if (goOn == 1)
                    {
                        new_message[i - (end + extraOffset) - 1] = message[i];
                    }
                    else
                    {
                        username[i - end] = message[i];
                        extraOffset++;
                    }
                }
                payload.set_extra(username);
                string out;
                payload.SerializeToString(&out);
                sprintf(buffer, "%s", out.c_str());
                bzero(new_message, LENGTH);
                bzero(username, LENGTH);
            }
            else if (check_is_private(message) == 1)
            {

                payload.set_flag(Payload_PayloadFlag::Payload_PayloadFlag_private_chat);
                int i;
                int offset = 0;
                int end = offset + 3;
                char new_message[LENGTH] = {};
                char username[LENGTH] = {};
                int goOn = 0;
                int extraOffset = 0;

                for (i = end; i < strlen(message); i++)
                {
                    if (message[i] == ' ' && goOn == 0)
                    {
                        goOn = 1;
                    }
                    else if (goOn == 1)
                    {
                        new_message[i - (end + extraOffset) - 1] = message[i];
                    }
                    else
                    {
                        username[i - end] = message[i];
                        extraOffset++;
                    }
                }
                payload.set_message(new_message);
                payload.set_extra(username);
                string out;
                payload.SerializeToString(&out);
                sprintf(buffer, "%s", out.c_str());
                bzero(new_message, LENGTH);
                bzero(username, LENGTH);
            }
            else if (check_is_status(message) > -1)
            {
                int status = check_is_status(message);
                if (status == 1)
                {
                    payload.set_message("ACTIVO");
                }
                else if (status == 2)
                {
                    payload.set_message("INACTIVO");
                }
                else
                {
                    payload.set_message("OCUPADO");
                }

                payload.set_flag(Payload_PayloadFlag::Payload_PayloadFlag_update_status);
                string out;
                payload.SerializeToString(&out);
                sprintf(buffer, "%s", out.c_str());
            }
            else
            {
                payload.set_message(message);
                payload.set_flag(Payload_PayloadFlag::Payload_PayloadFlag_general_chat);
                string out;
                payload.SerializeToString(&out);
                sprintf(buffer, "%s", out.c_str());
            }

            send(sockfd, buffer, strlen(buffer), 0);
        }

        bzero(message, LENGTH);

        bzero(buffer, LENGTH + 32);
    }
    catch_ctrl_c_and_exit(2);
}

void *incoming_messages_handler(void *arg)
{
    char message[LENGTH] = {};
    while (1)
    {
        int received_message = recv(sockfd, message, LENGTH, 0);
        if (received_message > 0)
        {
            Payload server_payload;
            server_payload.ParseFromString(message);
            if (server_payload.code() == 200)
            {
                printf("%s \n", server_payload.message().c_str());
            }
            else if (server_payload.code() == 401)
            {
                printf("%s \n", server_payload.message().c_str());
                break;
            }
            else
            {
                printf("Error del server %d -- %s!\n", server_payload.code(), server_payload.message().c_str());
            }

            str_overwrite_stdout();
        }
        else if (received_message == 0)
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
    char buffer[LENGTH + 32];
    if (argc != 4)
    {
        printf("Uso: %s <port> <username> <ip>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int port = atoi(argv[1]);
    // char *n = argv[2];

    signal(SIGINT, catch_ctrl_c_and_exit);

    strcpy(name, argv[2]);
    str_trim_lf(name, strlen(name));

    strcpy(ip, argv[3]);
    str_trim_lf(ip, strlen(ip));

    if (strlen(name) > 32 || strlen(name) < 2)
    {
        printf("El nombre debe de encontrarse entre 2 y 30 caracteres\n");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr;
    char *ip_normalized = ip;
    /* Socket settings */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip_normalized);
    server_addr.sin_port = htons(port);

    // Connect to Server
    int err = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err == -1)
    {
        printf("ERROR: connect\n");
        return EXIT_FAILURE;
    }
    printf("conecto");
    // Send name
    Payload register_payload;
    register_payload.set_sender(name);
    string my_ip = getIPAddress();
    if (my_ip.compare("error") == 0)
    {
        printf("Solo perdidas");
        return EXIT_FAILURE;
    };
    register_payload.set_ip(my_ip);
    register_payload.set_flag(Payload_PayloadFlag::Payload_PayloadFlag_register_);
    string out;
    register_payload.SerializeToString(&out);
    sprintf(buffer, "%s", out.c_str());
    send(sockfd, buffer, strlen(buffer), 0);

    printf("Bienvenido amigo \n");

    pthread_t send_msg_thread;
    if (pthread_create(&send_msg_thread, NULL, &send_msg_handler, NULL) != 0)
    {
        printf("ERROR: pthread\n");
        return EXIT_FAILURE;
    }

    pthread_t recv_msg_thread;
    if (pthread_create(&recv_msg_thread, NULL, &incoming_messages_handler, NULL) != 0)
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
