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

    if (ptr != NULL || strcmp(".txt", strstr(args[2], FILENAME_END))) incorrect_input();
    
    strcpy(file, args[2]);
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

int send_put(int s_fd)
{
    char sym[] = {'p', 'u', 't'};
    int res = send(s_fd, sym, 3, MSG_NOSIGNAL);
    if (res < 0) return END_CLIENT;
    return OK_CLIENT;
}

int check_message(char* string)
{
    if (strlen(string) < 29) return WRONG_DATA;
    if (string[2] != '.' || string[5] != '.' || string[10] != ' ' || string[13] != ':' || string[16] != ':' || \
    string[19] != ' ' || string[22] != ':' || string[25] != ':' || string[28] != ' ') return WRONG_DATA;
    if (!isdigit(string[0]) || !isdigit(string[1]) || \
        !isdigit(string[3]) || !isdigit(string[4]) || \
        !isdigit(string[6]) || !isdigit(string[7]) || !isdigit(string[8]) || !isdigit(string[9]) || \
        !isdigit(string[11]) || !isdigit(string[12]) || \
        !isdigit(string[14]) || !isdigit(string[15]) || \
        !isdigit(string[17]) || !isdigit(string[18]) || \
        !isdigit(string[20]) || !isdigit(string[21]) || \
        !isdigit(string[23]) || !isdigit(string[24]) || \
        !isdigit(string[26]) || !isdigit(string[27])) return WRONG_DATA;
    return CORRECT_DATA;
}

int check_data(unsigned int d, unsigned int m, unsigned int y)
{
    if (m > 12) return WRONG_DATA;
    if (m == 2 && (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0) && (d > 29)) return WRONG_DATA;
    if (m == 2 && !(y % 4 == 0 && y % 100 != 0) || (y % 400 == 0) && (d > 28)) return WRONG_DATA;
    if ((m == 1 || m == 3 || m == 5 || m == 7 || m == 8 || m == 10 || m == 12) && d > 31) return WRONG_DATA;
    if ((m == 4 || m == 6 || m == 9 || m == 11) && d > 30) return WRONG_DATA;
    return CORRECT_DATA;
}

int parse_part(char* string, unsigned int *data, unsigned int *time1, unsigned int *time2, char* message)
{
    unsigned int day = (string[0] - '0') * 10 + string[1] - '0';
    unsigned int month = (string[3] - '0') * 10 + string[4] - '0';
    unsigned int year = (string[6] - '0') * 1000 + (string[7] - '0') * 100 + (string[8] - '0') * 10 + string[9] - '0';
    if (day == 0 || month == 0 || year == 0 || check_data(day, month, year)) return WRONG_DATA;
    *data = 10000*year + 100*month + day;

    unsigned int h1 = (string[11] - '0') * 10 + string[12] - '0';
    unsigned int m1 = (string[14] - '0') * 10 + string[15] - '0';
    unsigned int s1 = (string[17] - '0') * 10 + string[18] - '0';
    if (h1 > 23 || m1 > 59 || s1 > 59) return WRONG_DATA;
    *time1 = 10000*h1 + 100*m1 + s1;

    unsigned int h2 = (string[20] - '0') * 10 + string[21] - '0';
    unsigned int m2 = (string[23] - '0') * 10 + string[24] - '0';
    unsigned int s2 = (string[26] - '0') * 10 + string[27] - '0';
    if (h2 > 23 || m2 > 59 || s2 > 59) return WRONG_DATA;
    *time2 = 10000*h2 + 100*m2 + s2;

    for (int i  = 0; i < strlen(string)-28; i++)
    {
        if (*(string + 29 + i) == '\n' || *(string + 29 + i) == '\0') {
            *(message + i) = '\0';
            break;
        }
        *(message + i) = *(string + 29 + i);
    }

    return CORRECT_DATA;
}

int send_part(struct msg_data* it[QUEUE_SIZE], int s, int num)
{
    int result = OK_CLIENT, sent_num = 0;
    char ok_string[3] = {0};
    
    for (int i = 0; i < num; i++)
    {
        struct msg_data *send_me = it[i];
        if (send(s, (void *)(&(send_me->file_number)), 4, MSG_NOSIGNAL) < 0) result = END_CLIENT;
        if (send(s, (void *)(&(send_me->data)), 4, MSG_NOSIGNAL) < 0) result = END_CLIENT;
        if (send(s, (void *)(&(send_me->time_1)), 4, MSG_NOSIGNAL) < 0) result = END_CLIENT;
        if (send(s, (void *)(&(send_me->time_2)), 4, MSG_NOSIGNAL) < 0) result = END_CLIENT;
        if (send(s, (void *)(&(send_me->len_message)), 4, MSG_NOSIGNAL) < 0) result = END_CLIENT;
        if (send(s, (void *)(send_me->message), strlen(send_me->message), MSG_NOSIGNAL) < 0) result = END_CLIENT;
        
        sent_num++;

        if (!strcmp(send_me->message, "stop")) 
        {
            free(send_me->message);
            free(send_me);
            break;
        }
        
        free(send_me->message);
        free(send_me);
    }
    
    for (int i = 0; i < sent_num; i++)
    {
        if (recv(s, (void *)ok_string, 2, MSG_NOSIGNAL) < 0 || ok_string[0] != 'o' || ok_string[1] != 'k') result = END_CLIENT;
        memset(ok_string, 0x00, 3*sizeof(char));
    }

    return result;
}

void read_messages(FILE* file, int s)
{
    int counter = 0, file_counter = 0;
    char* buffer = (char*)malloc(LEN_BUFFER*sizeof(char));
    memset(buffer, 0x00, LEN_BUFFER*sizeof(char));
    struct msg_data* items[QUEUE_SIZE] = {};
    while(fgets(buffer, LEN_BUFFER, file))
    {
        if (*buffer == '\n') 
        {
            memset(buffer, 0x00, LEN_BUFFER*sizeof(char));
            continue;
        }
        else if (check_message(buffer)) 
        {
            memset(buffer, 0x00, LEN_BUFFER*sizeof(char));
            continue;
        }
        else
        {
            unsigned int data = 0, time_1 = 0, time_2 = 0;
            char *msg = (char *)malloc((strlen(buffer) - 28)*sizeof(char));
            memset(msg, 0x00, (strlen(buffer) - 28)*sizeof(char));
            if (parse_part(buffer, &data, &time_1, &time_2, msg))
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
                if(send_part(items, s, QUEUE_SIZE) == END_CLIENT) 
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
        if(send_part(items, s, file_counter % QUEUE_SIZE) == END_CLIENT) 
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

int main(int argc, char *argv[])
{
    if (argc != 3)
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

    if (send_put(sockfd) == END_CLIENT)
    {
        close(sockfd);
        free(ipv4_addr);
        free(filename);
        return EXIT_FAILURE;
    }

    FILE *input_file = fopen(filename, "r");
    check_file(input_file);
    read_messages(input_file, sockfd);

    close(sockfd);
    fclose(input_file);
    free(ipv4_addr);
    free(filename);
    return EXIT_SUCCESS;
}
