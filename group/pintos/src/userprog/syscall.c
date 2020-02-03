#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/input.h"
#include "threads/malloc.h"
#include "lib/string.h"
#include "filesys/directory.h"
#include "filesys/inode.h"
#include "devices/block.h"

static void syscall_handler (struct intr_frame *);
static int get_user (const uint8_t *uaddr);
static void check_user_n (void *buf, int n);
static void check_user_str (void *str);

struct FD_PTR;
static struct FD_PTR* search_open_fds (block_sector_t fdsector);
static struct FD_PTR *get_fd_ptr (const char* name);
void close_all_user_files (void);
static bool create (const char * filepath, unsigned int initial_size);
static bool remove (const char* filename);
static bool readdir (int fd, char *name);
static bool isdir (int fd);
static int open (const char* filepath);
static int filesize (int fd);
static int read_from_keyboard(void * buffer, unsigned int size);
static int read (int fd, void *buffer, unsigned int size);
static int write_to_console (const void * buffer, unsigned int size);
static int write (int fd, const void * buffer, unsigned int size);
static int get_global_fd (struct FD_PTR* file);
static int get_user_fd (struct FD_PTR* file);
static int inumber (int fd);
static void seek (int fd, unsigned int position);
static void close (int fd);
static void close_fd (struct FD_PTR * file);
static unsigned int tell (int fd);
static struct FD_PTR* get_user_fdptr (int fd);
static void reset_buffer (void);
struct FD_PTR
  {
    uint8_t is_dir;
    block_sector_t sector;
    void *fd_object;
  };
/* Global FD table lock. */
struct lock global_fds_lock;

/* Global fds */
struct FD_PTR** fds;
unsigned int fd_cap;

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&global_fds_lock);
  fd_cap = 0;
}

void
close_all_user_files (void)
{
  lock_acquire (&global_fds_lock);
  struct thread * t = thread_current ();
  unsigned int i;
  for (i = 0; i < t->fd_cap; ++i)
    {
      int global_index = t->fds[i];
      struct FD_PTR* File = (global_index >= 0 && global_index < (int) fd_cap) ?
                            fds[global_index] : NULL;
      if (File != NULL)
        {
          close_fd (File);
          t->fds[i] = -1;
          fds[global_index] = NULL;
        }
    }
  if (t->fds != NULL)
    {
      free (t->fds);
      t->fds = NULL;
      t->fd_cap = 0;
    }
  lock_release (&global_fds_lock);
}

bool
create (const char * filepath, unsigned int initial_size)
{
  return filesys_create (filepath, initial_size);
}

int
get_global_fd (struct FD_PTR* file)
{
  lock_acquire (&global_fds_lock);
  if (fd_cap == 0)
    {
      fd_cap = 4;
      fds = (struct FD_PTR**) malloc (fd_cap * sizeof (struct FD_PTR*));
      if (fds == NULL)
        {
          fd_cap = 0;
          lock_release (&global_fds_lock);
          return -1;
        }
      unsigned int i;
      for (i = 0; i < fd_cap; ++i)
        fds[i] = NULL;
    }
  int index = -1;
  unsigned int j;
  for (j = 0; j < fd_cap; ++j)
    {
      if (fds[j] == NULL)
        {
          index = j;
          break;
        }
    }
  if (index < 0)
    {
      struct FD_PTR** temp_fds = (struct FD_PTR **) malloc (2 * fd_cap);
      if (temp_fds == NULL)
        {
          lock_release (&global_fds_lock);
          return -1;
        }
      unsigned int new_size = 2 * fd_cap;
      unsigned int k;
      for (k = 0; k < new_size; ++k)
        {
          if (k < fd_cap)
            temp_fds[k] = fds[k];
          else
            temp_fds[k] = NULL;
        }
      index = fd_cap;
      free (fds);
      fds = temp_fds;
      fd_cap = new_size;
    }
  fds[index] = file;
  lock_release (&global_fds_lock);
  return index;
}

