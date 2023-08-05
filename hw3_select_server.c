#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <dirent.h>
#include <sys/stat.h>

#define BUF_SIZE 1000
#define NAME_SIZE 256
#define PAKETSIZE 264
struct file_info
{ // 패킷 만들기 //크기 260
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
	int serv_sd, clnt_sd;
	struct sockaddr_in serv_adr, clnt_adr;
	struct timeval timeout;
	fd_set reads, cpy_reads;
	char temp_buf[BUF_SIZE];
	socklen_t adr_sz;
	int fd_max, str_len, fd_num, i;
	FILE *fp;
	char buf[BUF_SIZE];
	int read_cnt;

	// struct sockaddr_in serv_adr, clnt_adr;

	struct dirent *dir;
	DIR *d;
	struct stat file_stat; // 파일의 상태 및 정보를 저장할 구조체(파일의 크기, 종류 심볼릭 등등)
	struct file_info file_inf;

	if (argc != 2)
	{
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}

	serv_sd = socket(PF_INET, SOCK_STREAM, 0);

	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_adr.sin_port = htons(atoi(argv[1]));

	if (bind(serv_sd, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
		error_handling("bind() error");
	if (listen(serv_sd, 5) == -1)
		error_handling("listen() error");

	FD_ZERO(&reads);
	FD_SET(serv_sd, &reads);
	fd_max = serv_sd;

	while (1)
	{
		cpy_reads = reads;
		timeout.tv_sec = 5;
		timeout.tv_usec = 5000;

		if ((fd_num = select(fd_max + 1, &cpy_reads, 0, 0, &timeout)) == -1)
			break;

		if (fd_num == 0)
			continue;

		for (i = 0; i < fd_max + 1; i++)
		{
			if (FD_ISSET(i, &cpy_reads))
			{
				if (i == serv_sd) // connection request!
				{
					adr_sz = sizeof(clnt_adr);
					clnt_sd =
						accept(serv_sd, (struct sockaddr *)&clnt_adr, &adr_sz);
					FD_SET(clnt_sd, &reads);
					if (fd_max < clnt_sd)
						fd_max = clnt_sd;
					printf("connected client: %d \n", clnt_sd);
				}
				else // read message!
				{

					str_len = read(i, temp_buf, BUF_SIZE);
					if (str_len == 0) // close request!
					{
						FD_CLR(i, &reads);
						close(i);
						printf("closed client: %d \n", i);
					}
					else
					{
						//  파일 목록 보내기
						d = opendir("."); // 현재 디렉토리 열기
						if (d)
						{
							while ((dir = readdir(d)) != NULL)
							{
								stat(dir->d_name, &file_stat); // 파일 stat에 파일 정보들 저장시키고 (stat 함수는 구조체 stat을  반환하는데, 두번째 인자로 전달한 stat 구조체에 파일정보들을 반환(할당)함. 그 구조체에 멤버로
								// 파일을 관리하기 위한 정보를 담고 있는 inode(파일 권한), off_t st_size (해당 파일 크기)라는 녀석이 있고
								//  그 inode 정보를 읽을 때 STAT 함수를 통해서 읽을 수 있으며 이 함수를 사용해 읽어오면 반환값으로 두번째 인자로 전달한 주솟값(해당 구조체),
								// 구조체인 STAT이라는 스트럭처(위의 inode, st_size가 멤버를 갖고있는 구조체를 두번째인자에 줌으로서 거기에 자동 할당해줌) 에 자동할당해줌

								strncpy(file_inf.name, dir->d_name, NAME_SIZE); // 파일 인포 구조체에 각 파일명들 복사
								file_inf.size = file_stat.st_size;				// 파일 인포 구조체 사이즈에 파일 stat 정보 크기 저장

								printf("%ld\n", write(i, &file_inf, sizeof(file_inf))); // 클라이언트에게 전송(파일 인포)
							}
							// 파일의 목록 끝 표시
							memset(&file_inf, 0, sizeof(file_inf)); // 빈파일 구조체 전송(0으로 설정, 파일 목록 끝 표시를 위해)
							write(i, &file_inf, sizeof(file_inf));
							closedir(d);
						}
						// 클라이언트로부터 파일 이름 받기
						int str_len = 256;
						int recv_len = 0;
						int recv_cnt;
						while (recv_len < str_len)
						{
							recv_cnt = read(i, &file_inf.name[recv_len], NAME_SIZE - 1);
							// printf("%d\n",recv_cnt);
							if (recv_cnt == -1)
								error_handling("read() error!");
							recv_len += recv_cnt;
						}
						file_inf.name[recv_len] = 0;
						// 클라이언트가 quit하면 break
						if (strcmp(file_inf.name, "quit") == 0)
						{
							break;
						}

						stat(file_inf.name, &file_stat);
						file_inf.size = file_stat.st_size;

						// printf("size : %ld",file_inf.size);
						write(i, &file_inf, sizeof(file_inf));
						// 해당 파일 클라이언트에게 전송
						fp = fopen(file_inf.name, "rb");
						pkt *packet = malloc(sizeof(pkt));
						while (1)
						{
							memset(packet, 0, sizeof(pkt));
							read_cnt = fread(packet->content, 1, BUF_SIZE, fp);
							packet->read_size = read_cnt;
							// printf("read_size : %d", packet->read_size);
							if (read_cnt < BUF_SIZE)
							{
								write(i, packet, sizeof(pkt));
								break;
							}
							write(i, packet, sizeof(pkt));
						}
						// 파일 전송 완료 알림
						// memset(packet, 0, sizeof(pkt));
						// write(i, packet, sizeof(pkt));

						// 오류 방지 버퍼 지우기 위해 0으로 memset하기
						// memset(buf, 0, BUF_SIZE);
						// write(clnt_sd, buf, BUF_SIZE);  // 0값 전송

						fclose(fp);

						// str_len = read(i, buf, BUF_SIZE);
						// if (str_len == 0) // close request!
						// {
						// FD_CLR(i, &reads);
						// close(i);
						// 	printf("closed client: %d \n", i);
						// }
						// else
						// {
						// 	write(i, buf, str_len); // echo!
						// }
					}
					FD_CLR(i, &reads);
					close(i);
					printf("closed client: %d \n", i);
				}
			}
		}
	}
	close(serv_sd);
	return 0;
}

void error_handling(char *buf)
{
	fputs(buf, stderr);
	fputc('\n', stderr);
	exit(1);
}
