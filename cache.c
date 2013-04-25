//
//  cache.c
//  proxylabxcode
//
//  Created by Lock on 4/22/13.
//  Copyright (c) 2013 Lock. All rights reserved.
//

#include <stdio.h>
#include "cache.h"

void cache_init() {
	Sem_init(&cache.mutex, 0, 1);
	Sem_init(&cache.w, 0, 1);
	cache.head = cache.tail = NULL;
	cache.size = 0;
	cache.readcnt = 0;
}

CacheObject *cache_get(char *uri) {
    printf("[Thread %u] :cache_get: P(&cache.mutex)\n", (unsigned int)pthread_self());
	P(&cache.mutex);
	cache.readcnt++;
	if (cache.readcnt == 1) {
    	printf("[Thread %u] :cache_get: P(&cache.w)\n", (unsigned int)pthread_self());
		P(&cache.w);
	}
	printf("[Thread %u] :cache_get: readcnt = %d\n", (unsigned int)pthread_self(), cache.readcnt);
    printf("[Thread %u] :cache_get: V(&cache.mutex)\n", (unsigned int)pthread_self());
	V(&cache.mutex);

	CacheObject *ptr = cache.head;
	printf("~~~~~~~ Looking for uri = %s\n", uri);
	while (ptr != NULL) {
		// printf("\tLooking cache uri = %s..\n", ptr->uri);
		/* Look for a cache object with the same uri.
			When found, move this object to the tail.
			Then return this object. */
		if (!strcasecmp(uri, ptr->uri)) {

			P(&cache.mutex);
			/* Rearrange prev/next pointers */
			if (ptr->prev != NULL)
				ptr->prev->next = ptr->next;
			else 
				cache.head = ptr->next;

			if (ptr->next != NULL)
				ptr->next->prev = ptr->prev;

			/* No tail pointer update if the object is the tail itself */
			if (cache.tail != ptr) {
				(cache.tail)->next = ptr;
				ptr->prev = cache.tail;
				ptr->next = NULL;
				cache.tail = ptr;
			}
			V(&cache.mutex);

			print_cache();

			printf("... CACHE HIT! :)\n");
		    printf("[Thread %u] :cache_get: P(&cache.mutex)\n", (unsigned int)pthread_self());
			P(&cache.mutex); /* Lock mutex */
			cache.readcnt--;
			if (cache.readcnt == 0) {
		    	printf("[Thread %u] :cache_get: V(&cache.w)\n", (unsigned int)pthread_self());
				V(&cache.w);
			}
			printf("[Thread %u] :cache_get: readcnt = %d\n", (unsigned int)pthread_self(), cache.readcnt);
			V(&cache.mutex); /* Unlock mutex */
		    printf("[Thread %u] :cache_get: V(&cache.mutex)\n", (unsigned int)pthread_self());

			/* Return this object */
			return ptr;
		}

		ptr = ptr->next;
	}
	printf("... cache missed :(\n");

    printf("[Thread %u] :cache_get: P(&cache.mutex)\n", (unsigned int)pthread_self());
	P(&cache.mutex); /* Lock mutex */
	cache.readcnt--;
	if (cache.readcnt == 0) {
    	printf("[Thread %u] :cache_get: V(&cache.w)\n", (unsigned int)pthread_self());
		V(&cache.w);
	}
	printf("[Thread %u] :cache_get: readcnt = %d\n", (unsigned int)pthread_self(), cache.readcnt);
	V(&cache.mutex); /* Unlock mutex */
    printf("[Thread %u] :cache_get: V(&cache.mutex)\n", (unsigned int)pthread_self());

	return NULL;
}

/* Assume that there's no object with the same uri in the cache */
int cache_insert(char *uri, void *data, size_t objectsize) {

	/* Ignore spurious call */
	if (objectsize == 0)
		return -1;

	/* Check whether object size exceed maximum size */
	if (objectsize > MAX_OBJECT_SIZE)
		return -2;


	/* Check whether cache has space to store a new object
		Evict until this new object fits the cache */
	while (cache.size + objectsize > MAX_CACHE_SIZE) {
		cache_evict();
		printf("Evicted (cache size = %u, and we need %u)\n", (unsigned int)cache.size, (unsigned int)objectsize);
	}

	/* Create a new cache object. Copy data to the cache object's */
	CacheObject *newobject = Calloc(1, sizeof(CacheObject));
	newobject->size = objectsize;
	newobject->data = Calloc(1, objectsize);
	newobject->uri = Calloc(1, strlen(uri));
	memcpy(newobject->uri, uri, strlen(uri));
	memcpy(newobject->data, data, objectsize);

	printf("[Thread %u] :insert: P(&cache.w)\n", (unsigned int)pthread_self());
	P(&cache.w); /* Lock writer */

	if (cache.head == NULL && cache.tail == NULL) {
		cache.head = newobject;
		cache.tail = newobject;
		newobject->prev = NULL;
		newobject->next = NULL;
	} else {
		/* Append this cache object to the tail & update cache's tail pointer */
		(cache.tail)->next = newobject;
		newobject->prev = cache.tail;
		newobject->next = NULL;
		cache.tail = newobject;
	}
	/* Update cache size */
	cache.size += objectsize;

	printf("[Thread %u] :insert: V(&cache.w)\n", (unsigned int)pthread_self());
	V(&cache.w); /* Unlock writer */

	print_cache();

	return 0;
}

/* Remove the first object in the cache. Then, decrease the cache size */
void cache_evict() {
	if (cache.head == NULL)
		return;

	printf("[Thread %u] :evict: P(&cache.w)\n", (unsigned int)pthread_self());
	P(&cache.w); /* Lock writer */

	CacheObject *victim = cache.head;
	cache.head = victim->next;
	(cache.head)->prev = NULL;
	Free(victim->data);
	Free(victim);

	cache.size -= victim->size;

	printf("[Thread %u] :evict: V(&cache.w)\n", (unsigned int)pthread_self());
	V(&cache.w); /* Unlock writer */
}

void print_cache() {
	return;
	printf("** Cache (size = %u) **\n", (unsigned int)cache.size);
	CacheObject *ptr = cache.head;
	while (ptr != NULL) {
		printf("\t%s\n", ptr->uri);
		ptr = ptr->next;
	}
	printf("** end **\n");
}

