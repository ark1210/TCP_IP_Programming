#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <termios.h>
#include <unistd.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>

#define MAX_LINES 256	
#define BUF_SIZE 100
#define NAME_SIZE 20
#define PAKETSIZE 108	
void * send_msg(void * arg);
void * recv_msg(void * arg);
void error_handling(char * msg);
void printPackets();
void searchPacket(const char *searchTerm);	
void printWithHighlight(const char *text, const char *searchTerm);
int getch(); // getch 함수 선언
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
pthread_mutex_t lock;

char userSearchTerm[BUF_SIZE] = {0}; // 초기화

int main(int argc, char *argv[])
{
	//char userSearchTerm[BUF_SIZE]={0};
	int sock;
	struct sockaddr_in serv_addr;
	pthread_t snd_thread, rcv_thread;
	void * thread_return;
	if(argc!=3) {
		printf("Usage : %s <IP> <port>\n", argv[0]);
		exit(1);
	 }
	 // 뮤텍스 초기화
    pthread_mutex_init(&lock, NULL);
	
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
	    // 뮤텍스 제거
    pthread_mutex_destroy(&lock);
	close(sock);  
	return 0;
}
	

// void * send_msg(void * arg)
// {
// 	int sock = *((int *)arg);
// 	char userSearchTerm[BUF_SIZE];
// 	int idx = 0;

// 	printf("Search Word: ");
	
// 	while (1) 
// 	{
// 		int ch = getch(); // 문자 하나를 받아옴
		
// 		if (ch == '\n' || ch == EOF) break; // 엔터 키 누르면 종료

// 		if (ch == 127) // 백스페이스 키 처리
// 		{
// 			if (idx > 0) // 이미 입력된 문자가 있을 때만
// 			{
// 				putchar('\b'); // 커서를 하나 뒤로 이동
// 				putchar(' ');  // 현재 위치에 공백 출력 (문자 지움)
// 				putchar('\b'); // 커서를 다시 하나 뒤로 이동
// 				idx--; // 인덱스 줄임
// 				userSearchTerm[idx] = '\0'; // 문자열 끝 처리
// 			}
// 			continue; // 다음 루프로 이동
// 		}

// 		putchar(ch); // 입력받은 문자 출력
		
// 		userSearchTerm[idx++] = ch; // 문자를 검색어에 추가
// 		userSearchTerm[idx] = '\0'; // 문자열 끝 처리

// 		write(sock, userSearchTerm, strlen(userSearchTerm)); // 서버에 검색어 전송
// 		// 서버로부터 검색 결과를 받아옴
// 		char result[BUF_SIZE * MAX_LINES];
// 		read(sock, result, sizeof(result));

// 		// 화면을 지우고 새로운 검색 결과를 출력
// 		system("clear");
// 		printf("Search Word: %s\n", userSearchTerm);
// 		printf("-----------------------------\n");
// 		printWithHighlight(result, userSearchTerm);

// 	}
// 	return NULL;
// }

void * send_msg(void * arg)
{
    int sock = *((int *)arg);
    
    int idx = 0;

    printf("Search Word: "); // 처음 검색어 입력 프롬프트 출력

    while (1)
    {
        int ch = getch(); // 문자 하나를 받아옴
		 // 뮤텍스 잠그기
        pthread_mutex_lock(&lock);

        if (ch == '\n' || ch == EOF)
        {
            write(sock, userSearchTerm, strlen(userSearchTerm)); // 서버에 검색어 전송
        }

        if (ch == 127 || ch == 8) // 백스페이스 키 처리
        {
            if (idx > 0) // 이미 입력된 문자가 있을 때만
            {
                printf("\b \b"); // 백스페이스 처리
                idx--; // 인덱스 줄임
                userSearchTerm[idx] = '\0'; // 문자열 끝 처리
            }
        }
        else
        {
            putchar(ch); // 입력받은 문자 출력
            userSearchTerm[idx++] = ch; // 문자를 검색어에 추가
            userSearchTerm[idx] = '\0'; // 문자열 끝 처리
        }

        write(sock, userSearchTerm, strlen(userSearchTerm)); // 서버에 검색어 전송

		// 뮤텍스 해제
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

void * recv_msg(void * arg)
{
    int sock = *((int *)arg);
    char result[BUF_SIZE * MAX_LINES] = {0}; // 초기화

    while (1)
    {
        int bytes_read = read(sock, result, BUF_SIZE * MAX_LINES - 1); // 서버로부터 검색 결과를 받아옴
        result[bytes_read] = '\0'; // 문자열의 끝을 수동으로 처리
		printf("Received result from server: %s\n", result);
		  // 뮤텍스 잠그기
        pthread_mutex_lock(&lock);

        // 화면을 지우고 새로운 검색 결과를 출력
        system("clear");
        printf("Search Word: %s\n", userSearchTerm);
		 // 뮤텍스 해제
        pthread_mutex_unlock(&lock);
        printf("-----------------------------\n");
        printWithHighlight(result, userSearchTerm);
    }
    return NULL;
}


int getch()
{
	int c = 0;
	struct termios oldattr, newattr;

	tcgetattr(STDIN_FILENO, &oldattr);
	newattr = oldattr;
	newattr.c_lflag &= ~(ICANON | ECHO);
	newattr.c_cc[VMIN] = 1;
	newattr.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, TCSANOW, &newattr);
	c = getchar();
	tcsetattr(STDIN_FILENO, TCSANOW, &oldattr);

	return c;
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
        system("clear");
		printf("NOT FOUND: %s\n", searchTerm);

    }
}
void printWithHighlight(const char *text, const char *searchTerm) {
    const char *p = text;
    while (*p) {
        const char *match = strstr(p, searchTerm);
        if (match) {
            while (p < match) {
                putchar(*p++);
            }
            printf("\033[1;31m");  // 시작 ANSI 코드 (빨간색으로 강조)
            while (*p && p < match + strlen(searchTerm)) {
                putchar(*p++);
            }
            printf("\033[0m");  // 끝 ANSI 코드 (색상 초기화)
        } else {
            printf("%s", p);
            break;
        }
    }
}
