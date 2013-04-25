//
//  types.c
//  proxylabxcode
//
//  Created by Lock on 4/22/13.
//  Copyright (c) 2013 Lock. All rights reserved.
//

#include <stdio.h>
#include "csapp.h"
#include "ptypes.h"

/* Create an empty, bounded, shared FIFO buffer with n slots */
void sbuf_init(sbuf_t *sp, int n)
{
    sp->buf = Calloc(n, sizeof(int));
    sp->n = n;                       /* Buffer holds max of n items */
    sp->front = sp->rear = 0;        /* Empty buffer iff front == rear */
    Sem_init(&sp->mutex, 0, 1);      /* Binary semaphore for locking */   
    Sem_init(&sp->slots, 0, n);      /* Initially, buf has n empty slots */
    Sem_init(&sp->items, 0, 0);      /* Initially, buf has zero data items */
    
}


/* Clean up buffer sp */
void sbuf_deinit(sbuf_t *sp)
{
    Free(sp->buf);
}


/* Insert item onto the rear of shared buffer sp */

void sbuf_insert(sbuf_t *sp, int item)
{
    printf("[Thread %u] :sbuf_insert: P(&sp->slots)\n", (unsigned int)pthread_self());
    P(&sp->slots);                          /* Wait for available slot */
    printf("[Thread %u] :sbuf_insert: P(&sp->mutex)\n", (unsigned int)pthread_self());
    P(&sp->mutex);                          /* Lock the buffer */
    sp->buf[(++sp->rear)%(sp->n)] = item;   /* Insert the item */
    printf("[Thread %u] :sbuf_insert: V(&sp->mutex)\n", (unsigned int)pthread_self());
    V(&sp->mutex);                          /* Unlock the buffer */
    printf("[Thread %u] :sbuf_insert: V(&sp->items)\n", (unsigned int)pthread_self());
    V(&sp->items);                          /* Announce available item */
}


/* Remove and return the first item from buffer sp */
int sbuf_remove(sbuf_t *sp)
{
    
    int item;
    printf("[Thread %u] :sbuf_remove: P(&sp->items)\n", (unsigned int)pthread_self());
    P(&sp->items);                          /* Wait for available item */
    printf("[Thread %u] :sbuf_remove: P(&sp->mutex)\n", (unsigned int)pthread_self());
    P(&sp->mutex);                          /* Lock the buffer */
    item = sp->buf[(++sp->front)%(sp->n)];  /* Remove the item */
    printf("[Thread %u] :sbuf_remove: V(&sp->mutex)\n", (unsigned int)pthread_self());
    V(&sp->mutex);                          /* Unlock the buffer */
    printf("[Thread %u] :sbuf_remove: V(&sp->slots)\n", (unsigned int)pthread_self());
    V(&sp->slots);                          /* Announce available slot */
    return item;
}
