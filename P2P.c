#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <malloc.h>
#include <sys/stat.h>

#define INET_ADDRSTRLEN 16
#define FILE_NAME_LENGTH 100
#define INFO_SIZE 128
#define MAX_RECV_PEER 10
#define MAX_PEERS 10
#define MY_SEGMENT 65536
// Packet 구조체 정의
typedef struct
{
    char ip[INET_ADDRSTRLEN];
    char port[5];
    int id; // 해당 리시빙 피어의 번호
} pkt;
typedef struct {
    pkt *peers_info;
    char port[5];
} ThreadArgs;
typedef struct {
    int listen_sock;
    int my_id;
} AcceptThreadArgs;

typedef struct {
    char name[256]; // 파일 이름
    int size;     // 파일 크기
} FileInfo;

typedef struct { //쓰레드 1 인수
    int sending_peer_sock;
    int my_id;
    int peer_count;
    int r_clientsocks_count;
    FileInfo file_inf;
} ReceiverArgs;


typedef struct { //쓰레드 2 인수
    
    int my_id;
    int sock;
    int sock_id;
    
    FileInfo file_inf;
} ReceiverPeerArgs;


// typedef struct
// {
//     char content[BUF_SIZE];
//     int read_size;
// } pkt;
int peer_socks[MAX_PEERS]; // 연결된 피어들의 소켓 디스크립터 저장
int r_client_socks[MAX_PEERS]; // 전역 배열 accept 한 소켓 디스크립터 저장

