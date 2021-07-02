#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#define MAXCHAR 2048 // max number of letters to be supported
#define MAXCOMM 512 // max number of commands to be supported

char input[MAXCHAR+1];
char *commandArgs[MAXCOMM+1];
char inputRedir[MAXCOMM];
char outputRedir[MAXCOMM];
int bgProcessArray[MAXCOMM+1];
bool allowBG = true;

struct data{
  bool bgProcessFlag;
  bool inputRedirFlag;
  bool outputRedirFlag;
  int fgStatusCode;
  int bgProcessNum;
};

void initStruct(struct data* d){
  d->bgProcessFlag = false;
  d->inputRedirFlag = false;
  d->outputRedirFlag = false;
  d->fgStatusCode = -5;
  d->bgProcessNum = 0;
}

void initShell(struct data* d);
void resetCmdVars(struct data* d);
void getInput();
void parseInput(struct data* d);
void killBGProcesses();
void checkBGProcesses();

void catchSIGTSTP(int signo);


int main(){
  // signal for SIGINT
  struct sigaction SIGINT_action = {0};
	SIGINT_action.sa_handler = SIG_DFL;
	sigfillset(&SIGINT_action.sa_mask);
	SIGINT_action.sa_flags = 0;

  // signal for SIGTSTP
	struct sigaction SIGTSTP_action = {0};
	SIGTSTP_action.sa_handler = catchSIGTSTP;
	sigfillset(&SIGTSTP_action.sa_mask);
	SIGTSTP_action.sa_flags = 0;

  // signal for ignoring signals
	struct sigaction ignore_action = {0};
	ignore_action.sa_handler = SIG_IGN;

	sigaction(SIGINT, &ignore_action, NULL);
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);

  // init the struct data and the shell
  struct data d;
  initStruct(&d);
	initShell(&d);

	while(true){
    // reset the vars for the next prompt
		resetCmdVars(&d);
    // prompt for input
		getInput();
    // check the input
		parseInput(&d);

		if(strcmp(input, "") == 0){
      commandArgs[0] = strdup("");
			continue;
		} else if(commandArgs[0][0] == '#'){ // check for comment
			continue;
		} else if(strcmp(commandArgs[0], "exit") == 0){ // check for exit
			killBGProcesses();
      exit(0);
		} else if(strcmp(commandArgs[0], "status") == 0){ // check for status
			if(WIFEXITED(d.fgStatusCode) != 0){
				printf("exit value %d\n", WEXITSTATUS(d.fgStatusCode));
			} else if(WIFSIGNALED(d.fgStatusCode) != 0){
				printf("terminated by signal %d\n", WTERMSIG(d.fgStatusCode));
			}
			fflush(stdout);
		} else if(strcmp(commandArgs[0], "cd") == 0){ // check for cd
			if(commandArgs[1] != NULL){
				chdir(commandArgs[1]);
			}
			else { // if just "cd", then go to home dir
				chdir(getenv("HOME"));
			}
		}
		else {
			pid_t spawnPid = fork();
			int fin = 0, fout = 0, dup = 0;
			switch(spawnPid){
				case -1: // check for failed fork
					printf("fork failed\n");
					fflush(stdout);
				case 0: // child process
					sigaction(SIGTSTP, &ignore_action, NULL);
					if(d.bgProcessFlag == true && allowBG == false){
						exit(-5);
					}
					if(d.bgProcessFlag == false){
						sigaction(SIGINT, &SIGINT_action, NULL);
						if(d.inputRedirFlag == true){
							if(strcmp(inputRedir, "") != 0){
								fin = open(inputRedir, O_RDONLY);
								if(fin == -1){
									fprintf(stderr, "cannot open %s for input\n", inputRedir);
									fflush(stderr);
									exit(1);
								}
								dup = dup2(fin, STDIN_FILENO);
								if(dup == -1){
									fprintf(stderr, "error with dup2\n");
									fflush(stderr);
									exit(1);
								}
								fcntl(fin, F_SETFD, FD_CLOEXEC);
							}
						}
						if(d.outputRedirFlag == true){
							if(strcmp(outputRedir, "") != 0){
								fout = open(outputRedir, O_WRONLY | O_CREAT | O_TRUNC, 0666);
								if(fout == -1){
									fprintf(stderr, "cannot open %s for output\n", outputRedir);
									fflush(stderr);
									exit(1);
								}
								dup = dup2(fout, STDOUT_FILENO);
								if(dup == -1){
									fprintf(stderr, "error with dup2\n");
									fflush(stderr);
									exit(1);
								}
								fcntl(fout, F_SETFD, FD_CLOEXEC);
							}
						}
					}
					else {
						fin = open("/dev/null", O_RDONLY);
						if(fin == -1){
									fprintf(stderr, "cannot open /dev/null for input\n");
									fflush(stderr);
									exit(1);
								}
						dup = dup2(fin, STDIN_FILENO);
						if(dup == -1){
									fprintf(stderr, "error with dup2\n");
									fflush(stderr);
									exit(1);
								}
						fcntl(fin, F_SETFD, FD_CLOEXEC);

						fout = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0666);
						if(fout == -1){
									fprintf(stderr, "cannot open /dev/null for output\n");
									fflush(stderr);
									exit(1);
								}
						dup = dup2(fout, STDOUT_FILENO);
						if(dup == -1){
									fprintf(stderr, "error with dup2\n");
									fflush(stderr);
									exit(1);
								}
						fcntl(fout, F_SETFD, FD_CLOEXEC);

						if(d.inputRedirFlag == true){
							if(strcmp(inputRedir, "") != 0){
								fin = open(inputRedir, O_RDONLY);
								if(fin == -1){
									fprintf(stderr, "cannot open %s for input\n", inputRedir);
									fflush(stderr);
									exit(1);
								}
								dup = dup2(fin, STDIN_FILENO);
								if(dup == -1){
									fprintf(stderr, "error with dup2\n");
									fflush(stderr);
									exit(1);
								}
								fcntl(fin, F_SETFD, FD_CLOEXEC);
							}
						}

            if(d.outputRedirFlag == true){
							if(strcmp(outputRedir, "") != 0){
								fout = open(outputRedir, O_WRONLY | O_CREAT | O_TRUNC, 0666);
								if(fout == -1){
									fprintf(stderr, "cannot open %s for output\n", outputRedir);
									fflush(stderr);
									exit(1);
								}
								dup = dup2(fout, STDOUT_FILENO);
								if(dup == -1){
									fprintf(stderr, "error with dup2\n");
									fflush(stderr);
									exit(1);
								}
								fcntl(fout, F_SETFD, FD_CLOEXEC);
							}
						}
					}

          // execute command
          int execStatus = execvp(commandArgs[0], commandArgs);
					if(execStatus == -1){
						fprintf(stderr, "%s: no such file or directory\n", commandArgs[0]);
						fflush(stderr);
						exit(1);
					}
					fprintf(stderr, "invalid command\n");
					fflush(stderr);
					exit(1);
				default:
					if(d.bgProcessFlag == true && allowBG == true){
						bgProcessArray[d.bgProcessNum] = spawnPid;
						d.bgProcessNum++;
						printf("background pid is %d\n", spawnPid);
						fflush(stdout);
					} else if(d.bgProcessFlag == false){
						pid_t pidOutput = waitpid(spawnPid, &d.fgStatusCode, 0);
						if(WIFSIGNALED(d.fgStatusCode) != 0)
						{
							printf("terminated by signal %d\n", WTERMSIG(d.fgStatusCode));
							fflush(stdout);
						}
					}
			}
		}
		checkBGProcesses();
	}
	return 0;
}

