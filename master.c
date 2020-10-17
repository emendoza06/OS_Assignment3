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
	char words[64][64]; 		//Array holding strings from file
	unsigned int children_count;	//The amount of child processes

	//memory attributes
	pid_t pgid;			//Group pid
	unsigned int sem_key;		//semaphore key
	unsigned int sem_id;		//semaphore id
} shared_memory_data;


//Memory globals
shared_memory_data* shm_data_ptr; 	//shared memory pointer to shared memory struct
unsigned int shmid;			//shared memory segment id
int spawn_count;			//Tracks the amount of times a spawn was tried
int child_index;				//index tracker
int curr_processes_running = 0;		//Tracks the current amount of processes running


//Default arguments if no user input
int max_total_cp = 4;			//Maximum total amount of child processes to exist
int max_concurr_children = 2; 		//Number of children allowed to exist at the same time
int forced_time_quit = 100;		//Time in seconds when the process will termiante


//Prototypes
void check_before_run_child(int index);	//Check that a child can be spawned
void run_child(int index);		//Spawns and sends to palin
void free_shared_mem();			//Frees shared memory
void free_sem();			//Frees semephore
void sighandle(int signal);		//Function that receives signal
void run_timer(int seconds);		//Function that tracks time
void sighandle_time(int signal);	//Function that receives time signal



/*=============================================================================*/
/*=============================================================================*/
/*                           MAIN FUNCTION                                     */
/*=============================================================================*/
/*=============================================================================*/


int main(int argc, char* argv[]){ 

/*----------------------Signal Handler------------------------------------------*/

	signal(SIGINT, sighandle);


/*---------------------Create shared memory key----------------------------------*/
	
	//memory variables
	//Use ftok to convert pathname and id value 'a' to System V IPC key
	unsigned int key = ftok("./master", 'a');
	//If key generation fails then return with error message
	if(key == -1) { 
		perror("\nERROR: Could not generate key\n");
		exit(1);
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
				//Error if child processes <= 0
				if(max_total_cp <= 0) { 
					printf("\nERROR: total child processes must be betwen 1-20\n");
					return EXIT_FAILURE;
				}
				break;


			//Indicate the number of children allowed to exist in the system at the same time
			case 's': 
				//user input replaces default value
				max_concurr_children = atoi(optarg);
				//Error if concurrent child proceses <= 0
				if(max_concurr_children <= 0){ 
					printf("\nERROR: There must be at least 1 concurrent process\n");
					return EXIT_FAILURE;
				}
			break;

			
			//The time in seconds after which the process will terminate, even if it has not finished 
			case 't': 
				//User input replaces default
				forced_time_quit = atoi(optarg);
				//Error if seconds is <= 0 
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
		exit(1);
	}

	//Use shmat to attach shmid's shared memory segment to the address space of the calling process
	shm_data_ptr = (shared_memory_data*) shmat(shmid, NULL, 0);
	//If memory segment could not be attached then return with error message 
	if(shm_data_ptr == (void*)-1) {
		perror("\nERROR: Could not attach shared memory");
		exit(1);
	}




/*----------------Procedure for creating semaphore------------------------------*/

	//use ftok to convert pathname and id value 'b' to System V IPC key
	//Separate from shared memory key
	shm_data_ptr->sem_key = ftok("master.c", 'J');
	//If key generation fails then return with error message 
	if(shm_data_ptr->sem_key == -1) { 
		perror("\nERROR: Could not generate SEM key");
		exit(1);
	}

	//Get an id that can be used to access the System V semaphore with the given key
	//Sets permissions, creates a new segment, and fails if segment already exists
	shm_data_ptr->sem_id = semget(shm_data_ptr->sem_key, 1, 0666 | IPC_CREAT);
	if(shm_data_ptr->sem_id == -1) { 
		perror("\nERROR: Failed to create semaphore identifier");
		exit(1);
	}  

	//Perform control operation on System V semaphore set identified by semid
	//Sets the semaphore value. Returns with error message if value not set
	if(semctl(shm_data_ptr->sem_id, 0, SETVAL, 1) == -1) {
		perror("\nERROR: Failed to perform semaphore control operations");
		exit(1);
	}	
		



/*------------------Procedure for reading & saving data from file------------------------------*/	
	
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
	

/*------------------Start timer------------------------------------------------------------*/
	//Run timer to user's input value for time
	run_timer(forced_time_quit);



/*------------------Set value boundaries for child processes------------------------------*/	

	//The amount of concurrent child processes cannot be greater than the total amount of child processes
	if (max_concurr_children > max_total_cp){
	/*If concurrent children is greater than  total amount of child processes, reset value
	 to max total cp*/
	max_concurr_children = max_total_cp;		
	}

	//Max total children cannot exceed total amount of strings given to them to read
	if(total_strings < max_total_cp) { 
		max_total_cp = total_strings;
	}

	//Tell shared memory struct how many children there will be
	shm_data_ptr->children_count = max_total_cp;



/*-----------------Procedure for checking requirements before spawning children-------------------------*/	

	child_index = 0; 
	//Keep spawning until the spawn number reaches the allowed amount of max total child processes
	//Check that amount of children allowed to exist at the same time is not being exceeded 
	while(child_index < max_concurr_children){
		
		check_before_run_child(child_index++);
	}
	
	/*If amount of children allowed to exist at the same time is exceeded, we can still spawn
	but we need to wait for process to finish before respawning*/
	while(spawn_count > 0){
		wait(NULL);
		//Try spawning as long as we have not reached max amount of chidren allowed to spawn
		if(child_index < max_total_cp){
			check_before_run_child(child_index++);
		}
		//Requirements to spawn have not been met; do not spawn. Reduce spawn count.
		spawn_count--;
	}



/*--------------------------Procedure for releasing memory----------------------------------------------*/

	/*At  this point, the requirements above have not been met which means we have spawned all of
	the amount of children that we are allowed. Time to free memory*/
	free_shared_mem();
	free_sem();

	
	//Program ends here
	return 1;
}






