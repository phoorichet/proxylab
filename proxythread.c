//
//  proxythread.c
//  proxylabxcode
//
//  Created by Lock on 4/22/13.
//  Copyright (c) 2013 Lock. All rights reserved.
//

#include <stdio.h>
#include "csapp.h"
#include "ptypes.h"

//extern sbuf_t sbuf;

///* 
// * Thread routine once proxy server accept a request.
// * This function will do:
// * 1. Check the request header
// *    - Ignore dynamic object
// * TBD
// *
// * Be aware of cleaning all open file desciripters.
// */
//void *request_handler(void *vargp){
//  
//    Pthread_detach(pthread_self());
//    while (1) {
//        int connfd = sbuf_remove(&sbuf); /* Remove connfd from buffer */ 
//        // TBD server the request
//        Close(connfd);
//    }
//  
//  return NULL;
//}
