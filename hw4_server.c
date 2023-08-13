#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <ctype.h>

#define MAX_LINES 256
#define BUF_SIZE 100
#define MAX_CLNT 256
typedef struct packet
{
    int count;
    char buf[BUF_SIZE];
    int line_num;
} pkt;

void *handle_clnt(void *arg);
void send_msg(char *msg, int len);
void error_handling(char *msg);
void printPackets();
void searchPacket(const char *searchTerm, int sock);
int clnt_cnt = 0;
int clnt_socks[MAX_CLNT];
pthread_mutex_t mutx;
pkt packets[MAX_LINES];

int main(int argc, char *argv[])
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    int clnt_adr_sz;
    pthread_t t_id;

    if (argc != 2)
    {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    pthread_mutex_init(&mutx, NULL);
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");
    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    FILE *file = fopen("data.txt", "r");
    // if (!file)
    // {
    //     error_handling("Failed to open data.txt");
    //     return;
    // }

    char line[BUF_SIZE];
    int line_num = 0;                               // 현재 읽고 있는 줄 번호 0으로 초기값 설정, 각 줄이 pkt 배열의 요소들과 일치
    while (fgets(line, sizeof(line), file) != NULL) // 파일에서 한 줄씩 읽기(파일 끝 도달하면 반복 종료)
    {
        line[strcspn(line, "\n")] = 0; // 개행문자 제거

        char *last_space = strrchr(line, ' ');        // 줄에서 마지막 spacebar 문자의 위치를 찾기
        if (last_space && isdigit(*(last_space + 1))) // 만약 마지막 문자에 공백 문자가 있고, 그 뒤에 숫자가 오면 조건문 실행
        {
            // 숫자 pkt에 넣어버리기
            packets[line_num].count = atoi(last_space + 1);

            // 해당 공백 부분에 널문자 넣기
            *last_space = '\0';
            strncpy(packets[line_num].buf, line, BUF_SIZE - 1);
            packets[line_num].buf[BUF_SIZE - 1] = '\0';
        }
        else
        {
            // 만약 숫자 없으면 걍 라인 자체를 buf에 넣어버리기
            strncpy(packets[line_num].buf, line, BUF_SIZE - 1);
            packets[line_num].buf[BUF_SIZE - 1] = '\0';
            packets[line_num].count = 0; // count 조회수는 0으로 설정
        }
        packets[line_num].line_num = line_num;
        line_num++;
    }
    fclose(file);
    printPackets();

    char userSearchTerm[BUF_SIZE];

    // 데이터 로딩 후 패킷 검색
    // searchPacket(userSearchTerm);

    while (1)
    {
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_sz);

        pthread_mutex_lock(&mutx);
        clnt_socks[clnt_cnt++] = clnt_sock;
        pthread_mutex_unlock(&mutx);

        pthread_create(&t_id, NULL, handle_clnt, (void *)&clnt_sock);
        pthread_detach(t_id);
        printf("Connected client IP: %s \n", inet_ntoa(clnt_adr.sin_addr));
    }
    close(serv_sock);
    return 0;
}

