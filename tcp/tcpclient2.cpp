#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#define END_CLIENT -1
#define OK_CLIENT 0
#define LEN_IPV4 15
#define LEN_PORT 5
#define MAX_PORT 65535
#define LEN_FILENAME 255
#define FILENAME_END ".txt"

#define LEN_BUFFER 50000
#define WRONG_DATA 1
#define CORRECT_DATA 0


struct msg_data
{
    unsigned int file_number;
    unsigned int data;
    unsigned int time_1;
    unsigned int time_2;
    unsigned int len_message;
    char *message;
};

void incorrect_input()
{
    perror("ERROR: invalid arguments.\n");
    exit(EXIT_FAILURE);
}

void check_file(FILE* file)
{
    if (!file)
    {
        perror("ERROR: file opening error.\n");
        exit(EXIT_FAILURE);
    }
}

void get_input_info(char *args[], char* addr, unsigned int* port, char* file)
{
    if (strlen(args[1]) < 9 || strlen(args[1]) > LEN_IPV4+LEN_PORT+1 || strlen(args[2]) > 255) incorrect_input();
    
    char *ptr = strtok(args[1], ":");

    if (strlen(ptr) < 7 || strlen(ptr) > LEN_IPV4 || ptr == NULL) incorrect_input();
    
    strcpy(addr, ptr);
    ptr = strtok(NULL, ":");

    if (inet_addr(addr) == INADDR_NONE || strlen(ptr) < 1 || strlen(ptr) > LEN_PORT || atoi(ptr) < 0 || atoi(ptr) > MAX_PORT) incorrect_input();

    *port = atoi(ptr);
    ptr = strtok(NULL, ":");

    if (ptr != NULL || strcmp("get", args[2]) || strcmp(".txt", strstr(args[3], FILENAME_END))) incorrect_input();
    
    strcpy(file, args[3]);
}

int try_to_connect(struct sockaddr_in *saddr, int s_fd)
{
    for (int attempts = 0; attempts < 10; attempts++)
    {
        int result = connect(s_fd, (struct sockaddr*) saddr, sizeof(*saddr));
        if(result && attempts == 9)
        {
            int error = errno;
            printf("ERROR: Connection %d socket error %d.\n", s_fd, error);
            close(s_fd);
            return END_CLIENT;
        }
        else if (result)
        {
            usleep(100*1000);
        }
        else return OK_CLIENT;
    }
}

int send_get(int s_fd)
{
    char sym[] = {'g', 'e', 't'};
    int res = send(s_fd, sym, 3, MSG_NOSIGNAL);
    if (res < 0) return END_CLIENT;
    return OK_CLIENT;
}


void receive_messages(FILE* file, int s, int ip, int p)
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

        part = recv(s, buffer, 4, MSG_NOSIGNAL);
        if (part <= 0) break;
        memset(buffer, 0x00, LEN_BUFFER*sizeof(char));

        fprintf(file, "%d.%d.%d.%d:%d ", ip & 0xFF, \
        (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, (ip >> 24) & 0xFF, p);

        part = recv(s, buffer, 4, MSG_NOSIGNAL);
        if (part <= 0) break;
        data = ntohl(*((unsigned int*)buffer));
        memset(buffer, 0x00, LEN_BUFFER*sizeof(char));
        int dd = data % 100;
        int mm = (data / 100) % 100;
        int yyyy = data / 10000;
        fprintf(file, "%02d.%02d.%04d ", dd, mm, yyyy);


        part = recv(s, buffer, 4, MSG_NOSIGNAL);
        if (part < 0) break;
        time_1 = ntohl(*((unsigned int*)buffer));
        memset(buffer, 0x00, LEN_BUFFER*sizeof(char));
        int hh_1 = time_1 / 10000;
        int mm_1 = (time_1 / 100) % 100;
        int ss_1 = time_1 % 100;
        fprintf(file, "%02d:%02d:%02d ", hh_1, mm_1, ss_1);

        part = recv(s, buffer, 4, MSG_NOSIGNAL);
        if (part < 0) break;
        time_2 = ntohl(*((unsigned int*)buffer));
        memset(buffer, 0x00, LEN_BUFFER*sizeof(char));
        int hh_2 = time_2 / 10000;
        int mm_2 = (time_2 / 100) % 100;
        int ss_2 = time_2 % 100;
        fprintf(file, "%02d:%02d:%02d ", hh_2, mm_2, ss_2);

        part = recv(s, buffer, 4, MSG_NOSIGNAL);
        if (part < 0) break;
        len = ntohl(*((unsigned int*)buffer));
        memset(buffer, 0x00, LEN_BUFFER*sizeof(char));

        char *message = (char *)malloc((len + 1)*sizeof(char));
        memset(message, 0x00, (len + 1)*sizeof(char));
        part = recv(s, message, len, MSG_NOSIGNAL);
        message[len] = '\0';
        fprintf(file, "%s\n", message);
    }
    free(buffer);
    return;
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        perror("ERROR: invalid number of arguments.\n");
        return EXIT_FAILURE;
    }
    
    char* ipv4_addr = (char*)malloc(LEN_IPV4+1);
    memset(ipv4_addr, 0x00, (LEN_IPV4+1)*sizeof(char));
    unsigned int port = 0;
    char* filename = (char*)malloc(LEN_FILENAME+1);
    memset(filename, 0x00, (LEN_FILENAME+1)*sizeof(char));

    get_input_info(argv, ipv4_addr, &port, filename);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0x00, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ipv4_addr);

    if (try_to_connect(&addr, sockfd) == END_CLIENT)
    {
        free(ipv4_addr);
        free(filename);
        return EXIT_FAILURE;
    }

    if (send_get(sockfd) == END_CLIENT)
    {
        close(sockfd);
        free(ipv4_addr);
        free(filename);
        return EXIT_FAILURE;
    }

    FILE *output_file = fopen(filename, "a+");
    check_file(output_file);
    receive_messages(output_file, sockfd, inet_addr(ipv4_addr), port);

    close(sockfd);
    fclose(output_file);
    free(ipv4_addr);
    free(filename);
    return EXIT_SUCCESS;
}
