#include "filesys/free-map.h"
#include <bitmap.h>
#include <debug.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/synch.h"

static struct file *free_map_file;   /* Free map file. */
static struct bitmap *free_map;      /* Free map, one bit per sector. */


struct lock free_map_lock;
bool free_map_has_space (size_t cnt);
bool freemap_set_bits (size_t sectors, void* buffer);
bool free_map_request (size_t sectors, void* buffer);
void block_free (size_t sector);

/* Initializes the free map. */
void
free_map_init (void)
{
  free_map = bitmap_create (block_size (fs_device));
  if (free_map == NULL)
    PANIC ("bitmap creation failed--file system device is too large");
  bitmap_mark (free_map, FREE_MAP_SECTOR);
  bitmap_mark (free_map, ROOT_DIR_SECTOR);
  lock_init (&free_map_lock);
}

/* Allocates CNT consecutive sectors from the free map and stores
   the first into *SECTORP.
   Returns true if successful, false if not enough consecutive
   sectors were available or if the free_map file could not be
   written. */
bool
free_map_allocate (size_t cnt, block_sector_t *sectorp)
{
  lock_acquire (&free_map_lock);
  block_sector_t sector = bitmap_scan_and_flip (free_map, 0, cnt, false);
  if (sector != BITMAP_ERROR
      && free_map_file != NULL
      && !bitmap_write (free_map, free_map_file))
    {
      // bitmap_write failed. So undo the changes to the free_map
      bitmap_set_multiple (free_map, sector, cnt, false);
      sector = BITMAP_ERROR;
    }

  lock_release (&free_map_lock);
  if (sector != BITMAP_ERROR)
    *sectorp = sector;
  return sector != BITMAP_ERROR;
}

/* Makes CNT sectors starting at SECTOR available for use. */
void
free_map_release (block_sector_t sector, size_t cnt)
{
  lock_acquire (&free_map_lock);
  ASSERT (bitmap_all (free_map, sector, cnt));
  bitmap_set_multiple (free_map, sector, cnt, false);
  bitmap_write (free_map, free_map_file);
  lock_release (&free_map_lock);
}

/* Opens the free map file and reads it from disk. */
void
free_map_open (void)
{
  lock_acquire (&free_map_lock);
  free_map_file = file_open (inode_open (FREE_MAP_SECTOR));
  if (free_map_file == NULL)
    PANIC ("can't open free map");
  if (!bitmap_read (free_map, free_map_file))
    PANIC ("can't read free map");
  lock_release (&free_map_lock);
}

/* Writes the free map to disk and closes the free map file. */
void
free_map_close (void)
{
  lock_acquire (&free_map_lock);
  file_close (free_map_file);
  free_map_file = NULL;
  lock_release (&free_map_lock);
}

/* Creates a new free map file on disk and writes the free map to
   it. */
void
free_map_create (void)
{
  /* Create inode. */
  if (!inode_create (FREE_MAP_SECTOR, bitmap_file_size (free_map), false))
    PANIC ("free map creation failed");

  /* Write bitmap to file. */
  free_map_file = file_open (inode_open (FREE_MAP_SECTOR));
  if (free_map_file == NULL)
    PANIC ("can't open free map");
  if (!bitmap_write (free_map, free_map_file))
    PANIC ("can't write free map");
}

/* Checks whether free_map has enough memory to allocate CNT sectors. */
bool
free_map_has_space (size_t cnt)
{
  return cnt <= bitmap_count_all (free_map, false);
}

/*
  Sets SECTORS bits to FALSE on the FREEMAP if there is enough free bits.
*/
bool
freemap_set_bits (size_t sectors, void* buffer)
{
  if (!free_map_has_space (sectors))
    return false;
  bitmap_set_any (free_map, 0, sectors, true, buffer);
  return true;
}



bool
free_map_request (size_t sectors, void* buffer)
{
  lock_acquire (&free_map_lock);
  bool success = freemap_set_bits (sectors, buffer);
  if (success
      && free_map_file != NULL
      && !bitmap_write (free_map, free_map_file))
  {
    bitmap_set_sectors (free_map, buffer, sectors, false);
    success = false;
  }
  lock_release (&free_map_lock);
  return success;
}

void
block_free (size_t sector)
{
  lock_acquire (&free_map_lock);
  bitmap_mark (free_map, sector);
  lock_release (&free_map_lock);
}
