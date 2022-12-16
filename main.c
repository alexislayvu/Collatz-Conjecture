#include <mqueue.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MESSAGE_PRIORITY 1
#define NUMBER_OF_MESSAGES 10
#define MAX_RANDOM_NUMBER 10000
#define MESSAGE_QUEUE_NAME "/queue_name"

typedef struct MessageInfo {
  int index;
  int start_num;
  int current_num;
  int step_count;
} MessageInfo;

void print_help_instructions();
void produce_message(mqd_t *queue);
void consume_message(mqd_t *queue);

int number_of_messages = NUMBER_OF_MESSAGES;
int max_random_number = MAX_RANDOM_NUMBER;

int main(int argc, char *argv[]) {
  int arg_num = 1;

  while (arg_num < argc) {
    if (argv[arg_num][0] != '-') {
      break;
    }
    if (!strcmp(argv[arg_num], "-h")) {
      print_help_instructions();
    } else if (!strcmp(argv[arg_num], "-n")) {
      arg_num++;
      number_of_messages = atoi(argv[arg_num]);
      arg_num++;
    } else if (!strcmp(argv[arg_num], "-r")) {
      arg_num++;
      max_random_number = atoi(argv[arg_num]);
      arg_num++;
    } else {
      printf("Uh oh! I don't know the command '%s'\n", argv[arg_num]);
      print_help_instructions();
    }
  }

  if (number_of_messages > 10) {
    printf("Sorry, the number of messages cannot exceed 10.\n");
    exit(0);
  }

  time_t timey;
  time(&timey);
  srandom(timey);

  pthread_t produce;
  pthread_t consume;

  struct mq_attr queue_attrib;
  queue_attrib.mq_flags = 0;
  queue_attrib.mq_maxmsg = number_of_messages;
  queue_attrib.mq_msgsize = sizeof(MessageInfo);
  queue_attrib.mq_curmsgs = 0;

  mq_unlink(MESSAGE_QUEUE_NAME);
  mqd_t message_queue = mq_open(MESSAGE_QUEUE_NAME, O_RDWR | O_CREAT | O_EXCL,
                                0644, &queue_attrib);

  if (message_queue == (mqd_t)-1) {
    fprintf(stderr, "Can't open message queue\n");
    perror("mq_open");
    exit(0);
  }

  int iret1 =
      pthread_create(&consume, NULL, (void *)consume_message, &message_queue);
  int iret2 =
      pthread_create(&produce, NULL, (void *)produce_message, &message_queue);

  pthread_join(consume, NULL);
  pthread_join(produce, NULL);

  mq_close(message_queue);
  mq_unlink(MESSAGE_QUEUE_NAME);

  return 0;
}

void print_help_instructions() {
  printf("List of available commands:\n-n number_of_messages, -r "
         "max_random_number\n");
  exit(0);
}

void produce_message(mqd_t *queue) {
  for (int ix = 0; ix < number_of_messages; ++ix) {
    MessageInfo *buffer = malloc(sizeof(MessageInfo));

    buffer->start_num = 1 + rand() % max_random_number;
    buffer->current_num = buffer->start_num;
    buffer->index = ix + 1;
    buffer->step_count = 0;
    printf("producer->queue: (#%d sending %d)\n", buffer->index,
           buffer->start_num);

    int ret =
        mq_send(*queue, (void *)buffer, sizeof(MessageInfo), MESSAGE_PRIORITY);

    free(buffer);
  }

  printf("PRODUCER COMPLETE\n");
}

void consume_message(mqd_t *queue) {
  int messages_remaining = number_of_messages;

  while (messages_remaining != 0) {
    MessageInfo *buffer = malloc(sizeof(MessageInfo));
    unsigned nr = mq_receive(*queue, (void *)buffer, sizeof(MessageInfo), NULL);

    printf("queue->consumer: (#%d received %d) (%d steps)\n", buffer->index,
           buffer->current_num, buffer->step_count);

    if (buffer->current_num == 1) {
      printf("#%d COMPLETE! Original number %d finished in %d steps\n",
             buffer->index, buffer->start_num, buffer->step_count);
      messages_remaining--;
      free(buffer);
      continue;
    } else if (buffer->current_num % 2 == 0) {
      buffer->current_num /= 2;
      buffer->step_count++;
    } else {
      buffer->current_num *= 3;
      buffer->current_num++;
      buffer->step_count++;
    }

    /* reinsert into queue until the value reaches 1 */
    MessageInfo *new_buffer = malloc(sizeof(MessageInfo));
    new_buffer->start_num = buffer->start_num;
    new_buffer->current_num = buffer->current_num;
    new_buffer->index = buffer->index;
    new_buffer->step_count = buffer->step_count;

    int ret = mq_send(*queue, (void *)new_buffer, sizeof(MessageInfo),
                      MESSAGE_PRIORITY);
  }

  printf("CONSUMER COMPLETE\n");
}