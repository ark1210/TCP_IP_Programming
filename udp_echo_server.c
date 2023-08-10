#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#define BUF_SIZE 1024
// hw2(uecho)server.c =sender입니다.
// sender에 있는 파일 하나를 제공
// 그 전에 리시버가 먼저 파일 이름을 전송해줘야함. 전송된 것(file name)을 받아서 파일 제공
typedef struct
{
    char data[BUF_SIZE];
    int seq;
    int data_size; // 실제 데이터 크기를 추적.
} pkt;

// 클라이언트와 서버 양쪽에서 사용하는 구조체 packet 의 정의가 완전히 일치해야함
// 즉, 구조체의 필드순서, 크기 , 정렬 등이 정확히 같아야 데이터가 올바르게 전송
// sizeof(pkt)로 보내기 때문에 pedding byte까지 생각하기

void error_handling(char *message);

int main(int argc, char *argv[])
{
    int serv_sock;
    FILE *fp;
    struct sockaddr_in serv_adr, clnt_adr;
    pkt Pkt, Ack;
    int read_cnt;
    socklen_t clnt_adr_sz;
    struct timeval optVal = {0, 1000}; // 0, 1000 하면 loss 발견가능
    int optLen = sizeof(optVal);
    clock_t start, end;
    double time_taken;
    long total_bytes = 0;

    if (argc != 2)
    {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    serv_sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (serv_sock == -1)
        error_handling("UDP socket creation error");

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");

    memset(&Pkt, 0, sizeof(Pkt)); // 패킷을 0으로 초기화
    clnt_adr_sz = sizeof(clnt_adr);
    recvfrom(serv_sock, &Pkt, sizeof(pkt), 0, (struct sockaddr *)&clnt_adr, &clnt_adr_sz);

    setsockopt(serv_sock, SOL_SOCKET, SO_RCVTIMEO, &optVal, optLen);
    printf("Received file name: %s\n", Pkt.data);

    fp = fopen(Pkt.data, "rb");
    if (fp == NULL)
        error_handling("File open error");

    int seq_num = 0;
    while (!feof(fp))
    {
        // 데이터
        read_cnt = fread((void *)Pkt.data, 1, BUF_SIZE - 1, fp);
        // Pkt.data[read_cnt] = 0;
        Pkt.data_size = read_cnt; // 여기에 실제 읽은 바이트 수를 저장합니다.
        // Sequence

        Pkt.seq = seq_num;

        while (1)
        {
            // 전송
            sendto(serv_sock, &Pkt, sizeof(pkt), 0, (struct sockaddr *)&clnt_adr, clnt_adr_sz);

            if (recvfrom(serv_sock, &Ack, sizeof(pkt), 0, (struct sockaddr *)&clnt_adr, &clnt_adr_sz) == -1)
                continue;

            if (Ack.seq == 0)
                start = clock();

            if (Ack.seq == seq_num)
            {
                printf("[%d] : Succeed\n", Ack.seq);
                break;
            }
            printf("[%d] : failed\n", seq_num); // loss 발생 나타내기
        }
        total_bytes += read_cnt;
        seq_num++;
    }
    Pkt.seq = -1;
    sendto(serv_sock, &Pkt, sizeof(pkt), 0, (struct sockaddr *)&clnt_adr, clnt_adr_sz);

    end = clock();
    time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
    double throughput = total_bytes / time_taken;
    printf("File transfer completed. Throughput: %.2lf bytes/sec\n", throughput);

    fclose(fp);
    close(serv_sock);
    return 0;
}

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
