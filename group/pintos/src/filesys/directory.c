#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "filesys/free-map.h"

/* A directory. */
struct dir
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
  };

/* A single directory entry. */
struct dir_entry
  {
    char name[28];
    block_sector_t inode;
  };

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (block_sector_t sector, block_sector_t parent)
{
  if (!inode_create (sector, 0, true))
    return false;

  struct inode *inode = inode_open (sector);
  if (inode == NULL)
    return false;

  struct dir *dir = dir_open (inode);
  if (dir == NULL)
    return false;

  if (!dir_add (dir, "..", parent) || !dir_add (dir, ".", sector))
    return false;
  return true;
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode)
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      if (!inode_is_dir (inode)) {
        inode_close (inode);
        free (dir);
        return NULL;
      }
      dir->inode = inode;
      dir->pos = 0;
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL;
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir)
{
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir)
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir)
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp)
{
  struct dir_entry e;
  size_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
    if (e.inode != 0 && !strcmp (name, e.name))
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        return true;
      }
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode)
{
  struct dir_entry e;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  if (lookup (dir, name, &e, NULL))
    *inode = inode_open (e.inode);
  else
    *inode = NULL;

  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector)
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
    return false;

  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL))
    goto done;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.

     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
    if (e.inode == 0)
      break;

  /* Write slot. */
  strlcpy (e.name, name, sizeof e.name);
  e.inode = inode_sector;
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

 done:
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name)
{
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs))
    goto done;

  /* Open inode. */
  inode = inode_open (e.inode);
  if (inode == NULL)
    goto done;

  /* Erase directory entry. */
  e.inode = (block_sector_t) 0;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e)
    goto done;

  /* Remove inode. */
  inode_remove (inode);
  success = true;

 done:
  inode_close (inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;

  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e)
    {
      dir->pos += sizeof e;
      if (e.inode != 0 && strcmp(e.name, "..") && strcmp(e.name, "."))
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          return true;
        }
    }
  return false;
}

/* chdir, mkdir, readdir, and isdir.*/
bool
chdir (const char *dir)
{
  struct inode *new = resolve_path (dir);
  if (new == NULL)
    return false;

  struct dir *new_dir = dir_open (new);
  if (new_dir == NULL)
    return false;

  dir_close (thread_current ()->cwd);
  thread_current ()->cwd = new_dir;
  return true;
}

bool
mkdir (const char *dir)
{
  int success;
  bool last_exists;
  struct dir *curr = NULL;
  struct inode *next;
  char part[NAME_MAX + 1], ppart[NAME_MAX + 1];

  block_sector_t new_block;

  if (dir[0] == '/') {
    curr = dir_open_root ();
    next = inode_open (ROOT_DIR_SECTOR);
    last_exists = true;
  } else {
    success = get_next_part(part, &dir);
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
      success = get_next_part(part, &dir);

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

        /* Malloc a new block for the directory. */
        if (!free_map_allocate (1, &new_block))
        {
           dir_close (curr);
           return false;
        }

        /* Create a new directory. */
        if (!dir_create (new_block, inode_get_inumber (dir_get_inode (curr)))) {
          dir_close (curr);
          return false;
        }

        /* Add it to its parent. */
        last_exists = dir_add (curr, ppart, new_block);
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
