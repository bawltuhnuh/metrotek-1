#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <errno.h>
#include <arpa/inet.h>
#include <mqueue.h>
#include <fcntl.h>

#define QUEUE_PERMISSIONS 0660
#define MAX_MESSAGES 10
#define MAX_MSG_SIZE 256
#define SERVER_QUEUE_NAME "/server_queue_name"
#define MSG_BUFFER_SIZE (MAX_MSG_SIZE + 10)


struct net_params {
    char* source_ip;
    char* dest_ip;
    int source_port;
    int dest_port;
};

pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

int package_size = 0;

void* stat_monitor(void* params)
{
    struct net_params* s_params = (struct net_params*) params;

    int fd = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
    if (fd < 0)
    {
        printf("Socket error");
        return NULL;
    }

    int flags = fcntl(fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1)
    {
        perror("fcntl setfl");
    }
    char* buffer = (char*) malloc(65536);
    memset(buffer, 0, 65536);
    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    struct sockaddr_in daddr;
    memset(&daddr, 0, sizeof(daddr));
    int buflen;
    while (1)
    {
        buflen = recvfrom(fd, buffer, 65536, 0, NULL, NULL);
        if(buflen < 0)
        {
            if (errno == EAGAIN)
            {
                if (pthread_cond_signal(&cond) != 0)
                {
                    perror("cond signal");
                }
                continue;
            } else
            {
                perror("Recvfrom error");
            }
        } else
        {
            char source[INET_ADDRSTRLEN];
            char dest[INET_ADDRSTRLEN];
            int p_source;
            int p_dest;
            struct iphdr* ip = (struct iphdr*) buffer;
            saddr.sin_addr.s_addr = ip->saddr;
            daddr.sin_addr.s_addr = ip->daddr;
            inet_ntop(AF_INET, &(saddr.sin_addr), source, INET_ADDRSTRLEN);
            inet_ntop(AF_INET, &(daddr.sin_addr), dest, INET_ADDRSTRLEN);
            struct udphdr* udp = (struct udphdr*) (buffer + sizeof(struct iphdr));
            p_source = (int)ntohs(udp->source);
            p_dest = (int)ntohs(udp->dest);
            if ((s_params->source_ip == NULL || s_params->source_ip == source) &&
                (s_params->dest_ip == NULL || s_params->dest_ip == dest) &&
                (s_params->source_port == -1 || s_params->source_port == p_source) &&
                (s_params->dest_port == -1 || s_params->dest_port == p_dest))
            {
                if (pthread_mutex_lock(&lock) != 0)
                {
                    perror("monitor mutex lock");
                }
                package_size = (int)ntohs(ip->tot_len);
                if (pthread_cond_signal(&cond) != 0)        
                {
                    perror("monitor signal");
                }
                if (pthread_mutex_unlock(&lock) != 0)
                {
                    perror("monitor mutex unlock");
                }
            }
        }
    }
}

void* compute_stats()
{
    uint64_t sum = 0;
    uint64_t count = 0;
    
    mqd_t qd_server, qd_client;
    
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = MAX_MESSAGES;
    attr.mq_msgsize = MAX_MSG_SIZE;
    attr.mq_curmsgs = 0;
    
    char in_buf[MSG_BUFFER_SIZE];
    char out_buf[MSG_BUFFER_SIZE];
    
    if ((qd_server = mq_open(SERVER_QUEUE_NAME, O_RDONLY | O_CREAT | O_NONBLOCK, QUEUE_PERMISSIONS, &attr)) == -1)
    {
        perror("Server: server mq_open");
    }
    while (1)
    {
        if (pthread_mutex_lock(&lock) != 0)
        {
            perror("compute mutex lock");
        }
        while(package_size == 0)
        {        
		    if (pthread_cond_wait(&cond, &lock) != 0)
        	{
            		perror("cond wait");
        	}
		    if (mq_receive(qd_server, in_buf, MSG_BUFFER_SIZE, NULL) > 0)
        	{
                if ((qd_client = mq_open(in_buf, O_WRONLY)) == -1)
                {
                    perror("Server: client mq_open");
                    continue;
                }
                sprintf(out_buf, "Count: %d, total size: %d", count, sum);
                if (mq_send(qd_client, out_buf, strlen(out_buf) + 1, 0) == -1)
                {
                    perror("Server: mq_send");
                }
            }
	    }
        sum = sum + package_size;
        ++count;
        package_size = 0;
        if (pthread_mutex_unlock(&lock) != 0)
        {
            perror("compute mutex unlock");
        }
    }
}

int main(int argc, char*argv[])
{
    struct net_params params;
    params.source_port = -1;
    params.dest_port = -1;
    params.source_ip = NULL;
    params.dest_ip = NULL;
    int opt;
    while ((opt=getopt(argc, argv, "s:d:S:D:")) != -1) {
        switch (opt) {
        case 's':
        {
            params.source_port = atoi(optarg);
            break;
        }
        case 'd':
        {
            params.dest_port = atoi(optarg);
            break;
        }
        case 'S':
        {
            params.source_ip = malloc(sizeof(optarg));
            strcpy(params.source_ip, optarg);
            break;
        }
        case 'D':
        {
            params.dest_ip = malloc(sizeof(optarg));
            strcpy(params.dest_ip, optarg);
            break;
        }
        default:
            break;
        }
    }
    pthread_t thread1, thread2;
    int iret1, iret2;
    iret1 = pthread_create(&thread1, NULL, compute_stats, NULL);
    if (iret1)
    {
        printf("pthread error: %d", iret1);
        exit(1);
    }
    iret2 = pthread_create(&thread2, NULL, stat_monitor,(void*) &params);
    if (iret2)
    {
        printf("pthread error: %d", iret2);
        exit(1);
    }
    pthread_join(thread2, NULL);
}
