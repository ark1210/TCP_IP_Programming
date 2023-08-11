#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
//hw2(uecho)clinet.c =reciever입니다. 
//reciever는 일단 sender 폴더에 있는 원하는 파일 이름 전송
#define BUF_SIZE 1024

typedef struct
{
    char data[BUF_SIZE];
    int seq;
    int data_size; // 실제 데이터 크기를 추적.
} pkt;

//클라이언트와 서버 양쪽에서 사용하는 구조체 packet 의 정의가 완전히 일치해야함
//즉, 구조체의 필드순서, 크기 , 정렬 등이 정확히 같아야 데이터가 올바르게 전송
//sizeof(pkt)로 보내기 때문에 pedding byte까지 생각하기 

void error_handling(char *message);
//reciever는 optval 필요없음
int main(int argc, char *argv[]) {
    //struct timeval optVal = {0, 6};
    int sock;
     //int optLen = sizeof(optVal);
    FILE *fp;
    struct sockaddr_in serv_adr;
    pkt Pkt;
    pkt Ack;
    int read_cnt;

    if (argc != 3) {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock == -1) error_handling("socket() error");
    //setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &optVal, optLen);
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_adr.sin_port = htons(atoi(argv[2]));

    memset(&Pkt, 0, sizeof(Pkt)); // 패킷을 0으로 초기화

    printf("Enter the name of the file that is in server directory Then you can download: ");
    fgets(Pkt.data, sizeof(Pkt.data), stdin);
    Pkt.data[strcspn(Pkt.data, "\n")] = 0; // 줄바꿈 문자 제거
    printf("Sending file name: %s\n", Pkt.data); // 여기에 로그 추가


    int file_name_sent = 0;
    while (!file_name_sent) //클라이언트에서는 파일 이름을 보낸 후 서버로부터 동일한 내용의 확인 응답을 받을 때까지 계속 시도
    {
        sendto(sock, &Pkt, sizeof(pkt), 0, (struct sockaddr *)&serv_adr, sizeof(serv_adr));
        if (recvfrom(sock, &Ack, sizeof(pkt), 0, NULL, NULL) != -1) {
            if (strcmp(Ack.data, Pkt.data) == 0) { // 받은 응답의 내용이 보낸 파일 이름과 동일한지 확인
                file_name_sent = 1;
            }
        }
    }


    fp = fopen(Pkt.data, "wb");
    if (fp == NULL) error_handling("fopen() error!");

    int seq_num = 0;
    while(1){
        read_cnt = recvfrom(sock, &Pkt, sizeof(pkt), 0, NULL, NULL);
		//Pkt.data[read_cnt] = 0;
        if(read_cnt == -1) continue;
        if(Pkt.seq == -1)
            break;
        
        if (Pkt.seq == seq_num) {
            fwrite((void *)Pkt.data, 1, Pkt.data_size, fp); //data_size 필드를 사용해 적절한 바이트 수만큼 쓰기
            seq_num++;
             printf("%d\n", Pkt.seq);
        }

        sendto(sock, &Pkt, sizeof(pkt), 0, (struct sockaddr *)&serv_adr, sizeof(serv_adr));
       // printf("%d\n", Pkt.seq);
    }

    puts("Received file data"); //reciever
    fclose(fp);
    close(sock);
    return 0;

}

void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