int
get_user_fd (struct FD_PTR* file)
{
  int global_fd = get_global_fd (file);
  if (global_fd < 0)
    return -1;

  struct thread* t = thread_current ();
  if (t->fd_cap == 0)
    {
      t->fd_cap = 1;
      t->fds = (int *) malloc (sizeof (int));
      if (t->fds == NULL)
        {
          t->fd_cap = 0;
          fds[global_fd] = NULL;
          return -1;
        }
      t->fds[0] = -1;
    }

  int index = -1;
  unsigned int i;
  for (i = 0; i < t->fd_cap; ++i)
    {
      if (t->fds[i] < 0)
        {
          index = i;
          break;
        }
    }

  if (index < 0)
    {
      int* temp_fds = (int *) malloc (2 * t->fd_cap);
      if (temp_fds == NULL)
        {
          fds[global_fd] = NULL;
          return -1;
        }
      unsigned int new_size = 2 * t->fd_cap;
      unsigned int j;
      for (j = 0; j < new_size; ++j)
        {
          if (j < t->fd_cap)
            temp_fds[j] = t->fds[j];
          else
            temp_fds[j] = -1;
        }
      free (t->fds);
      t->fds = temp_fds;
      index = t->fd_cap;
      t->fd_cap = new_size;
    }
  t->fds[index] = global_fd;
  return index + 2;
}

int
open (const char* filepath)
{
  struct FD_PTR* fd_ptr = get_fd_ptr (filepath);
  if (fd_ptr == NULL)
    return -1;
  int fd = get_user_fd (fd_ptr);
  if (fd < 0)
    close_fd (fd_ptr);

  return fd;
}

bool
remove (const char* filepath)
{
  return filesys_remove (filepath);
}

struct FD_PTR*
get_user_fdptr (int fd)
{
  int user_index = fd - 2;
  struct thread* t = thread_current ();
  if (user_index < 0 || user_index >= (int) t->fd_cap)
    return NULL;

  struct FD_PTR* result = NULL;
  int global_index = t->fds[user_index];
  lock_acquire (&global_fds_lock);
  if (global_index >= 0 && global_index < (int) fd_cap)
    result = fds[global_index];
  lock_release (&global_fds_lock);
  return result;
}

int
filesize (int fd)
{
  struct FD_PTR* FileDes = get_user_fdptr (fd);
  return (FileDes == NULL || FileDes->is_dir) ? -1 : (int) file_length (FileDes->fd_object);
}

int
read_from_keyboard(void * buffer, unsigned int size)
{
  unsigned int i = 0;
  uint8_t key;
  while (i < size)
    {
      key = input_getc ();
      memset (((uint8_t*) buffer) + i, key, 1);
      ++i;
    }
  return i;
}

int
read (int fd, void *buffer, unsigned int size)
{
  if (fd == 0)
    return read_from_keyboard (buffer, size);

  struct FD_PTR* FileDes = get_user_fdptr (fd);
  return (FileDes == NULL || FileDes->is_dir) ? -1 : file_read (FileDes->fd_object, buffer, size);
}

int
write_to_console (const void * buffer, unsigned int size)
{
  unsigned int wrote = 0;
  const int CHUNK_SZ = 500;
  const int CHUNKS = size / CHUNK_SZ;
  int chunk;
  for (chunk = 0; chunk < CHUNKS; chunk++)
    {
      putbuf (buffer + wrote, CHUNK_SZ);
      wrote += CHUNK_SZ;
    }
  if (wrote < size)
    {
      putbuf (buffer + wrote, size - wrote);
      wrote += (size - wrote);
    }

  return (int) wrote;
}

int
write (int fd, const void * buffer, unsigned int size)
{
  if (fd == 1)
    return write_to_console (buffer, size);

  struct FD_PTR* FileDes = get_user_fdptr (fd);
  return (FileDes == NULL || FileDes->is_dir) ? -1 : (int) file_write (FileDes->fd_object, buffer, size);
}

