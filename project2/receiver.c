////////////////////////////////////////////////////
// RECEIVER
////////////////////////////////////////////////////

    // parse command line options
    // create a new socket
    // send a file request to the server
    //      packet type will be setup, the seq number will signify to the
    //      sender what seq # it is expecting for the first packet
    //
    // while 1
    //      read from socket
    //      if packet type is fin
    //          send fin ack
    //          break
    //      if the data is not corrupt, not lost, and has the expected seq#
    //          send ack with that value
    //          expected_seq_number++
    //      if seq_num < expected_seq_num
    //          send packet wuth ack of seq_num (server lost the ack)
    //      else
    //          do nothing
    // listen for teardown ack from server
    // close socket

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>      // define structures like hostent
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include "packet.c"

void error(char *msg)
{
    perror(msg);
    exit(0);
}

int loss_simulation(float prob)
{
    float p_loss = (float) rand() / (float) RAND_MAX;
    if (p_loss < prob)
        return 1;
    else
        return 0;
}

int main(int argc, char *argv[])
{
    int sockfd; //Socket descriptor
    int n;
    int curr_seq;
    struct sockaddr_in serv_addr;
    struct hostent *server; //contains tons of information, including the server's IP address
    socklen_t server_len;
    struct packet in_pkt;
    struct packet out_pkt;
    FILE *file;

    if (argc != 6) {
        perror("Invalid command line arguements\n");
        exit(0);
    }

    char* sender_hostname = argv[1];
    int portno = atoi(argv[2]);
    char* filename = argv[3];
    float prob_loss = atof(argv[4]);
    float prob_corrupt = atof(argv[5]);



    
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
    
    //request packet for file
    bzero(&out_pkt, sizeof(out_pkt));
    out_pkt.tpe = 0;
    out_pkt.seq = 0;
    out_pkt.data_size = strlen(filename) + 1;
    memcpy(out_pkt.data, filename, strlen(filename));
    
    printf("Client: Sending request for file: %s\n", out_pkt.data);
    n = sendto(sockfd,&out_pkt,sizeof(out_pkt),0, (struct sockaddr*) &serv_addr, server_len); //send to the socket
    if (n < 0) 
         error("ERROR writing to socket");
    printf("CLIENT: request for file sent\n");
    print_packet(&out_pkt);

    //ACK response packet
    curr_seq = 0;
    bzero(&out_pkt, sizeof(out_pkt));
    out_pkt.tpe = 2;
    out_pkt.seq = curr_seq; //fix this - might be "curr_seq - 1"
    out_pkt.data_size = 0;

    //prepare for file transfer
    file = fopen(strcat(filename, "_transfer"), "wb");

    srand(time(NULL)); //create truly random number

    while(1) {
        n = recvfrom(sockfd, &in_pkt, sizeof(in_pkt), 0, (struct sockaddr*) &serv_addr, &server_len);
        if (n < 0)
            printf("Packet loss\n");
        else
        {
            if (loss_simulation(prob_loss)) //packet lost
            {
                printf("CLIENT: simulated packet lost: seq # = %d\n", in_pkt.seq);
                continue;
            }
            else
            {
                if (loss_simulation(prob_corrupt)) // packet corrupted
                {
                    printf("CLIENT: simulated packet corrupted: seq # = %d\n", in_pkt.seq);
                    // n = sendto(sockfd, &out_pkt, sizeof(out_pkt), 0, (struct sockaddr*) &serv_addr, server_len);
                    // if (n < 0)
                    //     error("ERROR resending ACK packet\n");
                    // print_packet(&out_pkt);
                    continue;
                }
            }
        }

        // If you get a FIN, break
        if (in_pkt.tpe == 3) {
            printf("CLIENT: FIN packet recieved\n");
            break;
        }
        if (curr_seq != in_pkt.seq)
        {
            if (curr_seq < in_pkt.seq) //ignore packet only
            {
                printf("CLIENT: packet expected = %d\n", curr_seq);
                printf("CLIENT: packet ignored = %d\n", in_pkt.seq);
                continue;
            }
            else //resend ACK and ignore packet
            {
                printf("CLIENT: packet expected = %d\n", curr_seq);
                printf("CLIENT: packet ignored = %d\n", in_pkt.seq);
                out_pkt.seq = in_pkt.seq;
            }
        }
        else
        {
            if (in_pkt.tpe == 3)                // If it's a FIN, break out
                break;
            else if (in_pkt.tpe == 1) {         // If it's a data type packet, retrieve data and send ack
                printf("CLIENT: Recieved data packet\n");
                print_packet(&in_pkt);

                //done outside of while loop
                //bzero(&out_pkt, sizeof(out_pkt));
                //out_pkt.tpe = 2;
                fwrite(in_pkt.data, 1, in_pkt.data_size, file);
                out_pkt.seq = curr_seq;
                curr_seq++;
            }
            else
            {
                printf("CLIENT: Recieved non-data packet: seq # = %d\n", in_pkt.seq);
                continue;
            }   
        }
        n = sendto(sockfd, &out_pkt, sizeof(out_pkt),0, (struct sockaddr*) &serv_addr, server_len); //send to the socket
        if (n < 0) 
            error("ERROR sending ACK packet\n");
        printf("CLIENT: sent ACK packet\n");
        print_packet(&out_pkt);  
    }
    //Send FIN packet and close client/socket
    bzero((char *) &out_pkt, sizeof(out_pkt));
    out_pkt.tpe = 3;
    out_pkt.seq = curr_seq;
    out_pkt.data_size = 0;
    n = sendto(sockfd, &out_pkt, sizeof(out_pkt),0, (struct sockaddr*) &serv_addr, server_len); //send to the socket
    if (n < 0) 
        error("ERROR sending FIN packet");
    print_packet(&out_pkt);
    printf("Closing file, client, and socket\n");
    close(sockfd); //close socket
    fclose(file);
    
    return 0;
}