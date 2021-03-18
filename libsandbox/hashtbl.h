#ifndef _SANDBOX_HASHTABLE_H_
#define _SANDBOX_HASHTABLE_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct _ListElement {
  struct _ListElement *prev;
  struct _ListElement *next;
} ListElement;

typedef void (*free_extra_t)(void *);

typedef struct _HashBucket {
  struct _ListElement node;
  uint32_t hash;
  free_extra_t free_extra;
  void *extra;
  char string[1];
} HashBucket;

typedef struct _Hashtable {
  uint32_t population;
  uint32_t num_buckets;
  struct _ListElement *buckets;
} Hashtable;

typedef struct _HashResult {
  void *identifier;
  free_extra_t free_extra;
  void *extra;
  const char *string;
  bool present;
} HashResult;

void hashtbl_init(struct _Hashtable *h);
void hashtbl_destroy(struct _Hashtable *h);

HashResult hashtbl_insert(struct _Hashtable *h, const char *string, free_extra_t free_extra, void *extra);
HashResult hashtbl_lookup(struct _Hashtable *h, const char *string);
void hashtbl_remove(struct _Hashtable *h, struct _HashResult element);
void hashtbl_modify(struct _Hashtable *h, struct _HashResult element, free_extra_t free_extra, void *extra);

HashResult hashtbl_first(struct _Hashtable *h);
HashResult hashtbl_next(struct _Hashtable *h, struct _HashResult prev);

#endif//_SANDBXO_HASHTABLE_H_
