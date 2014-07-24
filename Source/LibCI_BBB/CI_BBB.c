/*
    CI_BBB.c - Communication Interface definitions
	===========================================
    Definitions of data types and functions to support the
    Communication Interface program.

	Compilation process, in the same folder where files CI_BBB.c and CI_BBB.h are:
	(1) Create the object file: gcc -static -c CI_BBB.c -o CI_BBB.o
	(2) Generate the static library file libCI_BBB.a: ar -rcs libCI_BBB.a CI_BBB.o

	To include in other files:
	* In the file: #include "CI_BBB.h"
	* In the gcc call: -l -lCI_BBB

    Author: Javier Perez de Frutos
    E-mail: javier.pdefrutos(at)hotmail.com
*/

#include "CI_BBB.h"

#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>


int setTimmer(_CI_parameters *arg) {

	sigemptyset(&arg->samplingAlarm.sa_mask); //Sets an empty mask
	sigaction(SIGALRM, &arg->samplingAlarm, NULL); //Adds alarm signal to the signal struct

	arg->samplingAlarm.sa_handler = end_sampling;
	arg->samplingAlarm.sa_flags = 0;
	signal(SIGALRM, end_sampling);

	alarm(arg->captureTime);

	printf("Alarm ready\n");

	return 0;
};

int gen_mask(_CI_parameters *arg) {

	int maskArg = 0b000000000;
	int i;

	for(i=0; i < arg->Nchannels; i++)
		maskArg = maskArg | (0b1 << arg->channels[i]);


	return maskArg;
};

int ADCfiles_txt(_CI_parameters *arg) {

	int i,j;
	time_t t = time(NULL);
	struct tm date;
	char filePath[100] = "/root/Documents/";
	char fileName[50] = "ADC_channel_";
	char fileDate[25];
	char fileTime[10];
	char numberFile[4];

	date = *localtime(&t);
	strftime(fileDate,sizeof fileDate,"%d_%m_%Y.txt",&date);

	for (i=0; i<arg->Nchannels;i++) {

	   // PATH GENERATION

	   		sprintf(numberFile,"%d_",arg->channels[i]);
	   		strcat(fileName,numberFile);
	   		strcat(fileName,fileDate);

	   		strftime(fileTime,sizeof fileTime,"%T",&date);

	   		strcpy(arg->sendFile[i],fileName);
	   		strcat(filePath,fileName);

	   	//OPENING FILE

			arg->dstfile[i] = fopen(filePath,"a+");
			if (arg->dstfile[i] == NULL) {
				return -1;
			};

			//Write some information related to the sampled data
			fprintf(arg->dstfile[i],"======================\n");
			fprintf(arg->dstfile[i],"------ %s ------\n",fileTime);
			fprintf(arg->dstfile[i],"======================\n");
			fprintf(arg->dstfile[i],"Channel %d, Time (s)\n",i+1);

	printf("File generated: %s AIN%d\n",filePath,(arg->channels[i] - 1));
	sprintf(filePath, "/root/Documents/");
	sprintf(fileName, "ADC_channel_");
	};
	return 0;
};

int ADCfile_csv(_CI_parameters *arg) {

	int i,j;
	time_t t = time(NULL);
	struct tm date;
	char filePath[100] = "/root/Documents/";
	char fileName[20] = "ADC_";
	char fileDate[25];
	char fileTime[10];


	date = *localtime(&t);
	strftime(fileDate,sizeof fileDate,"%d_%m_%Y.csv",&date);

	// PATH GENERATION

	   		strcat(fileName,fileDate);
	   		strftime(fileTime,sizeof fileTime,"%T",&date);

	   		strcpy(arg->sendFile[1], fileName);
	   		strcat(filePath,fileName);

	   	//OPENING FILE
			arg->dstfilecsv = fopen(filePath,"a+");
			if (arg->dstfilecsv == NULL) {
				return -1;
			};

			//Write some information related to the sampled data
			fprintf(arg->dstfilecsv,"=============================\n");
			fprintf(arg->dstfilecsv,"------ %s ------\n",fileTime);
			fprintf(arg->dstfilecsv,"=============================\n");

			for (i = 0; i < arg->Nchannels; i++) {
				fprintf(arg->dstfilecsv,"Channel%d",arg->channels[i]);
				if (i == arg->Nchannels-1)
					fprintf(arg->dstfilecsv,",Time(s)\n");
				else
					fprintf(arg->dstfilecsv,",");
			};


	printf("File generated: %s \n",filePath);
	sprintf(filePath, "/root/Documents/");
	sprintf(fileName, "ADC_");
	return 0;
};

