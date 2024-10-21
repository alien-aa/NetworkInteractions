#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#pragma comment(lib, "ws2_32.lib")


#define LEN_IPV4 15
#define LEN_FILENAME 255
#define FILENAME_END ".txt"
#define LEN_PORT 5
#define MAX_PORT 65535

#define LEN_BUFFER 50000
#define LEN_RESP 80

#define END_CLIENT -1
#define WRONG_DATA 1
#define CORRECT_DATA 0

#define QUEUE_SIZE 20

struct msg_data
{
    unsigned int file_number;
    unsigned int data;
    unsigned int time_1;
    unsigned int time_2;
    unsigned int len_message;
    char *message;
};

struct datagramm
{
    char* data;
    bool correct_sent;
    int num_of_bites;
    unsigned int file_number;
};

void *smalloc(size_t size)
{
    void *result = malloc(size);
    if (result == NULL)
    {
        perror("ERROR: memory allocation.");
        exit(EXIT_FAILURE);
    }
    return result;
}

int sock_err(const char* function, int s)
{
    int err = errno;
    printf("%s: socket error: %d\n", function, err);
    return EXIT_FAILURE;
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

char* create_dtgrm(struct msg_data* msg)
{
    size_t datagram_size = 20 + strlen(msg->message);

    char *datagram = (char *)smalloc(datagram_size);
    memset(datagram, 0x00, datagram_size);
    
    for (int i = 0; i < 4; i++)
    {
        datagram[i] = (msg->file_number >> (8*(3 - i))) & 0xFF;
    }
    for (int i = 4; i < 8; i++)
    {
        datagram[i] = (msg->data >> (8*(3 - i%4))) & 0xFF;
    }
    for (int i = 8; i < 12; i++)
    {
        datagram[i] = (msg->time_1 >> (8*(3 - i%8))) & 0xFF;
    }
    for (int i = 12; i < 16; i++)
    {
        datagram[i] = (msg->time_2 >> (8*(3 - i%12))) & 0xFF;
    }
    for (int i = 16; i < 20; i++)
    {
        datagram[i] = (msg->len_message >> (8*(3 - i%16))) & 0xFF;
    }

    memcpy((datagram + 20), msg->message, strlen(msg->message));
    return datagram;
}

int send_part(struct datagramm* it[], int s, int num, struct sockaddr_in* addr)
{
    int sent_num = 0;

    for (int i = 0; i < num; i++)
    {
        struct datagramm *send_me = it[i];
        if (send_me->correct_sent) continue;
        if (sendto(s, send_me->data, send_me->num_of_bites, 0, (struct sockaddr*)addr, sizeof(struct sockaddr_in)) < 0) sock_err("sending data", s);
        sent_num++;
    }

    return sent_num;
}

int check_datagramms(int s, struct datagramm* it[], int items_num)
{
    char responce[LEN_RESP] = {0};
    struct timeval tv = {0, 100*1000};
    fd_set fds;
    FD_ZERO(&fds); 
    FD_SET(s, &fds);
    int count_correct = 0;

    int index = -1;
    for (int j = 0; j < items_num; j++)
    {
        if (select(s + 1, &fds, 0, 0, &tv) > 0)
        {
            struct sockaddr_in addr;
            int addrlen = sizeof(addr);
            if (recvfrom(s, responce, LEN_RESP, 0, (struct sockaddr*) &addr, &addrlen) <= 0)
            { 
                sock_err("recvfrom", s);
                return 0;
            }
            for (int i = 0; i < 20; i++) 
            { 
                memcpy(&index, &responce[i * 4], sizeof(index));
                
                index = (index >> 24) & 0x000000FF | (index >> 8) & 0x0000FFFF | \
                        (index << 8) & 0x00FF0000 | (index << 24) & 0xFF000000; 
                
                if (index >= 0)
                {
                    for (int i = 0; i < items_num; i++)
                    {
                        if (it[i]->file_number == index && it[i]->correct_sent != 1) 
                        {
                            it[i]->correct_sent = 1;
                            count_correct++;
                            break;
                        }
                    }
                }
                else
                {
                    sock_err("recvfrom value", s);
                    return END_CLIENT;
                }
            }
        }
        else break;
    }
    
    return count_correct;
}

void read_file(FILE *file, struct sockaddr_in* addr, int s)
{
    int counter = 0, file_counter = 0, stop_index = 100;
    char* buffer = (char*)smalloc(LEN_BUFFER*sizeof(char));
    memset(buffer, 0x00, LEN_BUFFER*sizeof(char));
    struct datagramm* items[QUEUE_SIZE] = {};

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
            char *msg = (char *)smalloc((strlen(buffer) - 28)*sizeof(char));
            memset(msg, 0x00, (strlen(buffer) - 28)*sizeof(char));
            if (parse_part(buffer, &data, &time_1, &time_2, msg))
            {
                memset(buffer, 0x00, LEN_BUFFER*sizeof(char));
                free(msg);
                continue;
            }
            struct msg_data *curr_msg = (struct msg_data *)smalloc(sizeof(struct msg_data));
            memset(curr_msg, 0x00, sizeof(struct msg_data));
            curr_msg->file_number = file_counter;
            curr_msg->data = data;
            curr_msg->time_1 = time_1;
            curr_msg->time_2 = time_2;
            curr_msg->len_message = strlen(msg);
            curr_msg->message = (char *)smalloc(strlen(msg)*sizeof(char));
            strcpy(curr_msg->message, msg);
            struct datagramm *curr_dtgrm = (struct datagramm *)smalloc(sizeof(struct datagramm));
            curr_dtgrm->correct_sent = 0;
            curr_dtgrm->num_of_bites = 20 + strlen(curr_msg->message);
            curr_dtgrm->file_number = curr_msg->file_number;
            curr_dtgrm->data = create_dtgrm(curr_msg);
            if (!strcmp(curr_msg->message, "stop"))
            {  
                free(curr_msg);
                if (curr_dtgrm->data == NULL) 
                {
                    perror("ERROR: error while creating datagramm.\n");
                    free(curr_dtgrm);
                    exit(EXIT_FAILURE);
                } 
                items[counter] = curr_dtgrm;
                counter++;
                file_counter++;
                int check_num = 0, checked = 0;
                while (1)
                {
                    check_num = send_part(items, s, counter, addr);
                    checked += check_datagramms(s, items, check_num);
                    if (checked == counter) break;
                }
                for (int j = 0; j < counter; j++)
                {
                    free(items[j]);
                    items[j] = NULL;
                }
                return;
            } 
            free(curr_msg);
            if (curr_dtgrm->data == NULL) 
            {
                perror("ERROR: error while creating datagramm.\n");
                free(curr_dtgrm);
                exit(EXIT_FAILURE);
            } 
            items[counter] = curr_dtgrm;
            counter++;
            file_counter++;
            if (counter == QUEUE_SIZE)
            {
                int check_num = 0, checked = 0;
                while (1)
                {
                    check_num = send_part(items, s, QUEUE_SIZE, addr);
                    checked += check_datagramms(s, items, check_num);
                    if (checked == QUEUE_SIZE) break;
                }                
                for (int j = 0; j < QUEUE_SIZE; j++)
                {
                    free(items[j]);
                    items[j] = NULL;
                }
                return;
            }
            memset(buffer, 0x00, LEN_BUFFER*sizeof(char));
        }
    }
    
    if (file_counter % QUEUE_SIZE > 0)
    {

        int check_num = 0, checked = 0;
        while (1)
        {
            check_num = send_part(items, s, file_counter % QUEUE_SIZE, addr);
            checked += check_datagramms(s, items, check_num);
            if (checked == file_counter % QUEUE_SIZE) break;
        }
        for (int j = 0; j < QUEUE_SIZE; j++)
        {
            free(items[j]);
            items[j] = NULL;
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        perror("ERROR: invalid number of arguments.\n");
        return EXIT_FAILURE;
    }
    
    char* ipv4_addr = (char*)smalloc(LEN_IPV4+1);
    memset(ipv4_addr, 0x00, (LEN_IPV4+1)*sizeof(char));
    char* filename = (char*)smalloc(LEN_FILENAME+1);
    memset(filename, 0x00, (LEN_FILENAME+1)*sizeof(char));
    unsigned int port = 0;
    get_input_info(argv, ipv4_addr, &port, filename);

    init();

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) return sock_err("socket", sockfd);

    struct sockaddr_in addr;
    memset(&addr, 0x00, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ipv4_addr);

    FILE *input_file = fopen(filename, "r");
    check_file(input_file);

    read_file(input_file, &addr, sockfd);

    closesocket(sockfd);
    WSACleanup(); 
    fclose(input_file);
    free(ipv4_addr);
    free(filename);
    return EXIT_SUCCESS;
}
