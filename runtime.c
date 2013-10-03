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
  aliaslist *aliases = NULL;

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
        static void changeAlias(char *cmdline);
	/* adds the new job to background job list */ 
	static void addtojobs(pid_t pid, char* cmdline, int status);
	/* delete the job from background job list */
	int delfromjobs(pid_t pid);

        static void addtoaliases(char* previous_name, char* new_name);
	/* change status from BG to FG*/
	pid_t tofg(int jid);
  	pid_t tofg_mostrecent(joblist *jobs);
	void waitfg(pid_t pid);
	joblist* findjob(pid_t pid); 
  	pid_t tofg_mostrecent(joblist *jobs);
  	/* change status from BG to FG*/
  	pid_t tobg(int jid);
  	pid_t tobg_mostrecent(joblist *jobs);
        /* change status from BG to FG*/
        pid_t tobg(int jid);
        pid_t tobg_mostrecent(joblist *jobs);
  /************External Declaration*****************************************/

/**************Implementation***********************************************/
        int total_task;
	sigset_t mask;
	int status;
	void RunCmd(commandT** cmd, int n)
	{ 
      		int i;
      		total_task = n;
      		if(n == 1)
			RunCmdFork(cmd[0], TRUE);
      		else {
            /* Loop runs once for each command */
            for(i = 0; i < n; i++) {
              printf("%s\n", cmd[i]->cmdline);
              RunCmdFork(cmd[i], TRUE);
              //RunCmdPipe(cmd[i], cmd[i+1]);
            }
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
            /*
            //cmd1's stdout becomes cmd2's stdin
            //RunCmdFork(cmd1, TRUE);
            FILE *output;
            output = popen (cmd1->name, "r");
            if (!output)
            {
                fprintf(stderr, "Could not run command");
            }
            else
            {
                char buffer[1024];
                sprintf(buffer, "%s", output);
                execv(cmd2->name, output);
                pclose(output); 
            }
            */
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
		sigset_t mask;
		if (forceFork) {
			sigemptyset(&mask);
                	sigaddset(&mask,SIGCHLD);
                	sigprocmask(SIG_BLOCK, &mask, NULL);
		   
		 	/* child process */ 
 			if ((pid=fork())==0) {
                          // From the handout's hints section...not really sure what it does
        		  setpgid(0, 0);
			  sigprocmask(SIG_UNBLOCK, &mask, NULL);
			  execv(cmd->name, cmd->argv);
		   	  exit(0);
		   } else {
		   	/*parent waits for foreground job to terminate*/
			if (!cmd->bg) {
				addtojobs(pid, cmd->cmdline, FG);
				sigprocmask(SIG_UNBLOCK, &mask, NULL);
				//int status;
        			
				/*
				 Wait only if the process hasn't been stopped. If it has
        			 been stopped, don't continue to wait. (Gives access back
        			 to ./tsh)
				*/			
				waitfg(pid);
		   	} else {
	 			addtojobs(pid, cmd->cmdline, BG);
				sigprocmask(SIG_UNBLOCK, &mask, NULL);
			}
		   }
		} else {
        		execv(cmd->name, cmd->argv);
		}	
	}

  static bool IsBuiltIn(char* cmd)
  {
		if (!strcmp(cmd,"fg")) 
			return TRUE;
		if (!strcmp(cmd,"bg"))
			return TRUE;
		if (!strcmp(cmd,"jobs"))
			return TRUE;	
                if (!strcmp(cmd,"alias"))
			return TRUE;
		return FALSE;     
  }

   
 static void RunBuiltInCmd(commandT* cmd)
 {
      		if (!strcmp(cmd->argv[0],"fg")) {
        		/* No number passed */
  			if ((cmd->argv[1]==NULL) && (jobs !=NULL)) {
          		/* Restart the process if it has been stopped */
  				pid_t temp_pid = tofg_mostrecent(jobs);
          			if(temp_pid != (pid_t)-1) {
            				kill(-temp_pid, SIGCONT);
  					//waitpid(temp_pid, &status, WUNTRACED);
  					waitfg(temp_pid);
         		 	}
        		/* Job number passed */
  			} else if(cmd->argv[1] != NULL) {
          			// Restart the process if it has been stopped
  				pid_t temp_pid = tofg(atoi(cmd->argv[1]));
          			if(temp_pid != (pid_t)-1) {
            			    kill(-temp_pid, SIGCONT);
  				    //waitpid(temp_pid, &status, WUNTRACED);
  				    waitfg(temp_pid);
          			}
  			}
   		}			 
    		if (!strcmp(cmd->argv[0],"bg")) {
  			if ((cmd->argv[1]==NULL) && (jobs != NULL))  {
          			pid_t temp_pid = tobg_mostrecent(jobs);
          			if(temp_pid != (pid_t)-1) {
    					kill(temp_pid, SIGCONT);
          			}
  			} else if(cmd->argv[1] != NULL) {
          			pid_t temp_pid = tobg(atoi(cmd->argv[1]));
  				if(temp_pid != (pid_t)-1) {
            				kill(temp_pid, SIGCONT);
          			}
  			}
 		} 			
  		if (!strcmp(cmd->argv[0],"jobs")) {
  			printjobs();
  		}
      if (!strcmp(cmd->argv[0],"alias")) {
        if (cmd->argv[1]==NULL) {
          aliaslist* curr=aliases;
          while (curr != NULL) {
            printf("alias %s='%s'\n" , curr->previous_name, curr->new_name);
            curr=curr->next;
          }
        }
        else {
          changeAlias(cmd->cmdline);
        }
      }
}

/* print all the jobs in the job list */
static void printjobs() {
	joblist *temp = jobs;
	while(temp != NULL){
      		printf("[%d][%d] %s %s\n", temp->jid,temp->pid, temp->state, temp->command);
      		temp = temp->next;
    	}
}

static void changeAlias(char *cmdline)
{ 
        /* the command 'alias' */
        /* the command to change to */
        char equal = '=';
        int howMany = 0; //how large the command is
	char* letter = cmdline + 6; //start after the 'alias' command
        while (letter != NULL) {
            if (*letter == equal) {
	        break;	
            }
            letter++;
            howMany++;
        }
        char* command_to_change_to = malloc(howMany * sizeof(char) + 1);
        strncpy(command_to_change_to, cmdline + 6, howMany);
        //printf("command_to_change to: %s\n", command_to_change_to);
        
        /* the command to change */
        char quote = '"';
        int offset = howMany;
        letter = cmdline + 6 + offset + 2; //after the commands and ="
        howMany = 0;
        while (letter != NULL) {
            if (*letter == quote) {
                break;
            }
            letter++;
            howMany++;
        }
        char* command_to_change = malloc(howMany * sizeof(char) + 1);
        strncpy(command_to_change, cmdline + 6 + offset + 2, howMany);
 
        addtoaliases(command_to_change, command_to_change_to);
        /*
        aliaslist *temp = aliases;
        
        while (temp != NULL)
        {
            printf("temp->previous_name: %s\n", temp->previous_name);
            printf("temp->new_name: %s\n", temp->new_name);
            temp = temp->next;
        }
        */
        free (command_to_change_to);
        free (command_to_change);

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

static void addtoaliases(char* previous_name, char* new_name)
{
      /* add the new job to the of the list */
      //aliaslist *newalias = (aliaslist*)malloc(sizeof(aliaslist));
      //newalias->previous_name = previous_name;
      //newalias->new_name = new_name;
 
      bool needUpdated = FALSE;

      aliaslist* curr=aliases;
      aliaslist* prev=aliases;
      while (curr != NULL) {
        if (!strcmp(curr->new_name, new_name))
        {
            needUpdated = TRUE;
            break;
        }
        prev=curr;
        curr=curr->next;
      }

      if (needUpdated)
      {
        curr->previous_name = previous_name;
      }
      else
      {
        aliaslist *newalias = (aliaslist*)malloc(sizeof(aliaslist));
        
        int prev_len = strlen(previous_name);
        newalias->previous_name = (char *)malloc(prev_len * sizeof(char) + 1);
        strncpy(newalias->previous_name, previous_name, prev_len);
        
        int new_len = strlen(new_name);
        newalias->new_name = (char *)malloc(new_len * sizeof(char) + 1);
        strncpy(newalias->new_name, new_name, new_len);
        
        newalias->next = NULL;
        
        if (aliases == NULL) {
            aliases = newalias;
        } else {
            prev->next = newalias;
        }

      }
}


/* delete a job from bg process list */
int delfromjobs(pid_t pid)
{
  joblist* prev = NULL;
  joblist* curr = jobs;

  while(curr != NULL) {
    if (curr->pid == pid) {
      if (prev == NULL) {
        jobs = curr->next;
        break;
      }
      else {
        prev->next = curr->next;
        curr = curr->next;
      }
    }
    else {
      prev = curr;
      curr = curr->next;
    }
  }
  return 0;
}

/* return pid given jid from bg list */
/*
static pid_t jid2pid(int jid) {
	joblist *curr = jobs;
	while ((!(curr->jid == jid)) && (curr!=NULL)) {
		curr = curr->next;
	}
	if (curr==NULL) return (pid_t)0; //pid not found
	return curr->pid;
}	
*/

/* find jobs for fg command */
pid_t tofg(int jid)
{ 
	joblist *curr = jobs;
	while ((curr->jid != jid) && (curr != NULL)) {
		curr = curr->next;
	}
	if (curr==NULL) {
    printf("error: cannot find job \n");
    return (pid_t)-1;
  }
	else {
    curr->status = FG;
  }
  return curr->pid;
}

pid_t tofg_mostrecent(joblist *jobs) 
{
  joblist *curr = jobs;
  /*while ((curr->current != '+') && (curr != NULL)) {
    curr = curr->next;
  }*/
  while (curr->next != NULL) {
    curr = curr->next;
  }
  if (curr == NULL) {
    printf("No jobs to bring to foreground\n");
    return (pid_t)-1;
  } else {
    curr->status = FG;
  }
  return curr->pid;
}


/* wait for foreground job to complete */
void waitfg(pid_t pid) 
{
    joblist * fgjob;
    fgjob = findjob(pid);
    if (fgjob != NULL) {
    	while (fgjob->status == FG) {
		sleep(0);
		/* for debugging */
		/* printf("waiting for fg to change status \n"); */
    	}
    }
}

/* return a pointer to the job based on pid */
joblist* findjob(pid_t pid)
{
	joblist *curr = jobs;
	while ((curr->next !=NULL) && (curr->pid != pid)) {
		curr = curr->next; 
	}
	return curr;
}


/* find jobs for bg command */
pid_t tobg(int jid)
{ 
  joblist *curr = jobs;
  while ((curr->jid != jid) && (curr != NULL)) {
    curr = curr->next;
  }
  if (curr==NULL) {
    printf("error: cannot find job \n");
    return (pid_t)-1;
  }
  else {
    curr->status = BG;
    curr->state = "Running";
  }
  return curr->pid;
}

pid_t tobg_mostrecent(joblist *jobs) {
  joblist *curr = jobs;
  /*while ((curr->current != '+') && (curr != NULL)) {
    curr = curr->next;
  }*/
  while (curr->next != NULL) {
    curr = curr->next;
  }
  if (curr == NULL) {
    printf("No jobs to run in the background");
    return (pid_t)-1;
  } else {
    curr->status = BG;
    curr->state = "Running";
  }
  return curr->pid;
}
