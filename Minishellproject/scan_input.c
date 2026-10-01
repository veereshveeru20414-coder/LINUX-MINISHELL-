#include "shell.h"

char *external_commands[160];       
extern char promt_str[50];      
char curr_cmd[50];                  
int status, pid;
Slist *head = NULL; 


int delete_first(Slist **head)
{
    if (*head == NULL) // check head is empty or not
    {
        return -1;
    }
    Slist *temp = *head; //create a temp pointer and store the head address
    *head = temp -> link;   // works even if link is NULL
    free(temp); // and freed that node

    return 0;
}

int insert_at_first(Slist **head,int pid, char *cmd)
{
    Slist *new_node = malloc(sizeof(Slist));
    if (new_node == NULL)
    {
        return -1;
    }
    new_node -> pid = pid;
    strcpy(new_node -> cmd, cmd);
    new_node -> link = *head;
    *head = new_node;

    return 0;
}

void signal_handler(int signum)       //own_handler
{
    if (signum == SIGINT || signum == SIGTSTP)
    {
        if (pid == 0)
        {
            write(STDOUT_FILENO, "\n", 1);
            write(STDOUT_FILENO, ANSI_COLOR_GREEN, strlen(ANSI_COLOR_GREEN));
            write(STDOUT_FILENO, promt_str, strlen(promt_str));
            write(STDOUT_FILENO, ANSI_COLOR_RESET, strlen(ANSI_COLOR_RESET)); 
            
        }
    }
    else if(signum == SIGCHLD)
    {
        pid_t child_pid;

        while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0)
        {
            if (head)
            {
                printf("\n[%d] Done\t%s\n", child_pid, head->cmd);
                delete_first(&head);
            }
        }
    }
    
}

void scan_input(char *promt, char *input_string)
{
    extract_external_commands();

    signal(SIGINT, signal_handler);
    signal(SIGTSTP, signal_handler);

    while (1)
    {
        pid = 0;

        printf(ANSI_COLOR_GREEN "%s" ANSI_COLOR_RESET, promt);

        fflush(stdout);

        scanf("%49[^\n]", input_string);
        getchar();
        strcpy(curr_cmd,input_string);

        if (strncmp(input_string, "PS1=", 4) == 0)
        {
            if (input_string[4] == ' ')
            {
                printf("INFO: Prompt cannot contain spaces\n");
            }
            else
            {
                strcpy(promt, input_string + 4);
                system("clear");
            }
            continue;
        }
        else   
        {
            char *cmd = get_command(input_string);

            int ret = check_command_type(cmd);

            if (ret == BUILTIN)
            {
                execute_internal_commands(input_string);
            }
            else if (ret == EXTERNAL)  
            {
                pid = fork();

                if (pid > 0)
                {
                    waitpid(pid, &status, WUNTRACED);
                    if (WIFSTOPPED(status))
                    {
                        printf("\n[%d] Stopped\t%s\n", pid, curr_cmd);
                        insert_at_first(&head, pid, curr_cmd);
                    }
                    else if (WIFSIGNALED(status))
                    {
                        if (WTERMSIG(status) == SIGINT)
                        {
                            printf("\n");
                        }
                    }
                }
                else if (pid == 0)
                {
                    signal(SIGINT, SIG_DFL);
                    signal(SIGTSTP, SIG_DFL);
                    execute_external_commands(input_string);
                    exit(1);
                }
                else
                {
                    perror("fork");
                }
            }
            else if (ret == NO_COMMAND)
            {
                printf("Command '%s' not found\n", cmd);  // prints error message
            }
        }
    }

}  