void
seek (int fd, unsigned int position)
{
  struct FD_PTR* File = get_user_fdptr (fd);
  if (File != NULL && !File->is_dir)
    file_seek (File->fd_object, position);
}

unsigned int
tell (int fd)
{
  struct FD_PTR* File = get_user_fdptr (fd);
  return (File == NULL || File->is_dir) ? (unsigned int) -1 :
                          (unsigned int) file_tell (File->fd_object);
}

struct FD_PTR*
search_open_fds (block_sector_t fdsector)
{
  struct FD_PTR* FileDes = NULL;
  unsigned int i;
  lock_acquire (&global_fds_lock);
  for (i = 0; i < fd_cap; ++i)
    {
      FileDes = fds[i];
      if ((FileDes != NULL)  && (FileDes->sector == fdsector))
        return FileDes;
    }
  lock_release (&global_fds_lock);
  return NULL;
}

struct FD_PTR*
get_fd_ptr (const char* name)
{
  struct inode* inode = resolve_path (name);
  if (inode != NULL)
    {
      struct FD_PTR* exists = search_open_fds (inode_get_inumber (inode));
      struct FD_PTR* fd_ptr = malloc (sizeof (struct FD_PTR));
      if (fd_ptr == NULL)
        {
          if (exists != NULL)
            lock_release (&global_fds_lock);

          inode_close (inode);
          return NULL;
        }
      if (exists != NULL)
        {
          inode_close (inode);
          fd_ptr->fd_object = (inode_is_dir (inode)) ?
                              (void*) dir_reopen ((struct dir*) exists->fd_object) :
                              (void*) file_reopen ((struct file*) exists->fd_object);
          lock_release (&global_fds_lock);
        }
      else
        {
          fd_ptr->fd_object = (inode_is_dir (inode)) ?
                              (void*) dir_open (inode) : (void*) file_open (inode);
        }
      if (fd_ptr->fd_object == NULL)
        {
          free (fd_ptr);
          return NULL;
        }
      fd_ptr->is_dir = inode_is_dir (inode);
      fd_ptr->sector = inode_get_inumber (inode);
      return fd_ptr;
    }
  else
    return NULL;
}

void
close_fd (struct FD_PTR *FileDes)
{
  if (FileDes->is_dir)
    dir_close (FileDes->fd_object);
  else
    file_close (FileDes->fd_object);
  FileDes->fd_object = NULL;

  free (FileDes);
}

void
close (int fd)
{
  struct FD_PTR* FileDes = get_user_fdptr (fd);
  if (FileDes == NULL)
    return;
  struct thread* t = thread_current ();
  int user_index = fd - 2;
  int global_index = t->fds[user_index];
  t->fds[user_index] = -1;
  fds[global_index] = NULL;
  close_fd (FileDes);
}

int
inumber (int fd)
{
  struct FD_PTR* File = get_user_fdptr (fd);
  return (File == NULL) ? -1 : (int) File->sector;
}

bool
readdir (int fd, char *name) {
  struct FD_PTR* FileDes = get_user_fdptr (fd);
  return (FileDes == NULL || !FileDes->is_dir) ? false : dir_readdir (FileDes->fd_object, name);

}

bool
isdir (int fd) {
  struct FD_PTR* FileDes = get_user_fdptr (fd);
  return (FileDes == NULL || !FileDes->is_dir) ? false : true;

}

