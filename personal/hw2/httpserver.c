#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unistd.h>
#include <stdbool.h>

#include "libhttp.h"
#include "wq.h"

/*
 * Global configuration variables.
 * You need to use these in your implementation of handle_files_request and
 * handle_proxy_request. Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
wq_t work_queue;
int num_threads;
int server_port;
char *server_files_directory;
char *server_proxy_hostname;
int server_proxy_port;


struct Node {
  pthread_t* thread;
  struct Node* next;
};

struct Node* new_node(pthread_t* thread, struct Node* next) {
  struct Node* node = (struct Node*) malloc(sizeof(struct Node));
  node->thread = thread;
  node->next = next;
  return node;
}


char* read_fd(int fd) {
  return NULL;
}

void write_fd(int fd, char* buf) {

}

void thread_exit(int sig) {
  /* TODO: Release resources (memory, locks, etc.) */
  exit(3);
}

void thread_serve_forever(void (*request_handler)(int)) {
  while (1) {
    int fd = wq_pop(&work_queue);
    request_handler(fd);
    close(fd);
  }
}

void * thread_logic(void * handle_ptr) {
  // declare pointer to a function of the type of request_handler
  // cast handle_ptr to be of this type
  void (*request_handler)(int) = handle_ptr;
  // just like main(), serve forever
  //signal(SIGINT, thread_exit);
  thread_serve_forever(request_handler);
  return NULL;
}

bool is_file(const char* path) {
  struct stat statbuff;
  stat(path, &statbuff);
  return S_ISREG(statbuff.st_mode);
}

bool is_directory(const char* path) {
  struct stat statbuff;
  stat(path, &statbuff);
  if (!S_ISDIR(statbuff.st_mode)) {
    return false;
  }
  DIR * dr = opendir(path);
  if (dr == NULL) {
    return false;
  }
  closedir(dr);
  return true;
}

char* combine_words(char* word1, char* word2) {
  if (word1 == NULL) {
    word1 = "";
  }
  if (word2 == NULL) {
    word2 = "";
  }
  size_t size = strlen(word1) + strlen(word2); //+ 1;
  char* container = (char*) malloc((size + 1) * sizeof(char));

  int i = 0;
  while ((word1 + i) != NULL && word1[i] != '\0') {
    container[i] = word1[i];
    ++i;
  }
  //container[i] = '/';
  //++i;
  int j = 0;
  while ((word2 + j) != NULL && word2[j] != '\0') {
    container[i] = word2[j];
    ++j;
    ++i;
  }
  container[size] = '\0';
  return container;
}

char* combine_array(char* arr[]) {
  size_t size = 0;
  char* str;
  int i = 0;
  while ((str = arr[i]) != NULL) {
    size += strlen(str);
    ++i;
  }
  char* result = (char*) malloc((size + 1) * sizeof(char));
  i = 0;
  int j = 0, k = 0;
  while ((str = arr[i]) != NULL) {
    k = 0;
    while ((str+k) != NULL && str[k] != '\0') {
      result[j] = str[k];
      ++j;
      ++k;
    }
    ++i;
  }
  result[size] = '\0';
  return result;
}

void http_send_file(int fd, int status_code, int size, char* type, char* body) {
  /****************************************************************************/
  //printf("%s\n", "http_send_file() CALLED");
  char size_buf[20];
  sprintf(size_buf, "%d", size);
  /****************************************************************************/
  http_start_response(fd, status_code);
  if (type != NULL) {
    http_send_header(fd, "Content-Type", type);
  } else {
    http_send_header(fd, "Content-Type", "");
  }
  http_send_header(fd, "Content-Length", size_buf);
  http_end_headers(fd);
  if (body != NULL) {
    //http_send_string(fd, body);
    http_send_data(fd, body, size);
  } else {
    http_send_string(fd, "");
  }
  /****************************************************************************/
}

void http_404_file(int fd) {
  http_send_file(fd, 404, 0, "", "");
}

int read_file(char* path, char** buf_ptr) {
  int file_des = open(path, 0);
  int file_size = lseek(file_des, 0, SEEK_END);
  lseek(file_des, 0, SEEK_SET);
  char* buf = (char*) malloc((file_size + 1) * sizeof(char));
  int bytes_read = read(file_des, buf, file_size);
  buf[bytes_read] = '\0';
  close(file_des);
  //return buf;
  *buf_ptr = buf;
  return file_size;
}

