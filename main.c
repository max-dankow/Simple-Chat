#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <unistd.h>
#include <pthread.h>

const size_t MAX_CLIENT_NUMBER = 100;
const size_t BUFFER_SIZE = 1024;
const int IS_FREE = 0;

int active_connections[100];
pthread_t client_threads[100];

int init_socket_for_connection(int port)
{
    int listen_socket = -1;
  //создаем сокет  
    listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket == -1)
    {
        perror("Can't create socket.");
        exit(EXIT_FAILURE);
    }
  //связываение сокета
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(listen_socket, (struct sockaddr *) &addr, sizeof(addr)) == -1)
    {
        perror("Bind");
        exit(EXIT_FAILURE);
    } 
    return listen_socket;
}

int send_message(char* text, int fd, size_t length)
{
    size_t current = 0;
    while (current < length)
    {
        int code = send(fd, text + current, length - current, 0);
        if (code == -1)
        {
            return -1;
        }
        current += code;
    }
}
int receive_message(char* text, int fd, size_t length)
{
    size_t current = 0;
    while (current < length)
    {
        int code = recv(fd, text + current, length - current, 0);
        if (code == -1)
        {
            return -1;
        }
        current += code;
    }
}

void send_back_to_all(char* text, const int* connections, int sender)
{
    size_t message_length = strlen(text);
    for (size_t i = 0; i < MAX_CLIENT_NUMBER; ++i)
    {
        if (connections[i] != IS_FREE && i != sender)
        {
            send_message(&message_length, connections[i], sizeof(message_length));
            send_message(text, connections[i], message_length);
            printf("resended to %d\n", i);
        }
    }
}

void* process_client(void* arg)
{
    int* sock_ptr = (int*) arg;
    int sock = *sock_ptr;
    int my_number = sock_ptr - active_connections;
    printf("My number is %d\n", my_number);
    while (1)
    {
      //получаем длину сообщения
        size_t message_len;
        int code;
        code = recv(sock, &message_len, sizeof(message_len), 0);
        if (code <= 0)
        {
            break;
        }
      //получаем все сообщение  
        char* text = (char*) malloc(message_len + 1);
        if (receive_message(text, sock, message_len) == -1)
        {
            break;
        }
        printf("Server got: %s\n", text);
        send_back_to_all(text, active_connections, my_number);
        free(text);
    }
    printf("Disconnected.\n");
    *sock_ptr = IS_FREE;
    return NULL;
}

void run_server(int port)
{
    printf("Server started. Port is %d!\n", port);
    int listen_socket = init_socket_for_connection(port);
    listen(listen_socket, 20);
    memset(active_connections, 0, MAX_CLIENT_NUMBER * sizeof(int));
    char buffer[BUFFER_SIZE];

    while(1)
    {
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(0, &read_set);
        FD_SET(listen_socket, &read_set);
      //ожидаем событий ввода  
        int code = select(listen_socket + 1, &read_set, NULL, NULL, NULL);
        if (code == -1)
        {
            perror("Select error.");
            exit(EXIT_FAILURE);
        }
      //(1)получили запрос на подключение
        if (FD_ISSET(listen_socket, &read_set))
        {
            int connector_socket = accept(listen_socket, NULL, NULL);
            if (connector_socket == -1)
            {
                perror("Accept");
            }
            else
            {
                printf("New client connected!\n");
              //добавляем нового клиента в список
                ssize_t new_client_index = -1;
                for (size_t i = 0; i < MAX_CLIENT_NUMBER; ++i)
                {
                    if (active_connections[i] == IS_FREE)
                    {
                        active_connections[i] = connector_socket;
                        new_client_index = i;
                        break;
                    }
                }
                if (new_client_index == -1)
                {
                    fprintf(stderr, "Too many clients.\n");
                    exit(EXIT_FAILURE);
                }
              //запускаем обработку запросов клиента
                if (pthread_create(client_threads + new_client_index, NULL, &process_client,
                    active_connections + new_client_index) == -1)
                {
                    perror("pthread_create");
                    exit(EXIT_FAILURE);
                }
            }
        }
      //(2) нажатие на клавиатуре  
        if (FD_ISSET(0, &read_set))
        {
            printf("Terminateing...\n");
            for (size_t i = 0; i < MAX_CLIENT_NUMBER; ++i)
            {
                if (active_connections[i] != IS_FREE)
                {
                    pthread_join(client_threads[i], NULL);
                }
            }
            break;
        }
    }
    close(listen_socket);
    printf("Server terminated.\n");
}

void run_client(int port, char* server_addr)
{
    printf("Welcome!\n");
  //создаем сокет  
    int write_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(write_socket < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }
  //подключаемся к серверу
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(server_addr);
    if(connect(write_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("Connect");
        exit(EXIT_FAILURE);
    }
    int read_code = 0;
    char buffer[BUFFER_SIZE];
    char* line = (char*) malloc(BUFFER_SIZE);

    while (1)
    {
        printf("Enter message:");
        fflush(stdout);
      //создаем множество для мультиплексировния stdin и socket'a
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(0, &read_set);
        FD_SET(write_socket, &read_set);
      //ожидаем событий ввода  
        int code = select(write_socket + 1, &read_set, NULL, NULL, NULL);
        if (code == -1)
        {
            perror("Select error.");
            free(line);
            exit(EXIT_FAILURE);
        }
      //(1) ввели сообщение с клавиатуры, нужно отослать
        if (FD_ISSET(0, &read_set))
        {
            int len;
            read_code = getline(&line, &len, stdin);
            if (read_code <= 0)
            {
                break;
            }
            line[read_code - 1] = '\0';
            size_t length = strlen(line);
            if (length != 0)
            {
                send_message(&length, write_socket, sizeof(length));
                send_message(line, write_socket, strlen(line));
            }
        }
      //(2) пришло сообшение с сервера, необходимо напечатать
        if (FD_ISSET(write_socket, &read_set))
        {
          //получаем длину сообщения  
            size_t message_len;
            int code;
            code = recv(write_socket, &message_len, sizeof(message_len), 0);
            if (code <= 0)
            {
                break;
            }
          //получаем все сообщение  
            char* text = (char*) malloc(message_len + 1);
            if (receive_message(text, write_socket, message_len) == -1)
            {
                break;
            }
            printf("\rServer told: %s\n", text);
        }
    }

    free(line);
    close(write_socket);
    printf("Goodbye!\n");
}

void read_args(int argc, char** argv, int* mode, char** addr, int* port)
{
    if (argc < 3)
    {
        fprintf(stderr, "Wrong arguments.\n");
        exit(EXIT_FAILURE);
    }
    *mode = -1;
    if (strcmp(argv[1], "-s") == 0)
    {
        *mode = 0;
        if (argc != 3)
        {
            fprintf(stderr, "Wrong arguments.\n");
            exit(EXIT_FAILURE);       
        }
        sscanf(argv[2], "%d", port);
        return;
    }
    if (strcmp(argv[1], "-c") == 0)
    {
        *mode = 1;
        if (argc != 4)
        {
            fprintf(stderr, "Wrong arguments.\n");
            exit(EXIT_FAILURE);       
        }
        sscanf(argv[2], "%d", port);
        *addr = argv[3];
        return;
    }
    fprintf(stderr, "Wrong mode. Use -s to run server or -c  to run client.\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv)
{
    int mode, port;
    char* addr;
    read_args(argc, argv, &mode, &addr, &port);
    if (mode == 0)
    {
        run_server(port);
    }
    if (mode == 1)
    {
        run_client(port, addr);
    }
    return EXIT_SUCCESS;
}