#include <stdio.h>
#include <string.h>
#include "csapp.h"
#include "ptypes.h"
#include "proxythread.h"
#include "cache.h"

#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif


#define PORT 4647
#define HTTP_PORT 80

static const char *msg_http_version = "HTTP/1.0";
static const char *msg_user_agent = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *msg_accept = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *msg_accept_encoding = "Accept-Encoding: gzip, deflate\r\n";
static const char *msg_connection = "Connection: close\r\n";
static const char *msg_proxy_connection = "Proxy-Connection: close\r\n";

/* About request/response */
void process_conn(int browserfd);
int parse_uri(char *uri, char *path);
int parse_host(rio_t *browser_rp, char *buf, char *host);
void write_defaulthdrs(int webserverfd,char *method,char *host,char *path);
void readwrite_requesthdrs(rio_t *browser_rio, int browserfd);
void clienterror(int browserfd, char *cause, char *errnum,
    char *shortmsg, char *longmsg);

/* About thread */
#define THREAD_POOL_SIZE 10
#define SBUFSIZE 200

void *request_handler(void *vargp);
sbuf_t sbuf; /* Shared buffer of connected descriptors */

/* main - TODO: put comment here */
int main(int argc, char **argv)
{
    int listenfd, browserfd, port, i;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;
    // struct hostent *hp;
    // char *haddrp;
    pthread_t tid[THREAD_POOL_SIZE];

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
        Pthread_create(&tid[i], NULL, request_handler, NULL);
    }
    printf("done!\n");

    sbuf_init(&sbuf, SBUFSIZE);

    /* Listen to incoming clients.. forever */
    listenfd = Open_listenfd(port);
    
    printf("Listening on port %d\n", port);
    while (1) {
        clientlen = sizeof(clientaddr);
        browserfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        sbuf_insert(&sbuf, browserfd); /* Insert browserfd in buffer */
        printf("Accepted browserfd = %d to s_buf\n", browserfd);

//        /* Show information of connected client */
//        hp = Gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,
//            sizeof(clientaddr.sin_addr.s_addr), AF_INET);
//        haddrp = inet_ntoa(clientaddr.sin_addr);
//        printf("server connected to %s (%s)\n", hp->h_name, haddrp);
//
//        doit(browserfd);
//        Close(browserfd);
    }

    sbuf_deinit(&sbuf);

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

    /* Wait for the job, which is a browser's file descriptor in s_buf
        After this thread got the browserfd, it then process it, close it,
        and finally wait for the other one. Do this forever */
    while (1) {
        int browserfd = sbuf_remove(&sbuf);
        dbg_printf("[Thread %u] is handling browserfd = %d\n", (unsigned int)pthread_self(), browserfd);
        process_conn(browserfd);
        Close(browserfd);
        dbg_printf("    [Thread %u] has finished the job.\n", (unsigned int)pthread_self());
    }
    
    /* The thread never reaches here because of the while loop */
    return NULL;
}


/* 
 * process_conn - Process the connection
 * This function process the connection's request to GET data on the web server.
 */
void process_conn(int browserfd) {
    int is_static;
    // struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char host[MAXLINE], path[MAXLINE], cachebuf[MAX_OBJECT_SIZE];
    rio_t browser_rio, webserver_rio;
    int webserverfd, n, cachebuf_size, is_exceeded_max_object_size;

    /* Read request line and headers, only GET method is supported */
    Rio_readinitb(&browser_rio, browserfd);
    Rio_readlineb(&browser_rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);
    dbg_printf("[Thread %u] << %s", (unsigned int)pthread_self(), buf);
    if (strcasecmp(method, "GET")) {
        clienterror(browserfd, method, "501", "Not Implemented",
            "This proxy doesn't implement this method.");
        return;
    }

    /* Extract path from URI and get document type (for caching purpose) */
    is_static = parse_uri(uri, path);

    /* Read Host value and store it */
    if (!parse_host(&browser_rio, buf, host)) {
        clienterror(browserfd, method, "501", "Host header not found",
            "No host header was found.");
        return;
    }

    /* Connect to the server specified by "host" */
    // dbg_printf("* [Thread %u] Connecting to %s..", (unsigned int)pthread_self(), host);
    webserverfd = Open_clientfd(host, HTTP_PORT);
    // dbg_printf(" connected.\n");
    Rio_readinitb(&webserver_rio, webserverfd);

    /* Send HTTP header */
    write_defaulthdrs(webserverfd, method, host, path);

    /* Read the rest of the header and forward necessary ones */
    readwrite_requesthdrs(&browser_rio, webserverfd);

    /* Read each line of response from the web server
        Accumulate it to the cache buffer, then forward it to the browser */
    cachebuf_size = 0;
    is_exceeded_max_object_size = 0;
    while ((n = Rio_readlineb(&webserver_rio, buf, MAXLINE)) != 0) {

        //printf("++++++ Received %d from server\n", n);

        /* If accumulating this makes cachebuf exceeds max size, 
            then we don't need to put this cachebuf to the cache. */
        if (!is_exceeded_max_object_size &&
            cachebuf_size + n <= MAX_OBJECT_SIZE) {
            memcpy((cachebuf + cachebuf_size), buf, n);
            cachebuf_size += n;
        } else {
            is_exceeded_max_object_size = 1;
        }

        Rio_writen(browserfd, buf, n);
    }

    /* If this cachebuf doesn't exceed the maximum size,
        then put it in the cache */
    if (!is_exceeded_max_object_size) {
        int errorcode = cache_insert(uri, cachebuf, cachebuf_size);
        if (!errorcode) {
            printf("^^^^^^ Inserted cachebuf with size %d\n", cachebuf_size);
        } else {
            // TODO: To test robustness for BIG object
            printf("fuck %d\n",errorcode);
            exit(0);
        }
    }
    
    Close(webserverfd);
}

