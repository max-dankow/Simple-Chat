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

int init_connection_socket(int port)
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

void run_server(int port)
{
    printf("Hello server: %d!\n", port);
    int listen_socket = init_connection_socket(port);
    listen(listen_socket, 2);
    printf("listen is %d\n", listen_socket);
    int active_connections[MAX_CLIENT_NUMBER];
    memset(active_connections, 0, MAX_CLIENT_NUMBER * sizeof(int));
    char buffer[BUFFER_SIZE];

    while(1)
    {
      //создаем множество дескрипторов, отслеживемых для чтения  
        fd_set read_set;
        FD_ZERO(&read_set);
      //добавляем в него всех подключившихся уже клиентов и сокет для подключения
        FD_SET(listen_socket, &read_set);
        int max = listen_socket;
        for (size_t i = 0; i < MAX_CLIENT_NUMBER; ++i)
        {
            if (active_connections[i] != IS_FREE)
            {
                FD_SET(active_connections[i], &read_set);
                if (active_connections[i] > max)
                {
                    max = active_connections[i];
                }
            }
        }
      //ждем событий на чтение
        printf("waiting...\n");
        int code = select(max + 1, &read_set, NULL, NULL, NULL);
        if (code == -1)
        {
            perror("Select error.");
            exit(EXIT_FAILURE);
        }
      //проверяем, что произошло:
      //(1) получили запрос на подключение
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

      //(2) получили сообщение от клиента
        for (size_t i = 0; i < MAX_CLIENT_NUMBER; ++i)
        {
            if (active_connections[i] != IS_FREE && FD_ISSET(active_connections[i], &read_set))
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
                //send(, buffer, code, 0);
            }
        }
    }

    close(listen_socket);
    printf("Goodbye server!\n");
}

void run_client(int port, char* server_addr)
{
    printf("Hello client!\n");
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

    for (int i = 0; i < 10; ++i)
    {
        char str[100];
        scanf("%s", str);
        send(write_socket, str, strlen(str)+1, 0);
    }

    close(write_socket);
    printf("Goodbye client!\n");
}

void read_args(int argc, char** argv, int* mode, 
               char** addr, int* port)
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
    return 0;
}