void http_get_file(int fd, char* path) {
  //printf("%s%s\n", "http_get_file()...PATH: ", path);
  /**************************************************************************/
  char** buf_ptr = (char **) malloc(sizeof(char *));
  int bytes_read = read_file(path, buf_ptr);
  char* type = http_get_mime_type(path);
  /**************************************************************************/
  http_send_file(fd, 200, bytes_read, type, *buf_ptr);
  /**************************************************************************/
  free(type);
  free(*buf_ptr);
  free(buf_ptr);
}

bool equal_strings(char* str1, char* str2) {
  return strcmp(str1, str2) == 0;
}

char* all_links_string(char* path) {
  //printf("%s%s\n", "all_links_string()....PATH", path);
  char* parent = "..";
  char* link_args[6] = {"<p><a href=\"", "", "/\">", "", "</a></p>\n", NULL};
  //char* full_path_args[4] = {path, "/", "", NULL};
  //char*  full_path = NULL;
  int name_index = 1, parent_index = 3;
  char* all_links = NULL;
  char* link = NULL;
  char* temp = NULL;
  DIR * dr = opendir(path);
  struct dirent * de;
  while ((de = readdir(dr)) != NULL) {
    //full_path_args[2] = de->d_name;
    //full_path = combine_array(full_path_args);
    //printf("%s%s\n", "CURRENT: ", de->d_name);
    //if (is_directory(full_path)) {
      //printf("%s\n", "DIRECTORY DETECTED");
      link_args[name_index] = de->d_name;
      //printf("%d\n", 1);
      link_args[parent_index] = equal_strings(de->d_name, parent) ?
                            "Parent directory" : de->d_name;
      //printf("%d\n", 2);
      link = combine_array(link_args);
      //printf("%d\n", 3);
      temp = all_links;
      all_links = combine_words(all_links, link);
      //printf("%d\n", 4);
      if (temp != NULL) {
        free(temp);
        //printf("%d\n", 5);
      }
      free(link);
      //printf("%d\n", 6);
    //}
    //free(full_path);
  }
  closedir(dr);
  //printf("%s%s\n", "all_links_string() FINISHED... all_links: ", all_links);
  return all_links;
}

char* get_all_links(char* path) {
  //printf("%s%s\n", "get_all_links()....PATH: ", path);
  char* all_links = all_links_string(path);
  char* begin = "<html>\n<body>\n";
  char* end =   "</body>\n</html>\n";
  char* argv[4] = {begin, "", end, NULL};
  char* results = NULL;
  if (all_links != NULL) {
    argv[1] = all_links;
    results = combine_array(argv);
    free(all_links);
  } else {
    results = combine_array(argv);
  }
  return results;
}

void http_get_directory_links(int fd, char* path) {
  //printf("%s%s\n", "http_get_directory_links()...PATH: ", path);
  char* all_links = get_all_links(path);
  http_send_file(fd, 200, strlen(all_links), "text/html", all_links);
  free(all_links);
}

void http_get_directory(int fd, char* path) {
  //printf("%s%s\n", "http_get_directory....PATH: ", path);
  char* index_path = combine_words(path, "/index.html");
  //printf("%s%s%s\n", "Testing: ", index_path, " for file access.");
  if (access(index_path, F_OK) == 0 || is_file(index_path)) {
    http_get_file(fd, index_path);
  } else {
    //printf("%s\n", "NOT FILE");
    http_get_directory_links(fd, path);
  }
  free(index_path);
}

/*
 * Reads an HTTP request from stream (fd), and writes an HTTP response
 * containing:
 *
 *   1) If user requested an existing file, respond with the file
 *   2) If user requested a directory and index.html exists in the directory,
 *      send the index.html file.
 *   3) If user requested a directory and index.html doesn't exist, send a list
 *      of files in the directory with links to each.
 *   4) Send a 404 Not Found response.
 */
void handle_files_request(int fd) {
  //printf("%s\n", "CALLED handle_files_request(int fd)");
  struct http_request *request = http_request_parse(fd);
  char* path_args[4] = {server_files_directory, "/", request->path, NULL};
  //printf("%s%s\n", "RECIEVED PATH: ", request->path);
  char* path = combine_array(path_args);
  //printf("%s%s\n", "handle_files_request->PATH: ", path);
  if (is_file(path)) {
    http_get_file(fd, path);
  } else if (is_directory(path)) {
    http_get_directory(fd, path);
  } else {
    http_404_file(fd);
  }
  free(request->method);
  free(request->path);
  free(request);
  free(path);
}

