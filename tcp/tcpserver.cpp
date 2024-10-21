#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib") 

#include <stdio.h>
#include <stdlib.h>

#define FILENAME "msg.txt"
#define N 10 

#define PUT 1
#define GET 2
#define STOP 3
#define OK 4
#define ERR -1

#define LEN_BUFFER 50000
#define QUEUE_SIZE 5

struct msg_data
{
    unsigned int file_number;
    unsigned int data;
    unsigned int time_1;
    unsigned int time_2;
    unsigned int len_message;
    char *message;
};

void check_file(FILE* file)
{
    if (!file)
    {
        perror("ERROR: file opening error.\n");
        exit(EXIT_FAILURE);
    }
}

int init()
{
#ifdef _WIN32
	WSADATA wsa_data;
	return (0 == WSAStartup(MAKEWORD(2, 2), &wsa_data));
#else
	return 1; 
#endif
}

int set_non_block_mode(int s)
{
#ifdef _WIN32
    unsigned long mode = 1;
    return ioctlsocket(s, FIONBIO, &mode);
#else
    int fl = fcntl(s, F_GETFL, 0);
    return fcntl(s, F_SETFL, fl | O_NONBLOCK);
#endif
}

int sock_err(const char* function, int s)
{
    int err;
#ifdef _WIN32
    err = WSAGetLastError();
#else
    err = errno;
#endif
    fprintf(stderr, "%s: socket error: %d\n", function, err);
    return -1;
}

int put_or_get(SOCKET sock, int c_ip, int cp)
{
    char *buffer = (char *)malloc(LEN_BUFFER*sizeof(char));
    memset(buffer, 0x00, LEN_BUFFER*sizeof(char));
    if (recv(sock, buffer, 3, 0) < 0) 
    {
        return sock_err("receive", sock);
    }
    else if (buffer[0] == 'p' && buffer[1] == 'u' && buffer[2] == 't') 
    {
        return PUT;
    }
    else if (buffer[0] == 'g' && buffer[1] == 'e' && buffer[2] == 't')
    {
        return GET;
    }
    else 
    {
        printf("ERROR: recieved %s - not put or get\n", buffer);
        return ERR;
    }
}

int read_messages(SOCKET sock,  int c_ip, int cp, FILE *server_file)
{
    char *buffer = (char *)malloc(LEN_BUFFER*sizeof(char));
    memset(buffer, 0x00, LEN_BUFFER*sizeof(char));
    int part = 0;
    while (1)
    {
        unsigned int data = 0;
        unsigned int time_1 = 0;
        unsigned int time_2 = 0;
        unsigned int len = 0;

        part = recv(sock, buffer, 4, 0);
        if (part <= 0) return ERR;
        memset(buffer, 0x00, LEN_BUFFER*sizeof(char));

        fprintf(server_file, "%u.%u.%u.%u:%d ", (c_ip >> 24) & 0xFF, \
        (c_ip >> 16) & 0xFF, (c_ip >> 8) & 0xFF, c_ip & 0xFF, cp);

        part = recv(sock, buffer, 4, 0);
        if (part <= 0) return ERR;
        data = ntohl(*((unsigned int*)buffer));
        memset(buffer, 0x00, LEN_BUFFER*sizeof(char));
        int dd = data % 100;
        int mm = (data / 100) % 100;
        int yyyy = data / 10000;
        fprintf(server_file, "%02d.%02d.%04d ", dd, mm, yyyy);


        part = recv(sock, buffer, 4, 0);
        if (part < 0) return ERR;
        time_1 = ntohl(*((unsigned int*)buffer));
        memset(buffer, 0x00, LEN_BUFFER*sizeof(char));
        int hh_1 = time_1 / 10000;
        int mm_1 = (time_1 / 100) % 100;
        int ss_1 = time_1 % 100;
        fprintf(server_file, "%02d:%02d:%02d ", hh_1, mm_1, ss_1);

        part = recv(sock, buffer, 4, 0);
        if (part < 0) return ERR;
        time_2 = ntohl(*((unsigned int*)buffer));
        memset(buffer, 0x00, LEN_BUFFER*sizeof(char));
        int hh_2 = time_2 / 10000;
        int mm_2 = (time_2 / 100) % 100;
        int ss_2 = time_2 % 100;
        fprintf(server_file, "%02d:%02d:%02d ", hh_2, mm_2, ss_2);

        part = recv(sock, buffer, 4, 0);
        if (part < 0) return ERR;
        len = ntohl(*((unsigned int*)buffer));
        memset(buffer, 0x00, LEN_BUFFER*sizeof(char));

        char *message = (char *)malloc((len + 1)*sizeof(char));
        memset(message, 0x00, (len + 1)*sizeof(char));
        part = recv(sock, message, len, 0);
        message[len] = '\0';
        fprintf(server_file, "%s\n", message);

        if(send(sock, "ok", 2, 0) < 0) return sock_err("sending ok", sock);

        if (!strcmp(message, "stop"))
        {
            free(message);
            return STOP;
        } 
        free(message);
    }
}

