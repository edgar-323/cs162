#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include <stdio.h>
/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

struct inode_disk;
void zero_out_inode_disk (struct inode* inode, struct inode_disk* disk_inode, off_t size, off_t offset);
const size_t DIRECT_POINTERS = 124;
const size_t SECTORS_PER_BLOCK = BLOCK_SECTOR_SIZE / sizeof (block_sector_t);
const size_t MAX_FILE_SIZE = (8 * (1 << 20)) - (3 + 128) * 512;
bool inode_disk_resize (struct inode_disk* id, size_t size, void* buff_,
                    size_t total_sectors);

size_t calculate_additional_sectors (size_t curr_size, size_t new_size);
bool inode_resize (struct inode_disk *id, size_t new_size, block_sector_t);

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    uint32_t is_dir;
    uint32_t size;
    block_sector_t direct[124];
    block_sector_t single_indirect;
    block_sector_t double_indirect;
  };

struct pointer_block
  {
    block_sector_t pointer[128];
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode
  {
    struct lock lock;                   /* A lock to ensure no data races. */
    bool is_dir;                        /* Is this inode a directory or not? */
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    // struct inode_disk data;             /* Inode content. */
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (struct inode *inode, uint32_t pos)
{
  uint8_t block[512];

  ASSERT (inode != NULL);

  /* Read inode->sector into block. */
  block_read(fs_device, inode->sector, block);

  /* Check if the pos is in bounds. */
  if (pos >= ((struct inode_disk*) block)->size) {
    return -1;
  }

  /* If it's in the direct block, return the corresponding block. */
  if (pos < 124 * BLOCK_SECTOR_SIZE) {
    return ((struct inode_disk*) block)->direct[pos / BLOCK_SECTOR_SIZE];
  }

  /* If it's past the direct block, subtract off the part and check indirect. */
  pos -= 124 * BLOCK_SECTOR_SIZE;
  if (pos < BLOCK_SECTOR_SIZE * 128) {

    /* Read inode_disk->indirect into block. */
    block_read(fs_device, ((struct inode_disk*) block)->single_indirect, block);
    return ((struct pointer_block*) block)->pointer[pos / 512];
  }

  /* Finally, find the appropriate doubly-indirect block. */
  pos -= 128 * BLOCK_SECTOR_SIZE;
  if (pos < 16384 * BLOCK_SECTOR_SIZE) {

    /* Read inode_disk->double_indirect into block. */
    block_read(fs_device, ((struct inode_disk*) block)->double_indirect, block);

    /* Read inode_disk->pointer[pos / 65536] into block. */
    block_read(fs_device, ((struct pointer_block*) block)->pointer[pos / 65536], block);
    return ((struct pointer_block*) block)->pointer[pos / 512];
  }

  PANIC("position is past the end of max filesize.");
  return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void)
{
  list_init (&open_inodes);
  ASSERT (sizeof (struct inode_disk) == BLOCK_SECTOR_SIZE);
}

/* Initializes a new empty inode and writes the new
   inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t initial_size, bool isdir)
{
  struct inode_disk *disk_inode = NULL;
  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);
  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode == NULL)
    return false;
  disk_inode->is_dir = isdir;
  disk_inode->size = 0;
  bool success = inode_resize (disk_inode, initial_size, sector);
  free (disk_inode);
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;
  struct inode_disk disk_inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e))
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector)
        {
          if (inode->removed)
            return NULL;
          inode_reopen (inode);
          return inode;
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  block_read(fs_device, sector, &disk_inode);
  inode->is_dir = disk_inode.is_dir;
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  lock_init(&inode->lock);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    {
      lock_acquire (&inode->lock);
      inode->open_cnt++;
      lock_release (&inode->lock);
    }
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Returns INODE's is_dir. */
bool
inode_is_dir (const struct inode *inode)
{
  return inode->is_dir;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode)
{
  size_t i, j;
  struct inode_disk disk_inode;
  struct pointer_block temp1, temp2;
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  lock_acquire(&inode->lock);

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);

      /* Deallocate blocks if removed. */
      if (inode->removed)
        {
          block_read(fs_device, inode->sector, &disk_inode);
          for (i = 0; i < 124; i++)
            {
              if (disk_inode.direct[i] != 0)
                free_map_release(disk_inode.direct[i], 1);
            }

          if (disk_inode.single_indirect != 0) {
            block_read(fs_device, disk_inode.single_indirect, &temp1);
            for (i = 0; i < 128; i++)
              {
                if (temp1.pointer[i] != 0)
                  free_map_release(temp1.pointer[i], 1);
              }
            free_map_release(disk_inode.single_indirect, 1);
          }

          if (disk_inode.double_indirect != 0) {
            block_read(fs_device, disk_inode.double_indirect, &temp1);
            for (i = 0; i < 128; i++)
              {
                if (temp1.pointer[i] != 0) {
                  block_read(fs_device, temp1.pointer[i], &temp2);
                  for (j = 0; j < 128; j++)
                    {
                      if (temp2.pointer[j] != 0)
                        free_map_release(temp2.pointer[j], 1);
                    }
                  free_map_release(temp1.pointer[i], 1);
                }
              }
            free_map_release(disk_inode.double_indirect, 1);
          }

          free_map_release(inode->sector, 1);
        }

      lock_release(&inode->lock);
      free (inode);
    }
  else
    lock_release(&inode->lock);
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode)
{
  ASSERT (inode != NULL);
  lock_acquire(&inode->lock);
  inode->removed = true;
  lock_release(&inode->lock);
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset)
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  struct inode_disk disk_inode;

  lock_acquire(&inode->lock);

  if (inode->removed && inode->is_dir) {
    lock_release(&inode->lock);
    return 0;
  }

  /* Load inode contents into memory. */
  block_read(fs_device, inode->sector, &disk_inode);

  while (size > 0)
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = disk_inode.size - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          dcache_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else
        {
          dcache_read_at_offset (fs_device, sector_idx, buffer + bytes_read,
                                  sector_ofs, chunk_size);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }

  lock_release (&inode->lock);
  return bytes_read;
}

