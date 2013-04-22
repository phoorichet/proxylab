#include <stdio.h>
#include <string.h>
#include "csapp.h"
#include "ptypes.h"
#include "proxythread.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define THREAD_POOL_SIZE 10
#define SBUFSIZE 20

#define PORT 4647

static const char *msg_user_agent = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *msg_accept = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *msg_accept_encoding = "Accept-Encoding: gzip, deflate\r\n";

void doit(int connfd);
void read_requesthdrs(rio_t *rp);
void clienterror(int connfd, char *cause, char *errnum,
    char *shortmsg, char *longmsg);
int parse_uri(char *uri, char *hostname, char *path);
void *request_handler(void *vargp);

sbuf_t sbuf; /* Shared buffer of connected descriptors */

int main(int argc, char **argv)
{
    int listenfd, connfd, port, i;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;
    struct hostent *hp;
    char *haddrp;
    pthread_t tid;

	// if (argc != 1) {
	// 	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	// 	exit(0);
	// }
//    printmf("%s%s%s", msg_user_agent, msg_accept, msg_accept_encoding);
	port = (argc != 2)?PORT:atoi(argv[1]);
    printf("Running proxy at port %d..\n", port);
  
    printf("Pre-forking %d working threads..", THREAD_POOL_SIZE);
    /* Prefork thread to the thread pool */
    for (i = 0; i < THREAD_POOL_SIZE; i++) {
        Pthread_create(&tid, NULL, request_handler, NULL);
    }
    printf("done!\n");

    sbuf_init(&sbuf, SBUFSIZE);
    /* Listen to incoming clients.. forever */
    listenfd = Open_listenfd(port);
    
    printf("Listening on port %d\n", port);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        printf("Accpept: connfd: %d\n", connfd);
        sbuf_insert(&sbuf, connfd); /* Insert connfd in buffer */

//        /* Show information of connected client */
//        hp = Gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,
//            sizeof(clientaddr.sin_addr.s_addr), AF_INET);
//        haddrp = inet_ntoa(clientaddr.sin_addr);
//        printf("server connected to %s (%s)\n", hp->h_name, haddrp);
//
//        doit(connfd);
//        Close(connfd);
    }

    return 0;
}


/*
 * Thread routine once proxy server accept a request.
 * This function will do:
 * 1. Check the request header
 *    - Ignore dynamic object
 * TBD
 *
 * Be aware of cleaning all open file desciripters.
 */
void *request_handler(void *vargp){
    
    Pthread_detach(pthread_self());
    while (1) {
        int connfd = sbuf_remove(&sbuf); /* Remove connfd from buffer */
        printf("[Thread %d] handling fd: %d\n", pthread_self(), connfd);
        doit(connfd);
        Close(connfd);
    }
    
    return NULL;
}



void doit(int connfd) {
    // struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE],
        hostname[MAXLINE], path[MAXLINE];
    rio_t rio;

    /* Read request line and headers */
    Rio_readinitb(&rio, connfd);
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);
    printf("<< %s", buf);
    /* Only support GET */
    if (strcasecmp(method, "GET")) {
        clienterror(connfd, method, "501", "Not Implemented",
            "This proxy doesn't implement this method.");
        return;
    }
    if (parse_uri(uri, hostname, path)) {
        clienterror(connfd, method, "501", "Unrecognizable URI",
            "This proxy doesn't know how to recognize this URI.");
        return;
    }
    printf("* Hostname:%s Path:%s\n",hostname,path);

    read_requesthdrs(&rio);


}

/* read_requesthdrs - Read the rest of header
    But we ignore them */
void read_requesthdrs(rio_t *rp) {
    char buf[MAXLINE];

    do {
        Rio_readlineb(rp, buf, MAXLINE);
        printf("<< %s", buf);
    } while (strcmp(buf, "\r\n"));
}

void clienterror(int connfd, char *cause, char *errnum,
    char *shortmsg, char *longmsg) {
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Proxy Error</title>");
    sprintf(body, "%s<h1>%s: %s</h1>\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s</p>\r\n", body, longmsg, cause);
    sprintf(body, "%s</html>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(connfd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(connfd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(connfd, buf, strlen(buf));
    Rio_writen(connfd, body, strlen(body));

}

int parse_uri(char *uri, char *hostname, char *path) {
    char *ptr_begin, *ptr_end;
    int hostname_length = 0;

    /* Get hostname */
    ptr_begin = strstr(uri, "://");
    ptr_begin += 3;
    ptr_end = strchr(ptr_begin, '/');
    hostname_length = (int)(ptr_end-ptr_begin);
    strncpy(hostname, ptr_begin, hostname_length);
    hostname[hostname_length] = '\0';

    /* Get path */
    ptr_begin = ptr_end;
    strncpy(path, ptr_begin, strlen(uri));

    return 0;
}