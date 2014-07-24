/*
    CI_BBB.h - Communication Interface library
	===========================================
    Declaration of data types and functions to support the
    Communication Interface program.

	Compilation process, in the same folder where files CI_BBB.c and CI_BBB.h are:
	(1) Create the object file: gcc -Wall -static -c CI_BBB.c -o CI_BBB.o /usr/local/lib/freebasic/fbrt0.o -lpruio -L"/usr/local/lib/freebasic/" -lfb -lpthread -lprussdrv -ltermcap -lsupc++ -Wno-unused-variable
	(2) Generate the static library file libCI_BBB.a: ar -rcs libCI_BBB.a CI_BBB.o

	To include in other files:
	* In the file: #include "CI_BBB.h"
	* In the gcc call: -l -lCI_BBB

    Author: Javier Perez de Frutos
    E-mail: javier.pdefrutos(at)hotmail.com
*/

#ifndef CI_BBB_H_
#define CI_BBB_H_

#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <pruio_c_wrapper.h>
#include <pruio_pins.h>


#define MAX_CLIENTS 2
#define BUFFER_SIZE 256
#define fs 100000 //Sampling frequency (100 kHz) NOT ADC frequency!!
#define CaptureButton P8_17 //Pin 19 of header 8
#define StopButton P8_15 //Pin 15 of header 8
#define LED P8_19
#define i2cBUFF 100

struct CI_parameters{

		//Common
		FILE *dstfilecsv;
		FILE *dstfile[7]; //One file per channel, 7 at the most
		char sendFile[7][100]; //Directions of used files
		sigset_t *signalSet;
		PruIo *driver;
		unsigned int captureTime;
		struct sigaction samplingAlarm;
		int mode;
		int fileSelection;
		time_t usect0, usect1, sect0, sect1;
		double totalTime;
		int firstTime;

		//ADC specific
		int Nchannels;
		int channels[7];
		int avg;
		int clkDiv;
		int mask;
		int OpDelay;
		int SampDelay;

		//I2C specific
		FILE *dstfileI2C;
		int i2cFile,selectedDev;
		char devAddress[10];
		char *ptrdevAdd;
		char *i2cName;

	} typedef _CI_parameters;

struct program_parameters {
	int socket;
	int instr;
	struct CI_parameters *CI_conf;
} typedef _program_parameters;

extern int end;

extern int setTimmer(_CI_parameters *arg);
//Configures the sampling time alarm

extern int gen_mask(_CI_parameters *arg);
//Generates a mask to fetch the information from the desired channels of the A/D converter

extern int readADC_txt(_CI_parameters *arg);
//Writes text file with the output of the A/D converter

extern int readADC_csv(_CI_parameters *arg);
//Writes comma separated values file with the output of the A/D converter

extern int ADCfiles_txt(_CI_parameters *arg);
//Opens and configures the text output file for the A/D converter

extern int ADCfile_csv(_CI_parameters *arg);
//Opens and configures the comma separated values output file for the A/D converter

extern int openI2C(_CI_parameters *arg);
//Open I2C file for communicate with the bus

extern int I2Cfile_csv(_CI_parameters *arg);
//Opens and configures the comma separated values output file for the I2C bus

extern int I2Cfile_txt(_CI_parameters *arg);
//Opens and configures the text output file for the I2C bus

extern int readI2C(_CI_parameters *arg);
//Writes output file with the values read from the I2C bus

int sendFileCIBBB(_program_parameters *serverConf, int i);
//Send generated file to the remote computer (running CIprogram)

extern void end_sampling (int in);
//Handles the end of sampling time alarm

#endif /* CI_BBB H_ */
