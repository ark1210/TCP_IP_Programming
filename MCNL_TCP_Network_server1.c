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
/*
struct dirent
{
	long		    d_ino;		
	unsigned short	d_reclen;	
	unsigned short	d_namlen;	Length of name in d_name. 
	char		    d_name[260];  [FILENAME_MAX] */ /* File name. 
}; 
*/
struct file_info {
    char name[NAME_SIZE];
    long size;
};

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
DIR *d = opendir("."); // 현재 디렉토리를 열기
if (d) {
    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) { // 디렉토리의 각 항목을 읽기
    이 때, 각 파일에 대한 정보는 struct dirent *dir에 저장됨.
        printf("%s\n", dir->d_name); // 항목의 이름을 출력하기
    }
    closedir(d); // 디렉토리를 닫기
}
*/
    while(1){
        //  파일 목록 보내기
        d = opendir("."); //현재 디렉토리 열기
        if (d) {
            while ((dir = readdir(d)) != NULL) {
                stat(dir->d_name, &file_stat); //파일 stat에 파일 정보들 저장시키고
                strncpy(file_inf.name, dir->d_name, NAME_SIZE); //파일 인포 구조체에 각 파일명들 복사
                file_inf.size = file_stat.st_size;//파일 인포 구조체 사이즈에 파일 stat 정보 크기 저장
                write(clnt_sd, &file_inf, sizeof(file_inf)); //클라이언트에게 전송(파일 인포)
            }
            // 파일의 목록 끝 표시
            memset(&file_inf, 0, sizeof(file_inf)); //빈파일 구조체 전송(0으로 설정, 파일 목록 끝 표시를 위해)
            write(clnt_sd, &file_inf, sizeof(file_inf));
        }

        // 클라이언트로부터 파일 이름 받기
        read(clnt_sd, file_inf.name, NAME_SIZE);

        // 클라이언트가 quit하면 break
        if(strcmp(file_inf.name, "quit") == 0){
            break;
        }

        // 해당 파일 클라이언트에게 전송
        fp = fopen(file_inf.name, "rb");
        while (1) {
            read_cnt = fread((void *) buf, 1, BUF_SIZE, fp);
            if (read_cnt < BUF_SIZE) {
                write(clnt_sd, buf, read_cnt);
                break;
            }
            write(clnt_sd, buf, BUF_SIZE);
        }
        // 오류 방지 버퍼 지우기 위해 0으로 memset하기
        memset(buf, 0, BUF_SIZE);
        write(clnt_sd, buf, BUF_SIZE);  // 0값 전송
        
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
