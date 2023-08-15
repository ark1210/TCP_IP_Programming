#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define INET_ADDRSTRLEN 16
#define FILE_NAME_LENGTH 100
#define INFO_SIZE 128
#define MAX_RECV_PEER 3
// Packet 구조체 정의
typedef struct
{
    char ip[INET_ADDRSTRLEN];
    char port[5];
    int id; // 해당 리시빙 피어의 번호
} pkt;

void *listening_thread(void *arg);
void send_peer_info(int client_sock, pkt *info_packets, int count);
void* acceptThreadFunc(void* arg);
void *connectToOtherPeers(void *data);
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
            segment_size = atoi(optarg); // 인수 segment size에 할당
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

            client_socks[client_count] = client_sock;
            client_count++;
        }

        printf("Received Peer Information: IP %s, Port %s, ID %d\n", peer_packets[0].ip, peer_packets[0].port, peer_packets[0].id);
        printf("Received Peer Information: IP %s, Port %s, ID %d\n", peer_packets[1].ip, peer_packets[1].port, peer_packets[1].id);
        printf("Received Peer Information: IP %s, Port %s, ID %d\n", peer_packets[2].ip, peer_packets[2].port, peer_packets[2].id);
        // Receiving Peers의 정보 교환

        for (int i = 0; i < MAX_RECV_PEER; i++)
        {
            if(i == 0) // 리시빙 피어1
            {
                send_peer_info(client_socks[i], &peer_packets[1], 2); // 피어2와 피어3의 정보 전송
            }
            else if(i == 1) // 리시빙 피어2
            {
                send_peer_info(client_socks[i], &peer_packets[2], 1); // 피어3의 정보만 전송
            }
            // 리시빙 피어3에게는 정보를 전송하지 않습니다.
        }
    }

    if (peer == 'r')
    {
        // Receiving Peer 코드를 여기에 작성합니다.
        printf("Running as Receiving Peer connecting to IP %s and port %s\n", ip, opponent_port);

        // accept() 스레드 생성
        pthread_t accept_thread;
        if(pthread_create(&accept_thread, NULL, acceptThreadFunc, (void*)port) != 0) {
            perror("Failed to create accept thread");
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

        printf("test \n");

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
        
        // ID 및 기타 필요한 정보 수신
        pkt peers_info[MAX_RECV_PEER - 1];
        ssize_t bytes_read = read(sending_peer_sock, peers_info, sizeof(peers_info));
        if (bytes_read <= 0)
        {
            perror("read");
            exit(EXIT_FAILURE);
        }

        int num_peers = bytes_read / sizeof(pkt); //실제 수신된  피어의 수
        printf("num_peers : %d\n",num_peers);
        for (int i = 0; i < num_peers; i++) //수신된 피어의 수만큼 반복
        {
            printf("Received Peer Information: IP %s, Port %s, ID %d\n", peers_info[i].ip, peers_info[i].port, peers_info[i].id);
        }

        

        // 리시빙 피어3는 다른 연결을 시도하지 않습니다.
        if (id != 3)
        {
            pkt *peers_data = malloc(sizeof(pkt) * (MAX_RECV_PEER - 1));
            memcpy(peers_data, peers_info, sizeof(pkt) * (MAX_RECV_PEER - 1));

            pthread_t connect_thread;
            if (pthread_create(&connect_thread, NULL, connectToOtherPeers, (void *)peers_data) != 0)
            {
                perror("Failed to create connect thread");
                exit(EXIT_FAILURE);
            }
        }

    pthread_join(accept_thread, NULL);
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
    char* port = (char*) arg;
    int listen_sock, client_sock;
    struct sockaddr_in listen_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // 소켓 생성
    if ((listen_sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 주소 설정
    memset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 자신의 모든 IP 주소에서 연결을 수락
    listen_addr.sin_port = htons(atoi(port)); // 해당 포트에서 연결을 수락

    // 소켓에 주소 바인딩
    if (bind(listen_sock, (struct sockaddr*)&listen_addr, sizeof(listen_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // 연결 대기
    if (listen(listen_sock, 5) < 0) { // 동시에 대기할 수 있는 연결 요청의 최대 수는 5
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // 계속해서 연결을 수락
    while (1) {
        client_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_sock < 0) {
            perror("accept");
            continue;
        }

        // 이 부분에서 client_sock와의 통신이나 필요한 작업을 수행할 수 있습니다.
        // 예를 들어, 다른 피어로부터 받은 데이터를 처리하는 코드 등이 올 수 있습니다.

        close(client_sock);
    }

    close(listen_sock);
    return NULL;
}
void *connectToOtherPeers(void *data)
{
    int id =0;
    pkt *peers_info = (pkt *)data;
    
    for (int i = 0; i < MAX_RECV_PEER - 1; i++)
    {
        // 자신보다 큰 ID의 리시빙 피어에게만 연결을 시도
        if (peers_info[i].id > id)
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

            // 다른 Receiving Peer에 연결
            if (connect(peer_sock, (struct sockaddr *)&peer_addr, sizeof(peer_addr)) < 0)
            {
                perror("connect");
                close(peer_sock);
                continue; 
            }

            printf("Connected to Peer: IP %s, Port %s, ID %d\n", peers_info[i].ip, peers_info[i].port, peers_info[i].id);
            close(peer_sock);
        }
    }
    free(data);
    return NULL;
}
