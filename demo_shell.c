#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_LENGTH 80 // The maximum length of the command

// args: command line arguments
void freeMemory(char *argArray[])
{
    for (int i = 0; i < MAX_LENGTH / 2 + 1; i++)
    {
        if (argArray[i] != NULL)
        {
            free(argArray[i]);
            argArray[i] = NULL;
        }
    }
}

// split command by the delim
// output:length of arrray contains arguments
int parseCommand(char command[], char *args[])
{

    int i = 0;
    char *ptr = strtok(command, " ");
    while (ptr != NULL)
    {
        args[i] = ptr;
        i++;
        ptr = strtok(NULL, " ");
    }
    // assign last element = null to execvp knows command has done.
    args[i] = NULL;

    return i;
}

void readCommandFromUser(char command[])
{
    fgets(command, MAX_LENGTH, stdin);
    //unsigned int len = strlen(command);
    if (command[strlen(command) - 1] == '\n') // remove \n from usrcmd
        command[strlen(command) - 1] = '\0';
}
int redirectOutput(char dir[])
{
    int fd = creat(dir, O_WRONLY | O_CREAT | S_IRWXU);
    if (fd == -1)
    {
        printf("Can't open the file\n");
        exit(2); // standard error
    }
    if (dup2(fd, STDOUT_FILENO) < 0)

    {
        printf("Failed redirecting output\n");
        exit(2);
    }
    close(fd);
    return 0;
}
//input: file name
int redirectInput(char dir[])
{
    int fd = open(dir, O_RDONLY);
    if (fd == -1)
    {
        printf("Can't open the file.\n");
        exit(2); // stderr
    }
    if (dup2(fd, STDIN_FILENO) < 0)
    {
        printf("Failed redirecting input\n");
        exit(2);
    }
    close(fd);
    return 0;
}
// return position of "|" in array that contains commands and arguments
// otherwise return -2
// find pipe symbol
int findPipePosition(char *args[], int args_num)
{
    int i = 0;
    int res = 0;
    for(i = 0; i <args_num; i++)
    {
        if(strcmp(args[i], "|") == 0)
        {
            res= i;
            return res;
        }
    }
    return -1;
}
int exePipeProcess(char *args[], int args_num, int pipePosition)
{
    int flag; // 0 is read end, 1 is write end
    int pipefd[2];
    pid_t pid1, pid2;

    if(pipe(pipefd) < 0)
    {
        printf("Pipe could not be initialized\n");
        exit(2);
    }
    // Split commands and arguments after "|" to a new array
    char *args2[MAX_LENGTH];
    for(int i = pipePosition +1; i < args_num; i++)
    {
        args2[i - pipePosition - 1] = args[i];

    }
    int args2_num = args_num - pipePosition -1;
    args_num = pipePosition;

    args[args_num] = NULL;
    args2[args2_num] = NULL;

    pid1= fork();
    if(pid1 < 0)
    {
        printf("Could not fork \n");
        exit(2);
    }
    // redirect input to pipe ( another process will wait to a process write end in pipe 
    //then it will read end)
    // child process
    else if(pid1 == 0)
    {
        //duplicate read end of pipe to stdout
        if(dup2(pipefd[0], STDIN_FILENO) < 0)
        {
            printf("Failed redirecting input \n");
            exit(2);
        }
        // close write end of pipe
        close(pipefd[0]);
        close(pipefd[1]);

        // executing "|"
        if(execvp(args2[0], args2) ==-1)
        {
            printf("Failed to execute the command \n");
            exit(2);
        }
    }
    // parent process
    else
    {
        //parent still create a new process to execute
        pid2= fork();
        if(pid2 < 0)
        {
            printf("Could not fork \n");
            exit(2);
        }
        else if(pid2 == 0) 
        {
            // redirect output to pipe
            //duplicate write end of pipe to stdout
            if(dup2(pipefd[1], STDOUT_FILENO) < 0)
            {
                printf("Failed redirecting output \n");
                exit(2);
            }
            //close read end of pipe
            close(pipefd[1]);
            close(pipefd[0]);

            //executing "|"
            if(execvp(args[0], args) == -1)
            {
                printf("Failed to execute the command \n");
                exit(2);
            }

        }
        else
        {
            close(pipefd[0]);
            close(pipefd[1]);
            waitpid(pid2, NULL,0);
        }
        // parent executing, waiting for two children 
        // while(wait(&flag) != NULL)
        wait(NULL); 
    }
       return 0;
}

int main(void)
{
    char command[MAX_LENGTH];

    char *args[MAX_LENGTH / 2 + 1]; // MAximum 40 argments, command line arguments

    int should_run = 1;
    pid_t pid;
    int status;
    int argv = 0;
    int args_num;
    
    while (should_run)
    {
        status = 1;
        printf("osh>");
        fflush(stdout);
        readCommandFromUser(command);

        args_num = parseCommand(command, args);
       

        if (strcmp(args[0], "exit") == 0)
        {
            break;
        }
        else
        {
            // check last args is "&"
            if (strcmp(args[args_num - 1], "&") == 0)// parent doesn't wait, runs concurrently
            {
                status = 0;
                args[args_num - 1] = NULL;
                args_num--;
                // wait = 1;
            }

            // else parent waits for child to finish
            //     wait = 0;
            pid = fork();
            if (pid < 0)
            {
                printf("Can't fork process \n");
                return 1;
            }
            else if (pid == 0) // child process
            {
                //check redirecting output
                if (args_num > 2 && strcmp(args[args_num - 2], ">") == 0)
                {
                    redirectOutput(args[args_num - 1]);
                    args[args_num - 2] = NULL;
                    args_num = args_num - 2;
                }
                //check redirecting input
                if (args_num > 2 && strcmp(args[args_num - 2], "<") == 0)
                {
                    redirectInput(args[args_num - 1]);
                    args[args_num - 2] = NULL;
                    args_num = args_num - 2;
                }
                // check communication with pipe
                int pipePos = findPipePosition(args, args_num);
                if(args_num > 2 && pipePos!= -1)
                {
                    exePipeProcess(args, args_num, pipePos);
                    exit(2);
                }
                //
                if (execvp(args[0], args) == -1) // not equal 1 execvp will run its command to print res
                {
                    printf("Invalid command\n");
                    exit(2);
                }
            }
            else // pid>0 : parent process
            {
                // if (wait == 0)
                // {
                //     waitpid(pid, &status, 0);
                //     break;
                // }
                // if (wait == 1)
                // {
                //     waitpid(pid, &status, WUNTRACED);
                //     if (WIFEXITED(status))
                //     {
                //         printf("children b<%d> exited with status =%d.\n", pid, status);
                //     }
                //     break;
                // }

                // {$
                //     continue;
                // }
                // waitpid(pid, NULL, 0);
                if (status == 0) //
                {
                    continue;
                }
                waitpid(pid, NULL, 0);
            }
        }
    }

    

    //freeMemory(args);
    return 0;
}