void *listening_thread(void *arg);
void send_peer_info(int client_sock, pkt *info_packets, int count);
void* acceptThreadFunc(void* arg);
void *connectToOtherPeers(void *data);
void error_handling(char *message);
void *receiver_from_sending_peer(void *arg);
void *receiver_from_peer(void *arg);
// void *receiver_from_peer2(void *arg);
// void *receiver_from_peer3(void *arg);
void combine_files(size_t segment_size, const char* output_file, const char** input_files, size_t num_files);
int main(int argc, char *argv[])
{
    int id = 0; // 초기값은 -1이나 0으로 설정, 혹은 무효한 값으로 설정

    int opt;
    char peer = ' ';
    int max_num_recv_peer = 0;             // 리시버 피어 최대개수
    char file_name[FILE_NAME_LENGTH] = ""; // 파일 이름
    int segment_size = 0;                  // 세그먼트 사이즈
    char ip[INET_ADDRSTRLEN] = "";         // ip
    char port[5] = "";                     // 자신의 port
    char opponent_port[5] = "";            // 상대 port
    /*
    -s : sending peer로 실행
    -r : receiving peer로 실행
    -n : sending peer에서 받아들일 수 있는 최대 receiving peer 수
    -f : sending peer의 전송 File 이름
    -g : segment 크기, 단위는 KB
    -a : receiving peer일 경우 sending peer의 IP와 Port 정보 입력
    -p : sending peer 일 경우 listen 소켓의 port 번호, 그리고 recieving peer 일 때도 각 recieving peer들과 연결을 해야 하니까 자신의
    lisening 소켓의 port 번호를 의미함.
    Example
    $ ./p2p -s -n 3 -f hgu.mp4 -g 64 -p 9090
    $ ./p2p -r -a 192.168.10.2 9090 -p 7878


    */
    int next_arg = 0;

    while ((opt = getopt(argc, argv, "srn:f:g:a:p:")) != -1)
    {
        switch (opt)
        {
        case 's':
            peer = 's';
            break;
        case 'r':
            peer = 'r';
            break;
        case 'n':
            max_num_recv_peer = atoi(optarg); // -n 옵션의 인수를 최대개수로 대입
            break;
        case 'f':
            strncpy(file_name, optarg, FILE_NAME_LENGTH - 1); // 파일 이름 대입 (들어가면 입력된 후 나머지 다 null문자)
            file_name[FILE_NAME_LENGTH - 1] = '\0';
            break;
        case 'g':
            segment_size = atoi(optarg)*1024; // 인수 segment size에 할당
            break;
        case 'a':

            strncpy(ip, optarg, INET_ADDRSTRLEN - 1);
            /* "192.168.10.2" 총 길이는 13자로서 INET_ADDRSTRLEN - 1 값인 15보다 작으므로 문제없이 복사될 것입니다.
            근데 13자니까 192.168.10.2 9090 이라면 strncpy 로 인해서  192.168.10.2 9 여기까지 ip 배열에 저장이 된다고 생각할 수 있음
            그러나, getopt함수는 공백으로 구분된 인수를 처리 못함, 따라서 192.168.10.2까지만 저장함. 그래서 argv[optind]가 9090 할당 받을 수 있음.*/
            ip[INET_ADDRSTRLEN - 1] = '\0';

            if (optind < argc)
            {
                strncpy(opponent_port, argv[optind], 4);
                opponent_port[4] = '\0';
                optind++; // 포트 번호를 처리하였으므로 인수 인덱스를 증가시킵니다.
            }
            else
            {
                fprintf(stderr, "Missing port argument for -a option.\n");
                exit(EXIT_FAILURE);
            }
            break;
        case 'p':
            strncpy(port, optarg, 4);
            port[4] = '\0';
            break;
        default:
            fprintf(stderr, "Unknown option: -%c\n", optopt);
            exit(EXIT_FAILURE);
            break;
        }
    }

    // 더 많은 입력 검사가 필요할 수 있습니다.
    if (peer != 's' && peer != 'r')
    {
        fprintf(stderr, "Please specify either -s (sending peer) or -r (receiving peer).\n");
        exit(EXIT_FAILURE);
    }

    // 옵션에 따른 동작을 추가합니다.
    if (peer == 's')
    {
        // 파일 정보를 가져오기
        FileInfo file_inf;
        struct stat file_stat;
        strcpy(file_inf.name, file_name); // 예를 들면 "myfile.txt"
        if (stat(file_inf.name, &file_stat) == -1) {
            perror("stat");
            exit(1);
        }
        file_inf.size = file_stat.st_size;

        // Sending Peer 코드를 여기에 작성합니다.
        printf("Running as Sending Peer with file %s and segment size %d\n", file_name, segment_size);
        // Accept 연결
        int client_socks[MAX_RECV_PEER]; // 생성된 클라이언트 소켓(서버 소켓으로 생성된)
        int client_count = 0;
        struct sockaddr_in client_addrs[MAX_RECV_PEER];
        pkt peer_packets[MAX_RECV_PEER];
        int listen_sock;
        struct sockaddr_in listen_addr;

        // 소켓 생성
        if ((listen_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            perror("socket");
            exit(EXIT_FAILURE);
        }

        // 주소 설정
        memset(&listen_addr, 0, sizeof(listen_addr));
        listen_addr.sin_family = AF_INET;
        listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        listen_addr.sin_port = htons(atoi(port));

        int reuse = 1;
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        // Bind
        if (bind(listen_sock, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0)
        {
            perror("bind");
            exit(EXIT_FAILURE);
        }

        // Listen
        if (listen(listen_sock, max_num_recv_peer) < 0) // 연결 대기큐 3개로 제한
        {
            perror("listen");
            exit(EXIT_FAILURE);
        }

        while (client_count < max_num_recv_peer)
        {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_sock;

            if ((client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_len)) < 0)
            {
                perror("accept");
                continue;
            }

            // pkt 정보의 IP를 저장합니다.
            strncpy(peer_packets[client_count].ip, inet_ntoa(client_addr.sin_addr), INET_ADDRSTRLEN);

            // 리시빙 피어로부터 포트 번호를 읽습니다.
            char received_port[5]; // 4자리의 포트번호 + null terminator
            ssize_t bytes_read = read(client_sock, received_port, 4);
            if (bytes_read <= 0) {
                perror("read");
                close(client_sock);
                continue; // 이 클라이언트와의 연결을 종료하고 다음 클라이언트를 기다립니다.
            }
            received_port[bytes_read] = '\0'; // 문자열 종료

            // pkt 정보의 포트 번호를 저장합니다.
            strncpy(peer_packets[client_count].port, received_port, 5);

            peer_packets[client_count].id = client_count + 1; // ID는 1부터 시작
            // ID 전송
            write(client_sock, &peer_packets[client_count].id, sizeof(peer_packets[client_count].id)); //해당 리시버에게 id값 전송하기
            //write(i, &file_inf, sizeof(file_inf));
            client_socks[client_count] = client_sock;
            client_count++;
        }

        // printf("Received Peer Information: IP %s, Port %s, ID %d\n", peer_packets[0].ip, peer_packets[0].port, peer_packets[0].id);
        // printf("Received Peer Information: IP %s, Port %s, ID %d\n", peer_packets[1].ip, peer_packets[1].port, peer_packets[1].id);
        // printf("Received Peer Information: IP %s, Port %s, ID %d\n", peer_packets[2].ip, peer_packets[2].port, peer_packets[2].id);
            for(int i = 0; i < client_count; i++) {
                printf("Received Peer Information: IP %s, Port %s, ID %d\n", 
                    peer_packets[i].ip, peer_packets[i].port, peer_packets[i].id);
            }
       

        // Receiving Peers의 정보 교환
        // 모든 리시빙 피어가 연결된 후 각 리시빙 피어에게 연결 정보 전달
        for (int i = 0; i < client_count; i++) {
            int current_client_sock = client_socks[i];
            
            // 현재 리시빙 피어가 받을 메시지의 총 길이 계산
            int message_length = sizeof(pkt) * (client_count - i - 1); // 현재 리시빙 피어보다 높은 ID를 가진 피어들의 정보 크기
            send(current_client_sock, &message_length, sizeof(message_length), 0);
            
            // 현재 리시빙 피어보다 높은 ID를 가진 피어들의 정보를 전송
            for (int j = i + 1; j < client_count; j++) {
                send(current_client_sock, &peer_packets[j], sizeof(pkt), 0);
            }
        }

         //리시빙 총 피어 개수 전달
        for (int i = 0; i < client_count; i++) {
            
            write(client_socks[i], &client_count, sizeof(int));
            
            
        }

        // Sending Peer에서 모든 Receiving peers로부터 "init complete" 메시지 수신

        char init_complete_msg[14]; // "init complete" 문자열의 크기는 13 + 1(null) = 14
        for (int i = 0; i < client_count; i++)
        {
            int bytes_received = read(client_socks[i], init_complete_msg, 14); // 13 bytes를 읽습니다.
            init_complete_msg[bytes_received] = '\0'; // 문자열 종료를 보장합니다.

            printf("expected message received from Receiving Peer : %s %d\n",init_complete_msg, i+1);
                // close(client_socks[i]);
                 continue;
            // 해당 Receiving Peer의 초기화가 완료되었음을 확인했습니다.
        }

        //해당 파일 전체 사이즈와 이름 있는 패킷 일단 먼저 전송함.
        for (int i = 0; i < client_count; i++) {
            write(client_socks[i], &file_inf, sizeof(file_inf)); // 일단 리시빙 피어 1,2,3에게 전달.
        }


        FILE *fp;
        char content[segment_size];
        int read_size;
        
        int current_client_index = 0;
        fp=fopen(file_inf.name,"rb");
        while(1)
        {
            memset(&content,0,segment_size);
            memset(&read_size,0,sizeof(int));
            read_size = fread(content,1,segment_size,fp); 
          


            if(read_size < segment_size) 
            {
                write(client_socks[current_client_index],&read_size,sizeof(int));
                write(client_socks[current_client_index],content,read_size); 

                
                // 종료 신호 전송
                int end_signal = -1;
                for (int i = 0; i < client_count; i++) {
                    write(client_socks[i], &end_signal, sizeof(int));
                }
                break;  
            }
            write(client_socks[current_client_index],&read_size,sizeof(int)); 
            write(client_socks[current_client_index],content,read_size); 
            
            current_client_index = (current_client_index + 1) % client_count;
            

            

        }

        fclose(fp);


        
    }

    if (peer == 'r')
    {
        // Receiving Peer 코드를 여기에 작성합니다.
        printf("Running as Receiving Peer connecting to IP %s and port %s\n", ip, opponent_port);
        //peers_socks 초기화
        memset(peer_socks, -1, sizeof(peer_socks));
        memset(r_client_socks, -1, sizeof(r_client_socks));  // 초기에 모든 원소를 -1로 설정. -1은 사용되지 않는 슬롯을 나타냅니다.
        // Setup for acceptThreadFunc
        int listen_sock;
        int* client_socks = NULL; // 동적 배열로 선언
        struct sockaddr_in listen_addr;

        // 소켓 생성
        if ((listen_sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
            perror("socket");
            exit(EXIT_FAILURE);
        }

        // 주소 설정
        memset(&listen_addr, 0, sizeof(listen_addr));
        listen_addr.sin_family = AF_INET;
        listen_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
        listen_addr.sin_port = htons(atoi(port));

        
        int reuse = 1;
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        // 소켓에 주소 바인딩
        if (bind(listen_sock, (struct sockaddr*)&listen_addr, sizeof(listen_addr)) < 0) {
            perror("bind");
            exit(EXIT_FAILURE);
        }

        // 연결 대기
        if (listen(listen_sock, 5) < 0) {
            perror("listen");
            exit(EXIT_FAILURE);
        }
    
        int sending_peer_sock;
        struct sockaddr_in sending_peer_addr;
        

        // 소켓 생성
        if ((sending_peer_sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
        {
            perror("socket");
            exit(EXIT_FAILURE);
        }

        // 주소 설정
        memset(&sending_peer_addr, 0, sizeof(sending_peer_addr));
        sending_peer_addr.sin_family = AF_INET;
        sending_peer_addr.sin_addr.s_addr = inet_addr(ip);
        sending_peer_addr.sin_port = htons(atoi(opponent_port));

        //printf("test \n");

        // Sending Peer에 연결
        if (connect(sending_peer_sock, (struct sockaddr *)&sending_peer_addr, sizeof(sending_peer_addr)) < 0)
        {
            perror("connect");
            exit(EXIT_FAILURE);
        }
        // 포트 번호를 전송
        ssize_t sent_bytes = write(sending_peer_sock, port, strlen(port));
        if (sent_bytes < 0) {
        perror("Failed to send port number");
        }
        
        int my_id; //해당 리시버 피어의 id값 받기.
        ssize_t total_readed_bytes = 0;
        ssize_t current_readed_bytes;

        while (total_readed_bytes < sizeof(my_id)) {
            current_readed_bytes = read(sending_peer_sock, ((char *)&my_id) + total_readed_bytes, sizeof(my_id) - total_readed_bytes);
            
            if (current_readed_bytes == -1) {
                perror("Failed to read ID");
                exit(EXIT_FAILURE);
            }
            
            if (current_readed_bytes == 0) {
                printf("Connection closed before all data was received\n");
                exit(EXIT_FAILURE);
            }
            
            total_readed_bytes += current_readed_bytes;
        }

        printf("my_ID: %d\n", my_id);

        // accept() 스레드 생성
        AcceptThreadArgs args;
        args.listen_sock = listen_sock;
        //args.client_socks = client_socks;
        args.my_id = my_id;

        pthread_t accept_thread;
        if (pthread_create(&accept_thread, NULL, acceptThreadFunc, (void*)&args) != 0) {
            perror("Failed to create accept thread");
            exit(EXIT_FAILURE);
        }


            //먼저 메시지의 길이를 받기
        int message_length;
        ssize_t bytes_read = read(sending_peer_sock, &message_length, sizeof(message_length));
        if (bytes_read <= 0)
        {
            perror("read message_length");
            exit(EXIT_FAILURE);
        }

        // 그 후, 메시지 길이만큼 정보를 수신하기
        pkt peers_info[MAX_RECV_PEER - 1];
        int total_received = 0;
        while (total_received < message_length)
        {
            bytes_read = read(sending_peer_sock, ((char*)peers_info) + total_received, message_length - total_received);
            if (bytes_read <= 0)
            {
                perror("read peer info");
                exit(EXIT_FAILURE);
            }
            total_received += bytes_read;
        }

        int num_peers = total_received / sizeof(pkt); // 실제 수신된 피어의 수
        printf("num_peers : %d\n", num_peers);
        for (int i = 0; i < num_peers; i++) // 수신된 피어의 수만큼
        {
            printf("Received Peer Information: IP %s, Port %s, ID %d\n", peers_info[i].ip, peers_info[i].port, peers_info[i].id);
        }

        pthread_t connect_thread;
        if (num_peers != 0)
        {
            ThreadArgs *args = (ThreadArgs *)malloc(sizeof(ThreadArgs));
            args->peers_info = malloc(sizeof(pkt) * num_peers);
            memcpy(args->peers_info, peers_info, sizeof(pkt) * num_peers);
            strncpy(args->port, port, 5);
                
            if (pthread_create(&connect_thread, NULL, connectToOtherPeers, args) != 0)
            {
                perror("Failed to create connect thread");
                exit(EXIT_FAILURE);
            }
            pthread_join(connect_thread, NULL);
        }

        
            pthread_join(accept_thread, NULL);


            int peer_count=0; //peer_socks에 들어있는 연결된 피어 개수
            // 연결된 피어들의 수 카운트
            for (int i = 0; i < MAX_PEERS; i++)
            {
                if (peer_socks[i] != -1) {  // -1이 아닌 경우에만 카운트 증가
                    peer_count++;
                }
            }

            printf("Number of connected peers: %d\n", peer_count);

             int r_clientsocks_count=0; //r_client_socks에 들어있는 연결된 디스크립터 개수
            // 연결된 피어들의 수 카운트
            for (int i = 0; i < MAX_PEERS; i++)
            {
                if (r_client_socks[i] != -1) {  // -1이 아닌 경우에만 카운트 증가
                    r_clientsocks_count++;
                }
            }

            printf("Number of accepted peers: %d\n", r_clientsocks_count);

           //자신을 제외한 리시빙 피어들에게 자신의 id를 주기. 
            for(int i=0;i<peer_count;i++)
                {
                    //my_id 전송
                    write(peer_socks[i], &my_id, sizeof(int));
                     
                }
                for(int i=0;i<r_clientsocks_count;i++)
                {
                    //my_id 전송
                    write(r_client_socks[i], &my_id, sizeof(int));
                
                }

                int peersocks_id[MAX_PEERS];
                 for(int i=0;i<peer_count;i++)
                {
                    //my_id 전송
                    read(peer_socks[i], &peersocks_id[i], sizeof(int)); //peer_socks의 아이디는 peer_socks_id다.
                    printf("peersock_id : %d\n",peersocks_id[i]);
                     
                }
                int r_clientsocks_id[MAX_PEERS];
                for(int i=0;i<r_clientsocks_count;i++)
                {
                    //my_id 전송
                    read(r_client_socks[i], &r_clientsocks_id[i], sizeof(int));//해당 r_clientsocks[i] 아이디는 r_clientsocks_id[i]다.
                    printf("r_clientsocks_id : %d\n",r_clientsocks_id[i]);
                
                }

            int peer_num;
            ssize_t bytes_reading = read(sending_peer_sock, &peer_num, sizeof(int));
            printf("receiving peer's total numbers : %d\n",peer_num);

            // "init complete" 문자열을 sending peer에게 전송
            const char *message = "init complete";
            ssize_t bytes_ssent = write(sending_peer_sock, message, strlen(message));
            printf("%s\n",message);
            if (bytes_ssent < 0) 
            {
                perror("Failed to send init complete message");
                exit(EXIT_FAILURE);
            }

        
        

            FileInfo file_inf;
            int recv_len;
            int recv_cnt;
           
            //int recv_cnt3;
            char temp[sizeof(file_inf)];
           

            // 서버로부터 파일(패킷(이름,사이즈)) 내용받아 파일로 저장
            recv_len=0;
            while (recv_len<sizeof(file_inf)) {
                recv_cnt=read(sending_peer_sock, &temp[recv_len], sizeof(file_inf) - recv_len); //파일 목록 수신
                printf("file received numbers : %d\n",recv_cnt); 
                if(recv_cnt ==-1)
                    error_handling("read() error!");
                recv_len+=recv_cnt;
                
            }
            
            memcpy(&file_inf,temp,sizeof(file_inf));
            printf("sizeof(file_int) : %ld\n", sizeof(file_inf));
            printf("File: %s (%d bytes)\n", file_inf.name, file_inf.size);


            pthread_t thread_id;
            ReceiverArgs args1;
            args1.sending_peer_sock = sending_peer_sock;
            args1.my_id = my_id;
            args1.peer_count=peer_count;
            args1.r_clientsocks_count=r_clientsocks_count;

            //args1.client_socks = client_socks;
            args1.file_inf.size = file_inf.size;
            strcpy(args1.file_inf.name,file_inf.name);

                // 쓰레드 생성. receiver_from_sending_peer 함수 실행
            if(pthread_create(&thread_id, NULL, receiver_from_sending_peer, &args1) != 0) {
                perror("thread_id pthread_create failed");
                return -1;
            }
            
            pthread_t t2[peer_count];
            ReceiverPeerArgs args_peer[peer_count];//n
            
            for(int i=0;i<peer_count;i++)
            {
            
                args_peer[i].my_id = my_id;
                args_peer[i].sock=peer_socks[i];
                args_peer[i].sock_id=peersocks_id[i];
            
                args_peer[i].file_inf.size = file_inf.size;
            strcpy(args_peer[i].file_inf.name,file_inf.name);

                

                


            if(pthread_create(&t2[i], NULL, receiver_from_peer, &args_peer[i])!=0){
                perror("t2 pthread_create failed");
                return -1;
            }

            }

            pthread_t t3[r_clientsocks_count];
            ReceiverPeerArgs args_peer3[r_clientsocks_count];//n

            for(int j=0; j<r_clientsocks_count; j++)
            {
                args_peer3[j].my_id = my_id;
                args_peer3[j].sock = r_client_socks[j];
                args_peer3[j].sock_id = r_clientsocks_id[j];
                args_peer3[j].file_inf = file_inf;

                if(pthread_create(&t3[j], NULL, receiver_from_peer, &args_peer3[j]) != 0){
                    perror("t3 pthread_create failed");
                    return -1;
                }
            }

            
            
            
            
      
        for(int i = 0; i < peer_count; i++) 
        {
            pthread_join(t2[i], NULL);
        }

        for(int j = 0; j < r_clientsocks_count; j++)
        {
            pthread_join(t3[j], NULL);
        }


    

    if(peer_num == 3)
    {
        if(my_id==1)
        {
            
            
            const char* files[] = {
                "test1.mp4",
                "test1_2.mp4",
                "test1_3.mp4"
            };

            combine_files(MY_SEGMENT,"newtest.mp4",files,sizeof(files)/sizeof(files[0]));
            printf("Creating the newtest.mp4 successfully");

        }
        if(my_id==2)
        {   
             const char* files[] = {
                "test2_1.mp4",
                "test2.mp4",
                "test2_3.mp4"
            };

            combine_files(MY_SEGMENT,"newtest2.mp4",files,sizeof(files)/sizeof(files[0]));
            printf("Creating the newtest2.mp4 successfully");

        }
         if(my_id==3)
        {   
             const char* files[] = {
                "test3_1.mp4",
                "test3_2.mp4",
                "test3.mp4"
            };
            combine_files(MY_SEGMENT,"newtest3.mp4",files,sizeof(files)/sizeof(files[0]));
            printf("Creating the newtest3.mp4 successfully");
        }    
        
        sleep(3);
        return 0;
   
    }
        if(peer_num == 4)
        {
            if(my_id==1)
            {
                
                
                const char* files[] = {
                    "test1.mp4",
                    "test1_2.mp4",
                    "test1_3.mp4",
                    "test1_4.mp4",
                };

                combine_files(MY_SEGMENT,"newtest.mp4",files,sizeof(files)/sizeof(files[0]));
                printf("Creating the newtest.mp4 successfully");

            }
            if(my_id==2)
            {   
                const char* files[] = {
                    "test2_1.mp4",
                    "test2.mp4",
                    "test2_3.mp4",
                    "test2_4.mp4"


                };

                combine_files(MY_SEGMENT,"newtest2.mp4",files,sizeof(files)/sizeof(files[0]));
                printf("Creating the newtest2.mp4 successfully");

            }
            if(my_id==3)
            {   
                const char* files[] = {
                    "test3_1.mp4",
                    "test3_2.mp4",
                    "test3.mp4",
                    "test3_4.mp4"
                };
                combine_files(MY_SEGMENT,"newtest3.mp4",files,sizeof(files)/sizeof(files[0]));
                printf("Creating the newtest3.mp4 successfully");
            }
            if(my_id==4)
            {   
                const char* files[] = {
                    "test4_1.mp4",
                    "test4_2.mp4",
                    "test4_3.mp4",
                    "test4.mp4"
                };
                combine_files(MY_SEGMENT,"newtest4.mp4",files,sizeof(files)/sizeof(files[0]));
                printf("Creating the newtest4.mp4 successfully");
            }       
            
            sleep(3);
            return 0;
   
        }

    }
}

void send_peer_info(int client_sock, pkt *info_packets, int count)
{
    for (int i = 0; i < count; i++)
    {
        if (write(client_sock, &info_packets[i], sizeof(pkt)) != sizeof(pkt))
        {
            perror("write");
            exit(EXIT_FAILURE);
        }
    }
}


void* acceptThreadFunc(void* arg) {
    AcceptThreadArgs *args = (AcceptThreadArgs*) arg;
    int listen_sock = args->listen_sock;
    //int* client_socks = args->client_socks;
    int my_id = args->my_id;

    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int num_clients = 0; // 현재 연결된 클라이언트 수
    
    int temp = 1;
    if (my_id != temp) {
        // 계속해서 연결을 수락
        while (1) {
            int client_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &client_addr_len);
            if (client_sock < 0) {
                perror("accept");
                continue;
            }
            int received_port_num;
            read(client_sock, &received_port_num, sizeof(received_port_num));
         

            r_client_socks[num_clients] = client_sock;
            printf("succeed accept %d at index %d\n", client_sock, num_clients);
            printf("accepted client's port : %d\n",received_port_num);
            num_clients++;

            // my_id 값을 기반으로 반복문 탈출 조건 추가
            if (num_clients == my_id - 1) {
                break;
            }
        }
    }
    return NULL;
}
   

void *connectToOtherPeers(void *data)
{
    
    ThreadArgs *args = (ThreadArgs *)data;
    pkt *peers_info = args->peers_info;
    char *thread_port = args->port;  // 
    
    for (int i = 0; i <malloc_usable_size(peers_info)/sizeof(pkt) ; i++)
    {
        
            int peer_sock;
            struct sockaddr_in peer_addr;

            // 소켓 생성
            if ((peer_sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
            {
                perror("socket");
                continue;
            }

            // 주소 설정
            memset(&peer_addr, 0, sizeof(peer_addr));
            peer_addr.sin_family = AF_INET;
            peer_addr.sin_addr.s_addr = inet_addr(peers_info[i].ip);
            peer_addr.sin_port = htons(atoi(peers_info[i].port));

            //printf("Connected to Peer: IP %s, Port %s, ID %d\n", peers_info[i].ip, peers_info[i].port, peers_info[i].id);

            // 다른 Receiving Peer에 연결
            if (connect(peer_sock, (struct sockaddr *)&peer_addr, sizeof(peer_addr)) < 0)
            {
                perror("connect");
                close(peer_sock);
                continue; 
            }

            printf("Connected to Peer: IP %s, Port %s, ID %d\n", peers_info[i].ip, peers_info[i].port, peers_info[i].id);
            int port_num =atoi(thread_port);
            write(peer_sock,&port_num,sizeof(port_num));

            // 연결된 소켓 디스크립터 저장
            peer_socks[i] = peer_sock;
            //close(peer_sock);
        
    }
    free(data);
    return NULL;
}
void* receiver_from_sending_peer(void *arg)
{
    ReceiverArgs *args = (ReceiverArgs *)arg;
    int sending_peer_sock = args->sending_peer_sock;
    int my_id = args->my_id;
    int peer_count=args->peer_count;
    int r_clientsocks_count=args->r_clientsocks_count;


    //int* client_socks = args->client_socks;
    FileInfo file_inf = args->file_inf;

    FILE *fp;
    int read_file_size;
    int total_bytes = 0;

    char content[MY_SEGMENT];
    int read_size;
    int recv_cnt2;
    int read_cnt;

    // my_id 값에 따른 파일명 결정
    char filename[20];
    sprintf(filename, "test%d.mp4", my_id);
    fp = fopen(filename, "wb");
        // int tempp=0;
        // int tempp2=0;
        // int tempp3=0;
    while(1)
    {
        read(sending_peer_sock, &read_size, sizeof(int)); 

        if(read_size == -1) //read_size가 -1이면 다른애들한테 전송도 end_of_signal 주고 break시키기.
        {
            // 연결된 모든 리시빙 피어들에게 데이터 전송
          
            
                        
                        
                        
                for(int i=0;i<peer_count;i++)
                {
                    // 종료 신호 전송
                    int end_signal = -1;
                    write(peer_socks[i], &end_signal, sizeof(int));
                     
                }
                for(int i=0;i<r_clientsocks_count;i++)
                {
                    // 종료 신호 전송
                    int end_signal = -1;
                    write(r_client_socks[i], &end_signal, sizeof(int));
                
                }
              
            


            break;
        }

        // 윤성우 방법 시작
        int recv_len1 = 0; 
        while(recv_len1 < read_size)
        {
            recv_cnt2 = read(sending_peer_sock, &content[recv_len1], read_size - recv_len1);

            if(recv_cnt2 == -1)
                error_handling("read() error!");

            recv_len1 += recv_cnt2; 
        }

        read_cnt = fwrite(content, 1, read_size, fp); 
        total_bytes += read_cnt;
        printf("%d / %d (1nd Thread)\n", total_bytes, file_inf.size);

     
        // 연결된 모든 리시빙 피어들에게 데이터 전송
                for(int i=0;i<peer_count;i++)
                {
                     write(peer_socks[i], &read_size, sizeof(int));
                     write(peer_socks[i], content, read_size);
                     
                }
                for(int i=0;i<r_clientsocks_count;i++)
                {
                    write(r_client_socks[i], &read_size, sizeof(int));
                    write(r_client_socks[i], content, read_size);
                }
           
            
        
      
    }
        // printf("my_id 1 write count : %d\n",tempp);
        // printf("my_id 2 write count : %d\n",tempp2);
        // printf("my_id 3 write count : %d\n",tempp3);

    fclose(fp);
    printf("Download complete\n");

    return NULL; // 함수가 void* 타입을 반환하므로 NULL 반환
}

void *receiver_from_peer(void *arg) {
    ReceiverPeerArgs *args = (ReceiverPeerArgs *)arg;
    int my_id = args->my_id;
    FileInfo file_inf = args->file_inf;
    int sock = args->sock;
    int sock_id = args->sock_id;

    FILE *fp;
    
    // 파일명 결정
    char filename[20];
    sprintf(filename, "test%d_%d.mp4", my_id, sock_id);
    
    fp = fopen(filename, "wb");
    if (fp == NULL) {
        perror("Failed to open file");
        pthread_exit(NULL);
    }

    int total_bytes = 0;
    char content[MY_SEGMENT];
    int read_size;
    int recv_cnt;

    while (1) {
        // my_id에 따른 read 처리를 인수로 전달된 sock을 사용하여 일반화합니다.
        read(sock, &read_size, sizeof(int));

        if (read_size == -1) {  // -1을 수신한 경우 루프를 종료
            break;
        }

        int recv_len = 0;
        while (recv_len < read_size) {
            recv_cnt = read(sock, &content[recv_len], read_size - recv_len);
            if (recv_cnt <= 0) {
                perror("read() error");
                pthread_exit(NULL);
            }
            recv_len += recv_cnt;
        }

        int write_cnt = fwrite(content, 1, read_size, fp); 
        total_bytes += write_cnt;
        printf("%d / %d\n", total_bytes, file_inf.size);
    }

    fclose(fp);
    printf("Download to %s complete\n", filename);
    pthread_exit(NULL);
}

// void *receiver_from_peer3(void *arg) {

//     ReceiverPeerArgs *args = (ReceiverPeerArgs *)arg;
//     int my_id = args->my_id;
//     //int* client_socks = args->client_socks;
//     //int* peer_socks = args->peer_socks;
//     FileInfo file_inf = args->file_inf;

//      FILE *fp;
    
//     // my_id 값에 따른 파일명 결정
//     char filename[20];
//     sprintf(filename, "test%d_%d_%d.mp4", my_id,my_id,my_id);
    
//     fp = fopen(filename, "wb");
//     if (fp == NULL) {
//         perror("Failed to open file");
//         pthread_exit(NULL);
//     }

//     int total_bytes = 0;
//     char content[MY_SEGMENT];
//     int read_size;
//     int recv_cnt;

//     while (1) {
//         int terminate = 0;  // -1 수신 여부를 확인하기 위해 사용

//         if (my_id == 1) {
//             read(peer_socks[1], &read_size, sizeof(int));  // my_id가 1일 경우 peer_socks[1]에서만 읽음
//             // printf("my_id ==1, read_size : %d\n",read_size);
//         } else if (my_id == 2) {
//             read(r_client_socks[0], &read_size, sizeof(int));  // my_id가 2일 경우 client_socks[0]에서만 읽음
//            //  printf("my_id ==2, read_size : %d\n",read_size);
//         } else if (my_id == 3) {
//             read(r_client_socks[1], &read_size, sizeof(int));  // my_id가 3일 경우 client_socks[1]에서만 읽음
//            //  printf("my_id ==3, read_size : %d\n",read_size);
//         }

//         if (read_size == -1) {  // -1을 수신한 경우 루프를 종료
//             break;
//         }

//         int recv_len = 0;
//         while (recv_len < read_size) {
//             if (my_id == 1) {
//                 recv_cnt = read(peer_socks[1], &content[recv_len], read_size - recv_len);
//             } else if (my_id == 2) {
//                 recv_cnt = read(r_client_socks[0], &content[recv_len], read_size - recv_len);
//             } else if (my_id == 3) {
//                 recv_cnt = read(r_client_socks[1], &content[recv_len], read_size - recv_len);
//             }

//             if (recv_cnt <= 0) {
//                 perror("read() error");
//                 pthread_exit(NULL);
//             }
//             recv_len += recv_cnt;
//         }

//         // test3.mp4 파일에 쓰기
//         int write_cnt = fwrite(content, 1, read_size, fp); 
//         total_bytes += write_cnt;
//        ("%d / %d (3rd Thread)\n", total_bytes, file_inf.size);
//     }

//     fclose(fp);
//      printf("Download to %s complete\n", filename);

//    // pthread_exit(NULL);
// }

void combine_files(size_t segment_size, const char* output_file, const char** input_files, size_t num_files) {
    FILE* out_file = fopen(output_file, "wb");
    if (out_file == NULL) {
        perror("Error opening output file");
        return;
    }

    char* buffer = (char*)malloc(segment_size);
    if (buffer == NULL) {
        perror("Memory allocation failed");
        fclose(out_file);
        return;
    }

    FILE** in_files = (FILE**)malloc(num_files * sizeof(FILE*));
    if (in_files == NULL) {
        perror("Memory allocation failed for input file pointers");
        free(buffer);
        fclose(out_file);
        return;
    }

    for (size_t i = 0; i < num_files; i++) {
        in_files[i] = fopen(input_files[i], "rb");
        if (in_files[i] == NULL) {
            perror("Error opening input file");
            for (size_t j = 0; j < i; j++) {
                fclose(in_files[j]);
            }
            free(in_files);
            free(buffer);
            fclose(out_file);
            return;
        }
    }

    int still_reading = 1;
    while (still_reading) {
        still_reading = 0;
        
        for (size_t i = 0; i < num_files; i++) {
            size_t bytes_read = fread(buffer, 1, segment_size, in_files[i]);
            if (bytes_read > 0) {
                still_reading = 1; // 데이터를 읽었으면, 계속 읽기
                fwrite(buffer, 1, bytes_read, out_file);
            }
        }
    }

    for (size_t i = 0; i < num_files; i++) {
        fclose(in_files[i]);
    }
    free(in_files);
    free(buffer);
    fclose(out_file);
}


void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
