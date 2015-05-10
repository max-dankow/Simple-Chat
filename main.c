#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <unistd.h>

const size_t MAX_CLIENT_NUMBER = 100;
const size_t BUFFER_SIZE = 1024;
const int IS_FREE = 0;

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

void send_back_to_all(char* text, const int* connections, int sender)
{
    size_t message_length = strlen(text) + 1;
    for (size_t i = 0; i < MAX_CLIENT_NUMBER; ++i)
    {
        if (connections[i] != IS_FREE && i != sender)
        {
            send_message(text, connections[i], message_length);
            printf("resended to %d\n", i);
        }
    }
}

void run_server(int port)
{
    printf("Server started. Port is %d!\n", port);
    int listen_socket = init_socket_for_connection(port);
    listen(listen_socket, 2);
    int active_connections[MAX_CLIENT_NUMBER];
    memset(active_connections, 0, MAX_CLIENT_NUMBER * sizeof(int));
    char buffer[BUFFER_SIZE];

    while(1)
    {
      //создаем множество дескрипторов, отслеживемых для чтения  
        fd_set read_set;
        FD_ZERO(&read_set);
      //создаем множество для отслеживания ошибок в дескрипторах  
        fd_set error_set;
        FD_ZERO(&error_set);
      //добавляем в него всех подключившихся уже клиентов и сокет для подключения
        FD_SET(listen_socket, &read_set);
        int max = listen_socket;
      //заполняем оба множества дескрипторами клиентов  
        for (size_t i = 0; i < MAX_CLIENT_NUMBER; ++i)
        {
            if (active_connections[i] != IS_FREE)
            {
                FD_SET(active_connections[i], &read_set);
                FD_SET(active_connections[i], &error_set);
                if (active_connections[i] > max)
                {
                    max = active_connections[i];
                }
            }
        }
      //ждем событий
        printf("waiting...\n");
        int code = select(max + 1, &read_set, NULL, &error_set, NULL);
        if (code == -1)
        {
            perror("Select error.");
            exit(EXIT_FAILURE);
        }
      //проверяем, что произошло:
      //получили запрос на подключение
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
              //добавим нового клиента в "список" активных клиентов
                for (size_t i = 0; i < MAX_CLIENT_NUMBER; ++i)
                {
                    if (active_connections[i] == IS_FREE)
                    {
                        active_connections[i] = connector_socket;
                        connector_socket = IS_FREE;
                        break;
                    }
                }
                if (connector_socket != IS_FREE)
                {
                    fprintf(stderr, "Too many clients.\n");
                    exit(EXIT_FAILURE);
                }
            }
        }
      //проверяем события клиентов
        for (size_t i = 0; i < MAX_CLIENT_NUMBER; ++i)
        {
            if (active_connections[i] != IS_FREE)
            {
                if (FD_ISSET(active_connections[i], &error_set))
                {
                    printf("Disconnected(error_set).\n");
                    close(active_connections[i]);
                    active_connections[i] = IS_FREE;
                    continue;
                }
                if (FD_ISSET(active_connections[i], &read_set))
                {
                    code = recv(active_connections[i], buffer, BUFFER_SIZE, 0);
                    if(code <= 0) 
                    {
                        printf("Disconnected.\n");
                        close(active_connections[i]);
                        active_connections[i] = IS_FREE;
                        continue;
                    }
                    printf("Server got: %s\n", buffer);
                    send_back_to_all(buffer, active_connections, i);
                }
            }
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
            if (strlen(line) != 0)
            {
                send_message(line, write_socket, strlen(line) + 1);
            }
        }
      //(2) пришло сообшение с сервера, необходимо напечатать
        if (FD_ISSET(write_socket, &read_set))
        {
            code = recv(write_socket, buffer, BUFFER_SIZE, 0);
            if (code <= 0)
            {
                free(line);
                exit(EXIT_FAILURE);
            }
            printf("\rServer told: %s\n", buffer);
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