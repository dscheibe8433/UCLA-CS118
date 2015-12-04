#define DATA_LEN 800

struct packet
{
	int tpe;	// 0 -> setup, 1 -> data, 2 -> ACK, 3 -> FIN
	int seq;
	unsigned int data_size;
	char data[1024];
};

void print_packet(struct packet *p) {
	printf("---------------------\n");
	printf("\tPACKET DATA\n");
	printf("Type: %i\n", p->tpe);
	printf("Seq/ACK Number: %i\n", p->seq);
	printf("Data Size: %u\n", p->data_size);
	printf("Data:\n");

	for (int i = 0; i < p->data_size; i++)
		printf("%c", p->data[i]);
	printf("\n");
	printf("---------------------\n\n");
}
