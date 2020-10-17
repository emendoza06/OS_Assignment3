#Description: Main program reads from a list of palindromes from a file (one strig per line) into shared memory and forks off processes. Each child tests the string at the index assigned to it and writes the string into the appropriate file named palin.out and nopalin.out. In addition, processes writes into a log file containing PID Index String. Uses semaphores solution. 

#Invoking the solution master will take the command line options: master -h master [-n x] [-s x] [-t time] infile

Run the following Makefile arguments: make clean - removes built objects make - updates targets master and palin

#Meets the requirements: 
-Reads from file line by line
-Determines if a palinedrome or not and writes to appropriate file
-Signal handlers work
-output.log written to correctly
-Uses semaphores
-Frees memory


#Outstanding Issues: 
Noticed a segmentation fault after a certain amount of text in file read 