void serve_channel(int to_fd, int from_fd) {
  const size_t CHUNK_SIZE = 4096;
  char buf[CHUNK_SIZE];
  size_t bytes_read;
  while (1) {
    //sendfile(to_fd, from_fd, NULL, CHUNK_SIZE);
    bytes_read = read(from_fd, buf, CHUNK_SIZE);
    if (bytes_read > 0) {
      write(to_fd, buf, bytes_read);
    }
  }
}

void * channel_parser(void * args) {
  int* my_args = (int*) args;
  int downstream = my_args[0];
  int upstream = my_args[1];
  serve_channel(downstream, upstream);
  return NULL;
}

void proxy_helper(int downstream, int upstream) {
  /*
  * TODO: Your solution for task 3 belongs here!
  */
  /****************************************************************************/
  pthread_t helper;
  int helper_args[2];
  helper_args[0] = downstream;
  helper_args[1] = upstream;
  pthread_create(&helper, NULL, channel_parser, (void *) helper_args);
  /****************************************************************************/
  serve_channel(upstream, downstream);
  /****************************************************************************/
}

/*
 * Opens a connection to the proxy target (hostname=server_proxy_hostname and
 * port=server_proxy_port) and relays traffic to/from the stream fd and the
 * proxy target. HTTP requests from the client (fd) should be sent to the
 * proxy target, and HTTP responses from the proxy target should be sent to
 * the client (fd).
 *
 *   +--------+     +------------+     +--------------+
 *   | client | <-> | httpserver | <-> | proxy target |
 *   +--------+     +------------+     +--------------+
 */
void handle_proxy_request(int fd) {

  /*
  * The code below does a DNS lookup of server_proxy_hostname and
  * opens a connection to it. Please do not modify.
  */
  /****************************************************************************/
  struct sockaddr_in target_address;
  memset(&target_address, 0, sizeof(target_address));
  target_address.sin_family = AF_INET;
  target_address.sin_port = htons(server_proxy_port);
  struct hostent *target_dns_entry = gethostbyname2(server_proxy_hostname, AF_INET);
  int client_socket_fd = socket(PF_INET, SOCK_STREAM, 0);
  if (client_socket_fd == -1) {
    fprintf(stderr, "Failed to create a new socket: error %d: %s\n", errno, strerror(errno));
    exit(errno);
  }
  if (target_dns_entry == NULL) {
    fprintf(stderr, "Cannot find host: %s\n", server_proxy_hostname);
    exit(ENXIO);
  }
  char *dns_address = target_dns_entry->h_addr_list[0];
  memcpy(&target_address.sin_addr, dns_address, sizeof(target_address.sin_addr));
  int connection_status = connect(client_socket_fd, (struct sockaddr*) &target_address,
      sizeof(target_address));
  if (connection_status < 0) {
    /* Dummy request parsing, just to be compliant. */
    http_request_parse(fd);

    http_start_response(fd, 502);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    http_send_string(fd, "<center><h1>502 Bad Gateway</h1><hr></center>");
    return;

  }

  proxy_helper(fd, client_socket_fd);
}

 void init_thread_pool(int num_threads, void (*request_handler)(int)) {
  /*
   * TODO: Part of your solution for Task 2 goes here!
   */

   // if num_threads <= 0, then we only need the main thread.
   // otherwise, we create num_threads and initialize the work queue so that
   // these threads can dequeue available jobs (once main threads enqueues them)
   /*wq_init(&work_queue);
   for (int i = 0; i < num_threads; ++i) {
     pthread_t new_thread;
     pthread_create(&new_thread, NULL, thread_logic, request_handler);
   }*/
}

