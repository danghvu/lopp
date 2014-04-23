/* Includes */
#include <unistd.h>     /* Symbolic Constants */
#include <sys/types.h>  /* Primitive System Data Types */
#include <errno.h>      /* Errors */
#include <stdio.h>      /* Input/Output */
#include <stdlib.h>     /* General Utilities */
#include <pthread.h>    /* POSIX Threads */
#include <string.h>     /* String handling */

#include "aloc.h"

/* prototype for thread routine */
void *print_message_function ( void *ptr );

/* struct to hold data to be passed to a thread
   this shows how multiple data items can be passed to a thread */
typedef struct str_thdata
{
    int thread_no;
    char message[100];
} thdata;

int main()
{
    pthread_t thread[20];  /* thread variables */
    thdata data[20];         /* structs to be passed to threads */

    aloc_init(0);

    printf("Numcore: %d\n", aloc_getNumCores());

    int i = 0;
    for (;i<20;i++) {
      /* initialize data to pass to thread 1 */
      data[i].thread_no = i;
      strcpy(data[i].message, "Hello!");

      /* create threads 1 and 2 */
      pthread_create (&thread[i], NULL, &print_message_function, (void *) &data[i]);
    }

    /* Main block now waits for both threads to terminate, before it exits
       If main block exits, both threads exit, even if the threads have not
       finished their work */

    for (i=0;i<20;i++) {
      pthread_join(thread[i], NULL);
    }

    aloc_finalize();
    /* exit */
    exit(0);
} /* main() */

/**
 * print_message_function is used as the start routine for the threads used
 * it accepts a void pointer
**/
void *print_message_function ( void *ptr )
{
    thdata *data;
    data = (thdata *) ptr;  /* type cast to a pointer to thdata */
    aloc_bindThreadGroup(data->thread_no, 2);

    /* do the work */
    printf("Thread %d says %s \n", data->thread_no, data->message);

    pthread_exit(0); /* exit */
} /* print_message_function ( void *ptr ) */
