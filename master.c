/* 	NAME: 		Epharra Mendoza
	DATE: 		10/16/2020
	COURSE:		CMPSCI 4760-001
	ASSIGNMENT:	3
	FILENAME:	master.c
	DESCRIPTION:	The main process that spawns the child processes and waits for them to finish */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <stdbool.h>
#include <sys/sem.h>




/*=============================================================================*/
/*=============================================================================*/
/*			GLOBALS AND PROTOTYPES				       */
/*=============================================================================*/
/*=============================================================================*/


#define LSIZ 128			//List size of file strings
#define PERMS (S_IRUSR | S_IWUSR)	//Shared memory permissions

//Shared memory global
typedef struct{
	char words[64][64]; 		//Assuming 20 words in a file with length 256 max

	//memory attributes
	pid_t pgid;			//Group pid
	unsigned int sem_key;		//semaphore key
	unsigned int sem_id;		//semaphore id
} shared_memory_data;


//Memory globals
shared_memory_data* shm_data_ptr; 	//shared memory pointer to shared memory struct
int shmid;				//shared memory segment id


//Default arguments if no user input
int max_total_cp = 4;
int concurr_children = 2; 
int forced_time_quit = 100;





/*=============================================================================*/
/*=============================================================================*/
/*                           MAIN FUNCTION                                     */
/*=============================================================================*/
/*=============================================================================*/


int main(int argc, char* argv[]){ 
	
	//memory variables
	//Use ftok to convert pathname and id value 'a' to System V IPC key
	unsigned int key = ftok("./master", 'a');
	//If key generation fails then return with error message
	if(key == -1) { 
		perror("\nERROR: Could not generate key\n");
		return 1;
	}	

/*----------------------Capturing User Input---------------------------------------*/
	//User input option char
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
			
			//Indicate the maximum total of child processes master will ever create
			case 'n':
				//user input replaces default value
				max_total_cp = atoi(optarg);
				//set upper bound to 20
				if(max_total_cp > 20) { 
					max_total_cp = 20;
				}
				if(max_total_cp <= 0) { 
					printf("\nERROR: total child processes must be betwen 1-20\n");
					return EXIT_FAILURE;
				}
				break;

			//Indicate the number of children allowed to exist in the system at the same time
			case 's': 
				//user input replaces default value
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



/*----------------Procedure for allocating shared memory------------------------------*/

	//Use shmgt which allocates a shared memory segment and returns identifier of System V shared memory segment associated with the value of the argument key
	//Store shared memory segment identifier into shmid
	//Sets permissions, creates a new segment, and fails if segment already exists
	shmid = shmget(key, sizeof(shared_memory_data), PERMS | IPC_CREAT | IPC_EXCL);
	//If creation of shared memory segment fails then return with error message
	if(shmid == -1) { 
		perror("\nERROR: Could not create shared memory segment");
		return 1;
	}

	//Use shmat to attach shmid's shared memory segment to the address space of the calling process
	shm_data_ptr = (shared_memory_data*) shmat(shmid, NULL, 0);
	//If memory segment could not be attached then return with error message 
	if(shm_data_ptr == (void*)-1) {
		perror("\nERROR: Could not attach shared memory");
		return 1;
	}


/*----------------Procedure for creating semaphore------------------------------*/
	//use ftok to convert pathname and id value 'b' to System V IPC key
	//Separate from shared memory key
	shm_data_ptr->sem_key = ftok("./master", 'b');
	//If key generation fails then return with error message 
	if(shm_data_ptr->sem_key == -1) { 
		perror("\nERROR: Could not generate SEM key");
		return 1;
	}

	//Get an id that can be used to access the System V semaphore with the given key
	//Sets permissions, creates a new segment, and fails if segment already exists
	shm_data_ptr->sem_id = semget(shm_data_ptr->sem_key, 1, 0666 | IPC_CREAT | IPC_EXCL);
	if(shm_data_ptr->sem_id == -1) { 
		perror("\nERROR: Failed to create semaphore identifier");
		return 1;
	}  

	
		


/*------------------Procedure for saving data from file------------------------------*/	
	
	//Capture input file containing strings to be tested
	char* fileName = argv[optind];
	//open file
	FILE *fptr;
	//If there is an issue reading from file
	if((fptr = fopen(fileName, "r")) == NULL) { 
		perror("ERROR: File not found" );
		return (-1);
	}

	//Index to words array
	int i = 0; 

	//Read strings line by line from file and store into words array
	while(fgets(shm_data_ptr->words[i],LSIZ, fptr)) {
		//Define terminating character
		shm_data_ptr->words[i][strlen(shm_data_ptr->words[i]) -1] = '\0';
		printf("%s\n", shm_data_ptr->words[i]);
		//Increase index position of words array
		i++;
	}

	//Get total amount of strings in file and save
	int total_strings = i;
	
	//close file
	fclose(fptr);
	

	printf("\nConcurr children is %d forced time quit %d", concurr_children, forced_time_quit);	

	return 0;
}