void *handle_clnt(void *arg)
{
    int clnt_sock = *((int *)arg);
    int str_len = 0, i;

    char userSearchTerm[BUF_SIZE];

    while ((str_len = read(clnt_sock, userSearchTerm, sizeof(userSearchTerm) - 1)) != 0)
    {
        userSearchTerm[str_len] = '\0'; // 문자열 종료

        // 백스페이스 처리
        for (int i = 0; i < str_len; i++)
        {
            if (userSearchTerm[i] == 8 && i > 0)
            { // ASCII 8 is backspace
                // 문자열에서 마지막 문자 제거
                for (int j = i - 1; j < str_len - 1; j++)
                {
                    userSearchTerm[j] = userSearchTerm[j + 1];
                }
                userSearchTerm[str_len - 1] = '\0';
                i -= 2; // 백스페이스 문자와 이전 문자 모두 제거
            }
        }

        // printf("Received from client: %s\n", userSearchTerm);
        searchPacket(userSearchTerm, clnt_sock);
    }

    pthread_mutex_lock(&mutx);
    for (i = 0; i < clnt_cnt; i++) // remove disconnected client
    {
        if (clnt_sock == clnt_socks[i])
        {
            while (i++ < clnt_cnt - 1)
                clnt_socks[i] = clnt_socks[i + 1];
            break;
        }
    }
    clnt_cnt--;
    pthread_mutex_unlock(&mutx);
    close(clnt_sock);
    return NULL;
}
void send_msg(char *msg, int len) // send to all
{
    int i;
    pthread_mutex_lock(&mutx);
    for (i = 0; i < clnt_cnt; i++)
        write(clnt_socks[i], msg, len);
    pthread_mutex_unlock(&mutx);
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

// void searchPacket(const char *searchTerm, int sock)
// {
//     int found = 0;
//     char result[MAX_LINES * (BUF_SIZE + 20)]; // Enough space for results
//     result[0] = '\0';                         // Initialize result string

//     // Storing lines that match the search term
//     pkt foundPackets[MAX_LINES];
//     int foundCount = 0;
//     for (int i = 0; i < MAX_LINES; i++)
//     {
//         if (strstr(packets[i].buf, searchTerm))
//         {
//             foundPackets[foundCount++] = packets[i];
//             found = 1;
//         }
//     }

//     // Sorting by count in descending order
//     for (int i = 0; i < foundCount - 1; i++)
//     {
//         for (int j = 0; j < foundCount - i - 1; j++)
//         {
//             if (foundPackets[j].count < foundPackets[j + 1].count)
//             {
//                 pkt temp = foundPackets[j];
//                 foundPackets[j] = foundPackets[j + 1];
//                 foundPackets[j + 1] = temp;
//             }
//         }
//     }

//     // Adding sorted results to the result string
//     for (int i = 0; i < foundCount; i++)
//     {
//         // strcat(result, "FOUND in Line ");
//         char line_num_str[10];
//         sprintf(line_num_str, "%d: ", foundPackets[i].line_num);
//         strcat(result, line_num_str);
//         strcat(result, foundPackets[i].buf);
//         strcat(result, "\n");
//     }

//     if (!found)
//     {
//         strcpy(result, "NOT FOUND\n");
//     }

//     // Send the result to the client
//     write(sock, result, strlen(result));
// }
void searchPacket(const char *searchTerm, int sock) // 첫번째 인자는 검색할 문자열, 다른 하나는 소켓 파일 디스크립터
{
    int found = 0;                            // 문자열 검색이 발견되면 1로 설정하기 위함.
    char result[MAX_LINES * (BUF_SIZE + 20)]; // result 변수로 검색결과 저장함.
    result[0] = '\0';

    // Storing lines that match the search term
    pkt foundPackets[MAX_LINES];        // 일치하는 패킷 임시 저장
    int foundCount = 0;                 // 발견된 패킷의 수 저장
    for (int i = 0; i < MAX_LINES; i++) // 배열 요소 반복하면서 각 패킷내 문자열 찾기
    {
        if (strstr(packets[i].buf, searchTerm))
        {
            foundPackets[foundCount++] = packets[i];
            found = 1;
        }
    }

    // 버블 sorting  내림차순(count는 조회수)
    for (int i = 0; i < foundCount - 1; i++)
    {
        for (int j = 0; j < foundCount - i - 1; j++)
        {
            if (foundPackets[j].count < foundPackets[j + 1].count)
            {
                pkt temp = foundPackets[j];
                foundPackets[j] = foundPackets[j + 1];
                foundPackets[j + 1] = temp;
            }
        }
    }

    // 서버를 통해 정렬된 foundpacket 배열을 그대로 result 변수에 넣어버리기
    for (int i = 0; i < foundCount; i++)
    {
        // strcat(result, "FOUND in Line ");
        char line_num_str[10];
        sprintf(line_num_str, "%d: ", foundPackets[i].line_num);
        strcat(result, line_num_str);
        strcat(result, foundPackets[i].buf);
        strcat(result, "\n");
    }

    if (!found)
    {
        strcpy(result, "NOT FOUND\n");
    }
    printf("Sending result to client:\n%s\n", result);
    // Send the result to the client
    write(sock, result, strlen(result)); // result를 자체로 클라이언트에게 전송
}
