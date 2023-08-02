#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <dirent.h>
#include <sys/stat.h>

#define BUF_SIZE 1000
#define NAME_SIZE 256
#define PAKETSIZE 264
struct file_info { //패킷 만들기 //크기 260
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
    int serv_sd, clnt_sd;
    FILE *fp;
    char buf[BUF_SIZE];
    int read_cnt;

    struct sockaddr_in serv_adr, clnt_adr;
    socklen_t clnt_adr_sz;

    struct dirent *dir;
    DIR *d;
    struct stat file_stat; //파일의 상태 및 정보를 저장할 구조체(파일의 크기, 종류 심볼릭 등등)
    struct file_info file_inf;

    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    serv_sd = socket(PF_INET, SOCK_STREAM, 0);

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    bind(serv_sd, (struct sockaddr *) &serv_adr, sizeof(serv_adr));
    listen(serv_sd, 5);

    clnt_adr_sz = sizeof(clnt_adr);
    clnt_sd = accept(serv_sd, (struct sockaddr *) &clnt_adr, &clnt_adr_sz);
/*
struct dirent
{
	long		    d_ino;	inode 번호	
	unsigned short	d_reclen;	
	unsigned short	d_namlen;	. 
	char		    d_name[260];  파일 이름 
}; 
*/ 
/*
DIR *d = opendir("."); // 현재 디렉토리를 열기
if (d) {
    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) { // 디렉토리의 각 항목을 읽기, 반환값 dirent 구조체 포인터
    이 때, 각 파일에 대한 정보는 struct dirent 구조체에 저장됨.
        printf("%s\n", dir->d_name); // 항목의 이름을 출력하기
        }
    closedir(d); // 디렉토리를 닫기
}
*/
/*
    struct stat buf; //stat 구조체 
    stat("test.c", &buf); //stat함수 : 첫번째 인자 path/파일명,  stat 구조체 두번째 인자로 주솟값 전송, 해당 첫번째 인자로 들어온 파일 정보를 두번째 인자로 전달된 stat 구조체에 정보 할당. 

    printf("Inode = %d\n", (int)buf.st_ino);
    printf("Size = %d\n", (int)buf.st_size);

    --> 실행결과: Inode = 15963 Size = 675
*/
    while(1){
        //  파일 목록 보내기
        d = opendir("."); //현재 디렉토리 열기
        if (d) {
            while ((dir = readdir(d)) != NULL) {
                stat(dir->d_name, &file_stat); //파일 stat에 파일 정보들 저장시키고 (stat 함수는 구조체 stat을  반환하는데, 두번째 인자로 전달한 stat 구조체에 파일정보들을 반환(할당)함. 그 구조체에 멤버로
                //파일을 관리하기 위한 정보를 담고 있는 inode(파일 권한), off_t st_size (해당 파일 크기)라는 녀석이 있고
                // 그 inode 정보를 읽을 때 STAT 함수를 통해서 읽을 수 있으며 이 함수를 사용해 읽어오면 반환값으로 두번째 인자로 전달한 주솟값(해당 구조체),
                //구조체인 STAT이라는 스트럭처(위의 inode, st_size가 멤버를 갖고있는 구조체를 두번째인자에 줌으로서 거기에 자동 할당해줌) 에 자동할당해줌
                
                strncpy(file_inf.name, dir->d_name, NAME_SIZE); //파일 인포 구조체에 각 파일명들 복사
                file_inf.size = file_stat.st_size;//파일 인포 구조체 사이즈에 파일 stat 정보 크기 저장
                
                printf("%ld\n",write(clnt_sd, &file_inf, sizeof(file_inf))); //클라이언트에게 전송(파일 인포)
                //패킷을 만들고 server에서 write(서버소켓의 새로운 소켓, struct 패킷의 주소, 패킷의 사이즈)를 해버리면
                //client에서 받을때 client에도 패킷을 정의한후 read(내 소켓, client 패킷정의한 구조체 주소, 패킷의 사이즈)하면
                //패킷을 정의한 구조체 즉, server에서 write한 구조체의 내용들이 client 의 패킷의 구조체 내용들에 다 담겨지게 된다. (자동화가 이루어짐)

            }
            // 파일의 목록 끝 표시
             memset(&file_inf, 0, sizeof(file_inf)); //빈파일 구조체 전송(0으로 설정, 파일 목록 끝 표시를 위해)
             write(clnt_sd, &file_inf, sizeof(file_inf));
             closedir(d);
        }

        // 클라이언트로부터 파일 이름 받기
        int str_len=256;
        int recv_len=0;
        int recv_cnt;
        while(recv_len<str_len)
        {
            recv_cnt=read(clnt_sd, &file_inf.name[recv_len], NAME_SIZE-1);
            //printf("%d\n",recv_cnt);
            if(recv_cnt==-1)
            error_handling("read() error!");
            recv_len+=recv_cnt;
        }
        file_inf.name[recv_len]=0;
        

        // 클라이언트가 quit하면 break
        if(strcmp(file_inf.name, "quit") == 0){
            break;
        }

        stat(file_inf.name, &file_stat);
        file_inf.size = file_stat.st_size;

       // printf("size : %ld",file_inf.size);
       write(clnt_sd, &file_inf, sizeof(file_inf));
        // 해당 파일 클라이언트에게 전송
        fp = fopen(file_inf.name, "rb");
        pkt *packet = malloc(sizeof(pkt));
         while (1) {
            memset(packet,0,sizeof(pkt));
            read_cnt = fread(packet->content, 1, BUF_SIZE, fp);
            packet->read_size=read_cnt;
           // printf("read_size : %d", packet->read_size);
            if (read_cnt < BUF_SIZE) {
                write(clnt_sd, packet, sizeof(pkt));
                break;
            }
            write(clnt_sd, packet, sizeof(pkt));
        }
        // 오류 방지 버퍼 지우기 위해 0으로 memset하기
       // memset(buf, 0, BUF_SIZE);
        //write(clnt_sd, buf, BUF_SIZE);  // 0값 전송
        
        fclose(fp);
    }

    close(clnt_sd);
    close(serv_sd);
    return 0;
}

void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
