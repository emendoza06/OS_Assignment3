/* NAME: Epharra Mendoza
DATE: 10/12/2020
CMPSCI 4760-001
ASSIGNMENT 3 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>


/*-----------------------------------GLOBALS--------------------------*/
/*--------------------------------------------------------------------*/

//Default arguments if no user input
//int max_total_cp = 4; 
int concurr_children = 2; 
int forced_time_quit = 100;


/*-------------------------MAIN--------------------------------------*/
/*--------------------------------------------------------------------*/

int main(int argc, char* argv[]){ 
	
	//Captures user input 
	int opt; 

	//Use getopt function to parse arguments 
	while((opt = getopt(argc, argv, "hn:s:t:")) !=-1){
		switch(opt){ 
			//Help option 
			case 'h': 
				printf("\n master should take in several command line options as follows:\n");
				printf("\n master -h\n");
				printf("\n master [-n x] [-s x] [-t time] infile\n");
				printf("\n -h		Describe how the project should be run and then, terminate\n");
				printf("\n -n x		Indicate the maximum total child processes master will ever create (Default 4)\n");
				printf("\n -s x		Indicate the number of children allowed to exist in the system at the same time (Default 2)\n");
				printf("\n -t time	The time in seconds after which the process will terminate, even if it has not finished. (Default 100)\n");	
				printf("\n infile	Input file containing strings to be tested.\n");
				return EXIT_SUCCESS;

			break;

			//Indicate the maximum total of child processes master will ever create
			case 'n':
				max_total_cp = atoi(optarg);
				if(max_total_cp > 20 || max_total_cp <= 0){ 
					printf("\nERROR: Total processes in system must be between 1-20\n");
					return EXIT_FAILURE;
				}
			break;

			//Indicate the number of children allowed to exist in the system at the same time
			case 's': 
				concurr_children = atoi(optarg);
				if(concurr_children <= 0){ 
					printf("\nERROR: There must be at least 1 concurrent process\n");
					return EXIT_FAILURE;
				}
			break;

			//The time in seconds after which the process will terminate, even if it has not finished 
			case 't': 
				forced_time_quit = atoi(optarg);
				if(forced_time_quit <= 0) { 
					printf("\nERROR: Time must be at least 1\n");
					return EXIT_FAILURE;
				}
			break;

			//Invalid argument case 
			default: 
				printf("\nERROR: use option -h for help.\n");
				return EXIT_FAILURE;
		}
		
	}	

	//Capture input file containing strings to be tested
	char* fileName = argv[optind];

	printf("\nMax total cp is %d concurr children is %d forced time quit %d", max_total_cp, concurr_children, forced_time_quit);	

	return 0;
}
