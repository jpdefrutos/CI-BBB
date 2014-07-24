/*
    clientBBB.c - Communication Interface client program
	====================================================
    Implementation of a server program held in BeagleBone Black (BBB),
    that executes the commands required for the CI.
    The communication is done by means of a TCP socket, with the control computer.

	This program should be compiled for a ARM Cortex A-8 architecture (BBB).
	Compiler call: gcc -Wall -o CIserver serverBBB.c /usr/local/lib/freebasic/fbrt0.o -lpruio -L"/usr/local/lib/freebasic/" -lfb -lpthread -lprussdrv -ltermcap -lsupc++ -Wno-unused-variable -L. -lCI_BBB


    Author: Javier Perez de Frutos
    E-mail: javier.pdefrutos(at)hotmail.com
*/


#include <stdlib.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <linux/if_link.h>
#include <errno.h>
#include <sys/resource.h>
#include "CI_BBB.h"

#define SERVER_PORT 5002

int serverBBB (int serv_port);
int executeCI (_program_parameters *input);

end = 0; //For alarm handler

extern int errno;

int main(int argv, char *argc[]){

	/*
	Test instructions
	-------------------
	struct rlimit cputime;
	int priority;

	printf("PID: %d\n",getpid());
	getrlimit(RLIMIT_CPU,&cputime);
	printf("CPU time (s) Soft limit: %lld  Hard limit: %lld\n",cputime.rlim_cur,cputime.rlim_max);

	priority = getpriority(PRIO_PROCESS,0);
	printf("Process priority (old): %d\n",priority);

	setpriority(PRIO_PROCESS,0,-20);
	priority = getpriority(PRIO_PROCESS,0);
	printf("Process priority: %d\n",priority);
	 */

	setpriority(PRIO_PROCESS,0,-20);

	if (argv < 2) {
		printf("Default port: %d\n",SERVER_PORT);
		serverBBB(SERVER_PORT);
	} else {
		serverBBB(atoi(argc[1]));
	}
	exit(0);

}

