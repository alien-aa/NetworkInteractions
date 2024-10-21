#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#define MAX_PORT 65535
#define MIN_PORT 0

#define FILENAME "msg.txt"

#define DATAGRAM_RECV_LEN 50000
#define MESSAGES_NUM 20
#define CLIENTS_NUM 50

#define NOT_FOUND -1
#define DEL_CLIENT 1

struct database
{
sockaddr_in id;
time_t since_last_activity;
bool activity;
int messages_recieved[MESSAGES_NUM];
int len_array;
};

void incorrect_input() {
perror("ERROR: invalid arguments.\n");
exit(EXIT_FAILURE);
}

int sock_err(const char* function, int s)
{
int err = errno;
printf("%s: socket error: %d\n", function, err);
return EXIT_FAILURE;
}

int set_non_block_mode(int s)
{
int fl = fcntl(s, F_GETFL, 0);
return fcntl(s, F_SETFL, fl | O_NONBLOCK);
}

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

void check_file(FILE* file)
{
if (!file)
{
perror("ERROR: file opening error.\n");
exit(EXIT_FAILURE);
}
}

void get_input_info(char *args[], unsigned int *start_port, unsigned int *end_port) {
*start_port = atoi(args[1]);
*end_port = atoi(args[2]);

if (*start_port < MIN_PORT || *start_port > MAX_PORT ||
    *end_port < MIN_PORT || *end_port > MAX_PORT ||
    *start_port > *end_port) {
    incorrect_input();
}
}

int check_new_client(struct database db[], struct sockaddr_in addr, int len_array)
{
for (int i = 0; i < len_array; i++)
{
if (db[i].id.sin_port == addr.sin_port)
{
return i;
}
}
return NOT_FOUND;
}

int new_db_client(struct database db[], struct sockaddr_in addr, int index)
{
if (index < CLIENTS_NUM)
{
db[index].id = addr;
db[index].since_last_activity = time(NULL);
db[index].activity = 1;

    unsigned int ipv4 = ntohl(db[index].id.sin_addr.s_addr);
    int port = ntohs(db[index].id.sin_port);
    printf("New client detected: %u.%u.%u.%u:%i\n", (ipv4 >> 24) & 0xFF, \
            (ipv4 >> 16) & 0xFF, (ipv4 >> 8) & 0xFF, ipv4 & 0xFF, port);
    return index;
}
else
{
    return EXIT_FAILURE;
}
}

int check_activity(struct database db[], int index)
{
if (difftime(time(NULL), db->since_last_activity) >= 30)
{
db[index].activity = 0;
db[index].since_last_activity = 0;
memset(&db[index], 0x00, sizeof(db[index]));
unsigned int ipv4 = ntohl(db[index].id.sin_addr.s_addr);
int port = ntohs(db[index].id.sin_port);
printf("Client removed from db: %u.%u.%u.%u:%i\n", (ipv4 >> 24) & 0xFF,
(ipv4 >> 16) & 0xFF, (ipv4 >> 8) & 0xFF, ipv4 & 0xFF, port);
return DEL_CLIENT;
}
return 0;
}

int rec_result(struct database db[], char* data, int i, FILE *server_file)
{
int stop = 0;
char output_string[DATAGRAM_RECV_LEN] = {0};
int num_msg = 0, index = 0;
for (int j = 0; j < 4; j++)
{
num_msg = (num_msg << 8) | (unsigned char)data[index++];
}
for (int j = 0; j < db[i].len_array; j++)
{
if (db[i].messages_recieved[j] == num_msg)
{
//printf("WARNING: already recieved message\n");
return -1;
}
}

int c_ip = ntohl(db[i].id.sin_addr.s_addr);
int cp = ntohs(db[i].id.sin_port);
fprintf(server_file, "%u.%u.%u.%u:%d ", (c_ip >> 24) & 0xFF, \
    (c_ip >> 16) & 0xFF, (c_ip >> 8) & 0xFF, c_ip & 0xFF, cp);

char buffer[5] = {0};
strncpy(buffer, (data + 4), 4);
unsigned int d = ntohl(*((unsigned int*)buffer));
memset(buffer, 0x00, 5*sizeof(char));
int dd = d % 100;
int mm = (d / 100) % 100;
int yyyy = d / 10000;
fprintf(server_file, "%02d.%02d.%04d ", dd, mm, yyyy);

strncpy(buffer, (data + 8), 4);
unsigned int t1 = ntohl(*((unsigned int*)buffer));
memset(buffer, 0x00, 5*sizeof(char));
int hh_1 = t1 / 10000;
int mm_1 = (t1 / 100) % 100;
int ss_1 = t1 % 100;
fprintf(server_file, "%02d:%02d:%02d ", hh_1, mm_1, ss_1);

strncpy(buffer, (data + 12), 4);
unsigned int t2 = ntohl(*((unsigned int*)buffer));
memset(buffer, 0x00, 5*sizeof(char));
int hh_2 = t2 / 10000;
int mm_2 = (t2 / 100) % 100;
int ss_2 = t2 % 100;
fprintf(server_file, "%02d:%02d:%02d ", hh_2, mm_2, ss_2);

strncpy(buffer, (data + 16), 4);
unsigned int l = ntohl(*((unsigned int*)buffer));
memset(buffer, 0x00, 5*sizeof(char));

char *message = (char *)malloc((l + 1)*sizeof(char));
memset(message, 0x00, (l + 1)*sizeof(char));
strncpy(message, (buffer + 20), l);
message[l] = '\0';
fprintf(server_file, "%s\n", message);


db[i].messages_recieved[db[i].len_array] = num_msg;
db[i].len_array++;

if (!strcmp(message, "stop")) stop = 1;

return stop;
Найти еще
}

