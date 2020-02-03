/*
 * mm_alloc.c
 *
 * Stub implementations of the mm_* routines.
 */

#include "mm_alloc.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
/******************************************************************************/
/******************************************************************************/
struct metadata {
  int _size;
  int _waste;
  bool _free;
  struct metadata* _prev;
  struct metadata* _next;
};
struct metadata * first_block = NULL;
struct metadata * last_block = NULL;
const size_t metadata_size = sizeof(struct metadata);
void print_list() {
  struct metadata * node = first_block;
  /****************************************************************************/
  printf("%s\n", "FORWARD:");
  while (node != NULL) {
    printf("%s%d%s%d%s%d%s", "{", node->_size, ", ", node->_waste, ", ", (int) node->_free, "} -> ");
    node = node->_next;
  }
  printf("%s\n", "NULL");
  /****************************************************************************/
  printf("%s\n", "BACKWARDS");
  node = last_block;
  while (node != NULL) {
    printf("%s%d%s%d%s%d%s", "{", node->_size, ", ", node->_waste, ", ", (int) node->_free, "} -> ");
    node = node->_prev;
  }
  printf("%s\n\n", "NULL");
}
/******************************************************************************/
/******************************************************************************/
void* request(size_t n) {
  void* curr = sbrk(n);
  if ((curr == ((void*) -1)) || ((sbrk(0) - curr) != n)) {
    return NULL;
  }
  return curr;
}

void split(struct metadata* block, size_t n) {
    void* split_addr = ((void*) block) + (metadata_size + n);
    memset(split_addr, 0, metadata_size);
    struct metadata* split_block = (struct metadata*) split_addr;
    split_block->_size = (block->_size + block->_waste) - (metadata_size + n);
    split_block->_waste = 0;
    split_block->_free = true;
    split_block->_prev = block;
    split_block->_next = block->_next;
    last_block = (last_block == block) ? split_block : last_block;
    block->_next = split_block;
    block->_waste = 0;
}

void* check_for_split(struct metadata* block, size_t n) {
  if ((block->_size + block->_waste) >= (2 * (n + metadata_size))) {
    split(block, n);
  } else {
    block->_waste = (block->_size + block->_waste) - n;
  }
  block->_free = false;
  block->_size = n;
  return ((void*) block);
}

void* check_blocks(size_t n) {
  struct metadata* node = first_block;
  while (node != NULL) {
    if (node->_free && (node->_size + node->_waste) >= n) {
      return check_for_split(node, n);
    }
    node = node->_next;
  }
  return NULL;
}

void* request_new_block(size_t n) {
  void* addr;
  if ((addr = request(n + metadata_size)) == NULL) {
    return NULL;
  }
  struct metadata* block = (struct metadata*) addr;
  block->_size = n;
  block->_waste = 0;
  block->_free = false;
  block->_prev = last_block;
  if (last_block != NULL) {
    last_block->_next = block;
  }
  last_block = block;
  first_block = (first_block == NULL) ? block : first_block;
  return addr;
}

void* zero_fill(void* addr) {
  struct metadata* block = (struct metadata*) addr;
  void* data_addr = addr + metadata_size;
  memset(data_addr, 0, block->_size);
  return data_addr;
}

void *mm_malloc(size_t size) {
    /* YOUR CODE HERE */
    void* loc = NULL;
    if (size > 0) {
      loc = ((loc = check_blocks(size)) != NULL) ?
              loc : request_new_block(size);
    }

    return (loc != NULL) ? zero_fill(loc) : NULL;
}
/******************************************************************************/
/******************************************************************************/
void *mm_realloc(void *ptr, size_t size) {
    /* YOUR CODE HERE */
    if (size == 0) {
      mm_free(ptr);
      return NULL;
    } else if (ptr == NULL) {
      return mm_malloc(size);
    }
    // now we know ptr is Not NULL and size > 0.
    struct metadata* block = (struct metadata*) (ptr - metadata_size);
    if (block->_size == size) {
      return ptr;
    }

    void* new_ptr = mm_malloc(size);
    if (new_ptr == NULL) {
      return NULL;
    }

    if (size > block->_size) {
      memset(new_ptr + block->_size, 0, size - block->_size);
    }
    memcpy(new_ptr, ptr,
      (block->_size < size) ? block->_size : size);
    mm_free(ptr);

    return new_ptr;
}
/******************************************************************************/
/******************************************************************************/
void combine(struct metadata* left_most, struct metadata* right_most) {
  size_t new_size = left_most->_size;

  struct metadata* next = left_most->_next;
  struct metadata* last = right_most->_next;

  while (next != last) {
    new_size += (metadata_size + next->_size + next->_waste);
    next = next->_next;
  }
  left_most->_size = new_size;
  left_most->_waste = 0;
  left_most->_next = last;
  if (last != NULL) {
    last->_prev = left_most;
  }

  left_most->_free = true;
  last_block = (last_block == right_most) ? left_most : last_block;
}

struct metadata* left_most_free_block(struct metadata* block) {
  struct metadata* left_most = block;
  while (left_most->_prev != NULL &&
          left_most->_prev->_free) {
    left_most = left_most->_prev;
  }
  return left_most;
}

struct metadata* right_most_free_block(struct metadata* block) {
  struct metadata* right_most = block;
  while (right_most->_next != NULL &&
          right_most->_next->_free) {
    right_most = right_most->_next;
  }
  return right_most;
}

void mm_free(void *ptr) {
    /* YOUR CODE HERE */
    if (ptr == NULL) {
      return;
    }
    struct metadata* block = (struct metadata*) (ptr - metadata_size);
    combine(left_most_free_block(block), right_most_free_block(block));
}
/******************************************************************************/
/******************************************************************************/
