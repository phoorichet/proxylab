//
//  types.c
//  proxylabxcode
//
//  Created by Lock on 4/22/13.
//  Copyright (c) 2013 Lock. All rights reserved.
//

#include <stdio.h>
#include "proxy.h"
#include "csapp.h"
#include "ptypes.h"

int v_mutex,v_slots,v_items;

/* Create an empty, bounded, shared FIFO buffer with n slots */
void sbuf_init(sbuf_t *sp, int n)
{
    sp->buf = Calloc(n, sizeof(int));
    sp->n = n;                       /* Buffer holds max of n items */
    sp->front = sp->rear = 0;        /* Empty buffer iff front == rear */
    Sem_init(&sp->mutex, 0, 1);      /* Binary semaphore for locking */  
    v_mutex = 1; 
    Sem_init(&sp->slots, 0, n);      /* Initially, buf has n empty slots */
    v_slots = n;
    Sem_init(&sp->items, 0, 0);      /* Initially, buf has zero data items */
    v_items = 0;
}

/* Clean up buffer sp */
void sbuf_deinit(sbuf_t *sp)
{
    Free(sp->buf);
}

/* Insert item onto the rear of shared buffer sp */
void sbuf_insert(sbuf_t *sp, int item)
{
    dbg_printf("[Thread %u] :sbuf_insert: P(&sp->slots) [%d]\n", 
        (unsigned int)pthread_self(), v_slots);
    P(&sp->slots);                          /* Wait for available slot */
    v_slots--;
    dbg_printf("[Thread %u] :sbuf_insert: P(&sp->mutex) [%d]\n", 
        (unsigned int)pthread_self(), v_mutex);
    P(&sp->mutex);                          /* Lock the buffer */
    v_mutex--;
    sp->buf[(++sp->rear)%(sp->n)] = item;   /* Insert the item */
    V(&sp->mutex);                          /* Unlock the buffer */
    dbg_printf("[Thread %u] :sbuf_insert: V(&sp->mutex) [%d]\n", 
        (unsigned int)pthread_self(), ++v_mutex);
    V(&sp->items);                          /* Announce available item */
    dbg_printf("[Thread %u] :sbuf_insert: V(&sp->items) [%d]\n", 
        (unsigned int)pthread_self(), ++v_items);
}

/* Remove and return the first item from buffer sp */
int sbuf_remove(sbuf_t *sp)
{
    int item;
    dbg_printf("[Thread %u] :sbuf_remove: P(&sp->items) [%d]\n", 
        (unsigned int)pthread_self(), v_items);
    P(&sp->items);                          /* Wait for available item */
    v_items--;
    dbg_printf("[Thread %u] :sbuf_remove: P(&sp->mutex) [%d]\n", 
        (unsigned int)pthread_self(), v_mutex);
    P(&sp->mutex);                          /* Lock the buffer */
    v_mutex--;
    item = sp->buf[(++sp->front)%(sp->n)];  /* Remove the item */
    V(&sp->mutex);                          /* Unlock the buffer */
    dbg_printf("[Thread %u] :sbuf_insert: V(&sp->mutex) [%d]\n", 
        (unsigned int)pthread_self(), ++v_mutex);
    V(&sp->slots);                          /* Announce available slot */
    dbg_printf("[Thread %u] :sbuf_remove: V(&sp->slots) [%d]\n", 
        (unsigned int)pthread_self(), ++v_slots);
    return item;
}
