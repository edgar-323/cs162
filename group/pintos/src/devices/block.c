#include "devices/block.h"
#include <list.h>
#include <string.h>
#include <stdio.h>
#include "devices/ide.h"
#include "threads/malloc.h"
#include "threads/synch.h"

uint8_t cache_initialized = 0;
uint8_t freeze_cache = 0;
/* A block device. */
struct block
  {
    struct list_elem list_elem;         /* Element in all_blocks. */

    char name[16];                      /* Block device name. */
    enum block_type type;                /* Type of block device. */
    block_sector_t size;                 /* Size in sectors. */

    const struct block_operations *ops;  /* Driver operations. */
    void *aux;                          /* Extra data owned by driver. */

    unsigned long long read_cnt;        /* Number of sectors read. */
    unsigned long long write_cnt;       /* Number of sectors written. */
  };

/* List of all block devices. */
static struct list all_blocks = LIST_INITIALIZER (all_blocks);

/* The block block assigned to each Pintos role. */
static struct block *block_by_role[BLOCK_ROLE_CNT];

static struct block *list_elem_to_block (struct list_elem *);

/* Returns a human-readable name for the given block device
   TYPE. */
const char *
block_type_name (enum block_type type)
{
  static const char *block_type_names[BLOCK_CNT] =
    {
      "kernel",
      "filesys",
      "scratch",
      "swap",
      "raw",
      "foreign",
    };

  ASSERT (type < BLOCK_CNT);
  return block_type_names[type];
}

/* Returns the block device fulfilling the given ROLE, or a null
   pointer if no block device has been assigned that role. */
struct block *
block_get_role (enum block_type role)
{
  ASSERT (role < BLOCK_ROLE_CNT);
  return block_by_role[role];
}

/* Assigns BLOCK the given ROLE. */
void
block_set_role (enum block_type role, struct block *block)
{
  ASSERT (role < BLOCK_ROLE_CNT);
  block_by_role[role] = block;
}

/* Returns the first block device in kernel probe order, or a
   null pointer if no block devices are registered. */
struct block *
block_first (void)
{
  return list_elem_to_block (list_begin (&all_blocks));
}

/* Returns the block device following BLOCK in kernel probe
   order, or a null pointer if BLOCK is the last block device. */
struct block *
block_next (struct block *block)
{
  return list_elem_to_block (list_next (&block->list_elem));
}

/* Returns the block device with the given NAME, or a null
   pointer if no block device has that name. */
struct block *
block_get_by_name (const char *name)
{
  struct list_elem *e;

  for (e = list_begin (&all_blocks); e != list_end (&all_blocks);
       e = list_next (e))
    {
      struct block *block = list_entry (e, struct block, list_elem);
      if (!strcmp (name, block->name))
        return block;
    }

  return NULL;
}

/* Verifies that SECTOR is a valid offset within BLOCK.
   Panics if not. */
static void
check_sector (struct block *block, block_sector_t sector)
{
  if (sector >= block->size)
    {
      /* We do not use ASSERT because we want to panic here
         regardless of whether NDEBUG is defined. */
      PANIC ("Access past end of device %s (sector=%"PRDSNu", "
             "size=%"PRDSNu")\n", block_name (block), sector, block->size);
    }
}

/* Reads sector SECTOR from BLOCK into BUFFER, which must
   have room for BLOCK_SECTOR_SIZE bytes.
   Internally synchronizes accesses to block devices, so external
   per-block device locking is unneeded. */
void
block_read (struct block *block, block_sector_t sector, void *buffer)
{
  if (cache_initialized)
    {
      dcache_read (block, sector, buffer);
    }
  else
    {
      check_sector (block, sector);
      block->ops->read (block->aux, sector, buffer);
      block->read_cnt++;
    }
}

/* Write sector SECTOR to BLOCK from BUFFER, which must contain
   BLOCK_SECTOR_SIZE bytes.  Returns after the block device has
   acknowledged receiving the data.
   Internally synchronizes accesses to block devices, so external
   per-block device locking is unneeded. */
