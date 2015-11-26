////////////////////////////////////////////////////
// RECIEVER
////////////////////////////////////////////////////

	// parse command line options
	// create a new socket
	// send a file request to the server
	//		packet type will be setup, the seq number will signify to the
	//		sender what seq # it is expecting for the first packet
	//
	// while 1
	//		read from socket
	//		if packet type is fin
	//			send fin ack
	//			break
	//		if the data is not corrupt, not lost, and has the expected seq#
	//			send ack with that value
	//			expected_seq_number++
	//		if seq_num < expected_seq_num
	//			send packet wuth ack of seq_num (server lost the ack)
	//		else
	//			do nothing
	// listen for teardown ack from server
	// close socket

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>      // define structures like hostent
#include <stdlib.h>
#include <strings.h>

#include "packet.c"

void error(char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[])
{
    int sockfd; //Socket descriptor
    int n;
    struct sockaddr_in serv_addr;
    struct hostent *server; //contains tons of information, including the server's IP address
    socklen_t server_len;
    struct packet in_pkt;
    struct packet out_pkt;

	if (argc != 6) {
		perror("Invalid command line arguements\n");
		exit(0);
	}

	char* sender_hostname = argv[1];
	int portno = atoi(argv[2]);
	char* filename = argv[3];
	int prob_loss = atoi(argv[4]);
	int prob_corrupt = atoi(argv[5]);



    
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_DGRAM, 0); //create a new socket
    if (sockfd < 0) 
        error("ERROR opening socket");
    
    server = gethostbyname(sender_hostname); //takes a string like "www.yahoo.com", and returns a struct hostent which contains information, as IP address, address type, the length of the addresses...
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    
	server_len = sizeof(serv_addr);

    bzero((char *) &serv_addr, server_len);
    serv_addr.sin_family = AF_INET; //initialize server's address
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    
    bzero(&out_pkt, sizeof(out_pkt));
    memcpy(out_pkt.data, filename, strlen(filename));
    
    printf("Client: Sending request for file: %s\n", out_pkt.data);
    n = sendto(sockfd,&out_pkt,sizeof(out_pkt),0, (struct sockaddr*) &serv_addr, sizeof(serv_addr)); //send to the socket
    if (n < 0) 
         error("ERROR writing to socket");
    printf("after send\n");

    while(1) {
    	n = recvfrom(sockfd, &in_pkt, sizeof(in_pkt), 0, (struct sockaddr*) &serv_addr, &server_len);
	    if (n < 0)
	    	printf("Packet loss\n");
	    if (in_pkt.tpe == 3)				// If it's a FIN, break out
	    	break;
	    else if (in_pkt.tpe == 1) {			// If it's a data type packet, retrieve data and send ack
		    printf("CLIENT: Recieved packet\n");
		    print_packet(&in_pkt);

		    bzero(&out_pkt, sizeof(out_pkt));
		    out_pkt.tpe = 2;
			n = sendto(sockfd, &out_pkt, sizeof(out_pkt),0, (struct sockaddr*) &serv_addr, sizeof(serv_addr)); //send to the socket
	    	if (n < 0) 
	         	error("ERROR writing to socket");
	     }
	}
    
    close(sockfd); //close socket
    
    return 0;
}
