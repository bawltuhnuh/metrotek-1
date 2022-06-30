#include <sys/socket.h>
#include <sys/ioctl.h>

#include<net/if.h>
#include<netinet/in.h>
#include<netinet/ip.h>
#include<netinet/if_ether.h>
#include<netinet/udp.h>

#include<linux/if_packet.h>

#include <arpa/inet.h>

#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#include <mqueue.h>

#include <fcntl.h>
#include <signal.h>

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
    char* iface;
};

uint64_t package_total_size = 0;
uint64_t package_count = 0;

int keep_running = 1;

void* stat_monitor(void* params)
{
    struct net_params* s_params = (struct net_params*) params;

    int fd;
    struct ifreq ifr;
    struct sockaddr_ll addr;
    struct packet_mreq mreq;

    if ((fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP))) < 0)
    {
    	perror("socket");
    }
    /*
    int flags = fcntl(fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1) {
        perror("fcntl setfl");
    }
    */
    memset(&addr,0,sizeof(addr));
    memset(&ifr,0,sizeof(ifr));
    memset(&mreq,0,sizeof(mreq));

    strncpy(ifr.ifr_name,s_params->iface,IFNAMSIZ);
    if(ioctl(fd,SIOCGIFINDEX,&ifr))
    {
    	perror("ioctl index");
    }

    addr.sll_ifindex = ifr.ifr_ifindex;
    addr.sll_family = AF_PACKET;

    if (bind(fd, (struct sockaddr *)&addr,sizeof(addr)) < 0)
    {
    	perror("bind");
    }

    mreq.mr_ifindex = ifr.ifr_ifindex;
    mreq.mr_type = PACKET_MR_PROMISC;

    if (setsockopt(fd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, (void*)&mreq, (socklen_t)sizeof(mreq)) < 0)
    {
        perror("packet membership");
    }
    
    char* buffer = (char*) malloc(65536);
    memset(buffer, 0, 65536);
    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    struct sockaddr_in daddr;
    memset(&daddr, 0, sizeof(daddr));
    int buflen;
    while (keep_running)
    {
        buflen = recvfrom(fd, buffer, 65536, 0, NULL, NULL);
        if(buflen < 0)
        {/*
            if (errno == EAGAIN) {
                continue;
            } else {
                perror("Recvfrom error");
            }*/
            perror("Recvfrom error");
        } else
        {
            char source[INET_ADDRSTRLEN];
            char dest[INET_ADDRSTRLEN];
            int p_source;
            int p_dest;
            struct iphdr* ip = (struct iphdr*) (buffer + sizeof(struct ethhdr));
            saddr.sin_addr.s_addr = ip->saddr;
            daddr.sin_addr.s_addr = ip->daddr;
            inet_ntop(AF_INET, &(saddr.sin_addr), source, INET_ADDRSTRLEN);
            inet_ntop(AF_INET, &(daddr.sin_addr), dest, INET_ADDRSTRLEN);
            struct udphdr* udp = (struct udphdr*) (buffer + sizeof(struct iphdr)+ sizeof(struct ethhdr));
            p_source = (int)ntohs(udp->source);
            p_dest = (int)ntohs(udp->dest);
            if ((s_params->source_ip == NULL || strcmp(s_params->source_ip, source) == 0) &&
                (s_params->dest_ip == NULL || strcmp(s_params->dest_ip, dest) == 0) &&
                (s_params->source_port == -1 || s_params->source_port == p_source) &&
                (s_params->dest_port == -1 || s_params->dest_port == p_dest))
            {
                package_total_size += buflen;
                ++package_count;
            }
        }
    }
    free(buffer);
    free(s_params->source_ip);
    free(s_params->dest_ip);
    close(fd);
}

void* send_stats()
{
    mqd_t qd_server, qd_client;
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = MAX_MESSAGES;
    attr.mq_msgsize = MAX_MSG_SIZE;
    attr.mq_curmsgs = 0;
    
    char in_buf[MSG_BUFFER_SIZE];
    char out_buf[MSG_BUFFER_SIZE];
    
    if ((qd_server = mq_open(SERVER_QUEUE_NAME, O_RDONLY | O_CREAT /*| O_NONBLOCK*/, QUEUE_PERMISSIONS, &attr)) == -1)
    {
        perror("Server: server mq_open");
    }
    while (keep_running)
    {
	if (mq_receive(qd_server, in_buf, MSG_BUFFER_SIZE, NULL) > 0)
        {
            if ((qd_client = mq_open(in_buf, O_WRONLY)) == -1)
            {
                perror("Server: client mq_open");
                continue;
            }
            sprintf(out_buf, "Count: %d, total size: %d", package_count, package_total_size);
            if (mq_send(qd_client, out_buf, strlen(out_buf) + 1, 0) == -1)
            {
                perror("Server: mq_send");
            }
        }
    }
    if (mq_close(qd_server) == -1) {
        perror("Server: mq_close");
    }
    if (mq_unlink(SERVER_QUEUE_NAME) == -1) {
        perror("Server: mq_unlink");
        exit(1);
    }
}

void int_handler(int sig)
{
    keep_running = 0;
}

int main(int argc, char*argv[])
{
    signal(SIGINT, int_handler);
    struct net_params params;
    params.source_port = -1;
    params.dest_port = -1;
    params.source_ip = NULL;
    params.dest_ip = NULL;
    params.iface = NULL;
    int opt;
    while ((opt=getopt(argc, argv, "s:d:S:D:i:")) != -1) {
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
        case 'i':
        {
            params.iface = malloc(sizeof(optarg));
            strcpy(params.iface, optarg);
            break;
        }
        default:
            break;
        }
    }
    if (params.iface == NULL)
    {
        printf("-i iface is mandatory option\n");
        exit(1);
    }
    pthread_t thread1, thread2;
    int iret1, iret2;
    iret1 = pthread_create(&thread1, NULL, send_stats, NULL);
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
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
}
