/*
 *	File	: pc.c
 *
 *	Title	: Demo Producer/Consumer.
 *
 *	Short	: A solution to the producer consumer problem using
 *		pthreads.
 *
 *	Long 	:
 *
 *	Author	: Andrae Muys
 *
 *	Date	: 18 September 1997
 *
 *	Revised	:
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#define QUEUE_SIZE 100  /// The size of the queue
#define LOOP 10000     /// The number of work units every producer creates
#define PRODUCER 1     /// The number of producers
#define CONSUMER 1     /// The number of consumers

int work_done = 0;  /// The total number of work units executed
double average_time = 0;  /// The average time that the work unit waited in the queue

void *producer (void *args);
void *consumer (void *args);

/**
 * workFunction struct represents a work unit that has to be executed. The struct contains a function pointer to the
 * function to be executed and pointer to the arguments of the function.
 */
typedef struct {
    void * (*work)(void *);  /// the function to be executed
    void *arg;  /// The arguments of the function. Can be a pointer to anything as long as the typecast is correct
} workFunction;


/**
 * The queue struct. The queue is implemented to be circular, meaning that when the last element of the queue is placed
 * the next element inserted will override the first element of the queue as long as that element has already been
 * executed. Also the struct provides utilities for wait time measurement
 */
typedef struct {
    workFunction buf[QUEUE_SIZE];  /// The queue
    struct timeval time_buf[QUEUE_SIZE];  /// The timeval struct of the moment each element was inserted in the queue.

    long head, tail;  /// Head points to the item to be popped and tail points to the element that will be replaced
    int full, empty;  /// flags that show if the queue is full or empty.
    pthread_mutex_t *mut;  /// Mutex used for the atomic operations
    pthread_cond_t *notFull, *notEmpty;  /// Condition variables to signal threads when the queue is full or empty
} queue;



queue *queueInit (void);
void queueDelete (queue *q);
void queueAdd (queue *q, workFunction in, struct timeval timestamp);
void queueDel (queue *q, workFunction *out, struct timeval *timestamp);
void *workFunc (void *id);

int main (int argc, char **argv){
    queue *fifo;  /// The queue
    pthread_t pro[PRODUCER], con[CONSUMER];   /// Keeps the id of the threads created

    fifo = queueInit ();  // init the queue
    if (fifo ==  NULL){
        fprintf (stderr, "main: Queue Init failed.\n");
        exit (1);
    }

    // Create the consumer threads
    for (int q = 0; q < CONSUMER; ++q) {
        pthread_create (&con[q], NULL, consumer, fifo);
    }

    // Create the producer threads
    for (int p = 0; p < PRODUCER; ++p) {
        pthread_create (&pro[p], NULL, producer, fifo);
    }

    // We join only the producer threads because consumers run in an infinite loop. Once the main function returns
    // all the producer threads will be terminated
    for (int p = 0; p < PRODUCER; ++p) {
        pthread_join (pro[p], NULL);
    }

    printf("\n\nMean waiting time %.3f\n\n", average_time);

//    printf("Deleting queue...\n\n");
    queueDelete(fifo);  // delete the queue for cleanup

//    printf("Queue deleted\n\n");

    return 0;
}

/**
 * Function that is executed by the consuming threads. The function simply does some additions
 * @param arg
 * @return
 */
void *workFunc(void *arg){
    // Type cast the args to int array
    int *id = (int*)arg;

    for (int i = 0; i < 10000; ++i) {
        int a, b;
        a = 5;
        b = 6;

        a = a + b;
    }
    //printf("The work %d produced from producer %d was consumed\n", id[1], id[0]);

    return NULL;
}

/**
 * The producing thread runnable function
 * @param q pointer to the queue struct
 * @return
 */
void *producer (void *q){
    queue *fifo;
    int producerId = pthread_self();  /// The id of the thread

    fifo = (queue *)q;  // typecast the queue

    // Create the work and put it in the queue
    for (int i = 0; i < LOOP; i++){
        pthread_mutex_lock (fifo->mut);  // lock the mutex

        // check if the queue has space available
        while (fifo->full){
//            printf ("producer: queue FULL.\n");
            // if not wait for a signal and give the lock to an other thread
            pthread_cond_wait (fifo->notFull, fifo->mut);
        }

        workFunction work;  /// The work unit struct
        int *workArgs = (int*)malloc(2* sizeof(int));  // Allocate space for the arguments

        workArgs[0] = producerId;  // The first argument is the producer id
        workArgs[1] = i;  // The second argument is the incrementing number of the work

        work.arg = workArgs;
        work.work = workFunc;

        // Pass the timestamp of the creation of the task
        struct timeval addTime;
        gettimeofday(&addTime, NULL);
        queueAdd (fifo, work, addTime);

//        printf("producer %d added to queue\n", producerId);

        pthread_mutex_unlock (fifo->mut);
        pthread_cond_signal (fifo->notEmpty);
    }
//    printf("\n\nProducer exiting...\n\n");
    return (NULL);
}


