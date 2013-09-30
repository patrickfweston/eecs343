 /*
 *    Purpose: Runs commands
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.1 $
 *    Last Modification: $Date: 2005/10/13 05:24:59 $
 *    File: $RCSfile: runtime.c,v $
 *    Copyright: (C) 2002 by Stefan Birrer
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: runtime.c,v $
 *    Revision 1.1  2005/10/13 05:24:59  sbirrer
 *    - added the skeleton files
 *
 *    Revision 1.6  2002/10/24 21:32:47  sempi
 *    final release
 *
 *    Revision 1.5  2002/10/23 21:54:27  sempi
 *    beta release
 *
 *    Revision 1.4  2002/10/21 04:49:35  sempi
 *    minor correction
 *
 *    Revision 1.3  2002/10/21 04:47:05  sempi
 *    Milestone 2 beta
 *
 *    Revision 1.2  2002/10/15 20:37:26  sempi
 *    Comments updated
 *
 *    Revision 1.1  2002/10/15 20:20:56  sempi
 *    Milestone 1
 *
 ***************************************************************************/
	#define __RUNTIME_IMPL__

  /************System include***********************************************/
	#include <assert.h>
	#include <errno.h>
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
	#include <sys/wait.h>
	#include <sys/stat.h>
	#include <sys/types.h>
	#include <unistd.h>
	#include <fcntl.h>
	#include <signal.h>

  /************Private include**********************************************/
	#include "runtime.h"
	#include "io.h"

  /************Defines and Typedefs*****************************************/
    /*  #defines and typedefs should have their names in all caps.
     *  Global variables begin with g. Global constants with k. Local
     *  variables should be in all lower case. When initializing
     *  structures and arrays, line everything up in neat columns.
     */

  /************Global Variables*********************************************/

	#define NBUILTINCOMMANDS (sizeof BuiltInCommands / sizeof(char*))

  joblist *jobs = NULL;

  /************Function Prototypes******************************************/
	/* run command */
	static void RunCmdFork(commandT*, bool);
	/* runs an external program command after some checks */
	static void RunExternalCmd(commandT*, bool);
	/* resolves the path and checks for exutable flag */
	static bool ResolveExternalCmd(commandT*);
	/* forks and runs a external program */
	static void Exec(commandT*, bool);
	/* runs a builtin command */
	static void RunBuiltInCmd(commandT*);
	/* checks whether a command is a builtin command */
	static bool IsBuiltIn(char*);
  	/* prints the list of jobs */
  	static void printjobs();
	/* adds the new job to background job list */ 
	static void addtojobs(pid_t pid, char* cmdline, int status);
	/* delete the job from background job list */
	static int delfromjobs(pid_t pid);
	/* return pid from jid */
	static pid_t jid2pid(int jid);
	/* change status from BG to FG*/
	static void tofg(int jid);
  /************External Declaration*****************************************/

/**************Implementation***********************************************/
        int total_task;
	int status;
	void RunCmd(commandT** cmd, int n)
	{
      		int i;
      		total_task = n;
      		if(n == 1)
			RunCmdFork(cmd[0], TRUE);
      		else {
        		RunCmdPipe(cmd[0], cmd[1]);
        		for(i = 0; i < n; i++)
         		ReleaseCmdT(&cmd[i]);
      		}
	}
	
	void RunCmdFork(commandT* cmd, bool fork)
	{
		if (cmd->argc<=0)
			return;
		if (IsBuiltIn(cmd->argv[0]))
		{
			RunBuiltInCmd(cmd);
		}
		else
		{
			RunExternalCmd(cmd, fork);
		}
	}

	void RunCmdBg(commandT* cmd)
	{
		// TODO
	}	

	void RunCmdPipe(commandT* cmd1, commandT* cmd2)
	{
	}

	void RunCmdRedirOut(commandT* cmd, char* file)
	{
	}

	void RunCmdRedirIn(commandT* cmd, char* file)
	{
	}