void
reset_buffer (void) {
  reset_buffer_cache ();
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  uint32_t* args = ((uint32_t*) f->esp);
  uint32_t sysnum, arg0, arg1, arg2;

  /* Check all pieces of the stack before using!
     eg: to check args[i] we call:

     check_user_n (args + i, 4);

     we can lump these together. To check the first n args:

     check_user_n (args, n * 4);

     to check a buffer of size n:

     check_user_n (buf, n);

     to check a string:

     check_user_str(str);

     Once you've checked user data, feel free to refer to it however
     is most convenient.

     If you need to write to a user buffer, follow this procedure:

     - Malloc a buffer in the kernel to hold the result
     - Use put_user_n() to copy the kernel buffer into the user buffer.

     If any of the check_user or put_user calls fails,
     they will automatically kill the caller, so don't handle that. */

  /* Check the data corresponding to the syscall number. */
  check_user_n (args, 4);

  /* Print it out and switch on it. */
  sysnum = args[0];

  /* NOTE: To return a value, set f->eax to that value. */
  switch (sysnum)
    {
    case SYS_PRACTICE:               /* Returns arg incremented by 1 */
      check_user_n (args + 1, 4);

      f->eax = (uint32_t) args[1] + 1;
      break;

    /* Process control syscalls: */
    case SYS_HALT:                   /* Halt the operating system. */
      shutdown_power_off ();
      break;

    case SYS_EXIT:                   /* Terminate this process. */
      check_user_n (args + 1, 4);
      arg0 = args[1];

      thread_current ()->exit_code = arg0;
      thread_exit();
      break;

    case SYS_EXEC:                   /* Start another process. */
      check_user_n (args + 1, 4);
      arg0 = args[1];
      check_user_str ((char*) arg0);
      f->eax = (uint32_t) process_execute ((char*) arg0);
      break;

    case SYS_WAIT:                   /* Wait for a child process to die. */
      check_user_n (args + 1, 4);
      f->eax = (uint32_t) process_wait ((tid_t) args[1]);
      break;

    /* Filesys syscalls: */
    case SYS_CREATE:                 /* Create a file. */
      check_user_n (args + 1, 4);
      arg0 = args[1];
      check_user_str ((void*) arg0);
      check_user_n (args + 2, 4);
      arg1 = args[2];

      f->eax = (uint32_t) create ((const char *) arg0, (unsigned int) arg1);
      break;

    case SYS_REMOVE:                 /* Delete a file. */
      check_user_n (args + 1, 4);
      arg0 = args[1];
      check_user_str ((void*) arg0);

      f->eax = (uint32_t) remove ((const char *) arg0);
      break;

    case SYS_OPEN:                   /* Open a file. */
      check_user_n (args + 1, 4);
      arg0 = args[1];
      check_user_str ((void*) arg0);

      f->eax = (uint32_t) open ((const char *) arg0);
      break;

    case SYS_FILESIZE:               /* Obtain a file's size. */
      check_user_n (args + 1, 4);
      arg0 = args[1];

      f->eax = (uint32_t) filesize ((int) arg0);
      break;

    case SYS_READ:                   /* Read from a file. */
      check_user_n (args + 1, 12);
      arg0 = args[1];
      arg1 = args[2];
      arg2 = args[3];
      check_user_n ((void*) arg1, (int) arg2);

      f->eax = (uint32_t) read ((int) arg0, (void *) arg1,
                                    (unsigned int) arg2);
      break;

    case SYS_WRITE:                  /* Write to a file. */
      check_user_n (args + 1, 12);
      arg0 = args[1];
      arg1 = args[2];
      arg2 = args[3];
      check_user_n ((void*) arg1, (int) arg2);

      f->eax = (uint32_t) write ((int) arg0, (const void *) arg1,
                                  (unsigned int) arg2);
      break;

    case SYS_SEEK:                   /* Change position in a file. */
      check_user_n (args + 1, 8);
      arg0 = args[1];
      arg1 = args[2];

      seek ((int) arg0, (unsigned int) arg1);
      break;

    case SYS_TELL:                   /* Report current position in a file. */
      check_user_n (args + 1, 4);
      arg0 = args[1];

      f->eax = (uint32_t) tell ((int) arg0);
      break;
    case SYS_CLOSE:                  /* Close a file. */
      check_user_n (args + 1, 4);
      arg0 = args[1];

      close ((int) arg0); // void
      break;

    /* Project 3 and optionally project 4. */
    case SYS_MMAP:                   /* Map a file into memory. */
    case SYS_MUNMAP:                 /* Remove a memory mapping. */

    /* Project 4 only. */
    case SYS_CHDIR:                  /* Change the current directory. */
      check_user_n (args + 1, 4);
      arg0 = args[1];
      check_user_str ((void*) arg0);

      f->eax = (uint32_t) chdir ((const char *) arg0);
      break;

    case SYS_MKDIR:                  /* Create a directory. */
      check_user_n (args + 1, 4);
      arg0 = args[1];
      check_user_str ((void*) arg0);

      f->eax = (uint32_t) mkdir ((const char *) arg0);
      break;

    case SYS_READDIR:                /* Reads a directory entry. */
      check_user_n (args + 1, 8);
      arg0 = args[1];
      arg1 = args[2];
      check_user_n ((void*) arg1, (NAME_MAX + 1) * sizeof(char));

      f->eax = (uint32_t) readdir((int) arg0, (char *) arg1);
      break;

    case SYS_ISDIR:                  /* Tests if a fd represents a directory. */
      check_user_n (args + 1, 4);
      arg0 = args[1];

      f->eax = (uint32_t) isdir((int) arg0);
      break;

    case SYS_INUMBER:                /* Returns the inode number for a fd. */
      check_user_n (args + 1, 4);
      arg0 = args[1];

      f->eax = (uint32_t) inumber ((int) arg0);
      break;
    case SYS_RESET_BUFFER:                   /* Map a file into memory. */
      reset_buffer ();
      break;
    case SYS_GET_STATS:                 /* Remove a memory mapping. */
      check_user_n (args + 1, 4);
      arg0 = args[1];

      if ((uint32_t) arg0 == 0)
        f->eax = (uint32_t) hits;
      else
        f->eax = (uint32_t) misses;
      break;
    default:                         /* All unimplemented syscalls. */
      thread_current ()->exit_code = -1;
      thread_exit();
      break;
    }
}

