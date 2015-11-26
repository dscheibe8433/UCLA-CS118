// //////////////////////////////////////////////
// // SENDER
// //////////////////////////////////////////////

#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <strings.h>
#include <sys/wait.h>	/* for the waitpid() system call */
#include <signal.h>	/* signal name macros, and the kill() prototype */
#include <sys/stat.h>

#include "packet.c"

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

int main(int argc, char *argv[])
{
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
	int prob_loss = atoi(argv[3]);
	int prob_corrupt = atoi(argv[4]);

	struct sockaddr_in reciever_addr;
	struct sockaddr_in sender_addr;
	socklen_t sin_size = sizeof(serv_addr);
	socklen_t client_len = sizeof(cli_addr);

	int total_packets;
	int acked_packets = 0;
	int unacked_packets = 0;
	int bits_to_send;
	int sent_bits;
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
     
     int file_size;
     while (1) {
     	if (recvfrom(sockfd, &out_pkt, sizeof(out_pkt), 0, (struct sockaddr *)&cli_addr, &client_len) < 0) {
     		perror("Packet lost\n");
     		continue;
     	}
     	printf("SERVER: Got packet from client\n");
     	if (out_pkt.tpe != 0) {
     		printf("Got packet without expected setup type. Ignoring packet.");
     		continue;
     	}
     	file_size = get_filesize(out_pkt.data);
     	printf("Filesize = %i\n", (int)file_size);
     	if (file_size == -1) {
     		error("Invalid file name\n");
     		// TODO: Close connection?
     	}
     	memcpy(filename, out_pkt.data, strlen(out_pkt.data));

     	total_packets = file_size/DATA_LEN;
     	if (file_size % DATA_LEN != 0)
     		total_packets++;

     	bits_to_send = file_size;
     	while (acked_packets < total_packets) {
     		while (unacked_packets < cwnd && acked_packets+unacked_packets < total_packets) {
     			bzero(&out_pkt, sizeof(out_pkt));
     			if (bits_to_send >= DATA_LEN)
     				sent_bits = get_file_chunk(out_pkt.data, DATA_LEN, filename, (unacked_packets+acked_packets));
     			else
     				sent_bits = get_file_chunk(out_pkt.data, bits_to_send, filename, (unacked_packets+acked_packets));
     			out_pkt.data_size = sent_bits;
     			out_pkt.tpe = 1;
     			bits_to_send -= sent_bits;
     			unacked_packets++;
     			if (sendto(sockfd, &out_pkt, sizeof(out_pkt), 0, (struct sockaddr *)&cli_addr, client_len) < 0)
     				error("Error sending packet\n");
     			printf("SERVER: Sent packet\n");
     			print_packet(&out_pkt);
     		}
     		bzero(&in_pkt, sizeof(in_pkt));
     		if (recvfrom(sockfd, &in_pkt, sizeof(out_pkt), 0, (struct sockaddr *)&cli_addr, &client_len) < 0) {
     			perror("Packet lost\n");
     			continue;
     		}
     		if (in_pkt.tpe == 2) {
     			acked_packets++;
     			unacked_packets--;
     		}
    	}
    	bzero(&out_pkt, sizeof(out_pkt));
    	out_pkt.tpe = 3;
		if (sendto(sockfd, &out_pkt, sizeof(out_pkt), 0, (struct sockaddr *)&cli_addr, client_len) < 0)
			error("Error sending packet\n");
		// close(sockfd);
		// return 0;
 	}
     return 0; /* we never get here */
}


