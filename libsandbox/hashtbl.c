#include <assert.h>
#include <malloc.h>
#include <string.h>
#include "hashtbl.h"

const int min_buckets = 128;

// DJB2 hash
static uint32_t hashstring(const char *str) {
  uint32_t hash = 5381;
  uint32_t c;

  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  }

  return hash;
}

void hashtbl_init(Hashtable *h) {
  memset(h, 0, sizeof(*h));
}

void hashtbl_destroy(Hashtable *h) {
  HashResult hr = hashtbl_first(h);
  while (hr.present) {
    HashResult next = hashtbl_next(h, hr);
    hashtbl_remove(h, hr);
    hr = next;
  }

  assert(h->population == 0);
  free(h->buckets);
  h->buckets = NULL;
  h->num_buckets = 0;
}

static void transfer(ListElement *l, ListElement *to) {
  ListElement *prev = l->prev;
  ListElement *next = l->next;
  // unlink l
  l->prev->next = next;
  l->next->prev = prev;
  // append l into to
  l->prev = to->prev;
  l->prev->next = l;
  l->next = to;
  to->prev = l;
}

static void init_buckets(ListElement *buckets, uint32_t low, uint32_t high) {
  for (uint32_t i = low; i < high; i++) {
    ListElement *b = &buckets[i];
    b->next = b->prev = b;
  }
}

static void expand(Hashtable *h) {
  // Reallocate table to 2x size
  if (h->num_buckets == 0) {
    h->num_buckets = min_buckets;
    h->buckets = calloc(h->num_buckets, sizeof(ListElement));
    init_buckets(h->buckets, 0, min_buckets);
    return;
  }

  ListElement *new_buckets = calloc(h->num_buckets * 2, sizeof(ListElement));
  init_buckets(new_buckets, 0, h->num_buckets * 2);

  // Iterate hash numbers
  for (uint32_t i = 0; i < h->num_buckets; i++) {
    ListElement *item = h->buckets[i].next;
    while (item != &h->buckets[i]) {
      HashBucket *b = (HashBucket*)item;
      HashBucket *next = (HashBucket*)b->node.next;
      uint32_t newhash = b->hash & ((h->num_buckets * 2) - 1);
      transfer(&b->node, &new_buckets[newhash]);
      item = &next->node;
    }
  }

  free(h->buckets);
  h->buckets = new_buckets;
  h->num_buckets *= 2;
}

static HashResult make_result(HashBucket *b) {
  HashResult hr = { 0 };

  if (!b) {
    return hr;
  }

  hr.identifier = b;
  hr.string = b->string;
  hr.free_extra = b->free_extra;
  hr.extra = b->extra;
  hr.present = true;

  return hr;
}

HashResult hashtbl_lookup(Hashtable *h, const char *string) {
  if (!h->num_buckets) {
    return make_result(NULL);
  }

  uint32_t hashed = hashstring(string);
  ListElement *head = &h->buckets[hashed & (h->num_buckets - 1)];
  ListElement *next = head->next;

  while (next && next != head) {
    HashBucket *b = (HashBucket*)next;
    if (!strcmp(b->string, string)) {
      return make_result(b);
    }
    next = b->node.next;
  }

  return make_result(NULL);
}

HashResult hashtbl_insert(Hashtable *h, const char *string, free_extra_t free_extra, void *extra) {
  uint32_t hashed = hashstring(string);

  while ((h->population + 1) >= 3 * h->num_buckets / 4) {
    expand(h);
  }

  HashBucket *hb = calloc(1, sizeof(HashBucket) + strlen(string));
  hb->hash = hashed;
  hb->free_extra = free_extra;
  hb->extra = extra;
  strcpy(hb->string, string);

  uint32_t which_bucket = hashed & (h->num_buckets - 1);

  ListElement *target = &h->buckets[which_bucket];
  hb->node.prev = target->prev;
  hb->node.prev->next = &hb->node;
  target->prev = &hb->node;
  hb->node.next = target;

  h->population += 1;
  return make_result(hb);
}

void hashtbl_remove(Hashtable *h, HashResult element) {
  if (!element.present) {
    return;
  }

  HashBucket *b = element.identifier;
  ListElement *prev = b->node.prev;
  ListElement *next = b->node.next;
  prev->next = next;
  next->prev = prev;

  if (b->free_extra) {
    b->free_extra(b->extra);
  }

  free(b);
  h->population -= 1;
}

void hashtbl_modify(Hashtable *h, HashResult element, free_extra_t free_extra, void *extra) {
  if (!element.present) {
    return;
  }

  HashBucket *b = element.identifier;
  b->free_extra = free_extra;
  b->extra = extra;
}

HashResult hashtbl_first(Hashtable *h) {
  for (uint32_t i = 0; i < h->num_buckets; i++) {
    ListElement *l = &h->buckets[i];
    if (!l->next || l->next == l) {
      continue;
    }

    HashBucket *b = (HashBucket *)l->next;
    return make_result(b);
  }

  return make_result(NULL);
}

HashResult hashtbl_next(Hashtable *h, HashResult prev) {
  if (!prev.present) {
    return prev;
  }

  HashBucket *b = (HashBucket *)prev.identifier;
  HashBucket *n = (HashBucket *)b->node.next;
  uint32_t which_bucket = b->hash & (h->num_buckets - 1);
  if (&n->node == &h->buckets[which_bucket]) {
    // End of list, find another
    while (++which_bucket < h->num_buckets) {
      ListElement *l = &h->buckets[which_bucket];
      if (!l->next || l->next == l) {
        continue;
      }

      b = (HashBucket *)l->next;
      return make_result(b);
    }

    // None left
    return make_result(NULL);
  }

  return make_result(n);
}
