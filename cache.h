//
//  cache.h
//  proxylabxcode
//
//  Created by Lock on 4/22/13.
//  Copyright (c) 2013 Lock. All rights reserved.
//

#ifndef proxylabxcode_cache_h
#define proxylabxcode_cache_h

#include "csapp.h"
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

typedef struct {
	struct cacheobject_t *prev, *next;
	size_t size;
	char *URI;
	void *buf;
} cacheobject_t;

typedef struct {
	cacheobject_t *head, *tail;
	size_t size;
	sem_t mutex;
} cache_t;

cache_t cache;

void cache_init();
cacheobject_t *cache_get(char *URI);
int cache_insert(char *URI, size_t size, void *buf);
void cache_evict();

#endif
