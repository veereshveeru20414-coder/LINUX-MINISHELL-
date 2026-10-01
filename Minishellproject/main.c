#include "shell.h"

char promt_str[50] = "Minishell-$:";
int main()
{
    char input_str[50];
    system("clear");
    
    scan_input(promt_str,input_str); 

    printf("last Done\n");
    return 0;
}  