int parse_part(char* string, unsigned int *data, unsigned int *time1, unsigned int *time2, char* message)
{
    char *ptr = string;
    while(*ptr != ' ') ptr++;
    ptr++;
    unsigned int day = (ptr[0] - '0') * 10 + ptr[1] - '0';
    unsigned int month = (ptr[3] - '0') * 10 + ptr[4] - '0';
    unsigned int year = (ptr[6] - '0') * 1000 + (ptr[7] - '0') * 100 + (ptr[8] - '0') * 10 + ptr[9] - '0';
    if (day == 0 || month == 0 || year == 0) return ERR;
    *data = 10000*year + 100*month + day;

    unsigned int h1 = (ptr[11] - '0') * 10 + ptr[12] - '0';
    unsigned int m1 = (ptr[14] - '0') * 10 + ptr[15] - '0';
    unsigned int s1 = (ptr[17] - '0') * 10 + ptr[18] - '0';
    if (h1 > 23 || m1 > 59 || s1 > 59) return ERR;
    *time1 = 10000*h1 + 100*m1 + s1;

    unsigned int h2 = (ptr[20] - '0') * 10 + ptr[21] - '0';
    unsigned int m2 = (ptr[23] - '0') * 10 + ptr[24] - '0';
    unsigned int s2 = (ptr[26] - '0') * 10 + ptr[27] - '0';
    if (h2 > 23 || m2 > 59 || s2 > 59) return ERR;
    *time2 = 10000*h2 + 100*m2 + s2;

    for (int i  = 0; i < strlen(ptr)-1; i++)
    {
        *(message + i) = *(ptr + i);
    }
    *(message + strlen(ptr)-1) = '\0';

    return OK;
}

int send_part(struct msg_data* it[QUEUE_SIZE], int s, int num)
{
    int result = 0;
    
    for (int i = 0; i < num; i++)
    {
        struct msg_data *send_me = it[i];
        if (send(s, (const char *)(&(send_me->file_number)), 4, 0) < 0) result = ERR;
        if (send(s, (const char *)(&(send_me->data)), 4, 0) < 0) result = ERR;
        if (send(s, (const char *)(&(send_me->time_1)), 4, 0) < 0) result = ERR;
        if (send(s, (const char *)(&(send_me->time_2)), 4, 0) < 0) result = ERR;
        if (send(s, (const char *)(&(send_me->len_message)), 4, 0) < 0) result = ERR;
        if (send(s, (const char *)send_me->message, strlen(send_me->message), 0) < 0) result = ERR;
        
        result++;
        
        //free(send_me->message);
        //free(send_me);
    }

    return result;
}