void
block_write (struct block *block, block_sector_t sector, const void *buffer)
{
  if (cache_initialized)
    {
      dcache_write (block, sector, buffer);
    }
  else
    {
      check_sector (block, sector);
      ASSERT (block->type != BLOCK_FOREIGN);
      block->ops->write (block->aux, sector, buffer);
      block->write_cnt++;
    }
}

/* Returns the number of sectors in BLOCK. */
block_sector_t
block_size (struct block *block)
{
  return block->size;
}

/* Returns BLOCK's name (e.g. "hda"). */
const char *
block_name (struct block *block)
{
  return block->name;
}

/* Returns BLOCK's type. */
enum block_type
block_type (struct block *block)
{
  return block->type;
}

/* Prints statistics for each block device used for a Pintos role. */
void
block_print_stats (void)
{
  int i;

  for (i = 0; i < BLOCK_ROLE_CNT; i++)
    {
      struct block *block = block_by_role[i];
      if (block != NULL)
        {
          printf ("%s (%s): %llu reads, %llu writes\n",
                  block->name, block_type_name (block->type),
                  block->read_cnt, block->write_cnt);
        }
    }
}

/* Registers a new block device with the given NAME.  If
   EXTRA_INFO is non-null, it is printed as part of a user
   message.  The block device's SIZE in sectors and its TYPE must
   be provided, as well as the it operation functions OPS, which
   will be passed AUX in each function call. */
struct block *
block_register (const char *name, enum block_type type,
                const char *extra_info, block_sector_t size,
                const struct block_operations *ops, void *aux)
{
  struct block *block = malloc (sizeof *block);
  if (block == NULL)
    PANIC ("Failed to allocate memory for block device descriptor");

  list_push_back (&all_blocks, &block->list_elem);
  strlcpy (block->name, name, sizeof block->name);
  block->type = type;
  block->size = size;
  block->ops = ops;
  block->aux = aux;
  block->read_cnt = 0;
  block->write_cnt = 0;

  printf ("%s: %'"PRDSNu" sectors (", block->name, block->size);
  print_human_readable_size ((uint64_t) block->size * BLOCK_SECTOR_SIZE);
  printf (")");
  if (extra_info != NULL)
    printf (", %s", extra_info);
  printf ("\n");

  return block;
}

/* Returns the block device corresponding to LIST_ELEM, or a null
   pointer if LIST_ELEM is the list end of all_blocks. */
static struct block *
list_elem_to_block (struct list_elem *list_elem)
{
  return (list_elem != list_end (&all_blocks)
          ? list_entry (list_elem, struct block, list_elem)
          : NULL);
}

/* Filesys cache. */

struct cache_block;
struct cache_block * search_cache (struct block* block, block_sector_t sector);
int flag_helper (uint8_t value, uint8_t mask);
int is_dirty (struct cache_block* cache_block);
int is_valid (struct cache_block* cache_block);
void mark_dirty (struct cache_block* cache_block);
void mark_valid (struct cache_block* cache_block);
int cache_hit (struct block* block, block_sector_t sector,
            struct cache_block* cache_block);
void flush_block (struct cache_block* cache_block);
struct cache_block *make_eviction (struct block* block, block_sector_t sector);
struct cache_block *dcache_alloc (struct block *block, block_sector_t sector);
void clear_block (struct cache_block* cache_block);
void evict_block (struct cache_block* cache_block);
void set_block (struct block* block, block_sector_t sector,
                  struct cache_block* cache_block);

const uint8_t CACHE_SIZE = 64;
const unsigned int CHANCES = 3;
const uint8_t DIRTY_BIT = 1;
const uint8_t VALID_BIT = 2;
struct cache_block** dcache;
uint8_t clock_hand;
struct lock global_cache_lock;
struct cache_block
  {
    uint8_t flags; // Valid/Dirty
    block_sector_t sector; // location
    struct lock lock; // mutual exclusion
    uint8_t use;   // For clock_hand algorithm
    uint8_t data[BLOCK_SECTOR_SIZE]; // block content --> should be 512 bytes
    enum block_type type;
    struct block_operations* ops;
    void* aux;
  };