/*Try to run an external command*/
static void RunExternalCmd(commandT* cmd, bool fork)
{
  if (ResolveExternalCmd(cmd)){
    Exec(cmd, fork);
  }
  else {
    printf("%s: command not found\n", cmd->argv[0]);
    fflush(stdout);
    ReleaseCmdT(&cmd);
  }
}

/*Find the executable based on search list provided by environment variable PATH*/
static bool ResolveExternalCmd(commandT* cmd)
{
  char *pathlist, *c;
  char buf[1024];
  int i, j;
  struct stat fs;

  if(strchr(cmd->argv[0],'/') != NULL){
    if(stat(cmd->argv[0], &fs) >= 0){
      if(S_ISDIR(fs.st_mode) == 0)
        if(access(cmd->argv[0],X_OK) == 0){/*Whether it's an executable or the user has required permisson to run it*/
          cmd->name = strdup(cmd->argv[0]);
          return TRUE;
      }
    }
    return FALSE;
  }
  pathlist = getenv("PATH");
  if(pathlist == NULL) return FALSE;
  i = 0;
  while(i<strlen(pathlist)){
    c = strchr(&(pathlist[i]),':');
    if(c != NULL){
      for(j = 0; c != &(pathlist[i]); i++, j++)
	buf[j] = pathlist[i];
      i++;
    }
    else{
      for(j = 0; i < strlen(pathlist); i++, j++)
	buf[j] = pathlist[i];
    }
    buf[j] = '\0';
    strcat(buf, "/");
    strcat(buf,cmd->argv[0]);
    if(stat(buf, &fs) >= 0){
      if(S_ISDIR(fs.st_mode) == 0)
	if(access(buf,X_OK) == 0){/*Whether it's an executable or the user has required permisson to run it*/
	  cmd->name = strdup(buf); // path in name field 
	  return TRUE;
	}
    }
  }
  return FALSE; /*The command is not found or the user don't have enough priority to run.*/
}

	static void Exec(commandT* cmd, bool forceFork)
	{
		pid_t pid;
		if (forceFork) {
		   if ((pid=fork())==0) {
        setpgid(0, 0);
			  execv(cmd->name, cmd->argv);
		   	exit(0);
		   } else {
		   	/*parent waits for foreground job to terminate*/
			if (!cmd->bg) {
				addtojobs(pid, cmd->cmdline, FG);
				int status;
        // Wait only if the process hasn't been stopped. If it has
        // been stopped, don't continue to wait. (Gives access back)
        // to ./tsh
				waitpid(pid,&status, WUNTRACED);
		   	} else {
	 			addtojobs(pid, cmd->cmdline, BG);
			}
		   }
		} else {
        execv(cmd->name, cmd->argv);
		}	
		
		/*
		 parent waits for foreground job
		if (!cmd->bg) {
			int status;
			waitpid(pid, &status, 0);	
		}
		*/
	}

        static bool IsBuiltIn(char* cmd)
        {
		if (!strcmp(cmd,"fg")) 
			return TRUE;
		if (!strcmp(cmd,"bg"))
			return TRUE;
		if (!strcmp(cmd,"jobs"))
			return TRUE;	
		return FALSE;     
        }

   
	static void RunBuiltInCmd(commandT* cmd)
	{
      if (!strcmp(cmd->argv[0],"fg")) {
  			if ((cmd->argv[1]==NULL) && (jobs !=NULL)) {
          // Restart the process if it has been stopped
          kill(-jobs->pid, SIGCONT);
  				tofg(jobs->pid);
  				waitpid(jobs->pid, &status, 0);
  			} else {
          // Restart the process if it has been stopped
          kill(-jid2pid(atoi(cmd->argv[1])), SIGCONT);
  				tofg(atoi(cmd->argv[1]));
  				waitpid(jid2pid(atoi(cmd->argv[1])),&status,0);
  			}
   		}			 
    	if (!strcmp(cmd->argv[0],"bg")) {
  			if ((cmd->argv[1]==NULL) && (jobs != NULL))  {
  				kill(jobs->pid, SIGCONT);
  			} else {
  				kill(jid2pid(atoi(cmd->argv[1])), SIGCONT);
  			}
 		} 			
  		if (!strcmp(cmd->argv[0],"jobs")) {
  			printjobs();
  		}
	}

  static void printjobs() {
    joblist *temp = jobs;
    while(temp != NULL) {
      printf("[%d] %c %s %s\n", temp->jid, temp->current, temp->state, temp->command);
      temp = temp->next;
    }
  }

        void CheckJobs()
	{
 	}


