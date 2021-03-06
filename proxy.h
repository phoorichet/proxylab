#include "ptypes.h"

#define PORT 4647
#define HTTP_PORT 80

// #define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif

/* About request/response */
void *request_handler(void *vargp);
void process_conn(int browserfd);
// void parse_uri(char *uri, char *path);
int readline_host(rio_t *browser_rp, char *buf, char *host);
void write_defaulthdrs(int webserverfd,char *method,char *host,char *path);
void readwrite_requesthdrs(rio_t *browser_rio, int browserfd);
void clienterror(int browserfd, char *cause, char *errnum,
    char *shortmsg, char *longmsg);

/* About thread */
#define THREAD_POOL_SIZE 10
#define SBUFSIZE 300

pthread_t tid[THREAD_POOL_SIZE];
sbuf_t sbuf; /* Shared buffer of descriptors of connected client */
jmp_buf jmp_buf_env; /* For setjmp() longjmp() environment */

/* Sigpipe handler */
void sigpipe_handler(int sig);