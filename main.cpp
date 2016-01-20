/**
 * @author Dylan Walseth(walsethd)
 * Build with args -std=c++11 -lpthread
 **/

#include <iostream>
#include <time.h>
#include <pthread.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#define QUEUE_SIZE (20) /* total number of slots in the queue */
#define MAX_SLEEP_TIME (10000000) /* max total microseconds between producers producing*/
using namespace std;

// Which operation type will be performed
typedef enum {
    ADD,
    SUB,
    MUL,
    DIV
} operation_type;
// Pretty name
const char *operation_names[] = {"ADD", "SUB", "MUL", "DIV"};

// Operation object/struct
typedef struct operation{
    int a;
    int b;
    operation_type op_type;
} operation;

// Operations managers
int total_operations_produced = 0;
int total_operations_consumed = 0;
int total_operations;

// Queue managers
operation operations[QUEUE_SIZE];
int operations_in_queue = 0;
int current_write_pos = 0;
int current_read_pos = 0;

// Expected number of operations to finish
int num_operations;

// Flag to notify consumers to stop eating so much and loss some weight
bool flag_consumers = false;

// Mutex setup
pthread_mutex_t queue_mutex;
pthread_cond_t cond;

/**
 * Takes in an operation struct and returns the result based on
 * which operation_type
 */
int perform_operation(operation op){
    int result = 0;
    switch (op.op_type){
        case ADD:
            result = op.a + op.b;
            break;
        case SUB:
            result = op.a - op.b;
            break;
        case MUL:
            result = op.a * op.b;
            break;
        case DIV:
            result = op.a / op.b;
            break;
    }
    return result;
}

/**
 * Creates and moves operations into queue to be consumed by consumer
 * Provided with a seed to ensure random numbers
 */
void *producer(void *seed){
    // Need to create a new seed for each thread or else they will all just produce the same operations...
    srand((unsigned int)seed);
    int produced = 0;
    for(int i = 0; i < num_operations; i++){
        usleep((unsigned)rand()%MAX_SLEEP_TIME);
        pthread_mutex_lock(&queue_mutex);
        operation oper;
        oper.a = rand();
        oper.b = rand();
        oper.op_type = (operation_type)(rand()%4);
        while(operations_in_queue >= QUEUE_SIZE){
            pthread_cond_wait(&cond, &queue_mutex);
        }

        operations[current_write_pos] = oper;
        current_write_pos = (current_write_pos+1)%20;
        operations_in_queue++;
        total_operations_produced++;
        produced++;

        pthread_cond_broadcast(&cond);
        pthread_mutex_unlock(&queue_mutex);
    }
    printf("Producer died\n");
    pthread_exit(NULL);
    return 0;
};

/**
 * Consumes operations and executes operations in the queue
 * Takes in param because my IDE was complaining that is was expecting a param
 */
void *consumer(void *nothing){
    int consumed = 0;

    while(!flag_consumers || operations_in_queue > 0){
        pthread_mutex_lock(&queue_mutex);
        if(operations_in_queue > 0){
            operation op = operations[current_read_pos];
            int result = perform_operation(op);
            printf("A: %d B: %d Operation: %s Result: %d\n", op.a, op.b, operation_names[op.op_type], result);
            current_read_pos = (current_read_pos + 1)%20;
            operations_in_queue--;
            total_operations_consumed++;
            consumed++;
        }
        pthread_cond_broadcast(&cond);
        pthread_mutex_unlock(&queue_mutex);
    }
    printf("Consumer died after consuming %d operations\n", consumed);
    pthread_exit(NULL);
    return 0;
}

/**
 * Handles SIGUSR1 signal bu printing some info about the current queue
 */
void sigusr1_handler(int signum){
    printf("Operations produced: %d\n", total_operations_produced);
    printf("Operations in queue: %d\n", operations_in_queue);
}

/**
 * Handles SIGUSR2 signal bu printing some info about the current queue
 */
void sigusr2_handler(int signum){
    printf("Operations consumed: %d\n", total_operations_consumed);
    printf("Operations in queue: %d\n", operations_in_queue);
}

int main(int argc, char *argv[]) {
    if(argc != 4) {
        printf("Incorrect number of arguments, Expected: 3, Actual: %d\n", argc-1);
        return 1;
    }

    signal(SIGUSR1, sigusr1_handler);
    signal(SIGUSR2, sigusr2_handler);

    // Generate seed based on time
    srand(static_cast<unsigned int>(time(NULL)));

    pthread_mutex_init(&queue_mutex, NULL);
    pthread_cond_init(&cond, NULL);

    num_operations = stoi(argv[1]);
    int num_producers = stoi(argv[2]);
    int num_consumers = stoi(argv[3]);
    total_operations = num_producers * num_operations;

    pthread_t producers[num_producers];
    pthread_t consumers[num_consumers];
    for(int i = 0; i < num_producers; i++){
        /* Give each producer it's own seedling (If I tried to generate a new seed in each of the producers,
         * they are all created at nearly the same time and their seeds are the same, which is boring because they
         * will generate identical operations then.
         */
        int seed = rand();
        pthread_create(&producers[i], NULL, producer, (void*) seed);
    }

    for(int j = 0; j < num_consumers; j++){
        pthread_create(&consumers[j], NULL, consumer, NULL);
    }

    for(int i = 0; i < num_producers; i++){
        pthread_join(producers[i], NULL);
    }

    flag_consumers = true;
    for(int j = 0; j < num_consumers; j++){
        pthread_join(consumers[j], NULL);
    }
    printf("Total Operations: %d\n", total_operations);
    printf("Total Operations Produced: %d\n", total_operations_produced);
    printf("Total Operations Consumed: %d\n", total_operations_consumed);
    printf("These should all be same\n");
    pthread_exit(NULL);
    return 0;
}
