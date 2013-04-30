#include "ptypes.h"

#define PORT 4647
#define HTTP_PORT 80

/* About request/response */
void process_conn(int browserfd);
void parse_uri(char *uri, char *path);
int parse_host(rio_t *browser_rp, char *buf, char *host);
int parse_header_by_pattern(const char *pattern ,char *buf, char *host);
void write_defaulthdrs(int webserverfd,char *method,char *host,char *path);
void readwrite_requesthdrs(rio_t *browser_rio, int browserfd);
void clienterror(int browserfd, char *cause, char *errnum,
    char *shortmsg, char *longmsg);

/* About thread */
#define THREAD_POOL_SIZE 10
#define SBUFSIZE 500

void *request_handler(void *vargp);
sbuf_t sbuf; /* Shared buffer of connected descriptors */