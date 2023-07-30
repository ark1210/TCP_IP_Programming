#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 1000
#define NAME_SIZE 256

struct file_info {
    char name[NAME_SIZE];
    long size;
};

void error_handling(char *message);

int main(int argc, char *argv[]) {
    int sd;
    FILE *fp;
    
    char buf[BUF_SIZE];
    int read_cnt;
    struct sockaddr_in serv_adr;
    struct file_info file_inf;

    if(argc!=3) {
        printf("Usage: %s <IP> <port>\n", argv[0]);
        exit(1);
    }
    
    sd=socket(PF_INET, SOCK_STREAM, 0);   

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family=AF_INET;
    serv_adr.sin_addr.s_addr=inet_addr(argv[1]);
    serv_adr.sin_port=htons(atoi(argv[2]));

    connect(sd, (struct sockaddr*)&serv_adr, sizeof(serv_adr));

    while (1) {
        while (1) {
            read(sd, &file_inf, sizeof(file_inf)); //파일 목록 수신
            if (file_inf.size == 0) break; //파일 정보 크기 0이면 수신 종료
            printf("File: %s (%ld bytes)\n", file_inf.name, file_inf.size);
        }

        printf("Enter the name of the file you want to download (or 'quit' to exit): ");
        fgets(file_inf.name, NAME_SIZE, stdin); //입력하기
        file_inf.name[strcspn(file_inf.name, "\n")] = 0; // 엔터 문자열 제거 (널문자 대입) --> 파일의 정확한 이름을 알려주기 위해 가장 중요
        // quit입력하면 프로그램 종료
        if(strcmp(file_inf.name, "quit") == 0){
            write(sd, file_inf.name, NAME_SIZE);
            break;
        }

        // 서버에 내가 입력한 파일이름 전송
        write(sd, file_inf.name, NAME_SIZE);

        // 서버로부터 파일 내용받아 파일로 저장
        fp = fopen(file_inf.name, "wb");//텍스트와 바이너리로 열기
        while((read_cnt=read(sd, buf, BUF_SIZE)) != 0) {
            
            if (read_cnt < BUF_SIZE && buf[read_cnt - 1] == '\0') {
                fwrite((void*)buf, 1, read_cnt - 1, fp);  // Do not write the last NULL character
                break;
            }
            fwrite((void*)buf, 1, read_cnt, fp); // 서버로 부터 받은 데이터(buf)를 파일에 쓰기
        }
        fclose(fp); //0값 받으면 다운로드 완료됬다고 표현
        printf("Download complete\n");
    }
    close(sd);
    return 0;
}

void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
