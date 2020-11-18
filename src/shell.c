#include "shell_util.h"
#include "linkedList.h"
#include "helpers.h"

// Library Includes
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>

int reap[99999];
static int reap_counter = 0;
List_t bg_list;

void mcopy(char* src, char* dest, size_t n) {
    int i;
    for (i=0; i<n; i++) 
        *(dest+i) = *(src+i);
		
	 *(dest+n) = '\0';
}


int TimeComparator(void *node1, void *node2){
	time_t a;
	time_t b;
	ProcessEntry_t* temp1 = node1;
	ProcessEntry_t* temp2 = node2;
	a = temp1->seconds;
	b = temp2->seconds;

	if(a<b)
	{
		return -1;
	}
	else if (a>b)
	{
		return 1;
	}
	else
    return 0;
}	

void chd_handler() 
{
  pid_t childpid;
  //while (waitpid(-1, NULL, WNOHANG) > 0) 
  while ( (childpid = waitpid(-1, NULL, WNOHANG)) > 0) 
  {
  	reap[reap_counter] = childpid;
  	//printf("ChildPID is: %d.\n", reap[0]);
  	reap_counter++;
  }
}

static void usr_handler(int sig) 
{
	
	if(sig == SIGUSR1)
	{
		node_t* itr = bg_list.head;
		while(itr)
		{
			ProcessEntry_t * tempL = itr->value;
			printBGPEntry(tempL);
			itr = itr->next;
		}
		
		 
	}
}

