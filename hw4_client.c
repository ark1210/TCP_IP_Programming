#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define MAX_LINES 256	
#define BUF_SIZE 100
#define NAME_SIZE 20
#define PAKETSIZE 108	
void * send_msg(void * arg);
void * recv_msg(void * arg);
void error_handling(char * msg);
void printPackets();
void searchPacket(const char *searchTerm);	

typedef struct packet
{
    int count; //4
    char buf[BUF_SIZE]; //100
    int line_num;//4
} pkt;
char name[NAME_SIZE]="[DEFAULT]";
char msg[BUF_SIZE];
char temp[PAKETSIZE];
pkt packets[MAX_LINES];


int main(int argc, char *argv[])
{
	int sock;
	struct sockaddr_in serv_addr;
	pthread_t snd_thread, rcv_thread;
	void * thread_return;
	if(argc!=3) {
		printf("Usage : %s <IP> <port>\n", argv[0]);
		exit(1);
	 }
	
	//sprintf(name, "[%s]", argv[3]);
	sock=socket(PF_INET, SOCK_STREAM, 0);
	
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family=AF_INET;
	serv_addr.sin_addr.s_addr=inet_addr(argv[1]);
	serv_addr.sin_port=htons(atoi(argv[2]));
	  
	if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))==-1)
		error_handling("connect() error");
	
	pthread_create(&snd_thread, NULL, send_msg, (void*)&sock);
	pthread_create(&rcv_thread, NULL, recv_msg, (void*)&sock);
	pthread_join(snd_thread, &thread_return);
	pthread_join(rcv_thread, &thread_return);
	close(sock);  
	return 0;
}
	
void * send_msg(void * arg)   // send thread main
{
	int sock=*((int*)arg);
	//char name_msg[NAME_SIZE+BUF_SIZE];

	char userSearchTerm[BUF_SIZE];
    

	// while(1) 
	// {

		printf("Search Word: ");
		fgets(userSearchTerm, sizeof(userSearchTerm), stdin);
		userSearchTerm[strcspn(userSearchTerm, "\n")] = 0; // Removing newline character

		if(!strcmp(msg,"q\n")||!strcmp(msg,"Q\n")) 
		{
			close(sock);
			exit(0);
		}

		write(sock, userSearchTerm, strlen(userSearchTerm));
	//}
	return NULL;
}
	
void * recv_msg(void * arg)   // read thread main
{
    int sock=*((int*)arg);
    char result[MAX_LINES * (BUF_SIZE + 20)];
    int str_len = read(sock, result, sizeof(result) - 1);
    if(str_len == -1) 
        return (void*)-1;
    result[str_len] = 0;
    printf("%s", result);
    return NULL;
}

	
void error_handling(char *msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}
void printPackets()
{
    for (int i = 0; i < MAX_LINES; i++)
    {
        if (packets[i].buf[0] == '\0')
            break; // 문자열이 비어 있으면 루프 종료
        printf("Line %d\n", packets[i].line_num);
        printf("Buffer: %s\n", packets[i].buf);
        printf("Count: %d\n", packets[i].count);
        printf("------------------------\n");
    }
}

void searchPacket(const char *searchTerm)
{
    int found = 0;
    for (int i = 0; i < MAX_LINES; i++)
    {
        if (strstr(packets[i].buf, searchTerm))
        {
            printf("FOUND in Line %d: %s\n", packets[i].line_num, packets[i].buf);
            found = 1;
        }
    }
    if (!found)
    {
        printf("NOT FOUND: %s\n", searchTerm);
    }
}
