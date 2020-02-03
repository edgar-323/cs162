#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/block.h"
#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);
int get_next_part (char *part, const char **srcp);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format)
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format)
    do_format ();

  free_map_open ();
  thread_current ()->cwd = dir_open_root ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void)
{
  free_map_close ();
  flush_cache ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size)
{
  int success;
  bool last_exists;
  struct dir *curr;
  struct inode *next;
  char part[NAME_MAX + 1], ppart[NAME_MAX + 1];

  block_sector_t new_block;

  if (name[0] == '/') {
    curr = dir_open_root ();
    next = inode_open (ROOT_DIR_SECTOR);
    last_exists = true;
  } else {
    success = get_next_part(part, &name);
    if (success == -1 || success == 0)
      return false;

    curr = dir_reopen (thread_current ()->cwd);
    if (curr == NULL)
      return false;

    last_exists = dir_lookup (curr, part, &next);
  }

  while (1)
    {
      memcpy(ppart, part, NAME_MAX + 1);
      success = get_next_part(part, &name);

      /* If our filepath part is too long, false.  */
      if (success == -1) {
        dir_close (curr);
        if (last_exists)
          inode_close (next);
        return false;
      }

      /* If the last part of the path already exists, false. */
      if (success == 0 && last_exists) {
        dir_close (curr);
        if (last_exists)
          inode_close (next);
        return false;
      }

      /* If the last part of the path doesn't exist, create it. */
      if (success == 0 && !last_exists) {

        /* Malloc a new block for the file. */
        if (!free_map_allocate (1, &new_block))
        {
          dir_close (curr);
          return false;
        }

        inode_create (new_block, initial_size, false);

        /* Add it to its parent. */
        last_exists = dir_add (curr, ppart, new_block);
        dir_close (curr);
        if (!last_exists)
          free_map_release(new_block, 1);

        return last_exists;
      }

      /* We cannot continue if we hit a nonexistent part. */
      if (!last_exists) {
        dir_close (curr);
        return false;
      }

      /* Otherwise, continue parsing. */
      dir_close (curr);
      curr = dir_open (next);
      if (curr == NULL) {
        dir_close(curr);
        return false;
      }

      last_exists = dir_lookup (curr, part, &next);
    }
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  return file_open (resolve_path (name));
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name)
{
  int success;
  bool last_exists;
  struct dir *curr, *tail;
  struct inode *next;
  char part[NAME_MAX + 1], ppart[NAME_MAX + 1];

  if (name[0] == '/') {
    curr = dir_open_root ();
    next = inode_open (ROOT_DIR_SECTOR);
    last_exists = true;
  } else {
    success = get_next_part(part, &name);
    if (success == -1 || success == 0)
      return false;

    curr = dir_reopen (thread_current ()->cwd);
    if (curr == NULL)
      return false;

    last_exists = dir_lookup (curr, part, &next);
  }

  while (1)
    {
      memcpy(ppart, part, NAME_MAX + 1);
      success = get_next_part(part, &name);

      /* If our filepath part is too long, false.  */
      if (success == -1) {
        dir_close (curr);
        if (last_exists)
          inode_close (next);
        return false;
      }

      /* If the last part of the path exists, check if it's a file or a directory. */
      if (success == 0 && last_exists) {
        /* If it's a directory, only remove it if it's empty. */
        if (inode_is_dir (next)) {
          tail = dir_open (next);
          if (!dir_readdir (tail, part)) {
            dir_close (tail);
            dir_remove (curr, ppart);
            return true;
          }
          dir_close (tail);
          return false;
        }

        /* Otherwise, it's a regular file, so remove it. */
        return dir_remove (curr, ppart);
      }

      /* If the last part of the path doesn't exist, false. */
      if (success == 0 && !last_exists) {
        dir_close (curr);
        if (last_exists)
          inode_close (next);
        return false;
      }

      /* We cannot continue if we hit a nonexistent part. */
      if (!last_exists) {
        dir_close (curr);
        return false;
      }

      /* Otherwise, continue parsing. */
      dir_close (curr);
      curr = dir_open (next);
      if (curr == NULL) {
        dir_close(curr);
        return false;
      }

      last_exists = dir_lookup (curr, part, &next);
    }
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, ROOT_DIR_SECTOR))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}

/* Extracts a file name part from *SRCP into PART, and updates *SRCP so that the
   next call will return the next file name part. Returns 1 if successful, 0 at
   end of string, -1 for a too-long file name part.
   FROM PROJECT SPECS */
int
get_next_part (char *part, const char **srcp)
{
  const char *src = *srcp;
  char *dst = part;
  /* Skip leading slashes.  If it’s all slashes, we’re done. */
  while (*src == '/')
    src++;
  if (*src == '\0')
    return 0;
  /* Copy up to NAME_MAX character from SRC to DST.  Add null terminator. */
  while (*src != '/' && *src != '\0') {
    if (dst < part + NAME_MAX)
      *dst++ = *src;
    else
      return -1;
    src++;
  }
  *dst = '\0';

  /* Advance source pointer. */
  *srcp = src;
  return 1;
}

struct inode *
resolve_path (const char *path)
{
  int success;
  struct dir *curr = NULL;
  struct inode *next;
  char part[NAME_MAX + 1];

  if (path[0] == '/') {
    curr = dir_open_root ();
    next = inode_open (ROOT_DIR_SECTOR);
  } else {
    success = get_next_part(part, &path);
    if (success == -1 || success == 0)
      return NULL;

    curr = dir_reopen (thread_current ()->cwd);
    if (curr == NULL)
      return NULL;

    if (!dir_lookup (curr, part, &next))
      return NULL;
  }


  while (1)
    {
      dir_close (curr);
      success = get_next_part(part, &path);

      if (success == -1) {
        inode_close (next);
        return NULL;
      }

      if (success == 0)
        return next;

      curr = dir_open (next);
      if (curr == NULL)
        return NULL;

      if (!dir_lookup (curr, part, &next))
        return NULL;
    }
}
