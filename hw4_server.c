#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <ctype.h>
#define BUF_SIZE 100
#define MAX_CLNT 256
#define TRIE_SIZE 27

typedef struct TrieNode
{
    struct TrieNode *children[TRIE_SIZE];
    int isEndOfWord;
} TrieNode;

// Global variables
TrieNode *root;
int clnt_cnt = 0;
int clnt_socks[MAX_CLNT];
pthread_mutex_t mutx;

// Function declarations
void *handle_clnt(void *arg);
void send_msg(char *msg, int len);
void error_handling(char *msg);
TrieNode *createNode(void);
void insert(TrieNode *root, const char *word);
int search(TrieNode *root, const char *word);
void printTrie(TrieNode *root, char *buffer, int level);
// Main function
int main(int argc, char *argv[])
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    int clnt_adr_sz;
    pthread_t t_id;
    if (argc != 2)
    {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    pthread_mutex_init(&mutx, NULL);
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");
    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    // Initialize the Trie
    root = createNode();

    FILE *file = fopen("data.txt", "r");
    char line[BUF_SIZE];
    while (fgets(line, sizeof(line), file) != NULL)
    {
        line[strcspn(line, "\n")] = 0; // Remove newline character

        // Finding the last space before the number
        char *last_space = strrchr(line, ' ');
        if (last_space && isdigit(*(last_space + 1)))
        {
            printf("Number: %s\n", last_space + 1); // Print the number

            // Null-terminate to separate string and number
            *last_space = '\0';
            printf("String: %s\n", line);
            insert(root, line);
        }
        else
        {
            printf("String: %s\n", line);
            insert(root, line);
        }
    }
    fclose(file);

    char buffer[BUF_SIZE];
    printTrie(root, buffer, 0);

    printf("\nsearch : %d\n", search(root, "pohang"));

    while (1)
    {
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_sz);

        pthread_mutex_lock(&mutx);
        clnt_socks[clnt_cnt++] = clnt_sock;
        pthread_mutex_unlock(&mutx);

        pthread_create(&t_id, NULL, handle_clnt, (void *)&clnt_sock);

        pthread_detach(t_id);
        printf("Connected client IP: %s \n", inet_ntoa(clnt_adr.sin_addr));
    }
    close(serv_sock);
    return 0;
}

void printTrie(TrieNode *root, char *buffer, int level)
{
    if (root->isEndOfWord)
    {
        buffer[level] = '\0'; // Null terminate the word
        printf("Stored word: %s\n", buffer);
    }

    for (int i = 0; i < TRIE_SIZE; i++)
    {
        if (root->children[i])
        {
            if (i == TRIE_SIZE - 1) // 스페이스 문자를 인식하고 처리
            {
                buffer[level] = ' ';
            }
            else
            {
                buffer[level] = i + 'a'; // Convert the index back to a character
            }
            printTrie(root->children[i], buffer, level + 1);
        }
    }
}

// Trie related functions
TrieNode *createNode(void)
{
    TrieNode *node = (TrieNode *)malloc(sizeof(TrieNode));
    node->isEndOfWord = 0;
    for (int i = 0; i < TRIE_SIZE; i++)
    {
        node->children[i] = NULL;
    }
    return node;
}
void insert(TrieNode *root, const char *word)
{
    TrieNode *cur = root;
    while (*word)
    {
        char c = tolower(*word); // 문자를 소문자로 변환

        int index;
        if (c == ' ') // 스페이스 문자를 인식하고 처리
        {
            index = TRIE_SIZE - 1; // 예를 들어 스페이스를 트라이의 마지막 인덱스로 처리
        }
        else if (c < 'a' || c > 'z') // 알파벳 문자가 아닌 경우 건너뛰기
        {
            word++;
            continue;
        }
        else
        {
            index = c - 'a';
        }

        if (!cur->children[index])
        {
            cur->children[index] = createNode();
        }
        cur = cur->children[index];
        word++;
    }
    cur->isEndOfWord = 1;
}

int search(TrieNode *root, const char *word)
{
    TrieNode *cur = root;
    while (*word)
    {

        char c = tolower(*word); // 문자를 소문자로 변환
        if (c < 'a' || c > 'z')  // 알파벳 문자가 아닌 경우 건너뛰기
        {
            word++;
            continue;
        }
        int index = c - 'a';
        if (!cur->children[index])
            return 0;
        cur = cur->children[index];
        word++;
    }

    return cur->isEndOfWord;
}

// Other functions are the same as before, with modification to handle_clnt to perform search
void *handle_clnt(void *arg)
{
    int clnt_sock = *((int *)arg);
    int str_len = 0;
    char msg[BUF_SIZE];

    while ((str_len = read(clnt_sock, msg, sizeof(msg))) != 0)
    {
        msg[str_len] = '\0'; // 문자열 종료
        if (search(root, msg))
        {
            // 클라이언트에게만 메시지를 전송
            write(clnt_sock, "Result found!", 14);
        }
        else
        {
            // 클라이언트에게만 메시지를 전송
            write(clnt_sock, "No result found!", 17);
        }
    }

    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}
void send_msg(char *msg, int len) // send to all
{
    int i;
    pthread_mutex_lock(&mutx);
    for (i = 0; i < clnt_cnt; i++)
        write(clnt_socks[i], msg, len);
    pthread_mutex_unlock(&mutx);
}
void error_handling(char *msg)
{
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}
