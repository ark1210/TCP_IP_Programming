#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <dirent.h>
#include <sys/stat.h>

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
} pkt;
void error_handling(char *buf);

int main(int argc, char *argv[])
{
	int sd;
	FILE *fp;
    char temp[PAKETSIZE];
	char buf[BUF_SIZE];
    char temp_buf[BUF_SIZE];
	int read_cnt;
    struct stat file_stat; 

	//int str_len;
	struct sockaddr_in serv_adr;
	struct file_info file_inf;
    int recv_cnt,recv_len;

	if(argc!=3) {
		printf("Usage : %s <IP> <port>\n", argv[0]);
		exit(1);
	}
	
	sd=socket(PF_INET, SOCK_STREAM, 0);   
	if(sd==-1)
		error_handling("socket() error");
	
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family=AF_INET;
	serv_adr.sin_addr.s_addr=inet_addr(argv[1]);
	serv_adr.sin_port=htons(atoi(argv[2]));
	
	if(connect(sd, (struct sockaddr*)&serv_adr, sizeof(serv_adr))==-1)
		error_handling("connect() error!");
	else
		puts("Connected...........");

		fputs("do you want to see the server's directory?(Q to quit || yes || no): ", stdout);
		fgets(temp_buf, BUF_SIZE, stdin);
		
		if(!strcmp(temp_buf,"q\n") || !strcmp(temp_buf,"Q\n") ||!strcmp(temp_buf,"no\n"))
		exit(1);

		write(sd, temp_buf, strlen(temp_buf));
while(1){    
	while(1) 
	{
		recv_len=0;
        while (recv_len<PAKETSIZE) {
            recv_cnt=read(sd, &temp[recv_len], PAKETSIZE - recv_len); 
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
		
		printf("Enter the name of the file you want to download (or 'quit' to exit), 'cd [dir]' to change dir, 'upload [file]' to upload): ");
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
        // cd [dir]입력하면 디렉토리 변경
        if(strncmp(file_inf.name, "cd ", 3) == 0){
            write(sd, file_inf.name, NAME_SIZE);
            continue;
        }

        // upload [file]입력하면 파일 업로드
        if(strncmp(file_inf.name, "upload ", 7) == 0)
        {
           //write(sd, "upload", NAME_SIZE);
            write(sd, file_inf.name, NAME_SIZE);
            int offset = 7; // 7바이트를 제거 (upload 제거)
            int len = strlen(file_inf.name); 

            if (len > offset) {
                memmove(file_inf.name, file_inf.name + offset, len - offset + 1); // +1은 널 문자를 포함하기 위함입니다.
            }
            stat(file_inf.name, &file_stat);
            file_inf.size = file_stat.st_size;

            printf("size : %ld",file_inf.size);

            write(sd, &file_inf, sizeof(file_inf)); //해당 이름과 사이즈 패킷을 먼저 보냄

            //해당 파일 서버에게 전송
            fp = fopen(file_inf.name, "rb");
           pkt *packet = malloc(sizeof(pkt));
        while (1)
        {
            memset(packet, 0, sizeof(pkt));
            read_cnt = fread(packet->content, 1, BUF_SIZE, fp);
            packet->read_size = read_cnt;
             printf("\nread_size : %d", packet->read_size);
            if (read_cnt < BUF_SIZE)
            {
                write(sd, packet, sizeof(pkt));
                break;
            }
            write(sd, packet, sizeof(pkt));
        }
        // 오류 방지 버퍼 지우기 위해 0으로 memset하기
        // memset(buf, 0, BUF_SIZE);
        // write(clnt_sd, buf, BUF_SIZE);  // 0값 전송
        free(packet);
        fclose(fp);
            continue;
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

		fp = fopen(file_inf.name, "wb");//텍스트와 바이너리로 열기
        int read_file_size;
        int total_bytes = 0;
        
        pkt *packet = malloc(sizeof(pkt));

         while (1) 
        {
            memset(packet,0,sizeof(pkt));
            
            
            //read(sd,packet,sizeof(pkt));
        
            recv_len=0;
            while (recv_len<sizeof(pkt)) 
            {
                recv_cnt=read(sd, &temp[recv_len], sizeof(pkt) - recv_len); //파일 목록 수신

                //printf("%d\n",recv_cnt); 여기부분
                if(recv_cnt ==-1)
                    error_handling("read() error!");
                recv_len+=recv_cnt;
                //printf("%d %d\n",recv_len, recv_cnt);
            }
            temp[recv_len]=0;
            memcpy(packet,temp,sizeof(pkt));
         


           
            
            // if (total_bytes >= file_inf.size) {
            //      read_cnt = fwrite(packet->content, 1, packet->read_size, fp);     
            //     break;
            // }
            read_cnt = fwrite(packet->content, 1, packet->read_size, fp);
            total_bytes += packet->read_size;
            printf("%d / %ld\n", total_bytes, file_inf.size);
             if (total_bytes >= file_inf.size) {
                 read_cnt = fwrite(packet->content, 1, packet->read_size, fp);     
                break;
            }
        }
        fclose(fp); //0값 받으면 다운로드 완료됬다고 표현
        printf("downloaded complete\n");
	
		
		
		// str_len=read(sd, buf, BUF_SIZE-1);
		// buf[str_len]=0;
		// printf("buf from server: %s", buf);
	
	}
	
	close(sd);
	return 0;
}

void error_handling(char *buf)
{
	fputs(buf, stderr);
	fputc('\n', stderr);
	exit(1);
}
