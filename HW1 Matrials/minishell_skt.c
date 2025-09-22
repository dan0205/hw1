#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

void parse(char* line, char** args);
void exec(char** args);
int cd(char** args);
void alias(char* line);
int alias2(char** args);
void pipec(char* line);

typedef struct {
    char name[1024];
    char command[1024];
} Alias;
Alias alias_list[20];
int alias_count = 0;


int main(void){
  char *line=NULL;
  size_t cap=0;

  while(1){

    fflush(stdout);
    ssize_t n=getline(&line, &cap, stdin);

    if (n < 0)
      break;

    if (n > 0 && line[n-1] == '\n')
      line[n-1] = '\0';

    if (strcmp(line, "quit") == 0)
      break;

    char copy[1024];
    strcpy(copy, line);

    if (strncmp(copy, "alias ", 6) == 0) {
      alias(copy);
    } else if (strchr(copy, '|') != NULL) {
      pipec(copy);
    } else {
      char *args[21];
      parse(copy, args);
      alias2(args);
      if (!cd(args)) { exec(args); }
    }
  }
  free(line);
  return 0;
}

void parse(char* line, char** args){
  int i=0;
  char *token = strtok(line, " \t\n");
  while(token != NULL && i < 20){
    args[i] = token;
    token = strtok(NULL, " \t\n");
    i++;
  }
  args[i] = NULL;
}

void exec(char** args){
  pid_t pid = fork();

  if(pid < 0){
    perror("Fork failed");
    return;

  } else if(pid == 0){
    char full_path[1024];
    char* cmd = args[0];

    sprintf(full_path, "/bin/%s", cmd);
    if(access(full_path, X_OK) == 0){
      execv(full_path, args);
    }

    sprintf(full_path, "/usr/bin/%s", cmd);
    if(access(full_path, X_OK) == 0){
      execv(full_path, args);
    }

  } else {
    wait(NULL);
  }

}

int cd(char** args) {
    if (strcmp(args[0], "cd") == 0) {
        char* dir = args[1];

        if (strcmp(dir, "~") == 0) {
            dir = getenv("HOME");
            chdir(dir);

        } else if(chdir(dir) != 0) {
            perror("cd");
        }

        return 1;
    }
    return 0;
}

void alias(char* line) {
    char* pointer = line + 6;
    char* equal = strchr(pointer, '=');
    *equal = '\0';

    char* name = pointer;
    char* command = equal + 1;

    if (command[0] == '\'' && command[strlen(command) - 1] == '\'') {
        command++;
        command[strlen(command) - 1] = '\0';
    }

    for (int i = 0; i < alias_count; i++) {
        if (strcmp(alias_list[i].name, name) == 0) {
            strcpy(alias_list[i].command, command);
            return;
        }
    }

    if (alias_count < 20) {
        strcpy(alias_list[alias_count].name, name);
        strcpy(alias_list[alias_count].command, command);
        alias_count++;
    }
}

int alias2(char** args) {
    for (int i = 0; i < alias_count; i++) {
        if (strcmp(args[0], alias_list[i].name) == 0) {
          
            static char new[1024];
            strcpy(new, alias_list[i].command);
            
            int i = 1;
            while (args[i] != NULL) {
                strcat(new, " ");
                strcat(new, args[i]);
                i++;
            }
            
            parse(new, args);
            return 1;
        }
    }
    return 0;
}

void pipec(char* line) {
    char* commands[3];
    int num = 0;

    char* token = strtok(line, "|");
    while (token != NULL && num < 3) {
        commands[num] = token;
        token = strtok(NULL, "|");
        num++;
    }

    int num2 = num - 1;
    int pipes[2][2];
    pid_t pids[num];

    int input = STDIN_FILENO;

    for (int i = 0; i < num; i++) {

        pipe(pipes[i % 2]);

        pids[i] = fork();
        
        if (pids[i] < 0) { 
          perror("Fork failed");
          return;
        }

        if (pids[i] == 0) {
            if (i > 0) {
                dup2(input, STDIN_FILENO);
                close(input);
            }
            if (i < num2) {
                dup2(pipes[i % 2][1], STDOUT_FILENO);
            }
            close(pipes[i % 2][0]);
            close(pipes[i % 2][1]);

            char* args[21];
            parse(commands[i], args);
            alias2(args);
            
            char full_path[1024];
            char* cmd = args[0];

            sprintf(full_path, "/bin/%s", cmd);
            if(access(full_path, X_OK) == 0){
              execv(full_path, args);
            }

            sprintf(full_path, "/usr/bin/%s", cmd);
            if(access(full_path, X_OK) == 0){
              execv(full_path, args);
            }

        }
        
        if (i > 0) {
            close(input);
        }
        
        close(pipes[i % 2][1]);
        input = pipes[i % 2][0];
    }
    
    close(input);

    for (int i = 0; i < num; i++) {
        waitpid(pids[i], NULL, 0);
    }
}