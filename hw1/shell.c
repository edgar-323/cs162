#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "tokenizer.h"

typedef void (*sighandler_t)(int);

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_exit(struct tokens *tokens);
int cmd_help(struct tokens *tokens);
int cmd_pwd(struct tokens *tokens);
int cmd_cd(struct tokens *tokens);
int cmd_wait(struct tokens *tokens);

enum Direction {NONE = 0, LEFT = 1, RIGHT = 2} direction;

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens *tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} fun_desc_t;

struct Node {
  char * data;
  struct Node * next;
};
struct Node ** path_directories;


struct Process {
  //add fields as needed
  pid_t pid;
  struct Process * next;
};
struct Process ** active_processes;

fun_desc_t cmd_table[] = {
  {cmd_help, "?", "show this help menu"},
  {cmd_exit, "exit", "exit the command shell"},
  /* EDIT BEGIN */
  {cmd_pwd, "pwd", "show current working directory"},
  {cmd_cd, "cd", "change working directory"},
  {cmd_wait, "wait", "waits until all background jobs have terminated before returning to the prompt"}
  /* EDIT END */
};

void clean_up(void* ptr) {
  if (ptr) {
    free(ptr);
  }
}

int cmd_cd(struct tokens *tokens) {
  char* path = tokens_get_token(tokens, 1);
  int status = 0;

  if (path == NULL) {
    path = "";
  }


  status = chdir(path);
  return status;
}

int cmd_wait(unused struct tokens *tokens) {
  pid_t cpid;
  int status = 0;
  while ((cpid = waitpid(-1, &status, WUNTRACED)) != -1);
  return 0;
}

int cmd_pwd(unused struct tokens *tokens) {
  size_t size = 32;
  char *directory_buffer = NULL;
  char *return_ptr = NULL;
  int status = 0;
  while (1) {
    if (realloc(directory_buffer, size) == NULL) {
      status = -1;
      break;
    }
    return_ptr = getcwd(directory_buffer, size);
    if (return_ptr != NULL) {
      printf("%s\n", return_ptr);
      break;
    } if (errno == ERANGE) {
      //need to resize...
      size *= 2;
    } else {
      status = -1;
      break;
    }

  }
  clean_up(directory_buffer);


  return status;
}

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens *tokens) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens *tokens) {
  exit(0);
}

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}

/* Intialization procedures for this shell */
void init_shell() {
  /* Our shell is connected to standard input. */
  shell_terminal = STDIN_FILENO;

  /* Check if we are running interactively */
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive) {
    /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
     * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
     * foreground, we'll receive a SIGCONT. */
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    /* Saves the shell's process id */
    shell_pgid = getpid();

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Save the current termios to a variable, so it can be restored later. */
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}


char* combine_words(char* word1, char* word2) {
  size_t size = strlen(word1) + strlen(word2) + 1;
  char* container = (char*) malloc((size + 1) * sizeof(char));

  int i = 0;
  while ((word1 + i) != NULL && word1[i] != '\0') {
    container[i] = word1[i];
    ++i;
  }
  container[i] = '/';
  ++i;
  int j = 0;
  while ((word2 + j) != NULL && word2[j] != '\0') {
    container[i] = word2[j];
    ++j;
    ++i;
  }
  container[size] = '\0';
  return container;
}

void print_argv(char ** argv) {
  if (argv != NULL) {
    char* arg = argv[0];
    int i = 0;
    while (arg != NULL) {
      printf("%s%d%s%s\n", "arg #", i, ": ", arg);
      ++i;
      arg = argv[i];
    }
  }
}

int redirection(char** argv) {
  if (argv != NULL) {
    char left[] = "<";
    char right[] = ">";
    int i = 1;
    char* ptr = argv[i];
    while(ptr != NULL) {
      if (strcmp(ptr, left) == 0) {
        return 1;
      } else if (strcmp(ptr, right) == 0) {
        return 2;
      }
      ptr = argv[i];
      ++i;
    }
  }
  return 0;
}

