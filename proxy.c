#include <stdio.h>
#include <string.h>
#include "csapp.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define PORT 4647
#define HTTP_PORT 80

#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif

static const char *msg_http_version = "HTTP/1.0";
static const char *msg_user_agent = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *msg_accept = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *msg_accept_encoding = "Accept-Encoding: gzip, deflate\r\n";
static const char *msg_connection = "Connection: close\r\n";
static const char *msg_proxy_connection = "Proxy-Connection: close\r\n";

void doit(int connfd);
int parse_uri(char *uri, char *path);
int parse_host(rio_t *browser_rp, char *buf, char *host);
void write_defaulthdrs(int webserverfd,char *method,char *host,char *path);
void readwrite_requesthdrs(rio_t *browser_rio, int browserfd);
void clienterror(int connfd, char *cause, char *errnum,
    char *shortmsg, char *longmsg);

int main(int argc, char **argv)
{
    int listenfd, connfd, port;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;
    // struct hostent *hp;
    char *haddrp;

	// if (argc != 1) {
	// 	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	// 	exit(0);
	// }
	port = (argc != 2)?PORT:atoi(argv[1]);
    printf("Running proxy at port %d..\n", port);

    /* Listen to incoming clients.. forever */
    listenfd = Open_listenfd(port);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        /* Show information of connected client */
        // hp = Gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,
        //     sizeof(clientaddr.sin_addr.s_addr), AF_INET);
        haddrp = inet_ntoa(clientaddr.sin_addr);
        // printf("server connected to %s (%s)\n", hp->h_name, haddrp);
        printf("* Accepted connection from %s\n", haddrp);

        doit(connfd);
        Close(connfd);
    }

    return 0;
}


void doit(int browserfd) {
    int is_static;
    // struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char host[MAXLINE], path[MAXLINE];
    rio_t browser_rio, webserver_rio;
    int webserverfd, n;

    /* Read request line and headers, only GET method is supported */
    Rio_readinitb(&browser_rio, browserfd);
    Rio_readlineb(&browser_rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);
    dbg_printf("<< %s", buf);
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
    dbg_printf("* Connecting to %s..", host);
    webserverfd = Open_clientfd(host, HTTP_PORT);
    dbg_printf(" connected.\n");
    Rio_readinitb(&webserver_rio, webserverfd);

    /* Send HTTP header */
    write_defaulthdrs(webserverfd, method, host, path);

    /* Read the rest of the header and forward necessary ones */
    readwrite_requesthdrs(&browser_rio, webserverfd);

    /* Read response from the web server and forward it */
    while ((n = Rio_readlineb(&webserver_rio, buf, MAXLINE)) != 0) {
        Rio_writen(browserfd, buf, n);
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