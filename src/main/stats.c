//
// Created by vasilis on 20/08/20.
//

#ifdef ZOOKEEPER
  #include "zk_util.h"
  #define appl_stats zk_stats
#endif

#ifdef KITE
  #include "util.h"
  #define appl_stats kite_stats
#endif

#ifdef DERECHO
  #include "dr_util.h"
  #define appl_stats dr_stats
#endif


void *print_stats(void* no_arg)
{

  stats_ctx_t *ctx = calloc(1, sizeof(stats_ctx_t));
  int sleep_time = 10;
  ctx->curr_w_stats = (t_stats_t *) malloc(num_threads * sizeof(t_stats_t));
  ctx->prev_w_stats = (t_stats_t *) malloc(num_threads * sizeof(t_stats_t));
  ctx->curr_c_stats = (c_stats_t *) malloc(num_threads * sizeof(c_stats_t));
  ctx->prev_c_stats = (c_stats_t *) malloc(num_threads * sizeof(c_stats_t));

  sleep(4);
  memcpy(ctx->prev_w_stats, (void *) t_stats, num_threads * (sizeof(struct thread_stats)));
  struct timespec start, end;
  clock_gettime(CLOCK_REALTIME, &start);
  while (true) {
    sleep(sleep_time);
    clock_gettime(CLOCK_REALTIME, &end);
    ctx->seconds = (end.tv_sec - start.tv_sec) + (double) (end.tv_nsec - start.tv_nsec) / 1000000001;
    start = end;
    memcpy(ctx->curr_w_stats, (void *) t_stats, num_threads * (sizeof(struct thread_stats)));

    ctx->print_count++;
    if (EXIT_ON_PRINT && ctx->print_count == PRINT_NUM) {
      //if (MEASURE_LATENCY && machine_id == LATENCY_MACHINE) print_latency_stats();
      printf("---------------------------------------\n");
      printf("------------RUN TERMINATED-------------\n");
      printf("---------------------------------------\n");
      exit(0);
    }

    appl_stats(ctx);

  }
}