// init the vars for the shell
void initShell(struct data* d){
	memset(input, sizeof(input), '\0');
  int i;
	for(i = 0; i < MAXCOMM+1; ++i){
		commandArgs[i] = NULL;
		bgProcessArray[i] = -5;
	}
	d->bgProcessNum = 0;
	allowBG = true;
	d->bgProcessFlag = false;
	d->inputRedirFlag = false;
	memset(inputRedir, sizeof(outputRedir), '\0');
	d->outputRedirFlag = false;
	memset(outputRedir, sizeof(outputRedir), '\0');
	d->fgStatusCode = -5;
}

// reset the vars for the next command
void resetCmdVars(struct data* d){
	memset(input, sizeof(input), '\0');
  int i;
  for(i = 0; i < MAXCOMM+1; ++i){
		commandArgs[i] = NULL;
	}
	d->bgProcessFlag = false;
	d->inputRedirFlag = false;
	memset(inputRedir, sizeof(outputRedir), '\0');
	d->outputRedirFlag = false;
	memset(outputRedir, sizeof(outputRedir), '\0');
}

// get input from user
void getInput(){
	do {
		printf(": ");
		fflush(stdout);
		errno = 0;
		fgets(input, sizeof(input), stdin);
	} while(EINTR == errno);
	input[strlen(input)-1] = '\0';
}

