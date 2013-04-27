//
//  cache.c
//  proxylabxcode
//
//  Created by Lock on 4/22/13.
//  Copyright (c) 2013 Lock. All rights reserved.
//
/* This cache has been implemented using doubly-linked list data structure.
 *		with least-recently used eviction rule.
 * New cache object will be inserted at the end of the list.
 * A cache object that is retrieved from cache_get will be moved to the end.
 * Eviction will occur with the first object in the list.
 */

#include <stdio.h>
#include "cache.h"
#include <assert.h>

/* cache_init - Initialize the global cache variable */
void cache_init() {
	Sem_init(&cache.mutex, 0, 1);
	Sem_init(&cache.w, 0, 1);
	cache.head = cache.tail = NULL;
	cache.size = 0;
	cache.readcnt = 0;
}

/* cache_get - [Reader] Search and get a cached object from the cache
 *		If cache miss, return NULL.
 *		If cache hit, move the object to the end of the list then return it.
 *		Allow multiple readers to search the cache,
 *		but only 1 reader at a time to move it to the end of the list.
 */
CacheObject *cache_get(char *uri) {
    // printf("[Thread %u] :cache_get: P(&cache.mutex)\n", (unsigned int)pthread_self());
	P(&cache.mutex);
	cache.readcnt++;
	if (cache.readcnt == 1) {
    	// printf("[Thread %u] :cache_get: P(&cache.w)\n", (unsigned int)pthread_self());
		P(&cache.w);
	}
	// printf("[Thread %u] :cache_get: readcnt = %d\n", (unsigned int)pthread_self(), cache.readcnt);
    // printf("[Thread %u] :cache_get: V(&cache.mutex)\n", (unsigned int)pthread_self());
	V(&cache.mutex);

	CacheObject *ptr = cache.head;
	printf("~~~~~~~ Looking for uri = %s\n", uri);
	while (ptr != NULL) {

		/* Look for a cache object with the same uri.
			When found, move this object to the tail.
			Then return this object. */
		if (!strcasecmp(uri, ptr->uri)) {

			/* Rearrange prev/next, head/tail pointers */
			P(&cache.mutex);	/* Lock mutex */
			if (ptr->prev != NULL)
				ptr->prev->next = ptr->next;
			else 
				cache.head = ptr->next;
			/* Move it to the end if the object itself isn't tail */
			if (ptr->next != NULL) {
				ptr->next->prev = ptr->prev;
				(cache.tail)->next = ptr;
				ptr->prev = cache.tail;
				ptr->next = NULL;
				cache.tail = ptr;
			}
			V(&cache.mutex);	/* Unlock mutex */

			printf("... CACHE HIT! :)\n");
		    // printf("[Thread %u] :cache_get: P(&cache.mutex)\n", (unsigned int)pthread_self());
			P(&cache.mutex); /* Lock mutex */
			cache.readcnt--;
			if (cache.readcnt == 0) {
		    	// printf("[Thread %u] :cache_get: V(&cache.w)\n", (unsigned int)pthread_self());
				V(&cache.w);
			}
			// printf("[Thread %u] :cache_get: readcnt = %d\n", (unsigned int)pthread_self(), cache.readcnt);
		    // printf("[Thread %u] :cache_get: V(&cache.mutex)\n", (unsigned int)pthread_self());
			V(&cache.mutex); /* Unlock mutex */

			printf("******** After get *********\n");
			print_cache();
			
			/* Return this object */
			return ptr;
		}

		ptr = ptr->next;
	}

	printf("... cache missed :(\n");
    // printf("[Thread %u] :cache_get: P(&cache.mutex)\n", (unsigned int)pthread_self());
	P(&cache.mutex); /* Lock mutex */
	cache.readcnt--;
	if (cache.readcnt == 0) {
    	// printf("[Thread %u] :cache_get: V(&cache.w)\n", (unsigned int)pthread_self());
		V(&cache.w);
	}
	// printf("[Thread %u] :cache_get: readcnt = %d\n", (unsigned int)pthread_self(), cache.readcnt);
    // printf("[Thread %u] :cache_get: V(&cache.mutex)\n", (unsigned int)pthread_self());
	V(&cache.mutex); /* Unlock mutex */


	return NULL;
}

/* cache_insert - [Writer] Insert an object to the cache.
 *		Allow only 1 thread at a time to insert cache.
 */
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
	newobject->uri = Calloc(1, strlen(uri)+1);
	strncpy(newobject->uri, uri, strlen(uri));
	memcpy(newobject->data, data, objectsize);

	// printf("[Thread %u] :insert: P(&cache.w)\n", (unsigned int)pthread_self());
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

	// printf("[Thread %u] :insert: V(&cache.w)\n", (unsigned int)pthread_self());
	V(&cache.w); /* Unlock writer */

	printf("[Thread %u] After inserted *********\n", (unsigned int)pthread_self());
	check_cache();
	print_cache();

	return 0;
}

/* cache_evict - [Writer] Remove the first object in the cache. 
 *		Then, decrease the cache size.
 */
void cache_evict() {
	if (cache.head == NULL)
		return;

	// printf("[Thread %u] :evict: P(&cache.w)\n", (unsigned int)pthread_self());
	P(&cache.w); /* Lock writer */

	CacheObject *victim = cache.head;
	cache.head = victim->next;
	(cache.head)->prev = NULL;
	Free(victim->data);
	Free(victim);

	cache.size -= victim->size;

	// printf("[Thread %u] :evict: V(&cache.w)\n", (unsigned int)pthread_self());
	V(&cache.w); /* Unlock writer */

	printf("******** After evicted *********\n");
	check_cache();
	print_cache();

			
}

/* print_cache - Print all stored cache objected. For debug */
void print_cache() {
	// return;
	int objcnt = 0;
	printf("***** Cache (size = %u) ******\n", (unsigned int)cache.size);
	CacheObject *ptr = cache.head;
	while (ptr != NULL) {
		objcnt++;
		printf("p[%d]\t%s\n", objcnt, ptr->uri);
		ptr = ptr->next;
	}
	printf("*** end (object count = %d) ***\n", objcnt);
}

/* check_cache - Check cache consistency */
void check_cache() {
	printf("Checking cache consistency (expected cache.size = %u)..\n", 
		(unsigned int)cache.size);

	P(&cache.mutex);
	P(&cache.w);
	int objcnt = 0;
	size_t csize = 0;
	CacheObject *ptr = cache.head;
	while (ptr != NULL) {
		printf("c[%d]\t%s\n", ++objcnt, ptr->uri);

		if (ptr->prev != NULL) {
			assert(ptr->prev->next == ptr);
		} else {
			assert(ptr == cache.head);
		}
		if (ptr->next != NULL) {
			assert(ptr->next->prev == ptr);
		} else {
			assert(ptr == cache.tail);
		}
		csize += ptr->size;

		ptr = ptr->next;
	}
	assert(cache.size == csize);
	V(&cache.w);
	V(&cache.mutex);
}