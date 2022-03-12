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

#define QUEUESIZE 20
#define LOOP 10000
#define PRODUCER 3
#define CONSUMER 24

int work_done = 0;
double mean_time = 0;
pthread_mutex_t elapsed_time_mutex;

void *producer (void *args);
void *consumer (void *args);

typedef struct {
    void * (*work)(void *);
    void *arg;
} workFunction;

typedef struct {
    workFunction buf[QUEUESIZE];
    struct timeval time_buf[QUEUESIZE];

    long head, tail;
    int full, empty;
    pthread_mutex_t *mut;
    pthread_cond_t *notFull, *notEmpty;
} queue;



queue *queueInit (void);
void queueDelete (queue *q);
void queueAdd (queue *q, workFunction in, struct timeval timestamp);
void queueDel (queue *q, workFunction *out, struct timeval *timestamp);
void *workFunc (void *id);

int main (int argc, char **argv){
    queue *fifo;
    pthread_t pro[PRODUCER], con[CONSUMER];

    fifo = queueInit ();
    if (fifo ==  NULL){
        fprintf (stderr, "main: Queue Init failed.\n");
        exit (1);
    }

    pthread_mutex_init(&elapsed_time_mutex, NULL);

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

    printf("\n\nMean waiting time %.3f\n\n", mean_time);

//    printf("Deleting queue...\n\n");
    queueDelete(fifo);

//    printf("Queue deleted\n\n");

    return 0;
}

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

void *producer (void *q){
    queue *fifo;
    int producerId = pthread_self();

    fifo = (queue *)q;

    for (int i = 0; i < LOOP; i++){
        pthread_mutex_lock (fifo->mut);

        while (fifo->full){
//            printf ("producer: queue FULL.\n");
            pthread_cond_wait (fifo->notFull, fifo->mut);
        }

        workFunction work;
        int *workArgs = (int*)malloc(2* sizeof(int));

        workArgs[0] = producerId;
        workArgs[1] = i;

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

void *consumer (void *q){
    queue *fifo;

    workFunction d;

    fifo = (queue *)q;

    while (1) {
        pthread_mutex_lock (fifo->mut);

        while (fifo->empty){
//            printf ("consumer: queue EMPTY.\n");
            pthread_cond_wait (fifo->notEmpty, fifo->mut);
        }

        struct timeval add_time;
        struct timeval del_time;

        // Retrieve the function with the timestamp
        queueDel (fifo, &d,  &add_time);

        // TODO measure the time
        gettimeofday(&del_time, NULL);

//        printf(
//                "Consumer %ld started consuming %d from producer %d after %ld microseconds\n",
//                pthread_self(),
//                ((int *)d.arg)[1],
//                ((int *)d.arg)[0],
//                (del_time.tv_sec * 1000000 + del_time.tv_usec) - (add_time.tv_sec * 1000000 + add_time.tv_usec)
//        );

        long int elapsed_time = (del_time.tv_sec * 1000000 + del_time.tv_usec) - (add_time.tv_sec * 1000000 + add_time.tv_usec);
//        printf("Consumer got work after %ld microseconds\n", elapsed_time);

        // TODO run the function
        d.work(d.arg);

        pthread_mutex_lock(&elapsed_time_mutex);
        work_done++;
        mean_time *= work_done - 1;

        mean_time += elapsed_time;

        mean_time /= work_done;

        pthread_mutex_unlock (&elapsed_time_mutex);


        // TODO deallocate memory
        free(d.arg);

        pthread_mutex_unlock (fifo->mut);
        pthread_cond_signal (fifo->notFull);
    }

    return (NULL);
}

/*
  typedef struct {
  int buf[QUEUESIZE];
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

void queueAdd (queue *q, workFunction in, struct timeval timestamp){
    q->buf[q->tail] = in;
    q->time_buf[q->tail] = timestamp;
    q->tail++;
    if (q->tail == QUEUESIZE)
        q->tail = 0;
    if (q->tail == q->head)
        q->full = 1;
    q->empty = 0;

    return;
}

void queueDel (queue *q, workFunction *out, struct timeval *timestamp){
    *out = q->buf[q->head];
    *timestamp = q->time_buf[q->head];

    q->head++;
    if (q->head == QUEUESIZE)
        q->head = 0;
    if (q->head == q->tail)
        q->empty = 1;
    q->full = 0;

    return;
}