int serverBBB (int serv_port) {

	int socket_fd, client_fd, j, i, aux, recv_bytes;
	int exit_program = 0;
	int flags_block, flags_non_block, I2Cbus;
	_CI_parameters CIparam;
	_CI_parameters *ptr_CIparam = &CIparam;
	_program_parameters program_conf;
	_program_parameters *ptr_program_conf = &program_conf;

	struct sockaddr_in serverPARAM;
	char buffer[BUFFER_SIZE+1];
	FILE *backup_fd;

	//Prepare socket TCP
	socket_fd=socket(AF_INET,SOCK_STREAM,0); //Datagram socket... using sendfile() to send the file-> one package

	serverPARAM.sin_family = AF_INET;
	serverPARAM.sin_addr.s_addr = htonl(INADDR_ANY);
	serverPARAM.sin_port = htons((short int)serv_port);

	if(bind(socket_fd, (struct sockaddr *) &serverPARAM,sizeof(serverPARAM)) == -1) {
		perror("ERROR: bind\n");
		exit(1);
	}

	printf("TCP Socket open at port: %d\n", serv_port);

	//Wait for new client
	if(listen(socket_fd, MAX_CLIENTS) == -1) {
		printf("ERROR: Listen failed\n");
		exit(-1);
	}
	printf("Waiting for new client request...");

	//Send to the client side, the stored values of the previous measurement
	// Load new parameters if required, or keep the old ones.

		//Server will enter a loop to attend all incoming clients
		client_fd = accept(socket_fd, (struct sockaddr*) NULL, NULL);
		printf("  ...New client accepted.\n");

		if(client_fd < 0) {
			printf("ERROR: could not accept connection\n");
			exit(-1);
		};

		program_conf.socket = client_fd;
	do {
		//Init for several loops
		CIparam.firstTime = 1;
		CIparam.mask = 2; //Need an active step to load driver when using I2C. To use the buttons
		end = 0;

		if( access("./BACKUP/backupDRIVER.txt",F_OK) != -1){
			//File exists and thus can be opened to read
			if ((backup_fd = fopen("./BACKUP/backupDRIVER.txt","r+")) == NULL ){
				printf("ERROR: cannot open defautlDRIVER.txt\n");
				close(client_fd);
				close(socket_fd);
				exit(-1);
			};

			printf("backupDRIVER.txt file found.\nStored values in backupDRIVER.txt\n");
			//Order: avg, clkDiv, OpDelay, SampDelay
			fscanf(backup_fd,"%d",&CIparam.avg); printf("(D) Avg: %d\n",CIparam.avg);
			fscanf(backup_fd,"%d",&CIparam.clkDiv); printf("(D) clkDiv: %d\n",CIparam.clkDiv);
			fscanf(backup_fd,"%d",&CIparam.OpDelay); printf("(D) OpDelay: %d\n",CIparam.OpDelay);
			fscanf(backup_fd,"%d",&CIparam.SampDelay); printf("(D) SampDelay: %d\n",CIparam.SampDelay);

		} else {
			//File does not exists and so it must be created
			if ((backup_fd = fopen("./BACKUP/backupDRIVER.txt","w+")) == NULL ){
				printf("ERROR: cannot open defautlDRIVER.txt\n");
				close(client_fd);
				close(socket_fd);
				exit(-1);
				//Open for reading and writing (creates new file, if it does not exist yet)
			};

			printf("No backupDRIVER.txt file detected.\n"
					"A new one has being created with the following values,\n"
					"at ./BACKUP directory\n");
			CIparam.avg = 0; printf("(D) Avg: %d\n",CIparam.avg);
			CIparam.clkDiv = 0; printf("(D) clkDiv: %d\n",CIparam.clkDiv);
			CIparam.OpDelay = 0; printf("(D) OpDelay: %d\n",CIparam.OpDelay);
			CIparam.SampDelay = 0; printf("(D) SampDelay: %d\n",CIparam.SampDelay);

			fprintf(backup_fd,"%d\n",CIparam.avg);
			fprintf(backup_fd,"%d\n",CIparam.clkDiv);
			fprintf(backup_fd,"%d\n",CIparam.OpDelay);
			fprintf(backup_fd,"%d\n",CIparam.SampDelay);
		}
		fclose(backup_fd);
		printf("backupDRIVER.txt file closed.\n");

		printf("Waiting for hardware selection...");
		buffer[0]='\0';
		recv_bytes = recv(client_fd,buffer,BUFFER_SIZE,0);
		buffer[recv_bytes] = '\0';

		printf("...Hardware selected: %d\n",atoi(buffer));
		program_conf.instr = atoi(buffer);

		if(atoi(buffer) == 3)
			goto END_PROGRAM;

		switch(atoi(buffer)){
		case 1: //A/D selected
			printf("A/D converter configuration\n------------------------\n");
			//Get the information to configure the CI
			//First load the parameters not requested to the user from file backupADC.bin
			if ( access("./BACKUP/backupADC.txt",F_OK) != -1){
				//File exists and thus can be opened to read and overwrite
				if ((backup_fd = fopen("./BACKUP/backupADC.txt","r+")) == NULL ){
					printf("ERROR: cannot open defautlADC.txt\n");
					close(client_fd);
					close(socket_fd);
					exit(-1);
				};

				printf("backupADC.txt file found.\nStored values in backupADC.txt\n");

				//Tell the client that the files exists and is going to receive the stored configuration
				buffer[0]='\0';
				strcpy(buffer,"1\n");
				send(client_fd,buffer,strlen(buffer),0);
				buffer[0]='\0';
				printf("Client warned of the existence of backupADC.txt file.\n");

				printf("Sending the stored values to the user:\n");
				//Order: number of channels, list of channels, Mode, Time, File format
				fscanf(backup_fd,"%d",&CIparam.Nchannels); printf("(D) Number of channels: %d\n",CIparam.Nchannels);

				buffer[0]='\0';
				sprintf(buffer,"%d\n",CIparam.Nchannels);
				send(client_fd,buffer, strlen(buffer),0);
				sleep(1);

				for (i=0;i < CIparam.Nchannels; i++) {
					fscanf(backup_fd,"%d",&CIparam.channels[i]);
					printf("(D) Channels number %d: %d\n", i, CIparam.channels[i]);

					buffer[0]='\0';
					sprintf(buffer,"%d\n",CIparam.channels[i]);
					send(client_fd,buffer, strlen(buffer),0);
					sleep(1);
				}

				CIparam.mask = gen_mask(ptr_CIparam);
				printf("(D) Mask: %d\n",CIparam.mask);

				fscanf(backup_fd,"%d",&CIparam.mode); printf("(D) Mode: %d\n",CIparam.mode); //1 == Auto, 2 == Manual

				buffer[0]='\0';
				sprintf(buffer,"%d\n",CIparam.mode);
				send(client_fd,buffer, strlen(buffer),0);
				sleep(1);

				fscanf(backup_fd,"%d",&CIparam.captureTime);

				if(CIparam.mode) {
					printf("(D) Sampling time: %d\n",CIparam.captureTime); //in senconds

					buffer[0]='\0';
					sprintf(buffer,"%d\n",CIparam.captureTime);
					send(client_fd,buffer, strlen(buffer),0);
					sleep(1);
				}

				fscanf(backup_fd,"%d",&CIparam.fileSelection);
				printf("(D) File format: %d\n",CIparam.fileSelection); //1 == TXT, 2 == CSV

				buffer[0]='\0';
				sprintf(buffer,"%d\n",CIparam.fileSelection);
				send(client_fd,buffer, strlen(buffer),0);
				sleep(1);

				buffer[0]='\0';
				recv_bytes = recv(client_fd,buffer,BUFFER_SIZE,0);
				buffer[recv_bytes] = '\0';

				printf("Introduce new parameters?: %d\n",atoi(buffer));
			} else {
				//File does not exists and so it must be created
				if ((backup_fd = fopen("./BACKUP/backupADC.txt","w+")) == NULL ){
					printf("ERROR: cannot open defautlADC.txt\n");
					close(client_fd);
					close(socket_fd);
					exit(-1);
					//Open for reading and writing (creates new file, if it does not exist yet)
				};

				printf("No backupADC.txt file detected...\n");

				buffer[0]='\0';
				strcpy(buffer,"0\n");
				send(client_fd,buffer,strlen(buffer),0);

				//The file is empty, new data should be supplied

				buffer[0]='\0';
				recv_bytes = recv(client_fd,buffer,BUFFER_SIZE,0);
				buffer[recv_bytes] = '\0';
				printf("  ...a new one should be done? %d\n",atoi(buffer));
			};


			if(atoi(buffer) == 1) {
				fseek(backup_fd,0,SEEK_SET);
				// (1) new parameters form the user
				// (2) use loaded backup parameters
				printf("Ready to receive the new configuration:\n");
				//Order: number of channels, list of channels, Mode, Time (if required), File format
				buffer[0]='\0';
				recv_bytes = recv(client_fd,buffer,BUFFER_SIZE,0);
				buffer[recv_bytes] = '\0';

				CIparam.Nchannels = atoi(buffer);
				printf("NEW Number of channels: %d\n", CIparam.Nchannels);
				fprintf(backup_fd,"%d\n",CIparam.Nchannels);

				for (i=0;i < CIparam.Nchannels; i++) {
					buffer[0]='\0';
					recv_bytes = recv(client_fd,buffer,BUFFER_SIZE,0);
					buffer[recv_bytes] = '\0';

					CIparam.channels[i] = atoi(buffer);
					printf("NEW Channels number %d: %d\n", i, CIparam.channels[i]);
					fprintf(backup_fd,"%d\n",CIparam.channels[i]);
				}

				  //Now lets put everything in order using
				  // bubble sort algorithm!
				if(CIparam.Nchannels > 1) {
					for (i=0;i < (CIparam.Nchannels - 1); i++) {
						for (j=0;j < (CIparam.Nchannels - i - 1); j++) {
							if (CIparam.channels[j] > CIparam.channels[j+1]) {
								aux = CIparam.channels[j];
								CIparam.channels[j] = CIparam.channels[j+1];
								CIparam.channels[j+1] = aux;
							};
						};
					};
				};

				CIparam.mask = gen_mask(ptr_CIparam);
				printf("NEW Mask: %d\n",CIparam.mask);

				buffer[0]='\0';
				recv_bytes = recv(client_fd,buffer,BUFFER_SIZE,0);
				buffer[recv_bytes] = '\0';

				CIparam.mode = atoi(buffer);

				printf("NEW Mode: %d\n",CIparam.mode);
				fprintf(backup_fd,"%d\n",CIparam.mode);

				if (CIparam.mode == 1){
					buffer[0]='\0';
					recv_bytes = recv(client_fd,buffer,BUFFER_SIZE,0);
					buffer[recv_bytes] = '\0';

					CIparam.captureTime = atoi(buffer);
					printf("NEW Sampling time: %d\n",CIparam.captureTime);
					fprintf(backup_fd,"%d\n",CIparam.captureTime);
				} else {
					fprintf(backup_fd,"%d\n",0);
				}

				buffer[0]='\0';
				recv_bytes = recv(client_fd,buffer,BUFFER_SIZE,0);
				buffer[recv_bytes] = '\0';

				CIparam.fileSelection = atoi(buffer);
				printf("NEW File selected: %d\n", CIparam.fileSelection);
				fprintf(backup_fd,"%d\n",CIparam.fileSelection);
			};

			fclose(backup_fd);
			printf("backupADC.txt file closed.\n");
			break;

		case 2: //I2C selected
			printf("I2C bus configuration\n------------------------\n");
			//Get the information to configure the CI
			//First load the parameters not requested to the user from file backupI2C.bin

			if( access("./BACKUP/backupI2C.txt",F_OK) != -1){
				//File exists and thus can be opened to read and overwrite
				if ((backup_fd = fopen("./BACKUP/backupI2C.txt","r+")) == NULL ){
					printf("ERROR: cannot open defautlDRIVER.txt\n");
					close(client_fd);
					close(socket_fd);
					exit(-1);
				};
				printf("backupI2C.txt file found.\nStored value in backupI2C.txt\n");
				//Tell the client that the files exists and is going to receive the stored configuration
				//Send the user the direction of the last device used, mode, time and file format
				buffer[0]='\0';
				strcpy(buffer,"1\n");
				send(client_fd,buffer, strlen(buffer),0);

				//Order: I2C bus, devAddres, mode, sampling time, file format
				fscanf(backup_fd,"%d",&I2Cbus);

				if (I2Cbus == 2){
					CIparam.i2cName = "/dev/i2c-1";
				} else {
					CIparam.i2cName = "/dev/i2c-2";
				}
				printf("(D) I2C bus: %d\n", I2Cbus);
				buffer[0]='\0';
				sprintf(buffer,"%d\n",I2Cbus);
				send(client_fd,buffer, strlen(buffer),0);
				sleep(1);

				fscanf(backup_fd,"%s",&CIparam.devAddress); printf("(D) Slave device address: %s\n",CIparam.devAddress);
				sprintf(buffer,"%s\n",CIparam.devAddress);
				send(client_fd,buffer, strlen(buffer),0);
				sleep(1);

				fscanf(backup_fd,"%d",&CIparam.mode); printf("(D) Mode: %d\n",CIparam.mode); //1 == Auto, 2 == Manual

				buffer[0]='\0';
				sprintf(buffer,"%d\n",CIparam.mode);
				send(client_fd,buffer, strlen(buffer),0);
				sleep(1);

				fscanf(backup_fd,"%d",&CIparam.captureTime);

				if(CIparam.mode) {
					printf("(D) Sampling time: %d\n",CIparam.captureTime); //in senconds

					buffer[0]='\0';
					sprintf(buffer,"%d\n",CIparam.captureTime);
					send(client_fd,buffer, strlen(buffer),0);
					sleep(1);
				}

				fscanf(backup_fd,"%d",&CIparam.fileSelection);
				printf("(D) File format: %d\n",CIparam.fileSelection); //1 == TXT, 2 == CSV

				buffer[0]='\0';
				sprintf(buffer,"%d\n",CIparam.fileSelection);
				send(client_fd,buffer, strlen(buffer),0);
				sleep(1);


				buffer[0]='\0';
				recv_bytes = recv(client_fd,buffer,BUFFER_SIZE,0);
				buffer[recv_bytes] = '\0';

				printf("Introduce new parameters?: %d\n",atoi(buffer));

			} else {

				//File does not exists and so it must be created
				if ((backup_fd = fopen("./BACKUP/backupI2C.txt","w+")) == NULL ){
					printf("ERROR: cannot open defautlI2C.txt\n");
					close(client_fd);
					close(socket_fd);
					exit(-1);
					//Open for reading and writing (creates new file, if it does not exist yet)
				};

				printf("No backupI2C.txt file detected...\n");

				buffer[0]='\0';
				strcpy(buffer,"0\n");
				send(client_fd,buffer,strlen(buffer),0);

				//The file is empty, new data should be supplied

				buffer[0]='\0';
				recv_bytes = recv(client_fd,buffer,BUFFER_SIZE,0);
				buffer[recv_bytes] = '\0';
				printf("  ...a new one should be done? %d\n",atoi(buffer));
			}

			if(atoi(buffer) == 1) {
				fseek(backup_fd,0,SEEK_SET);
				//== 1 new parameters form the user
				//== 2: use loaded backup parameters
				//Order: devAddres, mode, sampling time and file selection
				printf("Ready to receive new configuration:\n");

				buffer[0]='\0';
				recv_bytes = recv(client_fd,buffer,BUFFER_SIZE,0);
				buffer[recv_bytes] = '\0';

				if (atoi(buffer) == 2){
					CIparam.i2cName = "/dev/i2c-1";
				} else {
					CIparam.i2cName = "/dev/i2c-2";
				}
				printf("NEW I2C bus: %s\n", CIparam.i2cName);
				fprintf(backup_fd,"%d\n",atoi(buffer));

				buffer[0]='\0';
				recv_bytes = recv(client_fd,buffer,BUFFER_SIZE,0);
				buffer[recv_bytes] = '\0';
				strcpy(CIparam.devAddress, buffer);
				printf("NEW Device address: %s", CIparam.devAddress);
				fprintf(backup_fd,"%s",CIparam.devAddress);

				buffer[0]='\0';
				recv_bytes = recv(client_fd,buffer,BUFFER_SIZE,0);
				buffer[recv_bytes] = '\0';
				CIparam.mode = atoi(buffer);
				printf("NEW Mode: %d\n",CIparam.mode);
				fprintf(backup_fd,"%d\n",CIparam.mode);

				if (CIparam.mode == 1){

					buffer[0]='\0';
					recv_bytes = recv(client_fd,buffer,BUFFER_SIZE,0);
					buffer[recv_bytes] = '\0';
					CIparam.captureTime = atoi(buffer);
					printf("NEW Sampling time: %d\n",CIparam.captureTime);
					fprintf(backup_fd,"%d\n",CIparam.captureTime);

				} else {
					CIparam.captureTime = 0;
					fprintf(backup_fd,"%d\n",0);
				}

				buffer[0]='\0';
				recv_bytes = recv(client_fd,buffer,BUFFER_SIZE,0);
				buffer[recv_bytes] = '\0';

				CIparam.fileSelection = atoi(buffer);
				printf("NEW File selected: %d\n",CIparam.fileSelection);
				fprintf(backup_fd,"%d\n",CIparam.fileSelection);
			};

			fclose(backup_fd);
			printf("backupI2C.txt file closed.\n");
			break;
		}

		//Load new driver
		printf("Creating driver...");
		CIparam.driver = pruio_new(CIparam.avg, CIparam.OpDelay, CIparam.SampDelay ,1); //New driver for the PRUSS
		if ((CIparam.driver->Errr) != NULL) {
			printf("ERROR: Initialization failed %s \n", CIparam.driver->Errr);
			exit(-1);
		};

		pruio_gpio_set(CIparam.driver,LED,PRUIO_OUT0,PRUIO_LOCK_CHECK);
		pruio_gpio_set(CIparam.driver,CaptureButton,PRUIO_IN,PRUIO_LOCK_CHECK);
		pruio_gpio_set(CIparam.driver,StopButton,PRUIO_IN,PRUIO_LOCK_CHECK);

		printf("  ...Driver ready.\n");
		//The CI configuration is loaded. Now execute the corresponding function

		program_conf.CI_conf = &CIparam;

		if (executeCI(ptr_program_conf) != 0){
			printf("ERROR: could not execute program\n");
			exit(-1);
		};

		usleep(100);
		printf("Reading process finished.\n");
		//Reading routine completed

		//Waiting for the user
		buffer[0]='\0';
		recv_bytes = recv(client_fd,buffer,BUFFER_SIZE,0);
		buffer[recv_bytes] = '\0';

		printf("Again?: %d\n\n",atoi(buffer));
		exit_program = atoi(buffer);

	} while(exit_program != 0);


END_PROGRAM:

	buffer[0]='\0';
	recv_bytes = recv(client_fd,buffer,BUFFER_SIZE,0);
	buffer[recv_bytes] = '\0';

	close(client_fd);
	close(socket_fd);

	if (atoi(buffer)) {
		system ("shutdown -h 1");
	}
	printf("\n\nEXIT PROGRAM\n");
	return 0;
}

