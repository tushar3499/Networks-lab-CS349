#include<stdio.h>
#include<string.h>	
#include<stdlib.h>	
#include<stdbool.h>
#include<sys/socket.h>
#include<arpa/inet.h>	//inet_addr
#include<unistd.h>	//write
#include<pthread.h> //for threading
#include<signal.h>
#include<time.h>
#include<stdbool.h>
#include<ctype.h>
#include<math.h>

void *connection_handler(void *);
char * server_ip_addr = "10.0.2.69";
int server_socket_num;
float ber; // bit error probability rate
char g[] = "100000111"; // generating function for crc
int N = 9;

int _socket[70000] = {0};

void printclose(int socketfd){ //function to print socket being closed on press of ctrl + c
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    getpeername(socketfd,(struct sockaddr*)&address,(socklen_t*)&addrlen);
    printf("Connection from IP : %s Port : %d closed\n",inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
 }

void sigintHandler(int sig_num){ //function to close all socket being used on press of ctrl + c
    signal(SIGINT, sigintHandler); 
    printf("Please Wait. Gracefully closing all sockets.\n");
    for(int i = 0; i < 70000; i++){
        if(_socket[i] != 0){
           printclose(i);
           close(i);
       }
   }
   printf("Closing server.\n");
   fflush(stdout); 
   exit(0);
}


bool isValidNumber(char str[]){// function to check wheather the fiven string contain only character numbers
    for(int i = 0; i < strlen(str); i++){
    	if(str[i] < '0'|| str[i] > '9') return false;
    }
    return true;
}

int string_to_number(char str[]){ //Function to convert string to number
    if(!isValidNumber(str)){
        return -1;
    }

    if(str[0] == '-') return -1; // If number is negative discard it
    int n = 0;
    for(int i = 0; str[i]!='\0'; i++) {
        n = n * 10 + (str[i] - '0');
    }
    if(n > 65535){ //if not valid socket number
        return -1;
    }
    return n;
}

void crc(char remainder[], char t[], int a){ //Function to compute cycle redundancy check
	 int e = 0, c = 0, d = 0;
	for(e = 0; e < N; e++) remainder[e] = t[e];
	do{
	    if(remainder[0] == '1'){
	         for(d = 1; d < N; d++)
	           remainder[d] = ((remainder[d] == g[d]) ? '0' : '1');
	    }
	    for(c = 0; c < N - 1; c++) remainder[c] = remainder[c + 1];
	    remainder[c] = t[e++];
	}while(e <= a + N - 1);
}

bool IsCorrupt(char * client_message){ //Checks wheather the received data is corrupt or not
	int len=strlen(client_message);
	char remainder[10];
	memset(remainder,'\0',sizeof(remainder));
	crc(remainder, client_message, len);
	for(int i = 0;i < N; i++){
		if(remainder[i] == '1') return true;
	}
	return false;
}

//Checks wheather the sequence number is expected or not
bool check_sequence_no(char * client_message, int expected_sequence_num){
	int len = strlen(client_message);
	char ch = client_message[len - N];
	int seq_num = ch - '0';
	if(seq_num == expected_sequence_num) return true;
	else return false;
}

void print_data(char buffer[]){ // function to print the received binary data into correct ASCII form
    char str3[1000] = "";
    int n = strlen(buffer);
    for(int i = 0; i < n - 9; i = i + 8){
        int ans = 0;
        for(int j = i; j < i + 8; j++){
            ans = 2 * ans + (buffer[j] - '0');
        }
        char c = ans;
        char str2[2];
        str2[0] = c;
        str2[1] = '\0';
        // Concatenating the cnverted character to string
        strcat(str3, str2);
    }
    printf("Data in ASCII : %s\n", str3);
    fflush(stdout); 
}

//Function to print the IP adress of the new incomming connection and socket number being assigned to it 
void print_message(int sock){
	struct sockaddr_in address;
    socklen_t addrlen = sizeof(struct sockaddr_in);
	getpeername(sock,(struct sockaddr*)&address,(socklen_t*)&addrlen);
    printf("Message from IP: %s, Port: %d \n",inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
}

/*
BER -> Bit Error Rate = No of bits to have error in a bit transmitted on an average
So we randomly choose these bits and generate error
*/

void generate_error(char * client_message){
	 srand(time(0));
     float percent = ber * 100.0;
     int len = strlen(client_message);
     for(int i = 0; i < len; i++){
          int random = abs(rand() % 100) + 1;
		      //If the generated random number is less than the BER, we flip the bit to introduce error at that position
          float val = (float)(random);
          if(val <= percent){
               if(client_message[i] == '0') client_message[i] = '1';
               else client_message[i] = '0';
          }
     } 
}

/*
  This will handle connection for each client
*/
void *connection_handler(void *socket_desc){
	//Expected sequence Number from client
	int expected_sequence_num = 0;
	//Get the socket descriptor
	int sock = *(int*)socket_desc;
	_socket[sock] = 1;
	int read_size;
	//Array to store the client message
	char *message, client_message[2000];
	memset(client_message, '\0', sizeof(client_message));
	//Receive while data is send from client
	while((read_size = recv(sock , client_message , 2000 , 0)) > 0 ){
		char reply[100];
		memset(reply,'\0',sizeof(reply));
		print_message(sock); //Print from which client data is received
		printf("Received data before error generation : %s\n", client_message);
		generate_error(client_message); //Generating error into the received data
		printf("Received data after error generation  : %s\n", client_message);
		print_data(client_message); // Print the received binary message into correct ASCII form
		puts("");
		//If data is not corrupt and sequence number is also correct send Ack
		if(!IsCorrupt(client_message) && check_sequence_no(client_message, expected_sequence_num)){
			//AcK + sequence number (first bit Nack and second bit is sequence Number of the packet expected from client)
			if(expected_sequence_num == 0) strcpy(reply, "01");
			else strcpy(reply, "00");	
			expected_sequence_num ^= 1;
		}
		else{
			//If data is corrupt and sequence number is not correct send NAck
			//NAcK + sequence number(first bit Nack and second bit is sequence Number of the packet expected from client)
			if(expected_sequence_num == 0) strcpy(reply,"10");
			else strcpy(reply,"11");
		}
		//Add crc bits to the Ack/NAck beign sent
		char crc_array[100];
		int len = strlen(reply);
	    for(int i = len; i < len + N - 1; i++){
	    	reply[i] = '0';
	    }
		crc(crc_array, reply, len);
		for(int e = len; e < len + N - 1; e++)
                    reply[e] = crc_array[e - len];
        //send the reply to client
		write(sock , reply , strlen(reply));
		memset(client_message, '\0', sizeof(client_message));
	}

	if(read_size == 0){
		puts("Client disconnected");
		fflush(stdout);
	}
	else if(read_size == -1){
		perror("recv failed");
	}

	//Free the socket pointer if the client disconnects
	free(socket_desc);
	_socket[sock] = 0;
}

int main(int argc , char *argv[]){
	if(argc < 2){
		puts("Please provide port number");
		exit(0);
	}
	signal(SIGINT, sigintHandler); 
	server_socket_num = string_to_number(argv[1]);
	if(server_socket_num == -1) {
		puts("Please enter valid port number");
		exit(0);
	}
	printf("Please enter error probability\n");
    scanf("%f", &ber);//taking input bit error probability rate
	int server_socket , new_socket , c, *new_sock;
	struct sockaddr_in server ,client;
	char * message;

	//Create socket
	server_socket = socket(AF_INET , SOCK_STREAM , 0);
	if (server_socket == -1){
		printf("Could not create socket");
	}

	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr(server_ip_addr);
	server.sin_port = htons(server_socket_num);

	//Bind
	if( bind(server_socket,(struct sockaddr *)&server , sizeof(server)) < 0){
		puts("bind failed");
		return 1;
	}
	puts("bind done");

	//Listen
	listen(server_socket , 3);

	//Accept and incoming connection
	puts("Waiting for incoming connections...");
	c = sizeof(struct sockaddr_in);
	while((new_socket = accept(server_socket, (struct sockaddr *)&client, (socklen_t*)&c))){
		printf("Connection accepted");
		struct sockaddr_in address;
		socklen_t addrlen = sizeof(struct sockaddr_in);
		getpeername(new_socket,(struct sockaddr*)&address,(socklen_t*)&addrlen);
		printf(" from IP: %s, Port: %d \n",inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
		//Create thread for new incoming connections
		pthread_t sniffer_thread;
		new_sock = malloc(1);
		*new_sock = new_socket;
		//check if possible to create thread
		if(pthread_create( &sniffer_thread , NULL ,  connection_handler , (void*) new_sock) < 0){
			perror("could not create thread");
			return 1;
		}
		//Now join the thread , so that we dont terminate before the thread
		//pthread_join( sniffer_thread , NULL);
		puts("Handler assigned");
	}

	if (new_socket < 0){
		perror("accept failed");
		return 1;
	}

	return 0;
}
