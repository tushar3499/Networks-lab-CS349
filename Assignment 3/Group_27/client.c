//System libraries
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <arpa/inet.h>
#include <string.h> 
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>


int server_socket_num;		//The socket number of the server
float ber;	//Bit error rate as inout from the user											
char g[] = "100000111";		//The generatir polynomial for the CRC
int N = 9;		//The length of generator polynomial

//This function checks if a given string can represent valid non negative integer
bool isValidNumber(char str[]){
    int i=0;
    for(i=0;i<strlen(str);i++){
     if(str[i]<'0'||str[i]>'9') return false;		//If any character apart from 0-9 occurs, it can't be a number
    }
    return true;
}

//This function converts a string to number
int string_to_number(char str[]){
    if(!isValidNumber(str)){
        return -1;
    }

    if(str[0]=='-') return -1;
    int i=0;
    int n = 0;
    for(i=0; str[i]!='\0'; i++) {
        n = n * 10 + (str[i] - '0');
    }
    if(n>65535){
        return -1;
    }
    return n;
}

//This function does the CRC- Cyclic Redundancy Check and returns the remainder of T(x) on division with g(x)
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

//Convert a string into binary equivalent by representing each character as the binary number corresponding to it's ASCII value
void convert_to_binary(char * data, char * bdata){
     int idx = 0;
     for(int i = 0; i < strlen(data); i++){
          int c = (int)data[i];
          for(int j = 7; j >= 0; j--){
               if((1 << j)&c){
                    bdata[idx++] = '1';
               }else bdata[idx++] = '0';
          }
     }
}

//To check if the message received is corrupted by using CRC
bool IsCorrupt(char * client_message){
     int len = strlen(client_message);
     char remainder[10];
     memset(remainder, '\0', sizeof(remainder));
     crc(remainder, client_message, len);
	 
	 //We check if any bit in the remainder is 1, which implies T(x) not divisible by g(x) at receiver, hence errors have been introduced
     for(int i = 0; i < N; i++){
          if(remainder[i] == '1') return true;
     }
     return false;
}

//To generate error in the message bu using user given BER
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

int main(int argc,char *argv[]){
	     //Number of arguments have to be 3
     if(argc < 3){
          puts("Please provide the server ip address and port number");
          exit(0);
     }
	 
	     //Ask for the BER from user
     printf("Please enter error probability\n");
     scanf("%f",&ber);
	 
	     //To ignore '\n' entered by the user
     char ch; scanf("%c", &ch);
	 
	    //Assigning the socket number of server with which connection has to be established
     server_socket_num = string_to_number(argv[2]);
     int client_socket;
     client_socket = socket(AF_INET,SOCK_STREAM,0);    //Create a socket for the client
     if (client_socket == -1){
          printf("Could not create socket");
     }
     struct sockaddr_in server;    //Structure for the socket defined above
     memset(&server,'0',sizeof(server)); 
     server.sin_family = AF_INET;  //Internet domain
     server.sin_port = htons(server_socket_num);	//PORT for the server
	 
	   //Assigning IP address to the server socket
     if(inet_pton(AF_INET,argv[1],&server.sin_addr) <= 0){ 
          perror("Invalid server address / Address not supported \n"); 
          return -1; 
     }
     if (connect(client_socket , (struct sockaddr *)&server , sizeof(server)) < 0){
		puts("Could not connect,try again");
		return 1;
	}
     puts("Connection successful\n");

	 char buffer[1620];		//Data to be sent to the server
     int seq_num = 0;		//Sequence number of the frame being sent
     int trigger = 5000;	//Timeout value(in ms)
	 
     while(true){
		 
		       //Declaration and initialisation of arrays to be used
          char data[200];
          char bdata[1620];
          memset(buffer,'\0', sizeof(buffer));
          memset(bdata,'\0', sizeof(bdata));
          memset(data,'\0', sizeof(data));
          clock_t start = clock();		//Timer initialisation
          puts("Enter data to send");
          gets(data);
          convert_to_binary(data, bdata);	//Get binary data for string input
		  
		      //The binary input is appended with the sequence number of the frame being sent
          if(seq_num == 0) strcat(bdata, "0");
          else strcat(bdata, "1");
		  
          int len = strlen(bdata);
          for(int i = len; i < len + N - 1; i++){
               bdata[i] = '0';
          }
          char crc_array[1000];
          crc(crc_array, bdata, len);
		  
		      //Append the CRC remainder to the data obtained
          for(int e = len; e < len + N - 1; e++)
                    bdata[e] = crc_array[e - len];
          strcpy(buffer, bdata);
          int val = send(client_socket , buffer , strlen(buffer) , 0);
          if(val < 0){
               puts("Could not send data");
               return 1;
          }
          while(true){
               char server_reply[2000];
               memset(server_reply, '\0', sizeof(server_reply));
               int val = recv(client_socket, server_reply , 2000 , 0);
			   
			         //If server closed the connection, we can't send any more data and the next data when sent terminates the program.
               if( val <= 0){
                    puts("No reply received, Connection closed from server");
                    exit(0);
               }
               bool did_timeout_occur = (((clock() - start) / 100) >= trigger);
			   
			         //If timeout occurred
               if(did_timeout_occur){
				   
				           //Resend the frame and reinitialize the timer
                    int val = send(client_socket , buffer , strlen(buffer) , 0);
                    if(val < 0){
                         puts("Could not send data");
                         return 1;
                    }
                    start = clock();
                    continue;
               }
               if(strlen(server_reply) != 0){
                    generate_error(server_reply);	//Generate error in the server reply
                    if(!IsCorrupt(server_reply)){
                         char reply_bit = server_reply[0];
						 
						          //If correct ACK received, change the current sequence number
                       if(reply_bit == '0' && ((1 - seq_num) == (int)(server_reply[1] - '0'))){
                            puts("Reply received: Ack");
                            seq_num ^= 1;
                            break;
                       }else{
                            //If we receive NAck, we resend the frame and reinitialise the timer 
                            int val = send(client_socket , buffer , strlen(buffer) , 0);
                            if(val < 0){
                                 puts("Could not send data");
                                 return 1;
                            }
                            else if(reply_bit=='1') puts("Reply received: NAck");
                            start = clock();
                       }
                    }else{
						
						              //On corrupted server reply, we resend the frame and reinitialise the timer 
                         int val = send(client_socket , buffer , strlen(buffer) , 0);
                         if(val < 0){
                              puts("Could not send data");
                              return 1;
                         }
                         start = clock();
                    }
               }

          }
          puts("Data Sent Successfully\n");
     }
     return 0;
}