/**
 * The consuming thread runnable function
 * @param q pointer to the queue struct
 * @return
 */
void *consumer (void *q){
    queue *fifo;

    workFunction d;  /// The work to be executed

    fifo = (queue *)q;  // type cast the queue

    // Consuming threads run an infinite loop waiting for work
    while (1) {

        // lock the mutex for atomic operation
        pthread_mutex_lock (fifo->mut);

        // check if the queue is empty
        while (fifo->empty){
//            printf ("consumer: queue EMPTY.\n");

            // if it is empty wait for a signal and give the lock to an other thread
            pthread_cond_wait (fifo->notEmpty, fifo->mut);
        }

        struct timeval add_time;  /// The timeval struct of the moment the work unit was added in the queue
        struct timeval del_time;  /// The timeval struct of the moment the work unit was removed from the queue

        // Retrieve the function with the timestamp
        queueDel (fifo, &d,  &add_time);

        gettimeofday(&del_time, NULL);  // get the timeval of the remove moment

//        printf(
//                "Consumer %ld started consuming %d from producer %d after %ld microseconds\n",
//                pthread_self(),
//                ((int *)d.arg)[1],
//                ((int *)d.arg)[0],
//                (del_time.tv_sec * 1000000 + del_time.tv_usec) - (add_time.tv_sec * 1000000 + add_time.tv_usec)
//        );

        // measure the elapsed time
        long int elapsed_time = (del_time.tv_sec * 1000000 + del_time.tv_usec) - (add_time.tv_sec * 1000000 + add_time.tv_usec);
//        printf("Consumer got work after %ld microseconds\n", elapsed_time);

        d.work(d.arg);  // execute the work function

        // update the average value
        work_done++;
        average_time *= work_done - 1;

        average_time += elapsed_time;

        average_time /= work_done;

        free(d.arg);

        pthread_mutex_unlock (fifo->mut);
        pthread_cond_signal (fifo->notFull);
    }

    return (NULL);
}

/*
  typedef struct {
  int buf[QUEUE_SIZE];
  long head, tail;
  int full, empty;
  pthread_mutex_t *mut;
  pthread_cond_t *notFull, *notEmpty;
  } queue;
*/

queue *queueInit (void){
    queue *q;

    q = (queue *)malloc (sizeof (queue));
    if (q == NULL) return (NULL);

    q->empty = 1;
    q->full = 0;
    q->head = 0;
    q->tail = 0;
    q->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
    pthread_mutex_init (q->mut, NULL);
    q->notFull = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
    pthread_cond_init (q->notFull, NULL);
    q->notEmpty = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
    pthread_cond_init (q->notEmpty, NULL);

    return (q);
}

void queueDelete (queue *q){
    pthread_mutex_destroy (q->mut);
    free (q->mut);
    pthread_cond_destroy (q->notFull);
    free (q->notFull);
    pthread_cond_destroy (q->notEmpty);
    free (q->notEmpty);
    free (q);
}

/**
 * Add the work and the timestamp to the queue
 * @param q the queue
 * @param in the work unit
 * @param timestamp the timestamp
 */
void queueAdd (queue *q, workFunction in, struct timeval timestamp){
    q->buf[q->tail] = in;
    q->time_buf[q->tail] = timestamp;

    // increment the tail
    q->tail++;

    // Check the status of the queue after the insertion
    if (q->tail == QUEUE_SIZE)
        q->tail = 0;
    if (q->tail == q->head)
        q->full = 1;
    q->empty = 0;

    return;
}

/**
 * remove work from the queue
 * @param q the queue
 * @param out the work unit
 * @param timestamp the timestamp of the insertion
 */
void queueDel (queue *q, workFunction *out, struct timeval *timestamp){
    *out = q->buf[q->head];
    *timestamp = q->time_buf[q->head];

    // increment the head
    q->head++;

    // Check the status of the queue after the pop
    if (q->head == QUEUE_SIZE)
        q->head = 0;
    if (q->head == q->tail)
        q->empty = 1;
    q->full = 0;

    return;
}