/* Reads a byte at user virtual address UADDR.
UADDR must be below PHYS_BASE.
Returns the byte value if successful, -1 if a segfault
occurred. */
static int
get_user (const uint8_t *uaddr)
{
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
  : "=&a" (result) : "m" (*uaddr));
  return result;
}

/* Validates that the user buffer of size n at buf is valid. */
static void
check_user_n (void *buf, int n)
{
  int i;
  struct thread *cur = thread_current();
  uint8_t *cbuf = (uint8_t*) buf;

  /* First, check if any part of the user buffer is in
     kernel memory. If it is, immediately exit. */
  if (!is_user_vaddr (cbuf + n)) {
    cur->exit_code = -1;
    thread_exit ();
    return;
  }

  /* Then, attempt to read bytes. */
  for (i = 0; i < n; i++)
    {
      /* If we hit an invalid address, kill the process. */
      if (get_user (cbuf + i) == -1) {
        cur->exit_code = -1;
        thread_exit ();
        return;
      }
    }
}

/* Checks that the user provided string is valid.
   Kill the process if we encounter a page fault, or if
   any part of the user buffer str is above PHYS_BASE. */
static void
check_user_str (void *str)
{
  struct thread *cur = thread_current();
  if (str == NULL)
    {
      cur->exit_code = -1;
      thread_exit ();
      return;
    }
  int i, byte;
  uint8_t *cstr = (uint8_t*) str;
  for (i = 0; 1; i++)
    {
      /* Unlike before, we have to check the addresses as we go. */
      if (!is_user_vaddr (cstr + i)) {
        cur->exit_code = -1;
        thread_exit ();
        return;
      }

      /* Try to load the next byte. */
      byte = get_user (cstr + i);
      ++i;
      /* If we hit an invalid address, immediately exit. */
      if (byte == -1) {
        cur->exit_code = -1;
        thread_exit ();
        return;
      }

      /* If we hit a NULL, we have a valid string. */
      if (byte == 0)
        return;
    }
}