int
cache_hit (struct block* block, block_sector_t sector,
            struct cache_block* cache_block)
{
  return is_valid (cache_block) &&
          (sector == cache_block->sector) &&
          (block->type == cache_block->type);
}

/* Search the cache for `sector` and return it if found else return NULL. */
struct cache_block *
search_cache (struct block* block, block_sector_t sector)
{
  lock_acquire (&global_cache_lock);
  int i;
  for (i = 0; i < CACHE_SIZE; ++i)
    {
      if (cache_hit (block, sector, dcache[i]))
        {
          hits++;
          if (dcache[i]->use < CHANCES)
            dcache[i]->use++;
          lock_release (&global_cache_lock);
          lock_acquire (&dcache[i]->lock);
          if (cache_hit (block, sector, dcache[i]))
            {
              return dcache[i];
            }
          else
            {
              lock_release (&dcache[i]->lock);
              lock_acquire (&global_cache_lock);
              i = -1;
            }
        }
    }

  misses++;
  return NULL;
}

int
flag_helper (uint8_t value, uint8_t mask)
{
  return (value & mask) == mask;
}

int
is_dirty (struct cache_block* cache_block)
{
  return flag_helper (cache_block->flags, DIRTY_BIT);
}

int
is_valid (struct cache_block* cache_block)
{
  return flag_helper (cache_block->flags, VALID_BIT);
}

void
flush_block (struct cache_block* cache_block)
{
  if (is_valid (cache_block) && is_dirty (cache_block))
    {
      cache_block->ops->write (cache_block->aux, cache_block->sector, cache_block->data);
    }
}

void
clear_block (struct cache_block* cache_block)
{
  cache_block->flags = 0;
  cache_block->sector = 0;
  cache_block->type = 0;
  cache_block->ops = NULL;
  cache_block->aux = NULL;
}

void
evict_block (struct cache_block* cache_block)
{
  flush_block (cache_block);
  clear_block (cache_block);
}

void
mark_valid (struct cache_block* cache_block)
{
  cache_block->flags |= VALID_BIT;
}

void
mark_dirty (struct cache_block* cache_block)
{
  cache_block->flags |= DIRTY_BIT;
}

void
set_block (struct block* block, block_sector_t sector,
                  struct cache_block* cache_block)
{
  cache_block->sector = sector;
  cache_block->ops = (struct block_operations*) block->ops;
  cache_block->aux = block->aux;
  cache_block->type = block->type;
  cache_block->ops->read (cache_block->aux, cache_block->sector,
                          cache_block->data);
  mark_valid (cache_block);
}

struct cache_block*
make_eviction (struct block* block, block_sector_t sector)
{
  /* Assumes thread_current owns global_cache_lock. */
  while (dcache[clock_hand]->use > 0)
    {
      dcache[clock_hand]->use--;
      clock_hand = (clock_hand + 1) % CACHE_SIZE;
    }
  struct cache_block* result = dcache[clock_hand];
  result->use = CHANCES;
  clock_hand = (clock_hand + 1) % CACHE_SIZE;

  lock_release (&global_cache_lock);
  lock_acquire (&result->lock);

  evict_block (result);
  set_block (block, sector, result);
  return result;
}

/* Returns the cache entry associated with a particular
   block, evicting an entry if necessary. */
struct cache_block *
dcache_alloc (struct block *block, block_sector_t sector)
{
  struct cache_block * result = search_cache (block, sector);
  result = (result == NULL) ? make_eviction (block, sector) : result;
  return result;
}

void
dcache_read (struct block* block, block_sector_t sector, uint8_t* buffer)
{
  check_sector (block, sector);
  struct cache_block* cache_block = dcache_alloc (block, sector);
  memcpy ((void*) buffer, (const void*) cache_block->data, BLOCK_SECTOR_SIZE);
  lock_release (&cache_block->lock);
  block->read_cnt++;
}

