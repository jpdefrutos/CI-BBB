/*
    clientBBB.c - Remote Control for IC-BBB - Client program
	====================================================
    Implementation of a client program held in the control computer,
    	from which control and configure the BeagleBone Black (BBB).
    The communication is done through a TCP socket.
    The program requires, as input parameter, the IPv4 of the BBB in
    	format of dots: XXX.XXX.X.X
	By default, the port used is the 5000

	This program should be compiled for the architecture of the
	control computer.

	Compiler call: gcc clientBBB.c -o CIprogram

    Author: Javier Perez de Frutos
    E-mail: javier.pdefrutos(at)hotmail.com
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <errno.h>
#include <linux/if_link.h>

#define BUFFER_SIZE 256
#define BUFFER_FILE_SIZE 1024
#define SERVER_PORT 5002

extern int errno;

int clientBBB(char *IPaddress, int serv_port);


int main(int argv, char *argc[]) {

	if (argv < 3) {
		char *IP = "192.168.7.2";

		printf("Default IP: %s Port: %d\n",IP,SERVER_PORT);
		clientBBB(IP,SERVER_PORT);
	} else {
		clientBBB(argc[1],atoi(argc[2]));
	}
	exit(0);
}

int clientBBB(char *IPaddress, int serv_port) {

	int socket_fd, again, count_bytes, recv_bytes, new_file;
	int HW, Nchann, chann[7], j, i, ok, TXT, aux;
	double percentage;
	struct sockaddr_in serverPARAM;
	char buffer[BUFFER_SIZE+1];
	char buffer_recv_file[BUFFER_FILE_SIZE+1];
	char *file_length, *file_name;
	//char file_path[BUFFER_SIZE];
	char file_path[100];
	struct stat dir_info;
	FILE *output_file, *backup;

	//Initialization of variables
	buffer[0]='\0';
	for (i=0;i<7;i++)
		chann[i]=0;
	ok = 1;
	new_file = 0;
	TXT = 0;

	printf("========================================================================\n");
	printf("REMOTE CONTROL FOR CI-BBB PLATFORM\n");
	printf("------------------------------------------------------------------------\n");
	printf("Micro- and Nanosystems Research Group, FIBAM project\n");
	printf("Automation and Engineering Science Department (ASE)\n");
	printf("Tampere University of Technology\n(Tampere, Finland)\n\n");
	printf("Author: Javier Perez de Frutos (javier_perezdefrutos(at)hotmail.com)\n");
	printf("July 2014\n");
	printf("========================================================================\n\n");

	/*
	//Check if destination directory exists or ask for a new one:
	backup = fopen("./backupCI.txt","r+");
	fscanf(backup,"%s\n",&file_path);
	getchar();

	do{
		if (stat(file_path,&dir_info) != 0) {
			printf("Destination directory does not exist.\nA new one will be created at: %s\n",file_path);
			printf("Would you like to change it? (y/n)\n");
			fgets(buffer,BUFFER_SIZE,stdin);
		} else {
			printf("Actual destination directory: %s\n",file_path);
			printf("Would you like to change it? (y/n)\n");
			fgets(buffer,BUFFER_SIZE,stdin);
		}

			if((buffer == "Y") | (buffer == "y")){
				printf("Please, introduce new directory:\n",buffer);
				fgets(file_path,BUFFER_SIZE,stdin);
				printf("New directory: %s\n",file_path);
			} else if((strcmp(buffer,"N\n") != 0) & (strcmp(buffer,"n\n") != 0)
					& (strcmp(buffer,"Y\n") != 0) & (strcmp(buffer,"y\n") != 0)) {

				printf("ERROR: Value not valid. Try again\n");
				buffer[0]='\0';
				fgets(buffer,BUFFER_SIZE,stdin);
			}
		}while((strcmp(buffer,"N\n") != 0) & (strcmp(buffer,"n\n") != 0)
				& (strcmp(buffer,"Y\n") != 0) & (strcmp(buffer,"y\n") != 0));

		if(mkdir(file_path,S_IRWXG) != 0){
			printf("ERROR: %s\n",strerror(errno));
			exit(-1);
		}
	};

	printf("Destination directory: %s\n",file_path);
*/
	//Open the socket
	if ((socket_fd = socket(AF_INET,SOCK_STREAM,0)) < 0){
		printf("ERROR: socket not open\n");
		exit(-1);
	}

	/* Configure parameters of the socket:
	 *  The IP of the BBB is introduced as parameter of the program
	 *  from the terminal. The predefined port is number 5000.
	 */

	serverPARAM.sin_family = AF_INET;
	serverPARAM.sin_port = htons(serv_port);
	inet_pton(AF_INET,IPaddress,&serverPARAM.sin_addr);

	if(connect(socket_fd, (struct sockaddr *)&serverPARAM, sizeof(serverPARAM)) < 0){
		printf("ERROR: connection failed \n");
		exit(-1);
	}

	//Now the connection is established

	do{
		again = 0; //Variable to control additional measurement processes

		for (i=0;i<8;i++) //Reset chann variable
			chann[i]=0;
		//Send configuration of the Communication Interface
		//==================================================

		//Ask the user for the hardware to use
		printf("\nIntroduce the number of the hardware you would like to use\n"
				"------------------------------------------------------\n"
				"1: A/D converter\n"
				"2: I2C communication bus\n"
				"3: Exit program\n\n");
		fgets(buffer,BUFFER_SIZE,stdin);

		while(atoi(buffer) > 3 || atoi(buffer) < 0) {
			buffer[0]='\0';
			printf("Wrong value, please introduce 1, 2 or 3\n");
			fgets(buffer,BUFFER_SIZE,stdin);
		};
		HW = atoi(buffer);

		//Tell the BBB which hardware it has to use
		send(socket_fd,buffer, strlen(buffer),0);

		if(HW == 3)
			goto END_PROGRAM;

		//The BBB sends the configuration stored in memory,
		// of the previous measurement, if the user wants to use it.
		switch (HW) {
		case 1:
			//A/D converter
			buffer[0]='\0';
			recv_bytes = recv(socket_fd,buffer,BUFFER_SIZE,0);
			buffer[recv_bytes] = '\0';

			//Server confirms the existence of a backup file
			if (atoi(buffer)) {

				printf("A previous stored configuration has been found.\nDisplaying parameters:\n");

				buffer[0]='\0';
				recv_bytes = recv(socket_fd,buffer,BUFFER_SIZE,0);
				buffer[recv_bytes] = '\0';

				Nchann=atoi(buffer);
				printf("Number of channels: %d\n",Nchann);

				printf("Channels:");
				for(i=0;i < Nchann; i++){
					buffer[0]='\0';
					recv_bytes = recv(socket_fd,buffer,BUFFER_SIZE,0);
					buffer[recv_bytes] = '\0';

					printf(" %d,",atoi(buffer));
				}
				printf("\n");

				buffer[0]='\0';
				recv_bytes = recv(socket_fd,buffer,BUFFER_SIZE,0);
				buffer[recv_bytes] = '\0';

				printf("Operation mode: ");
				if(atoi(buffer) == 1){
					printf("Automatic\n");
					buffer[0]='\0';
					recv_bytes = recv(socket_fd,buffer,BUFFER_SIZE,0);
					buffer[recv_bytes] = '\0';

					printf("Sampling time: %d s\n",atoi(buffer));
				}else
					printf("Manual\n");

				buffer[0]='\0';
				recv_bytes = recv(socket_fd,buffer,BUFFER_SIZE,0);
				buffer[recv_bytes] = '\0';

				printf("Output format file: ");
				if(atoi(buffer) == 1){
					printf("text file\n");
					TXT = 1;
				}else
					printf("comma separated value\n");
			} else {
				new_file = 1;
			};
			break;

		case 2:
			//I2C bus
			buffer[0]='\0';
			recv_bytes = recv(socket_fd,buffer,BUFFER_SIZE,0);
			buffer[recv_bytes] = '\0';

			//Server informs if there is a configuration file or not
			if (atoi(buffer)) {

				buffer[0]='\0';
				recv_bytes = recv(socket_fd,buffer,BUFFER_SIZE,0);
				buffer[recv_bytes] = '\0';

				printf("I2C bus number: %d\n",atoi(buffer));

				buffer[0]='\0';
				recv_bytes = recv(socket_fd,buffer,BUFFER_SIZE,0);
				buffer[recv_bytes] = '\0';

				printf("Device address: %s",buffer);

				buffer[0]='\0';
				recv_bytes = recv(socket_fd,buffer,BUFFER_SIZE,0);
				buffer[recv_bytes] = '\0';

				printf("Mode: ");
				if(atoi(buffer) == 1){
					printf("Automatic\n");

					buffer[0]='\0';
					recv_bytes = recv(socket_fd,buffer,BUFFER_SIZE,0);
					buffer[recv_bytes] = '\0';

					printf("Sampling time: %d s\n",atoi(buffer));
				}else
					printf("Manual\n");

				buffer[0]='\0';
				recv_bytes = recv(socket_fd,buffer,BUFFER_SIZE,0);
				buffer[recv_bytes] = '\0';

				printf("Output format file: ");
				if(atoi(buffer) == 1)
					printf("text file\n");
				else
					printf("comma separated value\n");
			} else {
				new_file = 1;
			};
			break;
		}

		if (new_file) {
			printf("There is no record of a previous configuration.\n");
			strcpy(buffer,"1\n");
		} else {
			printf("This is the configuration of the previous measurement.\n"
					"Do you wish to:\n(1) Introduce a new configuration\n(2) Use this configuration\n ");
			fgets(buffer,BUFFER_SIZE,stdin);
			while(atoi(buffer) != 2 & atoi(buffer) != 1) {
				buffer[0]='\0';
				printf("Wrong value, please introduce 1 or 2.\n");
				fgets(buffer,BUFFER_SIZE,stdin);
			}
		};
		//Notifies the server if new data is being sent or not
		send(socket_fd,buffer,strlen(buffer),0);

		//Ask the user to introduce the new values, if required
		// (1) New parameters
		// (2) Use default
		if(atoi(buffer) == 1) {
			switch(HW){
			case 1:
				buffer[0]='\0';
				printf("Please, introduce the total number of channels.\n");
				fgets(buffer,BUFFER_SIZE,stdin);
				printf("Number of channels: %s\n",buffer);
				while((atoi(buffer) > 7) || (atoi(buffer) < 0)) {
					buffer[0]='\0';
					printf("Value not valid.\n");
					fgets(buffer,BUFFER_SIZE,stdin);
					printf("Number of channels: %s\n",buffer);
				};
				send(socket_fd,buffer,strlen(buffer),0);

				Nchann = atoi(buffer);
				buffer[0]='\0';
				printf("Please, introduce the channel numbers you want to sample (1 to 7).\n");

				for (i=0; i<Nchann; i++){
					fgets(buffer,BUFFER_SIZE,stdin);
					printf("Channels selected: %d\n",atoi(buffer));

					aux = 1;
					if((atoi(buffer) > 7) || (atoi(buffer) < 1)) {
						buffer[0]='\0';
						printf(" --> Value not valid.");
						i--;
						aux = 0;
					};

					for(j = 0; j < 7; j++) {
						if (atoi(buffer) == chann[j]){
							printf(" --> Channel already selected.");
							i--;
							aux = 0;
							break;
						};
					};

					printf("\n");
					chann[i] = atoi(buffer);

					if (aux){
						send(socket_fd,buffer,strlen(buffer),0);
					};
				};
				printf("Please, introduce the operation mode:\n(A) Automatic\n(M) Manual (use Capture button on the board)\n");
				fgets(buffer,BUFFER_SIZE,stdin);

				while (ok) {
					if(strcmp(buffer,"A\n") == 0 || strcmp(buffer,"a\n") == 0){
						buffer[0]='\0'; sprintf(buffer,"1\n");
						printf("Operating mode selected: Automatic.\n");
						ok = 0;
					} else if (strcmp(buffer,"M\n") == 0 || strcmp(buffer,"m\n") == 0){
						buffer[0]='\0'; sprintf(buffer,"2\n");
						printf("Operating mode selected: Manual.\n");
						ok = 0;
					} else {
						printf("Value not valid.\n");
						fgets(buffer,BUFFER_SIZE,stdin);
					}
				}
				send(socket_fd,buffer,strlen(buffer),0);

				if(atoi(buffer) == 1) {
					printf("Please introduce the sampling time in seconds.\n");
					fgets(buffer,BUFFER_SIZE,stdin);
					printf("Sampling time: %d s\n",atoi(buffer));
					while(atoi(buffer) < 0) {
						printf(" --> Value not valid.\n");
						buffer[0]='\0';
						fgets(buffer,BUFFER_SIZE,stdin);
						printf("Sampling time: %d s",atoi(buffer));
					};
					printf("\n");
					send(socket_fd,buffer,strlen(buffer),0);
				};

				printf("Please, introduce the format of the output file\n"
						"(1) Text file\n"
						"(2) Comma separated values\n");
				fgets(buffer,BUFFER_SIZE,stdin);

				do{
					printf("Format selected:");
					if (atoi(buffer) == 1){
						printf(" Text file\n");
					} else if (atoi(buffer) == 2){
						printf(" Comma separated values\n");
					} else {
						buffer[0]='\0';
						printf("%s --> Value not valid.\n",buffer);
						fgets(buffer,BUFFER_SIZE,stdin);
					};
				}while(atoi(buffer) != 1 && atoi(buffer) != 2);
				send(socket_fd,buffer,strlen(buffer),0);
				break;

			case 2:
				//I2C bus, new parameters
				printf("Please, introduce the I2C module you want to use: 1 (I2C-1) or 2 (I2C-2).\n");
				fgets(buffer,BUFFER_SIZE,stdin);
				while((atoi(buffer) != 1) & (atoi(buffer) != 2)){
					printf("Value not valid. Please introduce 1 or 2\n");
					buffer[0]='\0';
					fgets(buffer,BUFFER_SIZE,stdin);
				}
				printf("I2C bus: %s",buffer);
				send(socket_fd,buffer,strlen(buffer),0);

				printf("Please, introduce the device address in hexadecimal format e.g. 0x40.\n");
				fgets(buffer,BUFFER_SIZE,stdin);
				printf("Device address: %s",buffer);
				send(socket_fd,buffer,strlen(buffer),0);

				printf("Please, introduce the operation mode:\n(A) Automatic\n(B) Manual\n");
				fgets(buffer,BUFFER_SIZE,stdin);
				while (ok) {
					if(strcmp(buffer,"A\n") == 0 || strcmp(buffer,"a\n") == 0){
						buffer[0]='\0'; sprintf(buffer,"1\n");
						printf("Operating mode selected: Automatic.\n");
						ok = 0;
					} else if (strcmp(buffer,"M\n") == 0 || strcmp(buffer,"m\n") == 0){
						buffer[0]='\0'; sprintf(buffer,"2\n");
						printf("Operating mode selected: Manual.\n");
						ok = 0;
					} else {
						printf("Value not valid.\n");
						fgets(buffer,BUFFER_SIZE,stdin);
					}
				}
				send(socket_fd,buffer,strlen(buffer),0);

				if(atoi(buffer) == 1) {
					printf("Please introduce the sampling time in seconds.\n");
					fgets(buffer,BUFFER_SIZE,stdin);
					printf("Sampling time (s): %s",buffer);
					while(atoi(buffer) < 0) {
						printf(" --> Value not valid.\n");
						buffer[0]='\0';
						fgets(buffer,BUFFER_SIZE,stdin);
						printf("Sampling time: %s",buffer);
					};
					printf("\n");
					send(socket_fd,buffer,strlen(buffer),0);
				};

				printf("Please, introduce the format of the output file\n"
						"(1) Text file\n"
						"(2) Comma separated values\n");
				fgets(buffer,BUFFER_SIZE,stdin);

				do{
					printf("Format selected:");
					if (atoi(buffer) == 1){
						printf(" Text file\n");
					} else if (atoi(buffer) == 2){
						printf(" Comma separated values\n");
					} else {
						printf("%s --> Value not valid.\n",buffer);
						buffer[0]='\0';
						fgets(buffer,BUFFER_SIZE,stdin);
					};
				}while(atoi(buffer) != 1 && atoi(buffer) != 2);
				send(socket_fd,buffer,strlen(buffer),0);
				break;
			}
		}
		printf("Press Capture button to start the program\nWait till the light turns off\n");

		//Now the server will aim to send the file to the client
		//First a string containing the length (bytes) of the file, and the
		// name of this. Separated by "-".
		aux=0;
		do{
				strcpy(file_path,"./");

				buffer_recv_file[0]='\0';
				buffer[0]='\0';

				recv_bytes = recv(socket_fd,buffer,BUFFER_SIZE,0); //First thing to arrive is the file length and the file name
				buffer[recv_bytes] = '\0';

				file_length = strtok(buffer,"-");
				file_name = strtok(NULL,"\0");
				strcat(file_path,file_name);
				output_file = fopen(file_path,"a");
				printf("Output file ready at: %s\n",file_path);

				count_bytes = 0;
				printf("Receiving file...\n");
				do {
					buffer_recv_file[0]='\0';
					recv_bytes = recv(socket_fd,buffer_recv_file,BUFFER_FILE_SIZE,0);
					buffer_recv_file[recv_bytes] = '\0';
					//The end of string byte is not transmitted through send() and recv()
					fprintf(output_file,buffer_recv_file);
					count_bytes += recv_bytes;
					percentage = (double)(count_bytes*100 / (atoi(file_length)));
					printf("Received %d of %d (%0.f%%)\n",count_bytes,atoi(file_length),percentage);
				} while(count_bytes < atoi(file_length));

				if (count_bytes >= atoi(file_length)) {
					printf(" ...File transmission completed\n"
							"Bytes expected %d\n"
							"Bytes received %d\n", atoi(file_length),count_bytes);
				}

				if (recv_bytes < 0) {
					printf("ERROR: transmission failed\n waiting for new try from the BBB\n");
					printf("You can find the file %s in the BBB, in route /root/Documents\n",file_name);
				}

				close(output_file);
				aux++;
				file_name[0]='\0';

		}while((aux<Nchann) & TXT == 1);

		//Ask the user for more measurements or to finish the program
		buffer_recv_file[0]='\0';
		buffer[0]='\0';

		printf("Want to make another measurement? (y/n)\n");
		fgets(buffer,BUFFER_SIZE,stdin);

		do{
			if(strcmp(buffer,"Y\n") == 0 || strcmp(buffer,"y\n") == 0){
				printf("New measurement in process\n");
				again = 1;
				buffer[0]='\0';
				sprintf(buffer,"%d\n",1);
				send(socket_fd,buffer,strlen(buffer),0);
				sprintf(buffer,"Y");
				break;
			} else if(strcmp(buffer,"N\n") == 0 || strcmp(buffer,"n\n") == 0){

				buffer[0]='\0';
				sprintf(buffer,"%d\n",0);
				send(socket_fd,buffer,strlen(buffer),0);
				sprintf(buffer,"N");
				break;
			} else {
				printf("%s --> Value not valid\n",buffer);
				buffer[0]='\0';
				fgets(buffer,BUFFER_SIZE,stdin);
			}
		}while(1);

	}while(again == 1); //Run again the client program


	//End of the program
END_PROGRAM:

	buffer[0]='\0';
	printf("Do you want to turn off BBB? (y/n)\n");
	fgets(buffer,BUFFER_SIZE,stdin);

	do{
			if(strcmp(buffer,"Y\n") == 0 || strcmp(buffer,"y\n") == 0){
				buffer[0]='\0';

				sprintf(buffer,"%d\n",1);
				send(socket_fd,buffer,strlen(buffer),0);

				printf("BBB will turn off after 1 minute.\n");
				break;
			} else if(strcmp(buffer,"N\n") == 0 || strcmp(buffer,"n\n") == 0) {
				buffer[0]='\0';

				sprintf(buffer,"%d\n",0);
				send(socket_fd,buffer,strlen(buffer),0);

				break;
			} else {
				printf("ERROR: Value not valid. Try again\n");
				buffer[0]='\0';
				fgets(buffer,BUFFER_SIZE,stdin);
			}
		}while(1);
	printf("\n\nEXIT PROGRAM\n");
	close(socket_fd);
	return 0;
}

