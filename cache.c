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
	cache.head = cache.tail = NULL;
	cache.size = 0;
}

CacheObject *cache_get(char *uri) {
	CacheObject *ptr = cache.head;

	while (ptr != NULL) {
		/* Look for a cache object with the same uri.
			When found, move this object to the tail.
			Then return this object. */
		if (!strcmp(uri, ptr->uri)) {
			/* Rearrange prev/next pointers */
			if (ptr->prev != NULL)
				ptr->prev->next = ptr->next;
			if (ptr->next != NULL)
				ptr->next->prev = ptr->prev;

			/* No tail pointer update if the object is the tail itself */
			if (cache.tail != ptr) {
				(cache.tail)->next = ptr;
				ptr->prev = cache.tail;
				ptr->next = NULL;
				cache.tail = ptr;
			}

			/* Return this object */
			return ptr;
		}

		ptr = ptr->next;
	}
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
		printf("Evicted (cache size = %d, and we need %d)\n", cache.size, objectsize);
	}

	/* Create a new cache object. Copy data to the cache object's */
	CacheObject *newobject = Malloc(sizeof(CacheObject));
	newobject->size = objectsize;
	newobject->data = Malloc(objectsize);
	memcpy(newobject->data, data, objectsize);

	/* Append this cache object to the tail & update cache's tail pointer */
	if (cache.tail != NULL) {
		(cache.tail)->next = newobject;
		newobject->prev = cache.tail;
		newobject->next = NULL;
		cache.tail = newobject;
	} else {
		newobject->prev = NULL;
		newobject->next = NULL;
		cache.tail = newobject;
	}

	/* Update cache size */
	cache.size += objectsize;

	return 0;
}

/* Remove the first object in the cache. Then, decrease the cache size */
void cache_evict() {
	if (cache.head == NULL)
		return;

	CacheObject *victim = cache.head;
	cache.head = victim->next;
	(cache.head)->prev = NULL;
	Free(victim);

	cache.size -= victim->size;
}

