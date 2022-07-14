// author @ Cavin Gada

#define BLUE "\x1b[34;1m"
#define DEFAULT "\x1b[0m"

#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdbool.h>


volatile sig_atomic_t interrupted = false;

void cd_helper(char* path) {
    struct passwd *p = getpwuid(getuid());
    int success;

    if (strcmp(path, "~") == 0 || strcmp(path, "") == 0 || strcmp(path, "~/")==0) {
        if (p == NULL) {
            fprintf(stderr, "Error: Cannot get passwd entry. %s\n", strerror(errno));
        }
        success = chdir(p->pw_dir);
    }

    else if (strlen(path)>=2 && path[0] == '~' && path[1] == '/') {
        if (p == NULL) {
            fprintf(stderr, "Error: Cannot get passwd entry. %s\n", strerror(errno));
        }
        char* newPath = p->pw_dir;
        strcat(newPath, (path+1));
        success = chdir(newPath);
    }
    else {
        success = chdir(path);
    }

    if (success==-1) {
        fprintf(stderr, "Error: Cannot change directory to '%s'. %s.\n", path, strerror(errno));
    }
}
void sig_handler(int sig) {
    interrupted = true;
}


int main(int argc, char* argv[]) {

    /* declare a struct sigaction */
    struct sigaction action = {0}; // https://github.com/nsf/termbox/issues/35 helped with valgrind error here
    /* set the handler */
    action.sa_handler = sig_handler;
    /* Install the signal handler */
    sigaction(SIGINT, &action, NULL);

    if (sigaction(SIGINT, &action, NULL)==-1) {
        fprintf(stderr, "Error: Cannot register signal handler. %s.\n", strerror(errno));
        return 1; // exit failure
    }
    
    while(1) {

        interrupted = false;

        // help from: https://www.delftstack.com/howto/c/get-current-directory-in-c/
        char current_path[PATH_MAX];
        if (getcwd(current_path, PATH_MAX)==NULL) {
            fprintf(stderr, "Error: Cannot get current working directory. %s.\n", strerror(errno));
            return 1;
        }

        char input[4096];
        printf("%s[%s]%s> ", BLUE, current_path, DEFAULT); 

         /* STDIN error */
         
        if (fgets(input, 4096, stdin) == NULL) {
            if (interrupted == true) {
                interrupted = false;
                printf("%s", "\n");
                continue;
            }
            fprintf(stderr,"Error: Failed to read from stdin. %s.\n", strerror(errno));
            exit(1);
        }

        if (strcmp(input, "\n")==0) {
            continue;
        }
        
        input[strlen(input)-1] = '\0'; // we need to replace the last character to null terminator from \n to prevent errors with chdir
        
        // credit for learning how to split input by spaces can be found here: https://m.cplusplus.com/reference/cstring/strtok/
        char* splitInput[strlen(input)]; // this char array will store all the strings. 
        int i = 0;

        char *pch = strtok(input, " ");

        while (pch != NULL) {
            splitInput[i] = pch;    // each index of the array will point to a pointer that represents a string
            pch = strtok(NULL, " ");
            i++;
        }
        
        splitInput[i] = NULL; // we need to set the last to null so when passing to execvp, the function knows when to stop reading args. 
                              // if we fail to set this to null then execvp will always be screwed. 
                              // this helped provide insight after countless hours of pain: https://stackoverflow.com/questions/40732550/exec-fails-due-to-bad-address 

        /* implement CD command*/
        if (strcmp(splitInput[0], "cd")==0) {

            if (i>2) {
                fprintf(stderr, "Error: Too many arguments to cd.\n");
            }
            else if (i==1) {
                cd_helper("~");
            }
            else {
                cd_helper(splitInput[1]);
            }
        }
        else if (strcmp(splitInput[0], "exit")==0) {
            exit(0);
        }
        /*
        implement other commands. format follows example in main.pdf doc
        */
        else if (strcmp(splitInput[0], "cd")!=0) {
            pid_t pid;
            int stat;

            if ((pid=fork()) < 0) {
                fprintf(stderr, "Error: fork() failed. %s.\n", strerror(errno));
            }
        
            //child 
            else if (pid==0) {
                int executionSuccess = execvp(splitInput[0], splitInput);
                if (executionSuccess==-1) {
                    // if the screw up is bc of an interrupt just continue
                    
                    if (interrupted == true){ 
                        interrupted = false;
                        continue;
                    }

                    fprintf(stderr, "Error: exec() failed. %s.\n", strerror(errno));
                }
                interrupted = false;
                exit(1);
            }
            //parent
            else {
                
                if (wait(&stat) == -1) {
                    // if the screw is bc of an interrupt just continue
                    if (interrupted == true){ 
                        interrupted = false;
                        continue;
                    }
                    fprintf(stderr, "Error: wait() failed. %s.\n", strerror(errno));
                    exit(1);
                }
                
                interrupted = false;
            }

        }
        interrupted = false; // ondt forget to set back to false at the end of the iteration!
    }
}