commandT* CreateCmdT(int n)
{
  int i;
  commandT * cd = malloc(sizeof(commandT) + sizeof(char *) * (n + 1));
  cd -> name = NULL;
  cd -> cmdline = NULL;
  cd -> is_redirect_in = cd -> is_redirect_out = 0;
  cd -> redirect_in = cd -> redirect_out = NULL;
  cd -> argc = n;
  for(i = 0; i <=n; i++)
    cd -> argv[i] = NULL;
  return cd;
}

/*Release and collect the space of a commandT struct*/
void ReleaseCmdT(commandT **cmd){
  int i;
  if((*cmd)->name != NULL) free((*cmd)->name);
  if((*cmd)->cmdline != NULL) free((*cmd)->cmdline);
  if((*cmd)->redirect_in != NULL) free((*cmd)->redirect_in);
  if((*cmd)->redirect_out != NULL) free((*cmd)->redirect_out);
  for(i = 0; i < (*cmd)->argc; i++)
    if((*cmd)->argv[i] != NULL) free((*cmd)->argv[i]);
  free(*cmd);
}

/* add a job to process list */
static void addtojobs(pid_t pid, char* cmdline, int status) 
{
	/* add the new job to the of the list */
	joblist *newJob = (joblist*)malloc(sizeof(joblist));
	newJob->pid = pid;
	
	joblist* curr=jobs;
	joblist* prev=jobs;
	while (curr!=NULL) { 
	  prev=curr;	
	  curr=curr->next;
	}

	if (jobs==NULL) {
	   newJob->jid = 1;
	   jobs=newJob;
	} else { 
	   newJob->jid = prev->jid+1;
           prev->next = newJob;

           joblist* temp = malloc(sizeof(joblist));
           temp = jobs;
           while (temp != NULL)
           {
             if (temp->status != FG)
             {
               if (temp->current == '+') {
                 temp->current = '-';
               }
               else {
                 temp->current = ' ';
               }
             }
             temp = temp->next;
           }
	}	 
        newJob->command = cmdline;
        newJob->current = '+';
        newJob->state = "Running";
        
	newJob->next = NULL;
	newJob->status = status;
}


/* delete a job from bg process list */
static int delfromjobs(pid_t pid)
{
	joblist* curr = jobs; 
	joblist* prev = jobs;
	while ((!(curr->pid==pid)) && (curr!=NULL))  {
		prev = curr;
		curr = curr->next;
	}	
	if (curr==NULL) return 1; //pid not found from the list
	
	/* remove the job from the list */
	prev->next = curr->next; 
	free(curr);
	return 0; //deletion completed successfully
}

       /* return pid given jid from bg list */
static pid_t jid2pid(int jid) {
	joblist *curr = jobs;
	while ((!(curr->jid == jid)) && (curr!=NULL)) {
		curr = curr->next;
	}
	if (curr==NULL) return (pid_t)0; //pid not found
	return curr->pid;
}	

static void tofg(int jid)
{
	joblist *curr = jobs;
	while ((!(curr->jid == jid)) && (curr!=NULL)) {
		curr = curr->next;
	}
	if (curr==NULL) printf("error: cannot find job \n");
	else curr->status = FG;
}