void send_client(FILE* file, int s)
{
    int counter = 0, file_counter = 0;
    char* buffer = (char*)malloc(LEN_BUFFER*sizeof(char));
    memset(buffer, 0x00, LEN_BUFFER*sizeof(char));
    struct msg_data* items[QUEUE_SIZE] = {0};

    while(fgets(buffer, LEN_BUFFER, file))
    {
        if (*buffer == '\n') 
        {
            memset(buffer, 0x00, LEN_BUFFER*sizeof(char));
            continue;
        }
        else
        {
            unsigned int data = 0, time_1 = 0, time_2 = 0;
            char *msg = (char *)malloc(strlen(buffer)*sizeof(char));
            memset(msg, 0x00, strlen(buffer)*sizeof(char));
            if (parse_part(buffer, &data, &time_1, &time_2, msg) == ERR)
            {
                memset(buffer, 0x00, LEN_BUFFER*sizeof(char));
                free(msg);
                continue;
            }
            struct msg_data *curr_msg = (struct msg_data *)malloc(sizeof(struct msg_data));
            memset(curr_msg, 0x00, sizeof(struct msg_data));
            curr_msg->file_number = htonl(file_counter);
            curr_msg->data = htonl(data);
            curr_msg->time_1 = htonl(time_1);
            curr_msg->time_2 = htonl(time_2);
            curr_msg->len_message = htonl(strlen(msg));
            curr_msg->message = (char *)malloc(strlen(msg)*sizeof(char));
            strcpy(curr_msg->message, msg);
            items[counter] = curr_msg;
            counter++; 
            file_counter++;
            if (counter == QUEUE_SIZE)
            {
                if(send_part(items, s, QUEUE_SIZE) == ERR) 
                {
                    free(buffer);
                    return;
                }
                counter = 0;
                for (int j = 0; j < QUEUE_SIZE; j++)
                {
                    items[j] = NULL;
                }
            }
            memset(buffer, 0x00, LEN_BUFFER*sizeof(char));
        }
    }
    if (file_counter % QUEUE_SIZE > 0)
    {
        if(send_part(items, s, file_counter % QUEUE_SIZE) == ERR) 
        {
            return;
        }
        counter = 0;
        for (int j = 0; j < file_counter % QUEUE_SIZE; j++)
        {
            items[j] = NULL;
        }
    }
    free(buffer);
    return;
}