void
dcache_read_at_offset (struct block* block, block_sector_t sector,
                        uint8_t* buffer, int sector_ofs, int size)
{
  check_sector (block, sector);
  size = (size > BLOCK_SECTOR_SIZE - sector_ofs) ?
          (BLOCK_SECTOR_SIZE - sector_ofs) : size;
  if (size <= 0 || sector_ofs < 0)
    return;

  struct cache_block* cache_block = dcache_alloc (block, sector);
  memcpy (buffer, (cache_block->data) + sector_ofs, size);
  lock_release (&cache_block->lock);
  block->read_cnt++;
}

void
dcache_write (struct block* block, block_sector_t sector, const uint8_t* buffer)
{
  check_sector (block, sector);
  ASSERT (block->type != BLOCK_FOREIGN);
  struct cache_block* cache_block;
  if (freeze_cache)
    {
      // PASS:
      cache_block = search_cache (block, sector);
      if (cache_block == NULL)
       {
         lock_release (&global_cache_lock);
         block->ops->write (block->aux, sector, buffer);
         block->write_cnt++;
       }
      else
       {
         memcpy (cache_block->data, buffer, BLOCK_SECTOR_SIZE);
         mark_dirty (cache_block);
         evict_block (cache_block);
         lock_release (&cache_block->lock);
       }
    }
  else
    {
      cache_block = dcache_alloc (block, sector);
      memcpy (cache_block->data, buffer, BLOCK_SECTOR_SIZE);
      mark_dirty (cache_block);
      lock_release (&cache_block->lock);
      block->write_cnt++;
    }
}

void
dcache_write_at_offset (struct block* block, block_sector_t sector,
                              const uint8_t* buffer, int sector_ofs,
                              int size, int wipe)
{
  check_sector (block, sector);
  ASSERT (block->type != BLOCK_FOREIGN);
  size = (size > BLOCK_SECTOR_SIZE - sector_ofs) ?
          (BLOCK_SECTOR_SIZE - sector_ofs) : size;
  if (size <= 0 || sector_ofs < 0)
    return;

  struct cache_block* cache_block = dcache_alloc (block, sector);
  if (wipe)
    memset (cache_block->data, 0, BLOCK_SECTOR_SIZE);
  memcpy (cache_block->data + sector_ofs, buffer, size);
  mark_dirty (cache_block);
  lock_release (&cache_block->lock);
  block->write_cnt++;
}

void
cache_init (void)
{
  if (cache_initialized)
    return;

  dcache = (struct cache_block**) malloc (CACHE_SIZE * sizeof (struct cache_block*));
  int i;
  for (i = 0; i < CACHE_SIZE; ++i)
    {
      dcache[i] = (struct cache_block*) malloc (sizeof (struct cache_block));
      clear_block (dcache[i]);
      dcache[i]->use = 0;
      lock_init (&dcache[i]->lock);
    }
  clock_hand = 0;
  lock_init (&global_cache_lock);
  cache_initialized = 1;

  misses = 0;
  hits = 0;
}

void
flush_cache (void)
{
  if (!cache_initialized)
    return;
  freeze_cache = 1;
  int i;
  for (i = 0; i < CACHE_SIZE; ++i)
    {
      lock_acquire (&dcache[i]->lock);
      evict_block (dcache[i]);
      lock_release (&dcache[i]->lock);
    }
  freeze_cache = 0;
}

void
reset_buffer_cache(void)
{
  if (!cache_initialized)
    return;

  flush_cache ();
  lock_acquire (&global_cache_lock);

  int i;
  for (i = 0; i < CACHE_SIZE; ++i) {
    //lock_acquire (&dcache[i]->lock);
    //evict_block (dcache[i]);
    dcache[i]->use = 0;
    //lock_release (&dcache[i]->lock);
  }

  hits = misses = 0;
  lock_release (&global_cache_lock);
}
