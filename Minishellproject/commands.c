#include "shell.h"

extern char *external_commands[160];
extern int status;

/*builtin commands*/
char *builtins[] = { "echo", "printf", "read", "cd", "pwd", "pushd", "popd", "dirs", "let", "eval","set", 
                     "unset","export", "declare", "typeset", "readonly", "getopts", "source","exit", "exec", 
                     "shopt","caller", "true", "type", "hash", "bind", "help","jobs", "fg", "bg", NULL };

void extract_external_commands(void)
{
    FILE *fp = fopen("external_commands.txt", "r");
    if(fp == NULL) 
    {
        perror("INFO: File opened failed\n");
        return;
    } 
    char buffer[100]; 
    int i = 0;

    while(fgets(buffer, sizeof(buffer), fp) != NULL)
    {
        buffer[strcspn(buffer, "\r\n")] = '\0'; // removes both \r and \n

        external_commands[i] = malloc(strlen(buffer) + 1); // Allocate memory for the command

        if (external_commands[i] == NULL)
        {
            perror("INFO: Error in malloc memory allocation\n");
            fclose(fp);
            return;
        }
        strcpy(external_commands[i], buffer); // Copy command into allocated memory
        //printf("Loaded : %s\n",buffer);
        i++;
    }
    external_commands[i] = NULL;  // Mark end of array
    fclose(fp);
}

char *get_command(char *str)   //fetching 1st command or word from input string, like -> ls -l -> just taking ls only here
{
    static char word[100];
    int i = 0;

    while (*str && *str != ' ' && *str != '\t' && *str != '\n')
    {
        word[i++] = *str++;
    }
    word[i] = '\0';

    return word;
}

int check_command_type(char *command)    
{
    // Check builtin commands
    for(int i = 0; builtins[i] != NULL; i++)
    {
        if(strcmp(builtins[i] ,command) == 0)
        {
            return BUILTIN;
        }
    } 

    // Check external commands
    for(int i = 0; external_commands[i] != NULL; i++)
    {
        if(strcmp(external_commands[i], command) == 0)
        {
            return EXTERNAL;
        }
    }
    return NO_COMMAND; // if no commands
}

void execute_internal_commands(char *input_string)
{
    if(strcmp(input_string, "exit") == 0)  
    {  
        exit(0);
    }
    else if(strncmp(input_string, "cd", 2) == 0)
    {   
        if(strlen(input_string) <= 2)
        {
            chdir(getenv("HOME"));
        }
        else
        {
            if (chdir(input_string + 3)== -1)
            {
                perror("cd ");
                //continue;
            }

        }
    }
    else if(strcmp(input_string, "pwd") == 0)
    {
        char buff[50];
        getcwd(buff, 50);
        printf("%s\n", buff);
    }
    else if(strcmp(input_string, "echo $$") == 0)
    {
        printf("%d\n", getpid());
    }
    else if(strcmp(input_string, "echo $?") == 0)
    {
        printf("%d\n", WEXITSTATUS(status));
    }
    else if(strcmp(input_string, "echo $SHELL") == 0)
    {
        printf("%s\n", getenv("SHELL"));
    }
    else if(strcmp(input_string, "jobs") == 0)
    {
        int i = 1;
        Slist *temp = head;
        if (temp == NULL)
        {
            printf("INFO : No stoped process\n");
        }
        else
        {
            while(temp)
            {
                printf("[%d] Stopped\t\t%s\n", i++, temp -> cmd);
                temp = temp->link;
            }
        }
    }
    else if(strcmp(input_string, "fg") == 0)
    {
        if(head == NULL)
        {
            printf("No stopped jobs\n");
        }
        else
        {
            kill(head -> pid, SIGCONT);
            printf("Running\t\t%s\n",head -> cmd);
            waitpid(head -> pid, &status, WUNTRACED);
            delete_first(&head);
        }
    }
    else if(strcmp(input_string, "bg") == 0)
    {
        if(head == NULL)
        {
            printf("No stopped jobs\n");
        }
        else
        {
            kill(head -> pid, SIGCONT);
            printf("[%d] %s &\n", head -> pid, head -> cmd);

            signal(SIGCHLD, signal_handler);
        }  
    }

}

void execute_external_commands(char *input_string)
{
    char *argv[50];
    int argc = 0;
    int i, j;
    int pipe_flag = 0;

    char *token = strtok(input_string, " ");

    while (token != NULL && argc < 49)
    {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }
    argv[argc] = NULL;

    for (i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "|") == 0)
            pipe_flag++;
    }

    if (pipe_flag)
    {
        for (i = 0; argv[i] != NULL; i++)
        {
            if (strcmp(argv[i], "|") == 0)
            {
                if (i == 0 || argv[i + 1] == NULL)
                {
                    printf("Invalid pipe syntax\n");
                    return;
                }
            }
        }

        char **cmd[20];
        int cmd_count = 0;

        cmd[cmd_count++] = &argv[0];

        for (i = 0; argv[i] != NULL; i++)
        {
            if (strcmp(argv[i], "|") == 0)
            {
                if (cmd_count >= 20)
                {
                    printf("Too many pipe commands\n");
                    return;
                }

                argv[i] = NULL;
                cmd[cmd_count++] = &argv[i + 1];
            }
        }

        int pipefd[20][2];

        for (i = 0; i < cmd_count - 1; i++)
        {
            if (pipe(pipefd[i]) == -1)
            {
                perror("pipe");
                return;
            }
        }

        for (i = 0; i < cmd_count; i++)
        {
            pid_t pid = fork();

            if (pid < 0)
            {
                perror("fork");
                return;
            }

            if (pid == 0)
            {
                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);

                if (i == 0)
                {
                    dup2(pipefd[i][1], STDOUT_FILENO);
                    close(pipefd[i][1]);
                }
                else if (i == cmd_count - 1)
                {
                    dup2(pipefd[i - 1][0], STDIN_FILENO);
                    close(pipefd[i - 1][0]);
                }
                else
                {
                    dup2(pipefd[i - 1][0], STDIN_FILENO);
                    dup2(pipefd[i][1], STDOUT_FILENO);
                    close(pipefd[i - 1][0]);
                    close(pipefd[i][1]);
                }

                for (j = 0; j < cmd_count - 1; j++)
                {
                    close(pipefd[j][0]);
                    close(pipefd[j][1]);
                }

                execvp(cmd[i][0], cmd[i]);
                perror(cmd[i][0]);
                exit(1);
            }
        }

        for (i = 0; i < cmd_count - 1; i++)
        {
            close(pipefd[i][0]);
            close(pipefd[i][1]);
        }

        for (i = 0; i < cmd_count; i++)
        {
            wait(NULL);
        }
    }
    else
    {
        if (argv[0] == NULL)
            return;

        execvp(argv[0], argv);
        perror(argv[0]);
        exit(1);
    }
}   