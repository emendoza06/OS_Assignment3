/*	NAME:		Epharra Mendoza
 	DATE:		10/16/2020
	COURSE:		CMPSCI 4760-001
	ASSIGNMENT:	3
	FILENAME:	palin.c
	DESCRIPTION:	Child process identifies palindromes; writes to palin.out or nopalin.out; writes to
			output log
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <getopt.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <sys/sem.h>


/*=============================================================================*/
/*=============================================================================*/
/*                    GLOBALS AND PROTOTYPES                                   */
/*=============================================================================*/
/*=============================================================================*/


#define PERMS (S_IRUSR | S_IWUSR)	//Shared memory permissions

//Shared memory global
typedef struct{
	char words[64][64];		//Array holding strings from file
	unsigned int children_count;	//The amount of child processes

	//memory attributes
	pid_t pgid;			//Group pid
	unsigned int sem_key;		//semaphore key
	unsigned int sem_id;		//semaphore id
} shared_memory_data;


//Memory globals
shared_memory_data* shm_data_ptr;	//shared memory pointer to shared memory struct
unsigned int shmid;			//shared memory segment id

struct sembuf sem_obtain = {0, -1, 0};	/*Obtain one unit from semaphore, load sembuf array to
				//	perform the operation*/
struct sembuf sem_return = {0, +1, 0};	//Return resources back to the semaphore set

//Globals for children
int child_id;				//holds id of current child
time_t now;				//holds current time

//Prototypes
bool isPalindrome(char str[]);		//Returns true/false if a palinedrome


/*=============================================================================*/
/*=============================================================================*/
/*                              MAIN                                           */
/*=============================================================================*/
/*=============================================================================*/

int main(int argc, char* argv[]){ 


/*--------------------------Procedure for reading passed arguments------------------*/

	//Index of words array
	int index;

	//If spawn number was not passed into palin from master
	if(argc < 2){
		perror("\nERROR: Missing argument");
		exit(1);
	}
	//Spawn number was provided
	else{
		//Save passed argument
		child_id = atoi(argv[1]);	
		index = child_id - 1;		//Index starts at 0, not 1	
	}



/*------------------------Procedure for accessing memory key----------------------*/

	//Use ftok to convert pathname and id value 'a' to System V IPC key, will be the same as master
	unsigned int key = ftok("./master", 'a');
	//If key generation fails then return with error message 
	if(key == -1) {
		perror("\nERROR: Could not access master key");
		exit(1);
	}



/*----------------Procedure for allocating shared memory------------------------------*/

	//Use shmgt which allocates a shared memory segment and returns identifier of System V shared memory segment associated with the value of the argument key
	//Store shared memory segment identifier into shmid
	//Sets permissions, creates a new segment, and fails if segment already exists
	shmid = shmget(key, sizeof(shared_memory_data), PERMS | IPC_CREAT);
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


/*------------------Print process that wants access to critical section------------------*/

	printf("\nProcess ID: %d, PID: %d wants access to critical section.	 %s\n", child_id, getpid(), ctime(&now));



/*----------------------Check if a palidrome--------------------------------------------*/

	bool palin = isPalindrome(shm_data_ptr->words[index]);



/*---------------------Procedure to for semaphore wait----------------------------------------*/
	struct sembuf sem_op;
	//Wait on semaphore
	sem_op.sem_num = 0; 
	sem_op.sem_op = -1;
	sem_op.sem_flg = 0;
	semop(shm_data_ptr->sem_id, &sem_op, 1);


/*------------------------Procedure for sleeping between 0-2 seconds--------------------------*/

	srand((unsigned) time(&now));	//seed for random sleep time
	sleep(rand() % (3));		//sleep between 0-2 seconds



/*-----------------------Procedure for writing to palin.out/nopalin.out and logfile-----------*/
	//Process now in critical section
	printf("\nProcess %d is in critical section %s", child_id, ctime(&now));

	//Open file and write to palin.out/nopalin.out depending on bool returned from isPalindrome
	FILE *palin_file = fopen(palin ? "palin.out" : "nopalin.out", "a+"); //append to file

	if(palin_file == NULL) {
		perror("\nERROR: Could not palindrome file");
		exit(1);
	}
	//Write word to file at index position
	fprintf(palin_file, "%s\n", shm_data_ptr->words[index]);
	printf("Wrote %s to file\n", shm_data_ptr->words[index]);
	//Close file
	fclose(palin_file);

	//Write to log file
	FILE *out_file = fopen("output.log", "a+");
	if(out_file == NULL) {
		perror("\nERROR: Could not open output log");
		exit(1);
	}
	//Write to output.log
	fprintf(out_file, "%d %d %s %s", getpid(), child_id, shm_data_ptr->words[index], ctime(&now));
	//close file
	fclose(out_file);



/*---------------------Print process that wants out of critical section---------------------------*/
	printf("\nProcess ID: %d, PID: %d exiting critical section.	%s\n", child_id, getpid(), ctime(&now));
	



/*----------------------Signal the semaphore free--------------------------------------------------*/
	//Return resources back to semaphore
	sem_op.sem_num = 0 ;
	sem_op.sem_op = 1;
	sem_op.sem_flg = 0;
	semop(shm_data_ptr->sem_id, &sem_op, 1);
	
	
	//Program ends here
	return 0;
}










/*=============================================================================*/
/*=============================================================================*/
/*                            FUNCTIONS                                        */
/*=============================================================================*/
/*=============================================================================*/


/*------------------------Function that checks if a palindrome----------------------------*/
//Algorithm from geeksforgeeks.com
bool isPalindrome(char str[]){
	bool palin = true;
	int l = 0;
	int h = strlen(str) -1;
	while(h > l){
		if(str[l++] != str[h--]){
			return false;
		}
	}
	return true;
}
