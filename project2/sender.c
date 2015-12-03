// //////////////////////////////////////////////
// // SENDER
// //////////////////////////////////////////////

#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <strings.h>
#include <sys/wait.h>   /* for the waitpid() system call */
#include <signal.h> /* signal name macros, and the kill() prototype */
#include <sys/stat.h>
#include <time.h>

#include "packet.c"

const int u_TIMEOUT = 0; // 100000; // usecs
const int TIMEOUT = 1;      // secs

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int is_valid_file(const char *filename)
{
  FILE *fp = fopen (filename, "r");
  if (fp!=NULL) {
    fclose(fp);
    return 1;
  }
  else
    return 0;
}

int get_filesize(char* filename) {
  if (!(is_valid_file(filename)))
    return -1;

  FILE* f = fopen(filename, "r");
  fseek(f, 0L, SEEK_END);
  int filesize = (int) ftell(f);
  fseek(f, 0L, SEEK_SET);
  fclose(f);
  return filesize;
}

int get_file_chunk(char* buffer, int n_chars, char* filename, int offset) {
    FILE *f = fopen(filename, "rb");
    if (f == NULL)
        return -1;
    int seek_size = offset*DATA_LEN;
    fseek (f , seek_size, SEEK_SET);
    fread(buffer, sizeof(char), n_chars, f);
    fclose(f);
    return sizeof(char) * n_chars;
}

int simulate_event(float prob) {
    float r = (float)rand()/(float)RAND_MAX;
    if (r < prob)
        return 1;
    else
        return 0;
}