int readADC_txt(_CI_parameters *arg) {

	unsigned int i;
	struct timeval timer;
	double secDiff;

	sigprocmask(SIG_BLOCK, &(arg->samplingAlarm.sa_mask), NULL);

	for(i = 0; i < arg->Nchannels; i++){

		if (fprintf(arg->dstfile[i],"%4d,%f\n",arg->driver->Value[arg->channels[i]],arg->totalTime) == 0){
			printf("ERROR:writing file\n");
			exit(-1);
		};

	};

	sigprocmask(SIG_UNBLOCK, &(arg->samplingAlarm.sa_mask), NULL);
	return 0;

};

int readADC_csv(_CI_parameters *arg) {

	unsigned int i, j;

	sigprocmask(SIG_BLOCK, &(arg->samplingAlarm.sa_mask), NULL);

		j=0;
		for(i = 0; i < arg->Nchannels; i++){

			if ((fprintf(arg->dstfilecsv,"%d",arg->driver->Value[arg->channels[i]])) == 0){
				printf("ERROR:writing file\n");
				exit(-1);
			};

			j++;
			if(j == arg->Nchannels){
				j = 0;
				fprintf(arg->dstfilecsv,",%f\n",arg->totalTime);
			} else {
				fprintf(arg->dstfilecsv,",");
			};
	};

	sigprocmask(SIG_UNBLOCK, &(arg->samplingAlarm.sa_mask), NULL);
	return 0;

};

int openI2C(_CI_parameters *arg) {

	if ((arg->i2cFile = open(arg->i2cName, O_RDWR)) < 0){
		printf("ERROR: opening I2C bus\n");
		return -1;
	};

	if ((ioctl(arg->i2cFile,I2C_SLAVE,arg->selectedDev)) < 0){
		printf("ERROR: unable to communicate with slave device\n");
		return -1;
	};
	//Open bus and sets the remote device as slave

	return 0;
};

int readI2C(_CI_parameters *arg) {

	int bytes_read,position,gainControl,magnitude;
	int temperature,MagIncDec,LinAlarm,cordic,offset,newData;
	int aux,i;
	struct timeval timer;
	double secDiff;

	//usleep(1);
	//Each reading process gets 5 bytes of information
	sigprocmask(SIG_BLOCK, &(arg->samplingAlarm.sa_mask), NULL);

	gettimeofday(&timer,NULL);
	arg->usect1 = timer.tv_usec;
	arg->sect1 = timer.tv_sec;

	if (arg->firstTime == 1){
		arg->totalTime = 0;
	}else{
		if((double)(difftime(arg->sect1,arg->sect0)) == 0) {
			arg->totalTime += (double)((difftime(arg->usect1,arg->usect0))/1000000);
		} else {
			arg->totalTime += (double)((secDiff) + (1+(difftime(arg->usect1,arg->usect0))/1000000));
		}
	}

	bytes_read = read(arg->i2cFile,(int)&newData,5);

	if(bytes_read < 0) {
		printf("ERROR: reading I2C device\n");
		printf("%s \n",strerror(errno));
		//	return -1;
	} else {

		temperature = (newData >> 32) & 0x00000000FF;

		magnitude = (newData >> 24) & 0x00000000FF;

		gainControl = (newData >> 16) & 0x00000000FF;

		MagIncDec = (newData >> 15) & 0x01;

		LinAlarm = (newData >> 14) & 0x01;

		cordic = (newData >> 13) & 0x01;

		offset = (newData >> 12) & 0x01;

		position = (newData & 0x0000000FFF);

		if(fprintf(arg->dstfileI2C,"%d,%d,%d,%d,%d,%d,%d,%d,%f\n",position,offset,cordic,LinAlarm,MagIncDec,gainControl,magnitude,temperature,arg->totalTime) < 0) {
			printf("ERROR: writing output file\n");
			return -1;
		}

	}

	arg->sect0 = timer.tv_sec;
	arg->usect0 = timer.tv_usec;
	arg->firstTime = 0;
	sigprocmask(SIG_UNBLOCK, &(arg->samplingAlarm.sa_mask), NULL);
	return 0;
};