/*=============================================================================*/
/*=============================================================================*/
/*                            FUNCTIONS                                        */
/*=============================================================================*/
/*=============================================================================*/



/*---------------------------Functions for spawning children---------------------------------------------*/

//Function for checking requirements before spawning
void check_before_run_child(int id){
	/*Check that processes in system is under max concurrent allowed; amount spawned is less than total 		children allowed*/
	if(((spawn_count < max_concurr_children) && (id < max_total_cp)) || ((max_concurr_children =1) && (id < max_total_cp))){
		//Proceed to run child process
		run_child(id);
	} 
	//Else requirements were not met and child cannot spawn. Decrease index by 1
	else {	
		child_index--;
	}
}

//Function for sending child to palin
void run_child(int id){
	//Increase child spawned by 1
	spawn_count++;
	//process id
	pid_t pid;
	pid = fork();
	//If fork was successful
	if(pid == 0){
		//The first instance of a child will set the group id
		if(id == 0){
			shm_data_ptr->pgid = getpid();
		}
		//Group processes together
		setpgid(0, shm_data_ptr->pgid);
		
		//Before passing index to palin, need to convert argument to a string
		char argument[256];
		sprintf(argument, "%d", id);
		//Send to palin
		execl("./palin", "palin", argument, (char*) NULL);
		exit(1);
	}
}


/*---------------------------Functions for releasing memory---------------------------------------------*/
//Free shared memory function
void free_shared_mem(){
	//If shared memory struct has data, we need to release memory
	if(shm_data_ptr != NULL){
		//If there was an error detaching memory, return with error message
		if(shmdt(shm_data_ptr) == -1){
			perror("\nERROR: Could not detach memory segment");
			exit(1);
		}
	}
	//If shared memory id exists
	if(shmid > 0) { 
		//If there was an error removing shared memory segment, return with error message
		if(shmctl(shmid, IPC_RMID, NULL) == -1) { 	//Remove ipc identifier
			perror("\nERROR: Could not remove shared memory segment");
		}
	}
}

//Free semaphore function
void free_sem(){
	//If there was an error removing semaphore, return with error message
	if(semctl(shm_data_ptr->sem_id, 0, IPC_RMID) == -1) { 
		perror("\nERROR: Could not remove semaphore");
		exit(1);
	}
}

/*----------------------------Functions for signal handling-------------------------------------------*/
//Function for parent to kill processes
void sighandle(int signal){
	//kill the children processes by group pid
	killpg(shm_data_ptr->pgid, SIGTERM);
	//Wait for children to finish
	while(wait(NULL) > 0);
	//Free memory
	free_shared_mem();
	exit(1);
}

//Timer algorithm from gnu.org
void run_timer(int seconds){
	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = &sighandle_time;
	sa.sa_flags = 0;
	if(sigaction(SIGALRM, &sa, NULL) < 0){
		perror("\nERROR: ");
		exit(1);
	}
	struct itimerval new;
	new.it_interval.tv_usec = 0;
	new.it_interval.tv_sec = 0;
	new.it_value.tv_usec = 0;
	new.it_value.tv_sec = (long int) seconds;
	if(setitimer (ITIMER_REAL, &new, NULL) < 0){
		perror("\nERROR :");
		exit(1);
	}
}

//Parent forces processes stop when time runs out
void sighandle_time(int signal){
	//kill the children processes by group id
	killpg(shm_data_ptr->pgid, SIGUSR1);
	//Wait for children to finish
	while(wait(NULL) > 0);
	//Free memory
	free_shared_mem();
	exit(1);
}