int main(int argc, char *argv[])
{
	int i; //loop counter
	char *args[MAX_TOKENS + 1];
	int exec_result;
	int exit_status;
	int erroFlag = 0;
	pid_t pid;
	pid_t wait_result;
    //List_t bg_list;
	
	// Redirection
	
	int fd1F,fd2F,fd3F,fd4F;
	//pipe
	int numP = 0;
	int posP1, posP2;
	//int i,fd[2],fd2[2],status;
    //Initialize the linked list
    bg_list.head = NULL;
    bg_list.length = 0;
    bg_list.comparator = NULL;  // Don't forget to initialize this to your comparator!!!
	
	// Setup segmentation fault handler
	if(signal(SIGSEGV, sigsegv_handler) == SIG_ERR)
	{
		perror("Failed to set signal handler");
		exit(-1);
	}

	while(1) {
		// DO NOT MODIFY buffer
		// The buffer is dynamically allocated, we need to free it at the end of the loop
		char * const buffer = NULL;
		size_t buf_size = 0;
		
		int stdO = dup(STDOUT_FILENO);
		int stdE = dup(STDERR_FILENO);
		int stdI = dup(STDIN_FILENO);
		int fd1,fd2,fd3,fd4;
		//intialize pipe
		numP = 0;
		posP1 = 0; 
		posP2 = 0;
		int Pflag = 0;
		int andflag = 0;
		int g,pp1[2],pp2[2], Pstatus;
		
		int rpl,rpr,rp2,rprr;
		rpl = 0;
		rpr = 0;
		rp2 = 0;
		rprr = 0;
		//---------------------------------------Signal Handler----
		signal(SIGCHLD, chd_handler);
		signal(SIGUSR1, usr_handler);
		
		// Print the shell prompt
		display_shell_prompt();
		
		// Read line from STDIN
		ssize_t nbytes = getline((char **)&buffer, &buf_size, stdin);

		// No more input from STDIN, free buffer and terminate
		if(nbytes == -1) {
			free(buffer);
			break;
		}

		// Remove newline character from buffer, if it's there
		if(buffer[nbytes - 1] == '\n')
			buffer[nbytes- 1] = '\0';

		// Handling empty strings
		if(strcmp(buffer, "") == 0) {
			free(buffer);
			continue;
		}
		
		// Parsing input string into a sequence of tokens
		size_t numTokens;
		*args = NULL;
		//----get current command-----
		char* tempBuffer = (char *)malloc(strlen(buffer)+1);
		mcopy(buffer,tempBuffer,strlen(buffer));
		//printf("This line is: %s\n", buffer);
		//---------
		numTokens = tokenizer(buffer, args);
		//printf("The first command is: %s\n", args[1]);
		
//-------------detect error--------------
		int p = 0;
		//pipe (head or tail)
		if(strcmp(args[0],"|") == 0 || strcmp(args[numTokens - 1],"|") == 0)
		{
			fprintf(stderr,PIPE_ERR);
			free(buffer);
			continue;
		}
		
		// check pipe and & and Redirection
		for(p = 0; p < numTokens; p++)
		{
			//pipe
			if(strcmp(args[p],"|") == 0 && (p - 1) >= 0 && (p + 1) < numTokens)
			{
				//check pipe left and right
				if(strcmp(args[p - 1],"|") == 0 || strcmp(args[p + 1],"|") == 0 ) //|| strcmp(args[p + 1],"&") == 0
				{
					erroFlag = 1;
					break;
				}
			}
			//check &
			if(strcmp(args[p],"&") == 0)
			{
				if (p != numTokens - 1)
				{
					erroFlag = 1;
					break;	
				}
				else
					andflag = 1;
			}
			
			if(strcmp(args[p],"|") == 0)
				{
					//args[p] = NULL; // set to NULL
					//printf("Find Pipe!\n");
					if(numP == 0)
					{
						numP = 1;
						posP1 = p;
					}
					else if(numP == 1)
					{
						numP = 2;
						posP2 = p;
					}
				}
			
			//check Redirection
			if (strcmp(args[p],"<") == 0 || strcmp(args[p],">") == 0 || strcmp(args[p],">>") == 0 || strcmp(args[p],"2>") == 0)
			{
				//check if last
				if (p == (numTokens - 1))
				{
					erroFlag = 2;
					break;
				}
				// check RHS
				if( p + 1 < numTokens)
				{
					if(strcmp(args[p+1],"&") == 0 || strcmp(args[p+1],"|") == 0 || strcmp(args[p+1],">") == 0 || strcmp(args[p+1],">>") == 0 || strcmp(args[p+1],"2>") == 0 || strcmp(args[p+1],"<") == 0)
					{
						erroFlag = 2;
						break;
					} 
				}
				//check LHS
				if( p - 1 >= 0)
				{
					if(strcmp(args[p-1],"&") == 0 || strcmp(args[p+1],">") == 0 || strcmp(args[p+1],">>") == 0 || strcmp(args[p+1],"2>") == 0 || strcmp(args[p+1],"<") == 0 || strcmp(args[p+1],"<") == 0)
					{
						erroFlag = 2;
						break;
					}
				}
			}	
		}
		//---------end check-------------
		//printf("There are %d Pipes.\n", numP);
		//check numpipe
		if(numP == 1)
		{
			//printf("Declear PP1\n");
			pipe(pp1);
		}
		else if (numP == 2)
		{
			//printf("Declear PP2\n");
			pipe(pp2);
		}
		//----check erroflag
		if(erroFlag == 1)
		{
			erroFlag = 0;
			fprintf(stderr,PIPE_ERR);
			free(buffer);
			continue;
		}
		if(erroFlag == 2)
		{
			erroFlag = 0;
			fprintf(stderr,RD_ERR);
			free(buffer);
			continue;
		}
		
//--------------end detect--------------


//------------------------------------reap children-----
		if(reap_counter > 0 )
		{
			//printf("There are %d bg, starting reap.\n", reap_counter);
			int i = 0;
			//char* tempCMD = (char *)malloc(9999);
			char* tempCMD = NULL;
			for(i = 0;i < reap_counter;i++)
			{
				//printf("First SID is %d\n", reap[0]);
				pid_t tempID = reap[i];
				//-----search list get cmd---------
				node_t* itr = bg_list.head;
				int length = bg_list.length;
				int t = 0;
				for(t = 0; t < length; t++)
				{
					ProcessEntry_t* tempE = itr->value;
					if(tempE->pid == tempID )
					{
						tempCMD = (char *)malloc(strlen(tempE->cmd)+1);
						//printf("Find!\n");
						mcopy(tempE->cmd,tempCMD,strlen(tempE->cmd));
						printf(BG_TERM,reap[i],tempCMD);
						removeByPid(&bg_list,reap[i]);
						break;
					}
					itr = itr->next;
				}
				//---finish search---------
				free(tempCMD);
				reap[i] = 0;
			}
			reap_counter = 0;
		}
		//---------------------------------------end reap children---
		
//--------------Start redirection---------------
		/**/
		for(p = 0; p < numTokens; p++)
		{
			if (numP == 0 || numP == 2)
{
				if (strcmp(args[p],">") == 0)
			{
				args[p] = NULL;
				fd1 = open(args[p+1], O_WRONLY | O_CREAT, 0777);
				if (fd1 == -1)
				{
					erroFlag = 2;
					break;
				}
				dup2(fd1, STDOUT_FILENO);
				close(fd1);
				fd1F = 1; // set flag
			}
			else if (strcmp(args[p],"<") == 0)
			{
				//printf("There is a < !\n");
				args[p] = NULL;
				
				fd2 = open(args[p+1], O_RDONLY);
				if (fd2 == -1)
				{
					erroFlag = 2;
					break;
				}
				dup2(fd2, STDIN_FILENO);
				close(fd2);
				fd2F = 1; // set flag
			}
			else if (strcmp(args[p],"2>") == 0)
			{
				fd3 = open(args[p+1], O_WRONLY | O_CREAT, 0777);
				if (fd3 == -1)
				{
					erroFlag = 2;
					break;
				}
				dup2(fd3, STDERR_FILENO);
				close(fd3);
				fd3F = 1;
			}
			else if (strcmp(args[p],">>") == 0)
			{
				args[p] = NULL;
				fd4 = open(args[p+1], O_WRONLY | O_CREAT | O_APPEND , 0777);
				if (fd4 == -1)
				{
					erroFlag = 2;
					break;
				}
				dup2(fd4, STDOUT_FILENO);
				close(fd4);
				fd4F = 1; // set flag
			}
			else if (strcmp(args[p],"|") == 0)
			{
				args[p] = NULL;
			}
}			// if pipe exist
			else if (numP == 1) 
			{
				if (strcmp(args[p],"|") == 0)
				{
					args[p] = NULL;
				}
				else if(strcmp(args[p],">") == 0)
				{
					if(p < posP1)
					{
						erroFlag = 1;
						break;
					}
					else
					{
						args[p] = NULL;
						fd1 = open(args[p+1], O_WRONLY | O_CREAT, 0777);
						if (fd1 == -1)
						{
							erroFlag = 2;
							break;
						}
						rpr = 1;
					}
				}
				else if (strcmp(args[p],">>") == 0)
				{
					if(p < posP1)
					{
						erroFlag = 1;
						break;
					}
					else
					{
						args[p] = NULL;
						fd4 = open(args[p+1], O_WRONLY | O_CREAT | O_APPEND , 0777);
						if (fd4 == -1)
						{
							erroFlag = 2;
							break;
						}
						rprr = 1;
					}
				}
				else if (strcmp(args[p],"<") == 0)
				{
					if(p > posP1)
					{
						erroFlag = 1;
						break;
					}
					else
					{
						args[p] = NULL;
						fd2 = open(args[p+1], O_RDONLY);
						if (fd2 == -1)
						{
							erroFlag = 2;
							break;
						}
						rpl = 1;
					}
					
				}
				else if (strcmp(args[p],"2>") == 0)
				{
					rp2 = 1;
					fd3 = open(args[p+1], O_WRONLY | O_CREAT, 0777);
					if (fd3 == -1)
					{
						erroFlag = 2;
						break;
					}
				}	
			}
		}
//--------------end redirection---------------		
	
	//----check erroflag
		if(erroFlag == 1)
		{
			erroFlag = 0;
			fprintf(stderr,PIPE_ERR);
			free(buffer);
			continue;
		}
		if(erroFlag == 2)
		{
			erroFlag = 0;
			fprintf(stderr,RD_ERR);
			free(buffer);
			continue;
		}
	
		
		
						
						
		//exit------------------------------------------------						
		if(strcmp(args[0],"exit") == 0) {
			// Terminating the shell
			//printf("There are %d in the reap.\n",reap_counter);
			//printf("There are %d in the list.\n",bg_list.length);
//reap children
			if(reap_counter > 0 )
		{
			
			//printf("There are %d bg, starting reap.\n", reap_counter);
			int i = 0;
			//char* tempCMD = (char *)malloc(9999);
			char* tempCMD = NULL;
			for(i = 0;i < reap_counter;i++)
			{
				//printf("First SID is %d\n", reap[0]);
				pid_t tempID = reap[i];
				//-----search list get cmd---------
				node_t* itr = bg_list.head;
				int length = bg_list.length;
				int t = 0;
				for(t = 0; t < length; t++)
				{
					ProcessEntry_t* tempE = itr->value;
					if(tempE->pid == tempID )
					{
						tempCMD = (char *)malloc(strlen(tempE->cmd)+1);
						//printf("Find!\n");
						mcopy(tempE->cmd,tempCMD,strlen(tempE->cmd));
						printf(BG_TERM,reap[i],tempCMD);
						removeByPid(&bg_list,reap[i]);
						break;
					}
					itr = itr->next;
				}
				//---finish search---------
				free(tempCMD);
				reap[i] = 0;
				
			}
			
			reap_counter = 0;
		}
//-----end reap----

//-----start delete nodes----
		if(bg_list.length > 0 )
		{
			//printf("There are %d nodes, starting clean.\n", bg_list.length);
			int i = 0;
			int length = bg_list.length;
			node_t* itr = bg_list.head;
			for(i = 0; i < length;i++)
			{
				ProcessEntry_t* tempE = itr->value;
				//pid_t tempPID = tempE->pid
				printf(BG_TERM,tempE->pid, tempE->cmd);
				kill(tempE->pid,SIGKILL);
				itr = itr->next;
			}
			 
		}
//----end delete nodes-------
			free(buffer);
			return 0;
		}
		
		//end exit-------------------------------------------------
		
		//------------start pipe------------
		if(numP > 0 && andflag == 0)
		{
			int temp_counter = numP;
			pid_t temp_pid;
			if (andflag == 1)
			{
				temp_counter = temp_counter - 1;
			}
			for( g = 0; g < temp_counter + 1; g++)
			{
				if( (temp_pid = fork()) == 0)
				{
					break;
				}
				else if(temp_pid < 0)
				{
					exit(EXIT_FAILURE);
				}
			}
			//first fork
			if(g == 0 && temp_pid == 0)
			{
				if(numP == 2)
				{
					close(pp2[0]);
					close(pp2[1]);
				}
				close(pp1[0]);
				dup2(pp1[1],STDOUT_FILENO);
//--------------start redirection-----------------------
				if(rpl == 1)
				{
					dup2(fd2, STDIN_FILENO);
					close(fd2);
				}
				if(rp2 == 1)
				{
					dup2(fd3, STDERR_FILENO);
					close(fd3);
				}
//--------------end redirection---------------------------
				exec_result = execvp(args[0], &args[0]);	
				if(exec_result == -1){ //Error checking
				//printf("We are here0!\n");
				printf(EXEC_ERR, args[0]);
				exit(EXIT_FAILURE);
									}
		    	exit(EXIT_SUCCESS);
			}
			else if (g == 1 && temp_pid == 0)  // second fork
			{
				if(numP == 2)
				{
					close(pp2[0]);
				}
//--------------start redirection-----------------------
				if(rp2 == 1)
				{
					dup2(fd3, STDERR_FILENO);
					close(fd3);
				}
				if(rpr == 1)
				{
					dup2(fd1, STDOUT_FILENO);
					close(fd1);
				}
				if(rprr == 1)
				{
					dup2(fd4, STDOUT_FILENO);
					close(fd4);
				}
//--------------end redirection---------------------------
				close(pp1[1]);
				dup2(pp1[0],STDIN_FILENO);
				if(numP == 2)
				{
					dup2(pp2[1],STDOUT_FILENO);
				}
				exec_result = execvp(args[posP1+1], &args[posP1+1]);	
				if(exec_result == -1){ //Error checking
				//printf("We are here1!\n");
				printf(EXEC_ERR, args[posP1+1]);
				exit(EXIT_FAILURE);
									}
		    	exit(EXIT_SUCCESS);
			}
			else if( g == 2 && temp_pid == 0) // third fork
			{
				close(pp1[0]);
				close(pp1[1]);
				close(pp2[1]);
				dup2(pp2[0],STDIN_FILENO);
				exec_result = execvp(args[posP2+1], &args[posP2+1]);	
				if(exec_result == -1){ //Error checking
				//printf("We are here2!\n");
				printf(EXEC_ERR, args[posP2+1]);
				exit(EXIT_FAILURE);
									}
		    	exit(EXIT_SUCCESS);
			}
			else if (andflag == 0)// parent
			{
				//printf("Test1!\n");
				pid_t p_pid;
				close(pp1[0]);
				close(pp1[1]);
				if(numP == 2)
				{
				close(pp2[0]);
				close(pp2[1]);
				}
				dup2(stdO,STDOUT_FILENO);
				dup2(stdE,STDERR_FILENO);
				dup2(stdI,STDIN_FILENO);
				/*
				if (p_pid == -1)
				{
					//printf("We are here!\n");
					printf(WAIT_ERR);
					exit(EXIT_FAILURE);
				}
				
				*/
				//reap child
				while( (p_pid = waitpid(-1,&Pstatus,WNOHANG)) != -1 )
				{
					if(p_pid > 0)
					{
						if(WIFEXITED(Pstatus)) // Success
						{
							Pflag = 1;
							//printf("We are Success!\n");
						}
					}
				}
			}
			//printf("Test2!\n");
		}
		
		if( Pflag == 1)
		{
			//printf("We are here2!\n");
			Pflag = 0;
			free(buffer);
			continue;
		}
		
		//------------end pipe--------------
		
		
		//--------------start background-------
		if ( strcmp(args[numTokens - 1],"&") == 0 )
		{
			args[numTokens - 1] = NULL;
			int temp_counter = numP;
			//printf("This is a bg command.\n");
			//printf("Command is: %s \n", tempBuffer);
			time_t testT;
			bg_list.comparator = TimeComparator;
			pid_t fpid;
			if(numP == 0)
			{
				fpid=fork();
			}
			else if(numP > 0)
		{
				for( g = 0; g < numP + 1; g++)
			{
				if( (fpid = fork()) == 0)
				{
					break;
				}
				else if(fpid < 0)
				{
					exit(EXIT_FAILURE);
				}
			}
		}
			if (fpid == 0) 
			{
				if (g == 0) // first bg pipe
				{
					if(numP == 2)
				{
					close(pp2[0]);
					close(pp2[1]);
				}
				close(pp1[0]);
				dup2(pp1[1],STDOUT_FILENO);
//--------------start redirection-----------------------
				if(rpl == 1)
				{
					dup2(fd2, STDIN_FILENO);
					close(fd2);
				}
				if(rp2 == 1)
				{
					dup2(fd3, STDERR_FILENO);
					close(fd3);
				}
//--------------end redirection---------------------------
				exec_result = execvp(args[0], &args[0]);	
				if(exec_result == -1){ //Error checking
				//printf("We are here0!\n");
				printf(EXEC_ERR, args[0]);
				exit(EXIT_FAILURE);
									}
		    	exit(EXIT_SUCCESS);
				}
				else if (g == 1) // second bg pipe
				{
					if(numP == 2)
				{
					close(pp2[0]);
					dup2(pp2[1],STDOUT_FILENO);
				}
				close(pp1[1]);
				dup2(pp1[0],STDIN_FILENO);
//--------------start redirection-----------------------
				if(rp2 == 1)
				{
					dup2(fd3, STDERR_FILENO);
					close(fd3);
				}
				if(rpr == 1)
				{
					dup2(fd1, STDOUT_FILENO);
					close(fd1);
				}
				if(rprr == 1)
				{
					dup2(fd4, STDOUT_FILENO);
					close(fd4);
				}
//--------------end redirection---------------------------
				exec_result = execvp(args[posP1+1], &args[posP1+1]);	
				if(exec_result == -1){ //Error checking
				//printf("We are here1!\n");
				printf(EXEC_ERR, args[posP1+1]);
				exit(EXIT_FAILURE);
									}
		    	exit(EXIT_SUCCESS);
				}
				else if (g == 2) // third bg pipe
				{
				close(pp1[0]);
				close(pp1[1]);
				close(pp2[1]);
				dup2(pp2[0],STDIN_FILENO);
				exec_result = execvp(args[posP2+1], &args[posP2+1]);	
				if(exec_result == -1){ //Error checking
				//printf("We are here2!\n");
				printf(EXEC_ERR, args[posP2+1]);
				exit(EXIT_FAILURE);
									}
		    	exit(EXIT_SUCCESS);
				}
				else
				{
					exec_result = execvp(args[0], &args[0]);
				}
				if(exec_result == -1)
				{ //Error checking
				printf(EXEC_ERR, args[0]);
				exit(EXIT_FAILURE);
				}
		    exit(EXIT_SUCCESS);
			}
			else // parent
			{
				if (numP > 0)
				{
				close(pp1[0]);
				close(pp1[1]);
				if(numP == 2)
				{
				close(pp2[0]);
				close(pp2[1]);
				}
				}
				dup2(stdO,STDOUT_FILENO);
				dup2(stdE,STDERR_FILENO);
				dup2(stdI,STDIN_FILENO);
				//printf("Start fork.\n");
				testT = time(0);
				//printf("ChildPID is: %d.\n", fpid);
				//pid_t tempid = getpid();
				char* tempCommand = malloc(1000);
				tempCommand = tempBuffer;
				ProcessEntry_t* entry = (ProcessEntry_t*)malloc(10000);
				entry->cmd = tempCommand;
				entry->pid = fpid;
				entry->seconds = testT;
				insertInOrder(&bg_list,entry);
				//printBGPEntry(entry);
				//printf("Insert OVER.\n");
				free(buffer);
				continue;
			}
			
			
		}
		//---------end background------
		
		
		//----------start CD -----------------
		else if(strcmp(args[0],"cd") == 0)
		{
			//testT = time(0);
			//printf("Time is: %ld\n", testT);
			//printf("First command is: %s\n", args[0]);
			//printf("This line is: %s\n", buffer);
			//printf("Last command is: %s\n", args[numTokens - 1]);
			//printf("Number of token is: %d\n", numTokens);
			//printf("HOME : %s ", getenv("HOME"));
			char *path = NULL;
    		
  		  	//puts(path);
    		//free(path);
			if(args[1] == NULL)
			{
				chdir(getenv("HOME"));
				path = getcwd(NULL,0);
				printf("%s\n",path);
				free(path);
			}
			else
			{
			int cd_result;
			cd_result = chdir(args[1]);
			if (cd_result == -1)
				{
				fprintf(stderr, DIR_ERR);
				}
			else
				{
				path = getcwd(NULL,0);
				printf("%s\n",path);
				free(path);
				}
			}
			
			continue;
			
		}
		//----------END CD--------------------
		
		//----------start estatus -----------------
		if(strcmp(args[0],"estatus") == 0)
		{
			printf("%d\n",WEXITSTATUS(exit_status));
			free(buffer);
			continue;
		}
		//----------END estatus--------------------
		

		pid = fork();   //In need of error handling......

		if (pid == 0)
		{ //If zero, then it's the child process
			exec_result = execvp(args[0], &args[0]);	
			if(exec_result == -1){ //Error checking
				//printf("We are here999!\n");
				printf(EXEC_ERR, args[0]);
				exit(EXIT_FAILURE);
			}
		    exit(EXIT_SUCCESS);
		}
		 else{ // Parent Process
			wait_result = waitpid(pid, &exit_status, 0);
			dup2(stdO,STDOUT_FILENO);
			dup2(stdE,STDERR_FILENO);
			dup2(stdI,STDIN_FILENO);
			if(wait_result == -1){
				printf(WAIT_ERR);
				exit(EXIT_FAILURE);
			}
		}

		// Free the buffer allocated from getline
		free(buffer);
	}
	
	//------START------------
		//printf("There are %d in the reap.\n",reap_counter);
		//printf("There are %d in the list.\n",bg_list.length);
	return 0;
}