/***************************************************************************
 *  Title: MySimpleShell 
 * -------------------------------------------------------------------------
 *    Purpose: A simple shell implementation 
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.1 $
 *    Last Modification: $Date: 2005/10/13 05:24:59 $
 *    File: $RCSfile: tsh.c,v $
 *    Copyright: (C) 2002 by Stefan Birrer
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: tsh.c,v $
 *    Revision 1.1  2005/10/13 05:24:59  sbirrer
 *    - added the skeleton files
 *
 *    Revision 1.4  2002/10/24 21:32:47  sempi
 *    final release
 *
 *    Revision 1.3  2002/10/23 21:54:27  sempi
 *    beta release
 *
 *    Revision 1.2  2002/10/15 20:37:26  sempi
 *    Comments updated
 *
 *    Revision 1.1  2002/10/15 20:20:56  sempi
 *    Milestone 1
 *
 ***************************************************************************/
	#define __MYSS_IMPL__

  /************System include***********************************************/
	#include <stdlib.h>
	#include <signal.h>
    	#include <string.h>
	#include <stdio.h>
 	#include <unistd.h>
 	#include <sys/types.h>
	#include <sys/wait.h>
	#include <errno.h>
  /************Private include**********************************************/
	#include "tsh.h"
	#include "io.h"
	#include "interpreter.h"
	#include "runtime.h"

  /************Defines and Typedefs*****************************************/
    /*  #defines and typedefs should have their names in all caps.
     *  Global variables begin with g. Global constants with k. Local
     *  variables should be in all lower case. When initializing
     *  structures and arrays, line everything up in neat columns.
     */

	#define BUFSIZE 80

  /************Global Variables*********************************************/


  /************Function Prototypes******************************************/
	/* handles SIGINT and SIGSTOP signals */	
	static void sig(int);
	static void sigchld_handler(int);
	void killAllJobs(joblist*);
  /************External Declaration*****************************************/

/**************Implementation***********************************************/

int main (int argc, char *argv[])
{
	setvbuf(stdout, NULL,_IONBF, 0);
	/* Initialize command buffer */
	char* cmdLine = malloc(sizeof(char*)*BUFSIZE);
	
	/* shell initialization */
	if (signal(SIGINT, sig) == SIG_ERR) PrintPError("SIGINT");
	if (signal(SIGTSTP, sig) == SIG_ERR) PrintPError("SIGTSTP");
	if (signal(SIGCHLD, sigchld_handler) == SIG_ERR) PrintPError("SIGCHLD");

	while (!forceExit) /* repeat forever */
	{
		/* print prompt */
		//printf("tsh> ");

		/* read command line */
		getCommandLine(&cmdLine, BUFSIZE);
	
		/* exit upon eof condition */
		if (feof(stdin)) 
		{
			forceExit=TRUE;
	    	continue;
		}
		
	    if(strcmp(cmdLine, "exit") == 0)
	    {
	    	killAllJobs(jobs);
	    	forceExit=TRUE;
	    	continue;
	    }

		/* print commandline to standard output */
		/* printf("%s", cmdLine); */

		/* checks the status of background jobs */
		CheckJobs(dones);
		
		/* interpret command and line
		 * includes executing of commands */
                if (aliases != NULL) //check if we have any aliases
                { 
                    // we'll create a new command line with possible aliases
                    char* new_cmdLine = (char*)malloc(BUFSIZE * sizeof(char) + 1);
                    // make sure we keep track of the fact that we found one
                    bool foundAlias = FALSE;
                    aliaslist *temp = aliases;

                    // this helps us parse out token at the command line
                    char *token;
                    token = strtok (cmdLine, " ");
                    while (token != NULL)
                    {                    
                      //each token should be separated by a space
                      strcat(new_cmdLine, " ");

                      while (temp != NULL) 
                      {
                        // if we find an aliases from our list that's in the
                        // old command line, then concatenate the alias to our 
                        // new command line
                        if (!strcmp(temp->new_name, token))
                        {
                            strcat(new_cmdLine, temp->previous_name);
                            foundAlias = TRUE;
                            break;
                        }
                        temp = temp->next;   
                      }

                      // if that token doesn't have an alias, just add the
                      // token back into the new command line
                      if (!foundAlias)
                      {
                          strcat(new_cmdLine, token);
                      }
                      foundAlias = FALSE;
                      token = strtok (NULL, " "); //get the next token
                    }
                   
                    //create a command to give to the Interpret function
                    int len = strlen(new_cmdLine);
                    char* tempCmd = malloc(sizeof(char) * (size_t)len + 1);
                    strcpy(tempCmd, new_cmdLine);

                    Interpret(tempCmd);
                }
                else
                {
                    // if no aliases, procede as normal
		    Interpret(cmdLine);
                }
	}

	/* shell termination */
	free(cmdLine);
	return 0;
} /* end main */

static void sig(int signo)
{
    // if the user presses Ctrl-C or Ctrl-Z
    if (signo == SIGINT || signo == SIGTSTP)
    {
      joblist *temp = jobs;
      //printf("\n");
      while (temp != NULL) {
          // Find the foreground job and kill it
          if (temp->status == FG) {
            // Kill using the signo
            kill(-temp->pid, signo);
            
	    /* not quite necessary as we delete the job in chld handler 
	    if(signo == SIGINT) {
              	delfromjobs(temp->pid);
            }i
	    */
            temp->state = "Stopped";
            temp->status = ST;
	    if (signo == SIGTSTP) {
    	  	printf("[%d]   %s                 %s\n", temp->jid, temp->state, temp->command);
    	    } else {
		printf("[%d]   %s                 %s\n", temp->jid, temp->state, temp->command);
	    }
		break;
          }
          temp = temp->next;
      } 
      if (temp == NULL) {
		//killAllJobs(jobs);
		//forceExit=TRUE;
		//exit(0);
      }
    }
}

/* child handler */
static void sigchld_handler(int signo) 
{
	errno = 0; /* clear errno */
	pid_t pid;
	int status;
	if ((pid = waitpid(-1, &status, WNOHANG)) > 0) 
	{
		/* change the status of the job so that waitfg can stop*/
        	joblist* fgjob = findjob(pid);
                if (fgjob->status == BG)
                {
                    addtodonelist(fgjob);
                }
		fgjob->status = ST;
		sleep(0);
		/* delete from job list */
        	delfromjobs(pid);
	} else {	
		/* print error message */
		if (errno != 0) {
			fprintf(stderr, "%s \n", strerror(errno));
		}
	}
}

void killAllJobs(joblist* jobs) {
	joblist* temp = jobs;
	while (temp != NULL) {
            kill(-temp->pid, SIGINT);
            temp = temp->next;
        }
}