/*
 * Opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
void serve_forever(int *socket_number, void (*request_handler)(int)) {

  struct sockaddr_in server_address, client_address;
  size_t client_address_length = sizeof(client_address);
  int client_socket_number;

  *socket_number = socket(PF_INET, SOCK_STREAM, 0);
  if (*socket_number == -1) {
    perror("Failed to create a new socket");
    exit(errno);
  }

  int socket_option = 1;
  if (setsockopt(*socket_number, SOL_SOCKET, SO_REUSEADDR, &socket_option,
        sizeof(socket_option)) == -1) {
    perror("Failed to set socket options");
    exit(errno);
  }

  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(server_port);

  if (bind(*socket_number, (struct sockaddr *) &server_address,
        sizeof(server_address)) == -1) {
    perror("Failed to bind on socket");
    exit(errno);
  }

  if (listen(*socket_number, 1024) == -1) {
    perror("Failed to listen on socket");
    exit(errno);
  }

  printf("Listening on port %d...\n", server_port);

  /****************************************************************************/
  //init_thread_pool(num_threads, request_handler);
  wq_init(&work_queue);
  int i;
  for (i = 0; i < num_threads; ++i) {
    pthread_t new_thread;
    pthread_create(&new_thread, NULL, thread_logic, request_handler);
  }
  /****************************************************************************/

  while (1) {

    client_socket_number = accept(*socket_number,
        (struct sockaddr *) &client_address,
        (socklen_t *) &client_address_length);

    if (client_socket_number < 0) {
      perror("Error accepting socket");
      continue;
    }

    printf("Accepted connection from %s on port %d\n",
        inet_ntoa(client_address.sin_addr),
        client_address.sin_port);
    /**************************************************************************/
    // TODO: Change me?
    if (num_threads > 0) {
      // let my children handle this
      wq_push(&work_queue, client_socket_number);
    } else {
      // I am the only thread
      request_handler(client_socket_number);
      close(client_socket_number);
    }
    /**************************************************************************/
    printf("Accepted connection from %s on port %d\n",
        inet_ntoa(client_address.sin_addr),
        client_address.sin_port);
  }

  shutdown(*socket_number, SHUT_RDWR);
  close(*socket_number);
}

int server_fd;
void signal_callback_handler(int signum) {
  printf("Caught signal %d: %s\n", signum, strsignal(signum));
  printf("Closing socket %d\n", server_fd);
  if (close(server_fd) < 0) perror("Failed to close server_fd (ignoring)\n");
  exit(0);
}

char *USAGE =
  "Usage: ./httpserver --files www_directory/ --port 8000 [--num-threads 5]\n"
  "       ./httpserver --proxy inst.eecs.berkeley.edu:80 --port 8000 [--num-threads 5]\n";

void exit_with_usage() {
  fprintf(stderr, "%s", USAGE);
  exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
  signal(SIGINT, signal_callback_handler);

  /* Default settings */
  server_port = 8000;
  void (*request_handler)(int) = NULL;

  int i;
  for (i = 1; i < argc; i++) {
    if (strcmp("--files", argv[i]) == 0) {
      request_handler = handle_files_request;
      free(server_files_directory);
      server_files_directory = argv[++i];
      if (!server_files_directory) {
        fprintf(stderr, "Expected argument after --files\n");
        exit_with_usage();
      }
    } else if (strcmp("--proxy", argv[i]) == 0) {
      request_handler = handle_proxy_request;

      char *proxy_target = argv[++i];
      if (!proxy_target) {
        fprintf(stderr, "Expected argument after --proxy\n");
        exit_with_usage();
      }

      char *colon_pointer = strchr(proxy_target, ':');
      if (colon_pointer != NULL) {
        *colon_pointer = '\0';
        server_proxy_hostname = proxy_target;
        server_proxy_port = atoi(colon_pointer + 1);
      } else {
        server_proxy_hostname = proxy_target;
        server_proxy_port = 80;
      }
    } else if (strcmp("--port", argv[i]) == 0) {
      char *server_port_string = argv[++i];
      if (!server_port_string) {
        fprintf(stderr, "Expected argument after --port\n");
        exit_with_usage();
      }
      server_port = atoi(server_port_string);
    } else if (strcmp("--num-threads", argv[i]) == 0) {
      char *num_threads_str = argv[++i];
      if (!num_threads_str || (num_threads = atoi(num_threads_str)) < 1) {
        fprintf(stderr, "Expected positive integer after --num-threads\n");
        exit_with_usage();
      }
    } else if (strcmp("--help", argv[i]) == 0) {
      exit_with_usage();
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
      exit_with_usage();
    }
  }

  if (server_files_directory == NULL && server_proxy_hostname == NULL) {
    fprintf(stderr, "Please specify either \"--files [DIRECTORY]\" or \n"
                    "                      \"--proxy [HOSTNAME:PORT]\"\n");
    exit_with_usage();
  }

  serve_forever(&server_fd, request_handler);

  return EXIT_SUCCESS;
}