char** build_argv(char* path, struct tokens * tokens) {
  const int len = tokens_get_length(tokens);
  char** argv = (char**) malloc((len + 1) * sizeof(char*));
  argv[0] = path;
  for (int i = 1; i < len; ++i) {
    argv[i] = tokens_get_token(tokens, i);
  }
  argv[len] = NULL;
  return argv;
}

char* search_directories(char* cmd) {
  struct Node* directory = path_directories[0];
  char* path;
  while (directory != NULL) {
    path = combine_words(directory->data, cmd);
    if (access(path, F_OK) == 0) {
      return path;
    }
    free(path);
    path = NULL;
    directory = directory->next;
  }
  return NULL;
}

int write_to_file(char* buffer, char* filename, char* mode);
char* log_file = "log.txt";

bool has_background_arg(char** argv) {
  if (argv != NULL && argv[0] != '\0') {
    int i = 1;
    //i should start at 2 because argv should be {cmd, argv[1,...,end]}
    //and so the earliest index "&" can appear at is 1.
    //Thus we should begin by looking for NULL at 2.
    //This is not a big deal. i am just ranting. fml.
    while (argv[i] != NULL) {
      ++i;
    }
    if (strcmp(argv[i-1], "&") == 0) {
      argv[i-1] = NULL;
      return true;
    }
  }
  return false;
}

void child_handle(int s) {
  printf("%s\n", "recived ctrl-z! in chi");
}

