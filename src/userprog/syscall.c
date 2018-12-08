#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "devices/input.h"

static int fd_counter = 2;

typedef int pid_t;

/*
struct file_descriptor{
	int fid;
	int owner;
	struct file* sys_file;
	struct list_elem elem;
};
*/

struct file_descriptor* 
get_id_fd (int id){
	struct file_descriptor* fd = NULL;
	struct list_elem* e = list_tail (&fd_list);
		while ((e = list_prev(e)) != list_head (&fd_list)){
		fd = list_entry (e, struct file_descriptor, elem);
		if(fd->fid == id) 
			break;
	}
	return fd;
}

static bool validate_user_ptr(void* ptr){
	struct thread* cur = thread_current ();
	if(ptr != NULL && is_user_vaddr (ptr)){
		return pagedir_get_page (cur->pagedir, ptr) != NULL;
	}
	else return false;
}

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&file_lock);
  list_init (&fd_list);
}

void halt(void){
	shutdown_power_off ();
}

void exit(int status){
	struct child_info * child;
	struct thread* cur = thread_current ();
	struct thread* parent = get_id_thread (cur->parent_id);

	if (parent != NULL){
		printf ("%s: exit(%d)\n",cur->name, status);
		struct list_elem* e = list_tail (&parent->children);
		while ((e = list_prev(e)) != list_head (&parent->children)){
			child = list_entry(e, struct child_info, elem);
			if(cur->tid == child->child_id){
				lock_acquire (&parent->child_lock);
				child->exit_status = status;
				child->exit_normally = true;
				lock_release (&parent->child_lock);
			}
		}
	}
	thread_exit ();
}

pid_t exec(char* cmd_line){
	if( !validate_user_ptr (cmd_line))
		exit (-1);


	pid_t pid;
	struct thread* cur = thread_current ();


	/* acquire before process_execute to avoid race condition */

	lock_acquire(&cur->child_lock);
	cur->load_status = 0;
	pid = process_execute (cmd_line);
	if (pid == -1){
		printf ("dsy\n");
		lock_release (&cur->child_lock);
		return -1;
	}
	while(cur->load_status == 0){
		cond_wait (&cur->child_cond, &cur->child_lock);
	}
	//printf ("%d: exec stop wait\n", cur->tid);
	if(cur->load_status == -1){
		char* exec_name;
  		char *args;
  		exec_name = strtok_r(cmd_line, " ", &args);
		//printf ("%s: exit(%d)\n",exec_name, -1);
		lock_release (&cur->child_lock);
		return -1;
	}

	lock_release (&cur->child_lock);
	return pid; 
}

int wait (pid_t pid){
	return process_wait (pid);
}

bool create (char* file_name, unsigned file_size){
	if ( !validate_user_ptr (file_name) )
		exit (-1);
	
	bool result = 0;

	lock_acquire (&file_lock);
	result = filesys_create (file_name, file_size);
	lock_release (&file_lock);
	return result;
}

bool remove (char* file_name){
	if ( !validate_user_ptr (file_name) )
		exit (-1);

	bool result;
	lock_acquire (&file_lock);
	result = filesys_remove (file_name);
	lock_release (&file_lock);
	return result;
}

int open (char* file_name){

	if ( !validate_user_ptr (file_name) )
		exit (-1);

	int result = -1;
	struct file* f;
	struct file_descriptor* fd;

	lock_acquire(&file_lock);

	f = filesys_open (file_name);
	if(f != NULL){
		fd = malloc (sizeof *fd);
		fd->fid = fd_counter++;
		fd->sys_file = f;
		fd->owner = thread_current ()->tid;
		list_push_back (&fd_list, &fd->elem);
		result = fd->fid;
	}

	lock_release(&file_lock);
	return result;
}

int filesize (int fid){
	struct file_descriptor* fd;
	struct file* f;

	int result = 0;
	lock_acquire (&file_lock);
	fd = get_id_fd (fid);
	if(fd != NULL){
		f = fd->sys_file;
		result = file_length (f);
	}
	lock_release (&file_lock);
	return result;
}

