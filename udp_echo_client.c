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

typedef struct {
    char data[BUF_SIZE];
    int seq;
}pkt;

void error_handling(char *message);
//reciever는 optval 필요없음
int main(int argc, char *argv[]) {
    //struct timeval optVal = {0, 6};
    int sock;
     //int optLen = sizeof(optVal);
    FILE *fp;
    struct sockaddr_in serv_adr;
    pkt Pkt;
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

    printf("Enter the name of the file that is in server direcrory Then you can download: ");
    scanf("%s", Pkt.data);


    sendto(sock, &Pkt, sizeof(pkt), 0, (struct sockaddr *)&serv_adr, sizeof(serv_adr));

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
            fwrite((void *)Pkt.data, 1, BUF_SIZE - 1, fp);
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