int main(int argc, char *argv[]) {
if (argc != 3) {
perror("ERROR: invalid number of arguments.\n");
return EXIT_FAILURE;
}

unsigned int start_port = 0, end_port = 0;
get_input_info(argv, &start_port, &end_port);

struct sockaddr_in addr;
int len_array = end_port - start_port + 1;
int *sock_array = (int *)smalloc(len_array*sizeof(int));
int *port_array = (int *)smalloc(len_array*sizeof(int));


for (int i = 0; i < len_array; i++)
{
    memset(&addr, 0x00, sizeof(addr));
    sock_array[i] = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_array[i] < 0) 
    {
        return sock_err("socket", sock_array[i]);
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons(start_port + i);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    port_array[i] = start_port + i;
    if (bind(sock_array[i], (struct sockaddr*) &addr, sizeof(addr)) < 0) 
    {
        return sock_err("bind", sock_array[i]);
    }
    set_non_block_mode(sock_array[i]);
}

printf("Listening on: ");
for (int i = 0; i < len_array; i++)
{
    printf("%d ", sock_array[i]);
}
printf("\n");

int ls = sock_array[0];
fd_set rfd;
fd_set wfd;
int nfds = ls;
int i;
struct timeval tv = { 1, 0 };

char revc_datagramm[DATAGRAM_RECV_LEN] = {0};
struct database *db = {0};
int end_server = 0;

int addrlen = 0;
FILE *output_file = fopen(FILENAME, "a");
check_file(output_file);

while (1)
{
    FD_ZERO(&rfd);
    FD_ZERO(&wfd);

    for (i = 0; i < len_array; i++)
    {
        FD_SET(sock_array[i], &rfd);
        FD_SET(sock_array[i], &wfd);
        if (nfds < sock_array[i]) nfds = sock_array[i];
    }
    if (select(nfds + 1, &rfd, &wfd, 0, &tv) > 0)
    {
        if (FD_ISSET(ls, &rfd))
        {}
        for (i = 0; i < len_array; i++)
        {
            if (FD_ISSET(sock_array[i], &rfd))
            {
                addrlen = sizeof(addr);
                int result = recvfrom(sock_array[i], revc_datagramm, DATAGRAM_RECV_LEN, 0, (struct sockaddr*) &addr, (socklen_t *) &addrlen);
                if (check_new_client(db, addr, len_array) == NOT_FOUND)
                {
                    if (new_db_client(db, addr, i) == EXIT_FAILURE)
                    {
                        perror("ERROR: add new client to database\n");
                        exit(EXIT_FAILURE);
                    }
                }
                else if (result > 0)
                {
                    db[i].activity = 1;
                    db[i].since_last_activity = time(NULL);
                    end_server = rec_result(db, revc_datagramm, i, output_file);
                    if (end_server == 1)
                    {
                        printf("'stop' message arrived. Terminating...\n");
                        for (int k = 0; k < len_array; k++)
                        {
                            int c_ip = ntohl(db[i].id.sin_addr.s_addr);
                            int cp = ntohs(db[i].id.sin_port);
                            printf("    Peer disconnected: %u.%u.%u.%u:%i\n",(c_ip >> 24) & 0xFF, \
                                    (c_ip >> 16) & 0xFF, (c_ip >> 8) & 0xFF, c_ip & 0xFF, cp); 
                            close(sock_array[k]);
                        }
                        free(sock_array);
                        free(port_array);
                        fclose(output_file);
                        return EXIT_SUCCESS;
                    }
                    else if (end_server == 0)
                    {
                        send_respose();
                    }
                }
            }
            if (FD_ISSET(sock_array[i], &wfd))
            {}
            if(check_activity(db, i))
            {
                close(sock_array[i]);
                sock_array[i] = -1;
                len_array--;
            }
        }
    }
    else
    {
        sock_err("select", sock_array[i]);
    }
}

free(sock_array);
free(port_array);
fclose(output_file);
return EXIT_SUCCESS;
Найти еще
}