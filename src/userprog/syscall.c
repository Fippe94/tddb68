#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "lib/kernel/stdio.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "lib/kernel/bitmap.h"
#include "devices/input.h"
#include <string.h>
#include "userprog/process.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"


static void syscall_handler (struct intr_frame *);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  unsigned size;
  char *name;
  int fd;
  int bytes_read;
  int bytes_written;
  struct file *openFile;
  void *buffer;
  int status;

  if (f->esp == NULL || f->esp >= PHYS_BASE || pagedir_get_page(thread_current()->pagedir,f->esp) == NULL)
    thread_exit();

  int *syscallid = (f->esp);
  f->esp += 4;
  switch (*syscallid){
    case SYS_HALT:
      power_off();
      break;
    case SYS_CREATE:
    printf("[%s:%s:%d] Hello world!\n", __FILE__, __FUNCTION__, __LINE__);
      name = *((char**)(f->esp));
      if (name == NULL || name >= PHYS_BASE || pagedir_get_page(thread_current()->pagedir,name) == NULL)
        thread_exit();
      f->esp +=4;
      printf("[%s:%s:%d] Hello world!\n", __FILE__, __FUNCTION__, __LINE__);
      size = *((int*)(f->esp));
      f->esp += 4;
      (f->eax) = filesys_create(name,size);
      printf("[%s:%s:%d] Hello world!\n", __FILE__, __FUNCTION__, __LINE__);
      break;
    case SYS_OPEN:
      // Finds the first fd that is not used by looking in the bitmap
      fd =  bitmap_scan_and_flip(thread_current()->fdMap,0,1,0);
      name = *((char**)(f->esp));
      if (name == NULL || name >= PHYS_BASE || pagedir_get_page(thread_current()->pagedir,name) == NULL)
        thread_exit();
      f->esp += 4;
      // If there is no free fd, return -1
      if (fd == BITMAP_ERROR){
        (f->eax) = -1;
        return;
      }
      openFile = filesys_open(name);
      // If the file could not be opened, return -1
      if (openFile == NULL){
        bitmap_reset(thread_current()->fdMap,fd);
        (f->eax) = -1;
      }
      else{
        thread_current()->fdAddMap[fd] = openFile;
        fd +=2;
        (f->eax) = fd;
      }
      break;
    case SYS_READ:
      fd = *((int*)(f->esp));
      f->esp +=4;
      buffer = *((void**)(f->esp));
      if (buffer == NULL || buffer >= PHYS_BASE || pagedir_get_page(thread_current()->pagedir,buffer) == NULL)
        thread_exit();
      f->esp += 4;
      size = *((int*)(f->esp));
      f->esp += 4;
      if (fd > 1){
        fd-=2;
        // If the file has been opened, read from it and return the result
        if (fd < 128 && bitmap_test(thread_current()->fdMap,fd) == 1){
        	openFile = thread_current()->fdAddMap[fd];
        	bytes_read = file_read(openFile,buffer,size);
        	f->eax = bytes_read;
        }
        else
  	     f->eax = -1;
      }
      // If fd == 0, read from console/keyboard instead
      else if (fd == 0){
        int counter = 0;
        while(counter++ < size){
        	*((uint8_t*)buffer) = input_getc();
        	putbuf(buffer,sizeof(uint8_t));
        	buffer+= sizeof(uint8_t);
        }
        f->eax = size;
      }
      else
        f->eax = -1;
      break;
    case SYS_WRITE:
    printf("[%s:%s:%d] Hello world!\n", __FILE__, __FUNCTION__, __LINE__);
      fd = *((int*)(f->esp));
      f->esp +=4;
      buffer = *((void**)(f->esp));
      //if (buffer == NULL || buffer >= PHYS_BASE || pagedir_get_page(thread_current()->pagedir,buffer) == NULL)
      //  thread_exit();
      f->esp += 4;
      size = *((int*)(f->esp));
      f->esp += 4;
      printf("[%s:%s:%d] Hello world!\n", __FILE__, __FUNCTION__, __LINE__);
      // If fd == 1, write to console
      if (fd == 1){
        printf("[%s:%s:%d] Hello world!\n", __FILE__, __FUNCTION__, __LINE__);
        putbuf(buffer,size);
        f->eax = size;
        printf("[%s:%s:%d] Hello world!\n", __FILE__, __FUNCTION__, __LINE__);
      }
      else if (fd > 1){
        fd -=2;
        // else, check if the file is open and write to it
        if (fd < 128 && bitmap_test(thread_current()->fdMap,fd) == 1){
          bytes_written = file_write(thread_current()->fdAddMap[fd],buffer,size);
        	f->eax = bytes_written;
        }
        else{
  	      f->eax = -1;
        }
      }
      else{
        f->eax = -1;
      }
      printf("[%s:%s:%d] Hello world!\n", __FILE__, __FUNCTION__, __LINE__);
      break;
    case SYS_CLOSE:
      fd = *((int*)(f->esp));
      f->esp +=4;
      //Check if file is open, and then close it
      fd -=2;
      if (fd >= 0 && fd < 128 && bitmap_test(thread_current()->fdMap,fd) == 1){
        file_close(thread_current()->fdAddMap[fd]);
        bitmap_reset(thread_current()->fdMap,fd);
      }
      break;
    case SYS_EXIT:
      if (f->esp == NULL || f->esp >= PHYS_BASE || pagedir_get_page(thread_current()->pagedir,f->esp) == NULL)
        thread_exit();
      status = *((int*)(f->esp));
      f->esp +=4;
      f->eax = status;
      thread_exit();
      break;
  }
}