int read (int id, void* buffer, unsigned size){
	if ( !validate_user_ptr (buffer) )
		exit (-1);

	int result = 0;
	struct file_descriptor* fd;


	lock_acquire (&file_lock);

	if (id == STDOUT_FILENO){
		result = -1;
	}
	else if(id == STDIN_FILENO){

		uint8_t *ptr = buffer;
		uint8_t c;
		unsigned i;
		for (i = 0; i < size && (c = input_getc()) != 0; i++){
			*ptr = c;
			ptr++;
		}
		result = (int) i;
	}
	else{
		fd = get_id_fd (id);
		if (fd != NULL){
			result = file_read (fd->sys_file, buffer, size);
		}
	}
	lock_release (&file_lock);

	return result;
}

int
write (int id, const void *buffer, unsigned size){
	if ( !validate_user_ptr (buffer) ){
		exit (-1);
	}

	struct file_descriptor* fd;
	int result = 0;


	lock_acquire (&file_lock);

	if(id == STDIN_FILENO){
		result = 0;
	}
	else if(id == STDOUT_FILENO){
		putbuf (buffer, size);
		result = size;
	}
	else{
		fd = get_id_fd (id);
		if(fd != NULL){
			result = file_write (fd->sys_file, buffer, size);
		}
	}

	lock_release (&file_lock);
	return result;
}

void
seek (int id, unsigned position){
	struct file_descriptor* fd;

	lock_acquire (&file_lock);

	fd = get_id_fd (id);
	if (fd != NULL)
		file_seek (fd->sys_file, position);

	lock_release (&file_lock);
	return;
}

unsigned
tell (int id){
	struct file_descriptor* fd;
	unsigned result;

	lock_acquire (&file_lock);

	fd = get_id_fd (id);
	if( fd != NULL)
		result = file_tell (fd->sys_file);

	lock_release (&file_lock);
	return result;
}

void
close (int id){
	struct file_descriptor* fd;


	lock_acquire (&file_lock);

	fd = get_id_fd (id);

	if (fd != NULL && fd->owner != thread_current ()->tid){
		lock_release (&file_lock);
		exit (-1);
	}

	if(fd != NULL && fd->owner == thread_current ()->tid){
		file_close (fd->sys_file);
		list_remove (&fd->elem);
		free (fd);
	}

	lock_release (&file_lock);
	return;
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int* stack_ptr = (int*) f->esp;
  
  if( !validate_user_ptr (stack_ptr) ||
      !validate_user_ptr (stack_ptr + 1) ||
      !validate_user_ptr (stack_ptr + 2) ||
      !validate_user_ptr (stack_ptr + 3)){
  	exit (-1);
  }
  else{
  	int sys_code = * stack_ptr;
  	int exit_status;
  	switch(sys_code){
  		case SYS_HALT:
          halt ();
		  break;
		case SYS_EXIT:
		  exit_status = *(stack_ptr + 1);
          exit (exit_status);
		  break;
		case SYS_EXEC:
          f->eax = exec ((char *) *(stack_ptr + 1));
		  break;
		case SYS_WAIT:
          f->eax = wait (*(stack_ptr + 1));
		  break;
		case SYS_CREATE:
		  f->eax = create ((char*) *(stack_ptr + 4), *(stack_ptr + 5));
		  break;
		case SYS_REMOVE:
		  f->eax = remove ((char*) *(stack_ptr + 1));
		  break;
		case SYS_OPEN:
		  f->eax = open ((char*) *(stack_ptr + 1));
		  break;
		case SYS_FILESIZE:
		  f->eax = filesize(*(stack_ptr + 1));
		  break;
		case SYS_READ:
		  f->eax = read (*(stack_ptr + 5), (void*) *(stack_ptr + 6), *(stack_ptr + 7));
		  break;
		case SYS_WRITE: 
		  f->eax = write (*(stack_ptr + 5), (void*) *(stack_ptr + 6), *(stack_ptr + 7));
		  break;
		case SYS_SEEK:
		  seek (*(stack_ptr + 4), *(stack_ptr + 5));
		  break;
		case SYS_TELL:
		  f->eax = tell (*(stack_ptr + 1));
		  break;
		case SYS_CLOSE:
		  close (*(stack_ptr + 1));
		  break;
  	}
  }
}