int exec_to_stdout(char* path, char** argv) {
  int status = 0;
  //write_to_file("-----LOG START-----\n", log_file, "a");
  bool run_in_background = has_background_arg(argv);
  pid_t pid = fork();
  if (pid == -1) {
    //TODO: ERROR!
    perror("fork");
    return -1;
  }
  if (pid == 0) {
    signal(SIGINT,  SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGKILL, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGTSTP, child_handle);
    signal(SIGCONT, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    setpgid(getpid(), getpid());
    status = execv(path, argv);
  } else {
    tcsetpgrp(shell_terminal, pid);
    if (!run_in_background) {
      waitpid(pid,0,0);
    }
    tcsetpgrp(shell_terminal, shell_pgid);
  }

  //write_to_file("-----LOG END----\n\n", log_file, "a");
  return status;
}

char* resize(char* buffer, int size, int factor) {
  if (size <= 0) {
    return NULL;
  }
  if (buffer == NULL) {
    return (char*) malloc(size * sizeof(char));
  }
  if (factor <= 0) {
    return buffer;
  }
  int new_size = size * factor;
  char* new_buffer = (char*) malloc(new_size * sizeof(char));
  if (new_buffer == NULL) {
    return NULL;
  }
  for (int i = 0; i < size; ++i) {
    new_buffer[i] = buffer[i];
  }
  free(buffer);
  return new_buffer;
}


char* read_fd(int fd) {
  const int CHUNK_SIZE = 4096;
  int size = CHUNK_SIZE;
  char* buffer = resize(NULL, size, 2);
  ssize_t total_bytes = 0;
  ssize_t current_bytes;
  while (1) {
    current_bytes = read(fd, buffer + total_bytes, CHUNK_SIZE);
    if (current_bytes == -1) {
      //ERROR
      printf("%s\n", "(read() == -1) ==> FAILURE");
      if (buffer != NULL) {
        free(buffer);
      }
      return NULL;
    } else if (current_bytes == 0) {
      // EOF
      break;
    } else {
      total_bytes += current_bytes;
    }
    if (total_bytes >= size) {
      buffer = resize(buffer, size, 2);
      if (buffer == NULL) {
        return NULL;
      }
      size *= 2;
    }
  }
  buffer[total_bytes] = '\0';
  return buffer;
}

int write_to_file(char* buffer, char* filename, char* mode) {
  if (mode == NULL) {
    mode = "w";
  }
  if (buffer == NULL) {
    return -1;
  }

  FILE* file_ptr = fopen(filename, mode);
  if (file_ptr == NULL) {
    return -1;
  }
  if (fputs(buffer, file_ptr) < 0) {
    if (file_ptr != NULL) {
      fclose(file_ptr);
    }
    return -1;
  }
  return fclose(file_ptr);
}

int exec_to_file(char* path, char** argv) {
  int status = 0;
  int filedes[2];
  bool run_in_background = has_background_arg(argv);
  if (pipe(filedes) == -1) {
    printf("%s\n", "pipe(filedes) FAILURE");
    return -1;
  }

  int i = 0;
  while (strcmp(argv[i], ">") != 0) {
    ++i;
  }

  argv[i] = NULL;
  pid_t pid = fork();
  if (pid == -1) {
    printf("%s\n", "fork() FAILURE");
    return -1;
  } else if (pid == 0) {
    signal(SIGINT,  SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGKILL, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGCONT, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    while ((dup2(filedes[1], STDOUT_FILENO) == -1) && (errno == EINTR)) {}
    close(filedes[1]);
    close(filedes[0]);
    status = execv(path, argv);
    status = -1;
  } else {
    close(filedes[1]);
    tcsetpgrp(shell_terminal, pid);
    if (!run_in_background) {
      waitpid(pid,0,0);
    }
    tcsetpgrp(shell_terminal, shell_pgid);
    if (status < 0) {
      printf("%s\n", "child FAILED execv... close(filedes[0]);");
      close(filedes[0]);
      return status;
    }
    char* output = read_fd(filedes[0]);
    close(filedes[0]);
    if (output == NULL) {
      printf("%s\n", "read_fd() FAILURE");
      return -1;
    }
    status = write_to_file(output, argv[++i], "w");
    return status;
  }
  return status;
}

char* read_from_file(char* filename, char* mode) {
  if (mode == NULL) {
    mode = "r";
  }
  if (filename == NULL) {
    printf("%s\n", "read_from_file: filename is NULL");
    return NULL;
  }
  if (access(filename, F_OK) != 0) {
    printf("%s\n", "read_from_file: access() denied. See errno.");
    return NULL;
  }
  FILE* file_ptr = fopen(filename, mode);
  if (file_ptr == NULL) {
    printf("%s\n", "read_from_file: fopen() returned NULL");
  }
  int fd = fileno(file_ptr);
  if (fd == -1) {
    printf("%s\n", "fileno() return -1");
    fclose(file_ptr);
    return NULL;
  }
  char* buffer = read_fd(fd);
  fclose(file_ptr);
  return buffer;
}

size_t buffer_size(char* buffer) {
  size_t cnt = 0;
  while ((buffer + cnt) != NULL && buffer[cnt] != '\0') {
    ++cnt;
  }
  return cnt;
}

int exec_from_file(char* path, char** argv) {
  int i = 0;
  bool run_in_background = has_background_arg(argv);
  while (strcmp(argv[i], "<") != 0) {
    ++i;
  }
  argv[i] = NULL;
  char* buffer = read_from_file(argv[++i], "r");
  size_t cnt = buffer_size(buffer);
  if (buffer == NULL) {
    printf("%s\n", "error reading contents of file");
    return -1;
  }
  int status = 0;
  int pipefd[2];
  if (pipe(pipefd) == -1) {
    printf("%s\n", "pipe(pipefd) FAILURE");
    return -1;
  }

  int pid = fork();
  if (pid == -1) {
    printf("%s\n", "fork() FAILURE");
    if (buffer != NULL) {
      free(buffer);
    }
    return -1;
  } else if (pid == 0) {
    close(pipefd[1]);
    while ((dup2(pipefd[0], STDIN_FILENO) == -1) && (errno == EINTR)) {}
    close(pipefd[0]);
    signal(SIGINT,  SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGKILL, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGCONT, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    status = execv(path, argv);
  } else {
    close(pipefd[0]);
    write(pipefd[1], buffer, cnt);
    close(pipefd[1]);
    tcsetpgrp(shell_terminal, pid);
    if (!run_in_background) {
      waitpid(pid,0,0);
    }
    tcsetpgrp(shell_terminal, shell_pgid);
    if (buffer != NULL) {
      free(buffer);
    }
  }

  return status;
}

int execute_exteral_program(struct tokens *tokens) {
    char* path = tokens_get_token(tokens, 0);
    int status = access(path, F_OK);
    char* full_path = NULL;
    if (status != 0) {
      full_path = search_directories(path);
      status = (full_path != NULL) ? 0 : -1;
    }

    if (status == 0) {
      path = (full_path != NULL) ? full_path : path;
      char** argv = build_argv(path, tokens);
      direction = redirection(argv);
      switch (direction) {
        case NONE:
          status = exec_to_stdout(path, argv);
          break;
        case LEFT:
          status = exec_from_file(path, argv);
          break;
        case RIGHT:
          status = exec_to_file(path, argv);
          break;
        default:
          status = -1;
          break;
      }
      if (full_path != NULL) {
        free(full_path);
      }
      free(argv);
    }

    return status;
}

struct Node* new_node() {
  struct Node* node = (struct Node *) malloc(sizeof(struct Node));
  return node;
}

char* get_directory(char* source, int begin, int end) {
  int size = (end - begin);
  char* result = (char*) malloc((size + 1) * sizeof(char));
  for (int i = 0; i < size; ++i) {
    result[i] = source[begin + i];
  }
  result[size] = '\0';
  return result;
}

void generate_paths() {
  path_directories = (struct Node**) malloc(sizeof(struct Node*));
  *path_directories = NULL;
  char * path_value = getenv("PATH");
  if (path_value == NULL || path_value[0] == '\0') {
    return;
  }

  struct Node* head = NULL;
  struct Node* last = NULL;
  bool first = true;

  int begin = 0;
  int end = begin+1;

  while (1) {
    if (path_value[end] == ':' || path_value[end] == '\0') {
      char * new_directory = get_directory(path_value, begin, end);
      if (first) {
        head = new_node();
        head->data = new_directory;
        last = head;
        first = false;
      } else {
        struct Node* node = new_node();
        node->data = new_directory;
        last->next = node;
        last = node;
      }
      last->next = NULL;
      if (path_value[end] == '\0') {
        break;
      }
      begin = end+1;
    }
    ++end;
  }

  *path_directories = head;
}

void destroy_paths() {
  struct Node * directory = *path_directories;
  struct Node* next_node;
  while (directory != NULL) {
    next_node = directory->next;
    free(directory);
    directory = next_node;
  }
  free(path_directories);
}


bool empty_tokens(struct tokens* tokens) {
  return tokens_get_length(tokens) == 0;
}

void start_process_list() {
  active_processes = (struct Process **) malloc(sizeof(struct Process *));
  active_processes[0] = NULL;
}

void clear_process_list() {
  struct Process* process = active_processes[0];
  struct Process* temp;
  while (process != NULL) {
    temp = process->next;
    free(process);
    process = temp;
  }
}

void destroy_process_list() {
  clear_process_list();
  free(active_processes);
  active_processes = NULL;
}

int main(unused int argc, unused char *argv[]) {
  init_shell();
  generate_paths();
  start_process_list();
  //print_directories();
  //signal(SIGINT, SIG_IGN);
  static char line[4096];
  int line_num = 0;

  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive) {
    fprintf(stdout, "%d: ", line_num);
  }

  signal(SIGINT,  SIG_IGN);
  signal(SIGQUIT, SIG_IGN);
  signal(SIGKILL, SIG_IGN);
  signal(SIGTERM, SIG_IGN);
  signal(SIGTSTP, child_handle);
  signal(SIGCONT, SIG_IGN);
  signal(SIGTTIN, SIG_IGN);
  signal(SIGTTOU, SIG_IGN);
  int return_status;
  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens *tokens = tokenize(line);

    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));
    return_status = 0;
    if (fundex >= 0) {
      return_status = cmd_table[fundex].fun(tokens);
    } else if (!empty_tokens(tokens)) {
      /* REPLACE this to run commands as programs. */
      return_status = execute_exteral_program(tokens);
    }

    if (return_status < 0) {
      perror("ERROR");
    }

    if (shell_is_interactive) {
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);
    }
    /* Clean up memory */
    tokens_destroy(tokens);
  }

  destroy_paths();
  destroy_process_list();
  return 0;
}
