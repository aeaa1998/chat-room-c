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
#define CLIENT_LIMIT 100
#define BUFFER_SIZE 2048
#define IP "0.0.0.0"
#define ACTIVO 1
#define OCUPADO 2
#define INACTIVO 3

atomic<long> cli_count(0);
int uid = 10;

/* Client structure */
typedef struct
{
    struct sockaddr_in address;
    int socket_d;
    int uid;
    char name[32];
    int status;
} client_t;

client_t *clients[CLIENT_LIMIT];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

int get_client_index(string name)
{
    for (int i = 0; i < CLIENT_LIMIT; ++i)
    {
        if (clients[i])
        {
            if (strcmp(clients[i]->name, name.c_str()) == 0)
            {
                return i;
            }
        }
    }
    return -1;
}

int get_client_index_uid(int uid)
{
    for (int i = 0; i < CLIENT_LIMIT; ++i)
    {
        if (clients[i])
        {
            if (clients[i]->uid == uid)
            {
                return i;
            }
        }
    }
    return -1;
}

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

void queue_add(client_t *cl)
{
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < CLIENT_LIMIT; ++i)
    {
        if (!clients[i])
        {
            clients[i] = cl;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void queue_remove(int uid)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < CLIENT_LIMIT; ++i)
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
    for (int i = 0; i < CLIENT_LIMIT; ++i)
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

void send_message_to_chat_group(string payload, int uid)
{

    for (int i = 0; i < CLIENT_LIMIT; ++i)
    {
        if (clients[i])
        {
            if (clients[i]->uid != uid)
            {
                if (write(clients[i]->socket_d, payload.c_str(), payload.length()) < 0)
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
    string message_list = "Lista de usuarios para " + payload.sender() + "\n";
    for (int i = 0; i < CLIENT_LIMIT; ++i)
    {
        if (clients[i])
        {
            if (strcmp(clients[i]->name, payload.sender().c_str()) != 0)

            {
                message_list = message_list + "Usuario: " + clients[i]->name + "\n";
            }
        }
    }
    return message_list;
}

void send_error_message(string message, int i)
{
    Payload error_payload;
    error_payload.set_code(500);
    error_payload.set_sender("server");
    error_payload.set_message(message);
    string out;
    error_payload.SerializeToString(&out);
    write(clients[i]->socket_d, out.c_str(), out.length());
}
void exit_user(int i)
{
    Payload error_payload;
    error_payload.set_code(401);
    error_payload.set_sender("server");
    error_payload.set_message("Usted ha sido pateado del server");
    string out;
    error_payload.SerializeToString(&out);
    write(clients[i]->socket_d, out.c_str(), out.length());
}

void confirm(string message, int sender_index, Payload_PayloadFlag flag)
{
    Payload confirmation_message;
    confirmation_message.set_sender("serve");
    confirmation_message.set_message(message);
    confirmation_message.set_code(200);
    confirmation_message.set_flag(flag);
    string out_lol;
    confirmation_message.SerializeToString(&out_lol);
    write(clients[sender_index]->socket_d, out_lol.c_str(), out_lol.length());
}

/* Manages what message to send private or not */
void send_message(char *mess, int uid)
{
    pthread_mutex_lock(&clients_mutex);
    string message(mess);
    Payload payload;
    payload.ParseFromString(message);
    int sender_index = get_client_index(payload.sender());
    Payload server_payload;
    server_payload.set_sender("server");
    server_payload.set_code(200);

    if (payload.flag() == Payload_PayloadFlag::Payload_PayloadFlag_user_list)
    {

        for (int i = 0; i < CLIENT_LIMIT; ++i)
        {
            if (clients[i])
            {
                if (strcmp(clients[i]->name, payload.sender().c_str()) == 0)
                {
                    string message_list = return_list(payload);
                    server_payload.set_flag(Payload_PayloadFlag::Payload_PayloadFlag_user_list);
                    server_payload.set_message(message_list);
                    string out;
                    server_payload.SerializeToString(&out);
                    if (write(clients[i]->socket_d, out.c_str(), out.length()) < 0)
                    {
                        send_error_message("No se pudo obtener la lista de clientes.", sender_index);
                        break;
                    }
                }
            }
        }
    }
    else if (payload.flag() == Payload_PayloadFlag::Payload_PayloadFlag_user_info)
    {
        int found = 0;
        string info_of_user = "";
        for (int i = 0; i < CLIENT_LIMIT; ++i)
        {
            if (clients[i])
            {
                if (strcmp(clients[i]->name, payload.extra().c_str()) == 0)
                {
                    found = 1;
                    string username(clients[i]->name);
                    string uuid_s = to_string(clients[i]->uid);
                    char address_arr[LENGTH];
                    inet_ntop(AF_INET, &clients[i]->address.sin_addr, address_arr, LENGTH);
                    string address(address_arr);
                    info_of_user = info_of_user + "Usuario: " + username + "\n" + "Status: " + getStatusString(clients[i]->status) + "\nCodigo unico: " + uuid_s + "\nIp asociada: " + address;
                }
            }
        }
        server_payload.set_flag(Payload_PayloadFlag::Payload_PayloadFlag_user_info);
        server_payload.set_message(info_of_user);
        string out;
        server_payload.SerializeToString(&out);

        for (int i = 0; i < CLIENT_LIMIT; ++i)
        {
            if (clients[i])
            {
                if (strcmp(clients[i]->name, payload.sender().c_str()) == 0)
                {
                    if (write(clients[i]->socket_d, out.c_str(), out.length()) < 0)
                    {
                        send_error_message("No se pudo obtener la informacion del usuario deseado.", sender_index);
                        break;
                    }
                }
            }
        }
        if (found == 0)
        {
            send_error_message("El usuario ingresado no existe.", sender_index);
        }
    }
    //Comparación de si es un mensaje privado
    else if (payload.flag() == Payload_PayloadFlag::Payload_PayloadFlag_private_chat)
    {
        int found = 0;
        for (int i = 0; i < CLIENT_LIMIT; ++i)
        {
            if (clients[i])
            {
                if (strcmp(clients[i]->name, payload.extra().c_str()) == 0)
                {
                    found = 1;
                    //Se manda solo el string del mensaje privado al username destinado
                    //El client ya solo imprime el texto sin tener que pensar que respuesta es
                    //MASK: {sender_username} + (privado): + {mensaje}
                    server_payload.set_flag(Payload_PayloadFlag::Payload_PayloadFlag_private_chat);
                    string message_priv = payload.sender() + " (private): " + payload.message();
                    server_payload.set_message(message_priv);
                    string out;
                    server_payload.SerializeToString(&out);
                    if (write(clients[i]->socket_d, out.c_str(), out.length()) < 0)
                    {
                        //Aca se mandaria 500 con el mensaje de error
                        send_error_message("No se pudo mandar el mensaje privado.", sender_index);
                        break;
                    }
                    break;
                }
            }
        }
        if (found == 0)
        {
            send_error_message("El usuario ingresado no existe.", sender_index);
        }
    }
    else if (payload.flag() == Payload_PayloadFlag::Payload_PayloadFlag_update_status)
    {
        int new_status;
        if (payload.message().compare("ACTIVO") == 0)
        {
            new_status = ACTIVO;
        }
        else if (payload.message().compare("INACTIVO") == 0)
        {
            new_status = INACTIVO;
        }
        else
        {
            new_status = OCUPADO;
        }
        for (int i = 0; i < CLIENT_LIMIT; ++i)
        {
            if (clients[i])
            {

                if (strcmp(clients[i]->name, payload.sender().c_str()) == 0)
                {
                    string message_update_status = "Status actualizado " + getStatusString(clients[i]->status) + " -> " + getStatusString(new_status);
                    server_payload.set_message(message_update_status);
                    string out;
                    server_payload.set_flag(Payload_PayloadFlag::Payload_PayloadFlag_update_status);
                    server_payload.SerializeToString(&out);
                    clients[i]->status = new_status;
                    if (write(clients[i]->socket_d, out.c_str(), out.length()) < 0)
                    {
                        send_error_message("Actualización del mensaje comprometido.", sender_index);
                        break;
                    }
                    break;
                }
            }
        }
    }
    else
    {
        if (payload.sender().empty())
        {
            server_payload.set_code(200);
            server_payload.set_flag(Payload_PayloadFlag::Payload_PayloadFlag_general_chat);
            server_payload.set_message(message);
            // for (int i = 0; i < CLIENT_LIMIT; ++i)
            // {
            //     if (clients[i])
            //     {
            //         if (clients[i]->uid != uid)
            //         {
            //             payload.set_code(200);
            //             string send;
            //             payload.SerializeToString(&send);
            //             if (write(clients[i]->socket_d, send.c_str(), send.length()) < 0)
            //             {
            //                 perror("ERROR: write to descriptor failed");
            //                 break;
            //             }
            //         }
            //     }
            // }
        }
        else
        {
            server_payload.set_flag(Payload_PayloadFlag::Payload_PayloadFlag_general_chat);
            string pm = payload.sender() + "(general): " + payload.message();
            server_payload.set_message(pm);
            // for (int i = 0; i < CLIENT_LIMIT; ++i)
            // {
            //     if (clients[i])
            //     {
            //         if (clients[i]->uid != uid)
            //         {
            //             payload.set_code(200);
            //             string send;
            //             payload.SerializeToString(&send);
            //             if (write(clients[i]->socket_d, send.c_str(), send.length()) < 0)
            //             {
            //                 perror("ERROR: write to descriptor failed");
            //                 break;
            //             }
            //         }
            //     }
            // }
        }

        send_message_to_chat_group(server_payload, uid);
    }

    pthread_mutex_unlock(&clients_mutex);
}

void *manage_added_client(void *arg)
{
    char buff_out[BUFFER_SIZE];
    char register_m[LENGTH];
    int leave_flag = 0;
    int invalid_flag = 0;

    cli_count++;
    client_t *cli = (client_t *)arg;

    // Name checks
    if (recv(cli->socket_d, register_m, LENGTH, 0) <= 0)
    {
        printf("Didn't enter the name.\n");
        leave_flag = 1;
    }
    string rm(register_m);
    Payload register_payload;
    register_payload.ParseFromString(rm);
    if (register_payload.sender().length() < 2 || register_payload.sender().length() >= 32 - 1)
    {
        printf("Didn't enter the name.\n");
        leave_flag = 1;
    }
    else if (get_client_index(register_payload.sender()) >= 0)
    {
        printf("Username already exists.\n");
        invalid_flag = 1;
        leave_flag = 1;
    }
    else
    {
        strcpy(cli->name, register_payload.sender().c_str());
        sprintf(buff_out, "%s se ha unido al chat\n", cli->name);
        printf("%s", buff_out);
        send_message(buff_out, cli->uid);
    }

    bzero(buff_out, BUFFER_SIZE);
    int sender_index = get_client_index_uid(cli->uid);
    if (invalid_flag == 0)
    {
        confirm("Todo bien todo correcto te registraste.", sender_index, Payload_PayloadFlag::Payload_PayloadFlag_register_);
    }
    while (1)
    {
        if (leave_flag)
        {
            if (invalid_flag)
            {
                exit_user(sender_index);
            }
            break;
        }

        int receive = recv(cli->socket_d, buff_out, BUFFER_SIZE, 0);
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
            sprintf(buff_out, "%s ha abandonado el chat\n", cli->name);
            printf("%s", buff_out);
            send_message(buff_out, cli->uid);
            leave_flag = 1;
        }
        else
        {
            printf("ERROR: -1\n");
            leave_flag = 1;
        }

        bzero(buff_out, BUFFER_SIZE);
    }

    /* Delete client from queue and yield thread */
    close(cli->socket_d);
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

    signal(SIGPIPE, SIG_IGN);

    if (setsockopt(listenfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char *)&option, sizeof(option)) < 0)
    {
        perror("ERROR: setsockopt failed");
        return EXIT_FAILURE;
    }

    if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("ERROR: Socket binding failed");
        return EXIT_FAILURE;
    }

    if (listen(listenfd, 10) < 0)
    {
        perror("ERROR: Socket listening failed");
        return EXIT_FAILURE;
    }

    printf("--- Bienvenidos al chat triste :c ---\n");

    while (1)
    {
        socklen_t clilen = sizeof(cli_addr);
        connfd = accept(listenfd, (struct sockaddr *)&cli_addr, &clilen);

        /* Check if max clients is reached */
        if ((cli_count + 1) == CLIENT_LIMIT)
        {
            printf("Capacidad de clientes excedida: ");
            print_client_addr(cli_addr);
            printf(":%d\n", cli_addr.sin_port);
            close(connfd);
            continue;
        }

        /* Client settings */
        client_t *cli = (client_t *)malloc(sizeof(client_t));
        cli->address = cli_addr;
        cli->socket_d = connfd;
        cli->uid = uid++;
        cli->status = ACTIVO;

        /* Add client to the queue and fork thread */
        queue_add(cli);
        pthread_create(&tid, NULL, &manage_added_client, (void *)cli);

        /* Reduce CPU usage */
        sleep(1);
    }

    return EXIT_SUCCESS;
}