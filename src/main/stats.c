//
// Created by vasilis on 20/08/20.
//


#include "stats_prot_sel.h"
#include "latency_util.h"

void print_latency_stats(void);
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

    appl_stats(ctx);



    ctx->print_count++;
    if (EXIT_ON_PRINT && ctx->print_count == PRINT_NUM) {
      if (MEASURE_LATENCY && machine_id == LATENCY_MACHINE) print_latency_stats();
      printf("---------------------------------------\n");
      printf("------------RUN TERMINATED-------------\n");
      printf("---------------------------------------\n");
      exit(0);
    }

  }
}


void print_latency_stats(void)
{
  FILE *latency_stats_fd;
  int i = 0;
  char filename[128];
  char* path = "../results/latency";
  const char * workload[] = {
    "WRITES", //
    "READS", //
    "MIXED", //
  };
  sprintf(filename, "%s/latency_%s_w_%d%s_%s.csv", path,
          system_name(),
          WRITE_RATIO / 10, "%",
          workload[MEASURE_READ_LATENCY]);

  latency_stats_fd = fopen(filename, "w");

  for (req_type_t req_t = RELEASE_REQ; req_t < LATENCY_TYPE_NUM; ++req_t) {
    if (latency_count.req_meas_num[req_t] == 0) continue;
    fprintf(latency_stats_fd, "#---------------- %s --------------\n", latency_req_to_str(req_t));
    for(i = 0; i < LATENCY_BUCKETS; ++i) {
      fprintf(latency_stats_fd, "%s: %d, %d\n",
              latency_req_to_str(req_t),
              i * (MAX_LATENCY / LATENCY_BUCKETS), latency_count.requests[req_t][i]);
    }
    fprintf(latency_stats_fd, "%s: -1, %d\n", latency_req_to_str(req_t),
            latency_count.requests[req_t][LATENCY_BUCKETS]); //print outliers
    fprintf(latency_stats_fd, "%s: max, %d\n",
            latency_req_to_str(req_t),
            latency_count.max_req_lat[req_t]); //print max
  }
  fclose(latency_stats_fd);

  printf("Latency stats saved at %s\n", filename);
}