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
	/* Initialize command buffer */
	char* cmdLine = malloc(sizeof(char*)*BUFSIZE);
	
	/* shell initialization */
	if (signal(SIGINT, sig) == SIG_ERR) PrintPError("SIGINT");
	if (signal(SIGTSTP, sig) == SIG_ERR) PrintPError("SIGTSTP");
	if (signal(SIGCHLD, sigchld_handler) == SIG_ERR) PrintPError("SIGCHLD");
	
	while (!forceExit) /* repeat forever */
	{
		/* print prompt */
		printf("tsh> ");

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
		CheckJobs();
		
		/* interpret command and line
		 * includes executing of commands */
                if (aliases != NULL)
                { 
                    bool foundAlias = FALSE;
                    aliaslist *temp = aliases;
                    char* command = malloc(5 * sizeof(char) + 1);
                    strncpy(command, cmdLine, 5);
                    while (temp != NULL) 
                    {
                        if (!strcmp(temp->new_name, command))
                        {
                            Interpret(temp->previous_name);
                            foundAlias = TRUE;
                            break;
                        } 
                        temp = temp->next;   
                    }
                    
                    if (!foundAlias)
                        Interpret(cmdLine);
                }
                else
                {
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
      printf("\n");
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
    	  	printf("[%d][%d] stopped by Ctrl-Z \n", temp->jid, temp->pid);
    	    } else {
		printf("[%d][%d] interrupted by Ctrl-C \n",temp->jid, temp->pid);
	    }
		break;
          }
          temp = temp->next;
      } 
      if (temp == NULL) {
		killAllJobs(jobs);
		forceExit=TRUE;
		exit(0);
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
		fgjob->status = ST;
		sleep(0);
		/* delete from job list */
                /* printf("pid:[%d] %s \n",fgjob->pid,fgjob->state); */
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