int main(int argc, char *argv[])
{
    srand(time(NULL));
    int sockfd, newsockfd, portno, pid;
    struct sockaddr_in serv_addr, cli_addr;
    struct sigaction sa;          // for signal SIGCHLD
    struct packet in_pkt, out_pkt;

    if (argc != 5) {
        perror("Invalid command line arguements\n");
        exit(0);
    }

    int portnumber = atoi(argv[1]);
    int cwnd = atoi(argv[2]);
    float prob_loss = atof(argv[3]);
    float prob_corrupt = atof(argv[4]);

    struct sockaddr_in reciever_addr;
    struct sockaddr_in sender_addr;
    socklen_t sin_size = sizeof(serv_addr);
    socklen_t client_len = sizeof(cli_addr);

    int total_packets;
    int acked_packets = 0;
    int unacked_packets = 0;
    int seq_num = 0;
    int expected_ack = 0;
    int bits_to_send;
    int sent_bits;
    int start_seq;
    int last_packet_bits;
    char filename[256];

     sockfd = socket(AF_INET, SOCK_DGRAM, 0);
     if (sockfd < 0) 
        error("ERROR opening socket");
     bzero((char *) &serv_addr, sizeof(serv_addr));
     portno = atoi(argv[1]);
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
     listen(sockfd,5);

     bzero(&in_pkt, sizeof(in_pkt));
     int file_size;
     while (1) {
        if (recvfrom(sockfd, &in_pkt, sizeof(in_pkt), 0, (struct sockaddr *)&cli_addr, &client_len) < 0) {
            //perror("Packet lost\n");
            continue;
        }
        printf("SERVER: Got packet from client\n");
        if (in_pkt.tpe != 0) {
            printf("Got packet without expected setup type. Ignoring packet.");
            continue;
        }
        file_size = get_filesize(in_pkt.data);
        printf("Filesize = %i\n", (int)file_size);
        if (file_size == -1) {
            error("Invalid file name\n");
            // TODO: Close connection?
        }
        acked_packets = 0;
        unacked_packets = 0;
        seq_num = 0;
        expected_ack = 0;
        bzero(filename, 256);
        memcpy(filename, in_pkt.data, strlen(in_pkt.data));

        seq_num = in_pkt.seq;
        expected_ack = in_pkt.seq;
        start_seq = in_pkt.seq;

        total_packets = file_size/DATA_LEN;
        if (file_size % DATA_LEN != 0) {
            total_packets++;
            last_packet_bits = file_size % DATA_LEN;
        }
        else
            last_packet_bits = DATA_LEN;

        bits_to_send = file_size;
        while (acked_packets < total_packets) {
            while (unacked_packets < cwnd && acked_packets+unacked_packets < total_packets) {
                bzero(&out_pkt, sizeof(out_pkt));
                if (total_packets > acked_packets+unacked_packets+start_seq+1)
                    sent_bits = get_file_chunk(out_pkt.data, DATA_LEN, filename, (unacked_packets+acked_packets));
                else
                    sent_bits = get_file_chunk(out_pkt.data, last_packet_bits, filename, (unacked_packets+acked_packets));
                out_pkt.data_size = sent_bits;
                out_pkt.tpe = 1;
                out_pkt.seq = seq_num;
                bits_to_send -= sent_bits;
                unacked_packets++;
                if (sendto(sockfd, &out_pkt, sizeof(out_pkt), 0, (struct sockaddr *)&cli_addr, client_len) < 0)
                    error("Error sending packet\n");
                printf("SERVER: Sent packet\n");
                print_packet(&out_pkt);
                seq_num++;
            }
            bzero(&in_pkt, sizeof(in_pkt));
            struct timeval tv;
            tv.tv_sec = TIMEOUT;
            tv.tv_usec = u_TIMEOUT;
            if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
                perror("Error");
            }
            if (recvfrom(sockfd, &in_pkt, sizeof(out_pkt), 0, (struct sockaddr *)&cli_addr, &client_len) < 0) {
                printf("SERVER: Timeout. Sending N packets again\n");
                seq_num -= unacked_packets;
                if (seq_num < 0)
                    seq_num = 0;
                unacked_packets = 0;
                continue;
            }

            if (simulate_event(prob_loss)) {
                printf("SERVER: simulating packet loss for the following received packet:\n");
                print_packet(&in_pkt);
                continue;
            }
            else if (simulate_event(prob_corrupt)) {
                printf("SERVER: simulating packet corruption for the following received packet:\n");
                print_packet(&in_pkt);
                continue;
            }
            if (in_pkt.tpe == 2) {
                if (in_pkt.seq >= expected_ack && in_pkt.seq < seq_num) {
                    printf("SERVER: recieved packet with in range ack:\n");
                    print_packet(&in_pkt);
                    for (int i = 0; i < in_pkt.seq-expected_ack+1; i++) {
                        acked_packets++;
                        unacked_packets--;
                    }
                    expected_ack = in_pkt.seq+1;
                }
                else {
                    printf("SERVER: received packet with out of range ack:\n");
                    print_packet(&in_pkt);
                }
            }
        }
        bzero(&out_pkt, sizeof(out_pkt));
        out_pkt.tpe = 3;
        printf("SERVER: recieved all acks. Sending FIN packet:\n");
        print_packet(&out_pkt);
        if (sendto(sockfd, &out_pkt, sizeof(out_pkt), 0, (struct sockaddr *)&cli_addr, client_len) < 0)
            error("Error sending packet\n");
        bzero(&in_pkt, sizeof(in_pkt));
        while (1) {
            if (recvfrom(sockfd, &in_pkt, sizeof(in_pkt), 0, (struct sockaddr *)&cli_addr, &client_len) < 0) {
                // perror("Packet lost\n");
                bzero(&out_pkt, sizeof(out_pkt));
                out_pkt.tpe = 3;
                printf("SERVER: Timeout sending FIN. resending:\n");
                print_packet(&out_pkt);
                if (sendto(sockfd, &out_pkt, sizeof(out_pkt), 0, (struct sockaddr *)&cli_addr, client_len) < 0)
                    error("Error sending packet\n");
                continue;
            }
            if (in_pkt.tpe == 3)
                printf("SERVER: received FIN from client\n");
                break;
        }
    }
     return 0; /* we never get here */
}


