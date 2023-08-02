#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 1000
#define NAME_SIZE 256
#define PAKETSIZE 264
struct file_info {
    char name[NAME_SIZE];
    long size;
};
typedef struct 
{
    char content[BUF_SIZE];
    int read_size;
}pkt;
void error_handling(char *message);

int main(int argc, char *argv[]) {
    int sd;
    FILE *fp;
    char temp[PAKETSIZE];
    char buf[BUF_SIZE];
    int read_cnt;
    struct sockaddr_in serv_adr;
    struct file_info file_inf;
    int recv_cnt,recv_len;
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
        while(1){
        recv_len=0;
        while (recv_len<PAKETSIZE) {
            recv_cnt=read(sd, &temp[recv_len], PAKETSIZE - recv_len); //파일 목록 수신
            //printf("%d\n",recv_cnt); 여기부분
            if(recv_cnt ==-1)
                error_handling("read() error!");
            recv_len+=recv_cnt;
            
        }
        temp[recv_len]=0;
        memcpy(&file_inf,temp,PAKETSIZE);
        printf("File: %s (%ld bytes)\n", file_inf.name, file_inf.size);
        if (file_inf.size == 0) break; //파일 정보 크기 0이면 수신 종료
        }

        printf("Enter the name of the file you want to download (or 'quit' to exit): ");
        fgets(file_inf.name, NAME_SIZE, stdin); //입력하기
        
        //int str_len=0;
        file_inf.name[strcspn(file_inf.name, "\n")] = 0; // 엔터 문자열 제거 (널문자 대입) --> 파일의 정확한 이름을 알려주기 위해 가장 중요
        //str_len=strlen(file_inf.name);
        //printf("string's length that you input : %d",str_len);
        
        // quit입력하면 프로그램 종료
        
        if(strcmp(file_inf.name, "quit") == 0){
            write(sd, file_inf.name, NAME_SIZE); 
            break;
        }

        // 서버에 내가 입력한 파일이름 전송
        write(sd, file_inf.name, NAME_SIZE);
        //printf("data that is server's transporting : %ld ", write(sd, file_inf.name, NAME_SIZE)); //256 바이트 전송

        // 서버로부터 파일 내용받아 파일로 저장
       recv_len=0;
        while (recv_len<PAKETSIZE) {
            recv_cnt=read(sd, &temp[recv_len], PAKETSIZE - recv_len); //파일 목록 수신
            //printf("%d\n",recv_cnt); 여기부분
            if(recv_cnt ==-1)
                error_handling("read() error!");
            recv_len+=recv_cnt;
            
        }
        temp[recv_len]=0;
        memcpy(&file_inf,temp,PAKETSIZE);
      // printf("File: %s (%ld bytes)\n", file_inf.name, file_inf.size);


        fp = fopen(file_inf.name, "wb");//텍스트와 바이너리로 열기
        int read_file_size;
        int total_bytes = 0;
        
        pkt *packet = malloc(sizeof(pkt));
         while (1) {
            memset(packet,0,sizeof(pkt));
            
            
            //read(sd,packet,sizeof(pkt));
        
        recv_len=0;
        while (recv_len<sizeof(pkt)) {
            recv_cnt=read(sd, &temp[recv_len], sizeof(pkt) - recv_len); //파일 목록 수신

            //printf("%d\n",recv_cnt); 여기부분
            if(recv_cnt ==-1)
                error_handling("read() error!");
            recv_len+=recv_cnt;
            //printf("%d %d\n",recv_len, recv_cnt);
        }
        temp[recv_len]=0;
        memcpy(packet,temp,sizeof(pkt));
         


           
            
            if (total_bytes >= file_inf.size) {
                 read_cnt = fwrite(packet->content, 1, packet->read_size, fp);     
                break;
            }
            read_cnt = fwrite(packet->content, 1, packet->read_size, fp);
            total_bytes += packet->read_size;
            printf("%d / %ld\n", total_bytes, file_inf.size);
        }
        fclose(fp); //0값 받으면 다운로드 완료됬다고 표현
        printf("Download complete\n");
        exit(1);
    }
    close(sd);
    return 0;
}

void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