/*
 * parse_uri - parse URI and extract the path
 *             return 0 if dynamic content, 1 if static
 */
/* $begin parse_uri */
int parse_uri(char *uri, char *path) //, char *cgiargs) 
{
    char *ptr;

    /* Get path */
    ptr = strstr(uri, "://");
    ptr += 3;
    ptr = strchr(ptr, '/');
    strncpy(path, ptr, (int)strlen(uri));

    return 1; // Part I: Treat everything as static
    // char *ptr;

    // if (!strstr(uri, "cgi-bin")) {  /* Static content */
        // strcpy(cgiargs, "");
        // strcpy(filename, ".");
        // strcat(filename, uri);
        // if (uri[strlen(uri)-1] == '/')
        //     strcat(filename, "home.html");
    //     return 1;
    // } else {  /* Dynamic content */
        // ptr = index(uri, '?');
        // if (ptr) {
        //     strcpy(cgiargs, ptr+1);
        //     *ptr = '\0';
        // } else 
        //     strcpy(cgiargs, "");
        // strcpy(filename, ".");
        // strcat(filename, uri);
    //     return 0;
    // }
}
/* $end parse_uri */

/* parse_host - Extract host from the header */
int parse_host(rio_t *browser_rp, char *buf, char *host) {
    char key[MAXLINE], value[MAXLINE];

    Rio_readlineb(browser_rp, buf, MAXLINE);
    sscanf(buf, "%s %s", key, value);
    if (strcasecmp(key, "Host:")) {
        return 0;
    }
    strncpy(host, value, strlen(value));
    host[strlen(value)] = '\0';
    return 1;
}

/* write_defaulthdrs - Write headers from requirement */
void write_defaulthdrs(int webserverfd,char *method,char *host,char *path) {
    char buf[MAXLINE];

    sprintf(buf, "%s %s %s\r\n", method, path, msg_http_version);
    Rio_writen(webserverfd, buf, strlen(buf));

    sprintf(buf, "Host: %s\r\n", host);
    Rio_writen(webserverfd, buf, strlen(buf));

    Rio_writen(webserverfd, (void *)msg_user_agent, 
        strlen(msg_user_agent));
    Rio_writen(webserverfd, (void *)msg_accept, 
        strlen(msg_accept));
    Rio_writen(webserverfd, (void *)msg_accept_encoding, 
        strlen(msg_accept_encoding));
    Rio_writen(webserverfd, (void *)msg_connection, 
        strlen(msg_connection));
    Rio_writen(webserverfd, (void *)msg_proxy_connection, 
        strlen(msg_proxy_connection));
}

/* readwrite_requesthdrs - Read the rest of the header
    and only forward lines that aren't specified in the writeup */
void readwrite_requesthdrs(rio_t *browser_rio, int browserfd) {
    char buf[MAXLINE], key[MAXLINE];

    do {
        /* Read each line, get only key and consider forwarding it */
        Rio_readlineb(browser_rio, buf, MAXLINE);
        // dbg_printf("<< %s", buf);
        strcpy(key, "");
        sscanf(buf, "%s", key);
        if (strcasecmp(key, "User-Agent") &&
            strcasecmp(key, "Accept:") &&
            strcasecmp(key, "Accept-Encoding:") &&
            strcasecmp(key, "Connection:") &&
            strcasecmp(key, "Proxy-Connection:")) {
            Rio_writen(browserfd, (void *)buf, strlen(buf));
        }
    } while (strcmp(buf, "\r\n"));
}

/* clienterror - Print error page */
void clienterror(int browserfd, char *cause, char *errnum,
    char *shortmsg, char *longmsg) {
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Proxy Error</title>");
    sprintf(body, "%s<h1>%s: %s</h1>\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s</p>\r\n", body, longmsg, cause);
    sprintf(body, "%s</html>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(browserfd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(browserfd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(browserfd, buf, strlen(buf));
    Rio_writen(browserfd, body, strlen(body));

}

/* 
 * Return 1 if the header contain fields that identify the request object 
 * can be retrived from our cache.
 * Return 0 otherwise.
 * !BE CAREFUL, this will be executed in thread.
 */
int contain_cache_header(char *buf){
    // TODO
    return 0;
}


/*
 * Return 1 if the requested url is in our cache already; return 0 otherwise.
 * !BE CAREFUL, this will be executed in thread.
 */
int in_cache(char *url){
    // TODO
    return 0;
}

/* 
 * Write cached object back to the client. 
 * !BE CAREFUL, this will be executed in thread.
 */
int send_from_cache(char *url, int client_fd){
    // TODO
    return 0;
}

/* 
 * Forward the request from the client to the Internet. Send the result back
 * to the client. Finally, update HIT counter in cache module to support LRU 
 * policy.
 * !BE CAREFUL, this will be executed in thread.
 */
int get_from_internet(char *buf, int client_fd){
    // TODO
    return 0;
}