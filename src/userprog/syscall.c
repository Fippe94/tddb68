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
  const char *cmd_line;
  tid_t tid;
  struct thread *t;
  struct child_status *cs;
  struct child_status *c_s;
  struct list_elem *e;
  bool found_tid = false;

  //printf("[%s:%s:%d] Hello world!\n", __FILE__, __FUNCTION__, __LINE__);
  if (f->esp == NULL || f->esp >= PHYS_BASE || pagedir_get_page(thread_current()->pagedir,f->esp) == NULL)
    thread_exit();
  int *syscallid = (f->esp);
   f->esp +=4;
  switch (*syscallid){
  case SYS_HALT:
    power_off();
    break;
  case SYS_CREATE:
    name = *((char**)(f->esp));
    if (name == NULL || name >= PHYS_BASE || pagedir_get_page(thread_current()->pagedir,name) == NULL)
      thread_exit();    
    f->esp +=4;
    size = *((int*)(f->esp));
    f->esp += 4;
    (f->eax) = filesys_create(name,size);
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
    fd = *((int*)(f->esp));
    f->esp +=4;
    buffer = *((void**)(f->esp));
    if (buffer == NULL || buffer >= PHYS_BASE || pagedir_get_page(thread_current()->pagedir,buffer) == NULL)
      thread_exit();
    f->esp += 4;
    size = *((int*)(f->esp));
    f->esp += 4;
    // If fd == 1, write to console
    if (fd == 1){
      putbuf(buffer,size);
      f->eax = size;
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
  case SYS_WAIT:
    tid = *((int*)(f->esp));
    for (e = list_begin(&t->child_list); e != list_end(&t->child_list);e = list_next(e)){
      c_s = list_entry(e,struct child_status,elem);
      if (c_s->tid == tid){
	found_tid = true;
	cs = c_s;
	break;
      }
    }
    if (!found_tid){
      f->eax = -1;
      return;
    }
    if (cs->exit_status == -1){
      sema_down(&cs->sema);
    }
    f->eax = cs->exit_status;
    free(cs); 
    break;
  case SYS_EXEC:
    cmd_line =  *((char**)(f->esp));
    char *cmd;
    int i;
    for (i = 0; i <= strlen(cmd_line);++i){
      cmd[i] = cmd_line[i];
    }

   
    cmd[i] = "\0";
    //    printf("\n");
    // printf("%c\n",*cmd_line);
    //printf("%x\n",&cmd_line);
    printf("[%s:%s:%d:%s] Hello world!\n", __FILE__, __FUNCTION__, __LINE__, thread_name());
if (cmd_line == NULL || cmd_line >= PHYS_BASE || pagedir_get_page(thread_current()->pagedir,cmd_line) == NULL)
    thread_exit();
//printf("[%s:%s:%d:%s] Hello world!\n", __FILE__, __FUNCTION__, __LINE__, thread_name());
 
//f->esp +=4; 
    //putbuf(&"1",1);
    printf("[%s:%s:%d:%s] Hello world!\n", __FILE__, __FUNCTION__, __LINE__, thread_name());
    tid = process_execute(cmd);       
    //printf("[%s:%s:%d:%s] Hello world!\n", __FILE__, __FUNCTION__, __LINE__, thread_name());
    if (tid == TID_ERROR)
      f->eax = -1;
    else
      f->eax = tid;
    break;
  case SYS_EXIT:
    if (f->esp == NULL || f->esp >= PHYS_BASE || pagedir_get_page(thread_current()->pagedir,f->esp) == NULL)
      thread_exit();
    status = *((int*)(f->esp));
    f->esp +=4;
    t = thread_current();
    t->cs->exit_status = status;
    t->exit_status = status;
    
    
    //free(&thread_current()->child_list);
    f->eax = status;
    thread_exit();
    break;
  }
}