int I2Cfile_csv(_CI_parameters *arg) {

	time_t t = time(NULL);
	struct tm date;
	char filePath[50] = "/root/Documents/";
	char fileName[30] = "I2C_values_";
	char fileDate[25];
	char fileTime[10];
	int j;

	// PATH GENERATION

	date = *localtime(&t);
	strftime(fileDate,sizeof fileDate,"%d_%m_%Y.csv",&date);
	strcat(fileName,fileDate);

	strftime(fileTime,sizeof fileTime,"%T",&date);

	strcpy(arg->sendFile[1], fileName);
	strcat(filePath,fileName);

	//OPENING FILE
	arg->dstfileI2C = fopen(filePath,"a+");
	if (arg->dstfileI2C == NULL) {
	return -1;
	};

	//Write some information related to the sampled data
	fprintf(arg->dstfileI2C,"=============================\n");
	fprintf(arg->dstfileI2C,"------ %s ------\n",fileTime);
	fprintf(arg->dstfileI2C,"=============================\n");
	fprintf(arg->dstfileI2C,"Data,Offset,CORDIC overflow,Linearity alarm,Magnitude Inc/Dec,Gain Control,MSB magnitude,MSB temperature,Time(s)\n");

	printf("File generated: %s \n",filePath);
	sprintf(filePath, "/root/Documents/");
	sprintf(fileName, "I2C_values_");
	return 0;
};

int I2Cfile_txt(_CI_parameters *arg) {

	time_t t = time(NULL);
	struct tm date;
	char filePath[50] = "/root/Documents/";
	char fileName[30] = "I2C_values_";
	char fileDate[25];
	char fileTime[10];
	int j;

	// PATH GENERATION

	date = *localtime(&t);
	strftime(fileDate,sizeof fileDate,"%d_%m_%Y.txt",&date);
	strcat(fileName,fileDate);

	strftime(fileTime,sizeof fileTime,"%T",&date);

	strcpy(arg->sendFile[1], fileName);
	strcat(filePath,fileName);

	//OPENING FILE
	arg->dstfileI2C = fopen(filePath,"a+");
	if (arg->dstfileI2C == NULL) {
	return -1;
	};

	//Write some information related to the sampled data
	fprintf(arg->dstfileI2C,"=============================\n");
	fprintf(arg->dstfileI2C,"------ %s ------\n",fileTime);
	fprintf(arg->dstfileI2C,"=============================\n");
	fprintf(arg->dstfileI2C,"Values,Time(s)");

	printf("File generated: %s \n",filePath);
	sprintf(filePath, "/root/Documents/");
	sprintf(fileName, "I2C_values_");
	return 0;
};

int sendFileCIBBB(_program_parameters *serverConf, int i) {

	_CI_parameters *arg = serverConf->CI_conf;
	char file_path[100] = "/root/Documents/";
	int file_fd, attemp, bytes_sent, retValue;
	char buffer[BUFFER_SIZE];
	struct stat file_info;

	strcat(file_path,arg->sendFile[i]);
	file_fd = open(file_path,O_RDONLY);

	if(fstat(file_fd,&file_info) != 0){
		printf("ERROR: fstat: %s\n",strerror(errno));
		return -1;
	};

	//Send to the client: length of the file and name of the file
	buffer[0] = '\0';
	sprintf(buffer,"%d-%s",file_info.st_size,arg->sendFile[i]);
	send(serverConf->socket,buffer,strlen(buffer),0);
	sleep(1); //Give time to the client to prepare the output file

	printf("Sending file: %s\n",arg->sendFile[i]);
	//Send the file
	attemp=0;
	while((bytes_sent = sendfile(serverConf->socket,file_fd,NULL,file_info.st_size)) <= 0){
		printf("ERROR: file not sent. Attempt no. %d   %s\n",attemp,strerror(errno));
		sleep(1);
		(attemp>=10)?(retValue = -1):(attemp++);
	};
	close(file_fd);
	printf("File sent. Total bytes: %d\n",bytes_sent);
	sleep(1);
	retValue = 0;
	return retValue;
};

void end_sampling (int in){

	signal (in, SIG_IGN);
	end = 1;
	//Ignore alarm
	return;

};

