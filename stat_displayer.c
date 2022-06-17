#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <mqueue.h>

#define SERVER_QUEUE_NAME "/server_queue_name"
#define QUEUE_PERMISSIONS 0660
#define MAX_MESSAGES 10
#define MAX_MSG_SIZE 256
#define MSG_BUFFER_SIZE (MAX_MSG_SIZE + 10)

int main(int argc, char* argv[])
{
    char queue_name[] = "/client_queue_name";
    mqd_t qd_server, qd_client;
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = MAX_MESSAGES;
    attr.mq_msgsize = MAX_MSG_SIZE;
    attr.mq_curmsgs = 0;
    if ((qd_client = mq_open(queue_name, O_RDONLY | O_CREAT, QUEUE_PERMISSIONS, &attr)) == -1)
    {
        perror("Client: client mq_open");
        exit (1);
    }
    if ((qd_server = mq_open(SERVER_QUEUE_NAME, O_WRONLY)) == -1)
    {
        perror("Client: server mq_open");
        exit(1);    
    }
    char in_buf[MSG_BUFFER_SIZE];
    char temp_buf[10];
    if (mq_send(qd_server, queue_name, strlen(queue_name) + 1, 0) == -1)
    {
        perror("Client: message wasnt send");
        exit(1);
    }
    if (mq_receive(qd_client, in_buf, MSG_BUFFER_SIZE, NULL) == -1)
    {
        perror("Client: mq_receive");
        exit(1);
    }
    if (mq_close(qd_client) == -1)
    {
        perror("Client: mq_close");
    }
    if (mq_unlink(queue_name) == -1)
    {
        perror("Client: mq_unlink");
        exit(1);
    }
    printf("%s", in_buf);
    exit(0);
}
