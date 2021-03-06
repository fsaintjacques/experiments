#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/limits.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct params {
  /** number of threads to run concurrently */
  uint8_t n_threads;
  /** number of writes each thread will perform */
  uint16_t n_runs;
  /** number of bytes per write */
  size_t n_bytes;

  /** file descriptor on which the write will append */
  int fd;
  char *path;
} params_t;

bool parse_arguments(int argc, char **argv, params_t *params) {

  int flags = O_RDWR | O_APPEND | O_CREAT;

  if (argc != 5 || sscanf(argv[1], "%" SCNu8, &(params->n_threads)) != 1 ||
      sscanf(argv[2], "%" SCNu16, &(params->n_runs)) != 1 ||
      sscanf(argv[3], "%zu", &(params->n_bytes)) != 1 ||
      (params->fd = open(argv[4], flags, 0755)) == -1) {
    return false;
  }

  params->path = argv[4];

  if (params->n_threads == 0 || params->n_runs == 0 || params->n_bytes == 0) {
    return false;
  }

  return true;
}

typedef struct thread_ctx {
  /** character this thread will write continuously. */
  char byte;
  /** number of times the character is repeated per run. */
  size_t n_bytes;
  /** number of runs */
  uint16_t n_runs;
  /** wait barrier to synchronize threads startup */
  atomic_bool *barrier;
  /** file descriptor to write to */
  int fd;

  /** number of bytes written */
  ssize_t bytes_written;
} thread_ctx_t;

void *write_loop(void *arg) {
  thread_ctx_t *ctx = (thread_ctx_t *)arg;
  const size_t n_bytes = ctx->n_bytes + 1;

  char *buf = malloc(n_bytes);
  if (buf == NULL) {
    goto quit;
  }

  memset(buf, ctx->byte, n_bytes);
  buf[n_bytes - 1] = '\n';

  while (*(ctx->barrier)) {
    asm volatile("" : : : "memory");
  }

  for (size_t i = 0; i < ctx->n_runs; ++i) {
    ssize_t count = write(ctx->fd, buf, n_bytes);
    if (count == -1) {
      perror("write: ");
      ctx->bytes_written = -1;
      goto quit;
    }

    ctx->bytes_written += count;
  }

quit:
  free(buf);
  return &(ctx->bytes_written);
}

int run_loop(params_t params) {
  const uint8_t n_threads = params.n_threads;

  atomic_bool barrier = true;

  pthread_t threads[n_threads];
  thread_ctx_t ctxs[n_threads];

  for (uint8_t t = 0; t < n_threads; ++t) {
    pthread_t *thread = &threads[t];
    thread_ctx_t *ctx = &ctxs[t];

    ctx->byte = '!' + t;
    ctx->n_bytes = params.n_bytes;
    ctx->n_runs = params.n_runs;
    ctx->fd = params.fd;
    ctx->barrier = &barrier;
    ctx->bytes_written = 0;

    if (pthread_create(thread, NULL, write_loop, ctx) != 0) {
      return EXIT_FAILURE;
    }
  }

  sleep(1);

  barrier = false;

  int ret_val = EXIT_SUCCESS;

  for (uint8_t t = 0; t < n_threads; ++t) {
    pthread_t thread = threads[t];
    ssize_t **bytes_writen;
    pthread_join(thread, NULL);

    const ssize_t bytes_written = ctxs[t].bytes_written;
    const size_t n_bytes = ctxs[t].n_bytes;
    const size_t n_runs = ctxs[t].n_runs;
    if (((n_bytes + 1) * n_runs) != bytes_written) {
      ret_val = EXIT_FAILURE;
    }
  }

  return ret_val;
}

int main(int argc, char **argv) {
  params_t params;

  if (!parse_arguments(argc, argv, &params)) {
    return EXIT_FAILURE;
  }

  return run_loop(params);
}