int executeCI (_program_parameters *param) {

	_CI_parameters *ptrCIconf = param->CI_conf;
	int i;
	struct timeval timer;
	double secDiff;

	  if(pruio_config(ptrCIconf->driver,0,ptrCIconf->mask,0,0,ptrCIconf->clkDiv)){
		  printf("ERROR: %s \n",pruio_config(ptrCIconf->driver,0,ptrCIconf->mask,0,1,ptrCIconf->clkDiv));
	  };

	printf("\nDriver loaded, now reading the ");
	switch(param->instr){
	case 1:
		printf("A/D converter.\n------------------\n");
	//A/D converter selected///////////////////////////////////
		//File configuration
		  switch(ptrCIconf->fileSelection) {
		  case 1:
			  if(ADCfiles_txt(ptrCIconf)) {
				  printf("ERROR: file not created \n");
				  return -1;
			  };
			  break;

		  case 2:
			  if(ADCfile_csv(ptrCIconf)) {
				  printf("ERROR: file not created \n");
				  return -1;
			  };
			  break;
		  };

		  printf("A/D converter: ");
		//Start sampling
		  switch(ptrCIconf->mode) {
		    case 1: //Automatic
			  printf("Automatic mode\n");

			  while ((ptrCIconf->driver->Gpio[0].Stat & 0x8000000) == 0) {
				usleep(100);
			};

			usleep(100); //Avoid bouncing

			if((ptrCIconf->driver->Gpio[0].Stat & 0x8000000) != 0) {
				printf("Sampling starts\n");
				setTimmer(ptrCIconf);

				if(ptrCIconf->fileSelection == 1){
					while(!end) {
						pruio_gpio_out(ptrCIconf->driver,LED,1); //Turn on LED -> sampling data

						gettimeofday(&timer,NULL);
						ptrCIconf->usect1 = timer.tv_usec;

						if (ptrCIconf->firstTime == 1){
							ptrCIconf->totalTime = 0;
						}else{
							if((secDiff = (double)(ptrCIconf->usect1 - ptrCIconf->usect0)) < 0) {
								ptrCIconf->totalTime += (double)(secDiff/1000000 + 1);
							} else {
								ptrCIconf->totalTime += (double)(secDiff/1000000);
							}
						}

						readADC_txt(ptrCIconf);

						ptrCIconf->usect0 = timer.tv_usec;
						ptrCIconf->firstTime = 0;

						pruio_gpio_out(ptrCIconf->driver,LED,0); //Turn off LED -> EOC
					};
				} else	if (ptrCIconf->fileSelection == 2) {

					while(!end) {
						pruio_gpio_out(ptrCIconf->driver,LED,1); //Turn on LED -> sampling data

						gettimeofday(&timer,NULL);
						ptrCIconf->usect1 = timer.tv_usec;

						if (ptrCIconf->firstTime == 1){
							ptrCIconf->totalTime = 0;
						}else{
							if((secDiff = (double)(ptrCIconf->usect1 - ptrCIconf->usect0)) < 0) {
								ptrCIconf->totalTime += (double)(secDiff/1000000 + 1);
							} else {
								ptrCIconf->totalTime += (double)(secDiff/1000000);
							}
						}
						usleep(1);
						readADC_csv(ptrCIconf);

						ptrCIconf->usect0 = timer.tv_usec;
						ptrCIconf->firstTime = 0;

						pruio_gpio_out(ptrCIconf->driver,LED,0); //Turn off LED -> EOC
					};
				};
			};

			//Close the files generated
			if (ptrCIconf->fileSelection == 1) {
				for(i=0;i<ptrCIconf->Nchannels;i++){
					close(ptrCIconf->dstfile[i]);
				}
			} else {
				close (ptrCIconf->dstfilecsv);
			};
			printf("Data file/s generated\n");

			break;

		  case 2: //Manual
			  printf("Manual mode\n");
			do {
				if ((ptrCIconf->driver->Gpio[0].Stat & 0x8000000) != 0) {
					usleep(100); //Avoid bounce of the button

					while ((ptrCIconf->driver->Gpio[0].Stat & 0x8000000) != 0) {
						if(ptrCIconf->fileSelection == 1){
								pruio_gpio_out(ptrCIconf->driver,LED,1); //Turn on LED -> sampling data
								readADC_txt(ptrCIconf);
								pruio_gpio_out(ptrCIconf->driver,LED,0); //Turn off LED -> EOC

						} else if (ptrCIconf->fileSelection == 2) {
								pruio_gpio_out(ptrCIconf->driver,LED,1); //Turn on LED -> sampling data
								readADC_csv(ptrCIconf);
								pruio_gpio_out(ptrCIconf->driver,LED,0); //Turn off LED -> EOC
						};
					};
					printf("out of loop\n");
				};
			} while((ptrCIconf->driver->Gpio[1].Stat & 0x8000) == 0);
			break;
		  }
		  //Close the files generated
			if (ptrCIconf->fileSelection == 1) {
				for(i=0;i<ptrCIconf->Nchannels;i++){
					close(ptrCIconf->dstfile[i]);
				}
			} else {
				close (ptrCIconf->dstfilecsv);
			};
		  break;
//End of A/D converter section/////////////////////////////////////////
//I2C communication////////////////////////////////////////////////////
	case 2:
		printf("I2C bus: ");
		ptrCIconf->ptrdevAdd = &ptrCIconf->devAddress;
		ptrCIconf->selectedDev = strtol(ptrCIconf->ptrdevAdd,NULL,0);

		if (ptrCIconf->fileSelection == 1) {
			if (I2Cfile_txt(ptrCIconf) != 0) {
				printf("ERROR: destination file not opened\n");
				return -1;
			};
		} else if (ptrCIconf->fileSelection == 2){
			if (I2Cfile_csv(ptrCIconf) != 0) {
				printf("ERROR: destination file not opened\n");
				return -1;
			};
		};

		if (openI2C(ptrCIconf) < 0) {
			printf("ERROR: I2C communication not open\n");
			return -1;
		};

		switch(ptrCIconf->mode) {
			case 1: //Automatic mode: set alarm and keep fetching data!
				printf("Automatic mode\n");
				while ((ptrCIconf->driver->Gpio[0].Stat & 0x8000000) == 0) {
					usleep(100);
				};

				printf("Sampling starts\n");
				setTimmer(ptrCIconf);

				while (!end) {
					pruio_gpio_out(ptrCIconf->driver,LED,1); //Turn on LED -> sampling data
					if (readI2C(ptrCIconf) < 0) {
						printf("ERROR:readi2c\n");
						return -1;
					};
					pruio_gpio_out(ptrCIconf->driver,LED,0); //Turn off LED -> EOC
				};
				break;
			case 2://Manual mode. use Capture and Stop buttons
				printf("Manual mode\n");
				do {
					if ((ptrCIconf->driver->Gpio[0].Stat & 0x8000000) != 0) {
						usleep(100); //Avoid bounce of the button

						//Now if the event was not accidental...
						while ((ptrCIconf->driver->Gpio[0].Stat & 0x8000000) != 0) {

							pruio_gpio_out(ptrCIconf->driver,LED,1); //Turn on LED -> sampling data
							if (readI2C(ptrCIconf) < 0) {
								printf("ERROR:readi2c\n");
								return -1;
							};
							pruio_gpio_out(ptrCIconf->driver,LED,0); //Turn off LED -> EOC

							//Fetches data from value[] and store it in buffer1
						};
					};
				} while((ptrCIconf->driver->Gpio[1].Stat & 0x8000) == 0);
				break;
		};

		break;
	};

	//Send generated file to the client program, in the remote computer
	if((param->instr == 1) & (ptrCIconf->fileSelection == 1)) {
	//A/D converter TXT files
		for(i=0;i<ptrCIconf->Nchannels;i++){
			if (sendFileCIBBB(param,i) != 0){
				printf("ERROR: file not send\n");
				exit(-1);
			}
		};
	} else {
		//A/D converter CSV file, I2C TXT and CSV files
		if (sendFileCIBBB(param,1) != 0){
			printf("ERROR: file not send\n");
			exit(-1);
		}
	};

	pruio_destroy(ptrCIconf->driver);
	return 0;
};