int main(int argc, char* argv[])
{
	if (argc != 2)
	{
        perror("ERROR: invalid number of arguments.\n");
        return EXIT_FAILURE;
	}

	unsigned int port = atoi(argv[1]);
	init();

	int listen_s = socket(AF_INET, SOCK_STREAM, 0);
    set_non_block_mode(listen_s);
    if (listen_s < 0) return sock_err("init non-block socket", listen_s);

    struct sockaddr_in addr;
    memset(&addr, 0x00, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listen_s, (struct sockaddr*) &addr, sizeof(addr)) < 0) return sock_err("bind", listen_s);

    if (listen(listen_s, 1) < 0) return sock_err("listen", listen_s);

    printf("Listening TCP port: %d\n", port);

    int cs[N] = {0}, c_ip[N] = {0}, cp[N] = {0};
    int c_mode[N] = {0}; 
    int cs_cnt = 0, mode = 0;
    WSAEVENT events[2];
    WSANETWORKEVENTS ne;
    events[0] = WSACreateEvent();
    events[1] = WSACreateEvent();
    WSAEventSelect(listen_s, events[0], FD_ACCEPT);
    for (int i = 0; i < cs_cnt; i++)
    {
        WSAEventSelect(cs[i], events[1], FD_READ | FD_WRITE | FD_CLOSE);
    }
    do
    {
        DWORD dw = WSAWaitForMultipleEvents(2, events, FALSE, 1000, FALSE);
        WSAResetEvent(events[0]);
        WSAResetEvent(events[1]);

        if (0 == WSAEnumNetworkEvents(listen_s, events[0], &ne) && (ne.lNetworkEvents & FD_ACCEPT))
        {
            int add_index = -1;
            for (int j = 0; j < N; j++)
            {
                if(cs[j] == NULL)
                {
                    add_index = j;
                    break;
                }
            }

            int addrlen = sizeof(addr);
            SOCKET c_s = accept(listen_s, (struct sockaddr*) &addr, &addrlen);
            if (c_s < 0)
            {
                sock_err("accept", listen_s);
                break;
            }
            cs[add_index] = c_s;
            c_ip[add_index] = ntohl(addr.sin_addr.s_addr);
            cp[add_index] = ntohs(addr.sin_port);
            if (cs[add_index] < 0) return sock_err("init non-block socket", cs[add_index]);

            WSAEventSelect(cs[add_index], events[1], FD_READ | FD_WRITE | FD_CLOSE);
            ne.lNetworkEvents = FD_READ;
            
            cs_cnt++;
            printf("    Client connected: %u.%u.%u.%u:%i\n", (c_ip[add_index] >> 24) & 0xFF, \
            (c_ip[add_index] >> 16) & 0xFF, (c_ip[add_index] >> 8) & 0xFF, c_ip[add_index] & 0xFF, cp[add_index]);
        }
        for (int i = 0; i < cs_cnt; i++)
        {
            if (0 == WSAEnumNetworkEvents(cs[i], events[1], &ne))
            {
                if (ne.lNetworkEvents & FD_CLOSE)
                {
                    printf("    Peer disconnected: %u.%u.%u.%u:%i\n", (c_ip[i] >> 24) & 0xFF, \
                    (c_ip[i] >> 16) & 0xFF, (c_ip[i] >> 8) & 0xFF, c_ip[i] & 0xFF, cp[i]);
                    
                    closesocket(cs[i]);
                    cs[i] =  NULL;
                    c_ip[i] = NULL;
                    cp[i] = NULL;
                    c_mode[i] = NULL;
                    cs_cnt--;

                    continue;
                }

                if (ne.lNetworkEvents & FD_READ)
                {
					if (c_mode[i] == 0)
                    {
                        mode = put_or_get(cs[i], c_ip[i], cp[i]);
                        c_mode[i] = mode;
                    } 
                    if (c_mode[i] == PUT) 
                    {
                        FILE *server_file = fopen(FILENAME, "a+");
                        mode = read_messages(cs[i], c_ip[i], cp[i], server_file);
                        if (mode == STOP) 
                        {
                            printf("'stop' message arrived. Terminating...\n");
                            for (int k = 0; k < cs_cnt; k++)
                            {
                                printf("    Peer disconnected: %u.%u.%u.%u:%i\n", (c_ip[k] >> 24) & 0xFF, \
                                (c_ip[k] >> 16) & 0xFF, (c_ip[k] >> 8) & 0xFF, c_ip[k] & 0xFF, cp[k]);
                                
                                closesocket(cs[k]);
                                cs[k] =  NULL;
                                c_ip[k] = NULL;
                                cp[k] = NULL;
                                c_mode[k] = NULL;
                            }
                            
                            fclose(server_file);
                            closesocket(listen_s);
                            WSACleanup(); 
                            return EXIT_SUCCESS;
                        }
                        fclose(server_file);
                    }
                    if (c_mode[i] == GET) 
                    {
                        ne.lNetworkEvents |= FD_WRITE;
                    }
                    else continue;
                }
                
                if (ne.lNetworkEvents & FD_WRITE)
                {
                    if (c_mode[i] == GET)
                    {
                        FILE *server_file = fopen(FILENAME, "r");
                        send_client(server_file, cs[i]);
                        printf("    Peer disconnected: %u.%u.%u.%u:%i\n", (c_ip[i] >> 24) & 0xFF, \
                        (c_ip[i] >> 16) & 0xFF, (c_ip[i] >> 8) & 0xFF, c_ip[i] & 0xFF, cp[i]);
                        closesocket(cs[i]);
                        cs[i] =  NULL;
                        c_ip[i] = NULL;
                        cp[i] = NULL;
                        c_mode[i] = NULL;
                        cs_cnt--;
                        fclose(server_file);
                    }
                    continue;
                }
            }
        }
    } while (1);
    
    closesocket(listen_s);
    WSACleanup(); 
    return EXIT_SUCCESS;
}