void
zero_out_inode_disk (struct inode* inode, struct inode_disk* disk_inode, off_t size, off_t offset)
{
  ASSERT (disk_inode != NULL);

  uint8_t zeros[BLOCK_SECTOR_SIZE];
  memset (zeros, 0, BLOCK_SECTOR_SIZE);
  off_t bytes_written = 0;

  while (size > 0)
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = disk_inode->size - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          dcache_write (fs_device, sector_idx, zeros);
        }
      else
        {
          dcache_write_at_offset (fs_device, sector_idx,
                                  zeros,
                                  sector_ofs, chunk_size,
                                  !((sector_ofs > 0) ||
                                    (chunk_size < sector_left)));
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs. */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset)
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  struct inode_disk disk_inode;

  lock_acquire(&inode->lock);

  if (inode->deny_write_cnt) {
    lock_release (&inode->lock);
    return 0;
  }
  if (inode->removed && inode->is_dir) {
    lock_release (&inode->lock);
    return 0;
  }

  /* Load inode contents into memory. */
  block_read (fs_device, inode->sector, &disk_inode);

  /* Resize the file if necessary. */
  if ((uint32_t) (offset + size) > disk_inode.size)
    {
      size_t old_sz = disk_inode.size;
      if (!inode_resize (&disk_inode, offset + size, inode->sector))
        {
          lock_release (&inode->lock);
          return 0;
        }
      block_read (fs_device, inode->sector, &disk_inode);
      zero_out_inode_disk (inode, &disk_inode, disk_inode.size - old_sz, old_sz);
    }


  while (size > 0)
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = disk_inode.size - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          dcache_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else
        {
          dcache_write_at_offset (fs_device, sector_idx,
                                  buffer + bytes_written,
                                  sector_ofs, chunk_size,
                                  !((sector_ofs > 0) ||
                                    (chunk_size < sector_left)));
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }

  lock_release (&inode->lock);
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode)
{
  lock_acquire(&inode->lock);
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  lock_release(&inode->lock);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode)
{
  lock_acquire (&inode->lock);
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
  lock_release (&inode->lock);
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (struct inode *inode)
{
  ASSERT (inode != NULL);
  lock_acquire (&inode->lock);
  size_t size = -1;
  struct inode_disk* id;
  uint8_t buff[BLOCK_SECTOR_SIZE];
  block_read (fs_device, inode->sector, buff);
  id = (struct inode_disk*) buff;
  size = id->size;
  lock_release (&inode->lock);
  return size;
}

bool
inode_resize (struct inode_disk *id, size_t new_size, block_sector_t sector)
{
  ASSERT (id != NULL);
  if (id->size > new_size)
    PANIC ("CANNOT DOWN-SIZE INODE_DISK");
  else if (id->size == new_size)
    {
      block_write (fs_device, sector, id);
      return true;
    }
  else if (new_size > MAX_FILE_SIZE)
    return false;

  size_t additional_sectors = calculate_additional_sectors (id->size, new_size);
  if (additional_sectors == 0)
    {
      id->size = new_size;
      block_write (fs_device, sector, id);
      return true;
    }
  void* buffer = malloc (additional_sectors * sizeof (block_sector_t));
  bool success = (buffer != NULL);
  if (success)
    {
      success = inode_disk_resize (id, new_size, buffer, additional_sectors);
      if (success)
        {
          block_write (fs_device, sector, id);
        }
      free (buffer);
    }
  return success;
}

size_t
calculate_additional_sectors (size_t curr_size, size_t new_size)
{
  size_t curr_data_sectors = bytes_to_sectors (curr_size);
  size_t new_data_sectors = bytes_to_sectors (new_size);
  size_t curr_meta_sectors;
  size_t new_meta_sectors;
  size_t additional_data_sectors;
  size_t additional_meta_sectors;

  if (curr_data_sectors <= DIRECT_POINTERS)
    curr_meta_sectors = 0;
  else if (curr_data_sectors <= (DIRECT_POINTERS + SECTORS_PER_BLOCK))
    curr_meta_sectors = 1;
  else
    curr_meta_sectors = 2 + DIV_ROUND_UP (bytes_to_sectors (curr_size - (DIRECT_POINTERS + SECTORS_PER_BLOCK) * BLOCK_SECTOR_SIZE), 128);

  if (new_data_sectors <= DIRECT_POINTERS)
    new_meta_sectors = 0;
  else if (new_data_sectors <= (DIRECT_POINTERS + SECTORS_PER_BLOCK))
    new_meta_sectors = 1;
  else
    new_meta_sectors = 2 + DIV_ROUND_UP (bytes_to_sectors (new_size - (DIRECT_POINTERS + SECTORS_PER_BLOCK) * BLOCK_SECTOR_SIZE), 128);

  additional_data_sectors = (new_data_sectors - curr_data_sectors);
  additional_meta_sectors = (new_meta_sectors - curr_meta_sectors);
  return (additional_data_sectors + additional_meta_sectors);
}

bool
inode_disk_resize (struct inode_disk* id, size_t size,
                    void* buff_, size_t total_sectors)
{
  block_sector_t buffer[128];
  if (!free_map_request (total_sectors, buff_))
    return false;

  block_sector_t* get_sector = (block_sector_t*) buff_;
  size_t sector_index = 0;
  size_t i, j;

  for (i = 0; i < DIRECT_POINTERS; i++)
    {
      if (size > BLOCK_SECTOR_SIZE * i && id->direct[i] == 0)
        {
          if (sector_index >= total_sectors)
            PANIC ("WRONG MATH");
          id->direct[i] = get_sector[sector_index++];
        }
    }
  if (id->single_indirect == 0 && size <= DIRECT_POINTERS * BLOCK_SECTOR_SIZE)
    {
      ASSERT (sector_index == total_sectors);
      id->size = size;
      return true;
    }

  if (id->single_indirect == 0)
    {
      memset(buffer, 0, BLOCK_SECTOR_SIZE);
      if (sector_index >= total_sectors) {
        PANIC ("WRONG MATH");
        return false;
      }
      id->single_indirect= get_sector[sector_index++];
    }
  else
    block_read (fs_device, id->single_indirect, buffer);

  for (i = 0; i < SECTORS_PER_BLOCK; i++)
    {
      if (size > (DIRECT_POINTERS + i) * BLOCK_SECTOR_SIZE && buffer[i] == 0)
        {
          if (sector_index >= total_sectors)
            PANIC ("WRONG MATH");
          buffer[i] = get_sector[sector_index++];
        }
    }
  block_write (fs_device, id->single_indirect, buffer);
  if (id->double_indirect == 0 &&
      size <= (DIRECT_POINTERS + SECTORS_PER_BLOCK) * BLOCK_SECTOR_SIZE)
    {
      ASSERT (sector_index == total_sectors);
      id->size = size;
      return true;
    }

  if (id->double_indirect == 0)
    {
      memset (buffer, 0, BLOCK_SECTOR_SIZE);
      if (sector_index >= total_sectors)
        PANIC ("WRONG MATH");
      id->double_indirect = get_sector[sector_index++];
    }
  else
    block_read (fs_device, id->double_indirect, buffer);

  block_sector_t buffer_helper[SECTORS_PER_BLOCK];

  for (i = 0; i < SECTORS_PER_BLOCK ; ++i)
    {
      if (size <= (DIRECT_POINTERS + SECTORS_PER_BLOCK * (i + 1)) * BLOCK_SECTOR_SIZE)
        break;

      if (buffer[i] == 0)
        {
          if (sector_index >= total_sectors)
            PANIC ("WRONG MATH");
          buffer[i] = get_sector[sector_index++];
          memset (buffer_helper, 0, BLOCK_SECTOR_SIZE);
        }
      else
        {
          block_read (fs_device, buffer[i], buffer_helper);
        }

      for (j = 0; j < SECTORS_PER_BLOCK; ++j)
        {
          if (size <= ((DIRECT_POINTERS + SECTORS_PER_BLOCK * (i + 1) + j) * BLOCK_SECTOR_SIZE)
              && buffer_helper[j] != 0)
            {
              block_free (buffer_helper[j]);
              buffer_helper[j] = 0;
            }
          if (size > ((DIRECT_POINTERS + SECTORS_PER_BLOCK * (i + 1) + j) * BLOCK_SECTOR_SIZE)
              && buffer_helper[j] == 0)
            {
              if (sector_index >= total_sectors)
                PANIC ("WRONG MATH");
              buffer_helper[j] = get_sector[sector_index++];
            }
        }
      block_write (fs_device, buffer[i], buffer_helper);
    }
  block_write (fs_device, id->double_indirect, buffer);
  ASSERT (sector_index == total_sectors);
  id->size = size;
  return true;
}