// parse input mainly for & and $$
void parseInput(struct data* d){
	bool isBGLastArg = false;
	int numArgs = 0;
	int pid = getpid();

	char *token = strtok(input, " ");

	while(token != NULL){
		if(strcmp(token, "<") == 0){ // check for input redirect
			isBGLastArg = false;
			d->inputRedirFlag = true;
			token = strtok(NULL, " ");
			strcpy(inputRedir, token);
		} else if(strcmp(token, ">") == 0){ // check for output redirect
			isBGLastArg = false;
			d->outputRedirFlag = true;
			token = strtok(NULL, " ");
			strcpy(outputRedir, token);
		} else if(strcmp(token, "&") == 0){ // check for &
			isBGLastArg = true;
		}
		else {
			isBGLastArg = false;
			commandArgs[numArgs] = strdup(token);

			int curChar;
			for(curChar = 0; commandArgs[numArgs][curChar]; ++curChar){
        // check for $$
				if(commandArgs[numArgs][curChar] == '$' && commandArgs[numArgs][curChar + 1] == '$'){
					commandArgs[numArgs][curChar] = '\0';
					sprintf(commandArgs[numArgs], "%s%d", commandArgs[numArgs], pid);
				}
			}
			numArgs++;
		}
		token = strtok(NULL, " ");
	}
	if(isBGLastArg == true && allowBG == true){
		d->bgProcessFlag = true;
	}
}

// kill the bg processes
void killBGProcesses(){
	int i;
	for(i = 0; i < MAXCOMM+1; ++i){
		if(bgProcessArray[i] != -5){
			kill(bgProcessArray[i], SIGTERM);
			int childExitMethod = -5;
			pid_t pidOutput = waitpid(bgProcessArray[i], &childExitMethod, 0);
		}
	}
}

// check on the bg processes
void checkBGProcesses(){
	int i;
	for(i = 0; i < MAXCOMM+1; ++i){
		if(bgProcessArray[i] != -5){
			int childExitMethod = -5;
			pid_t pidOutput = waitpid(bgProcessArray[i], &childExitMethod, WNOHANG);
			if(pidOutput > 0){
				printf("background pid %d is done: ", bgProcessArray[i]);
				if(WIFEXITED(childExitMethod) != 0){
					printf("exit value %d\n", WEXITSTATUS(childExitMethod));
				} else if(WIFSIGNALED(childExitMethod)){
					printf("terminated by signal %d\n", WTERMSIG(childExitMethod));
				}
			}
		}
	}
	fflush(stdout);
}

// catch for SIGTSTP (^Z)
void catchSIGTSTP(int signo){
	if(allowBG == true){
		allowBG = false;
		char* message = "\nEntering foreground-only mode(& is now ignored)\n";
		write(1, message, 51);
		fflush(stdout);
	}
	else {
	   allowBG = true;
		char* message = "\nExiting foreground-only mode\n";
		write(1, message, 31);
		fflush(stdout);
	}
}
