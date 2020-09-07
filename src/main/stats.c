//
// Created by vasilis on 20/08/20.
//

#ifdef ZOOKEEPER
  #include "zk_util.h"
  #define appl_stats zk_stats
#endif

#ifdef KITE

#include <latency_util.h>
#include "util.h"
  #define appl_stats kite_stats
#endif

#ifdef DERECHO
  #include "dr_util.h"
  #define appl_stats dr_stats
#endif

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




  //
  //fprintf(latency_stats_fd, "#---------------- ACQUIRES --------------\n");
  //for(i = 0; i < LATENCY_BUCKETS; ++i)
  //  fprintf(latency_stats_fd, "acquires: %d, %d\n", i * (MAX_LATENCY / LATENCY_BUCKETS), latency_count.acquires[i]);
  //fprintf(latency_stats_fd, "acquires: -1, %d\n", latency_count.acquires[LATENCY_BUCKETS]); //print outliers
  //fprintf(latency_stats_fd, "acquires-hl: %d\n", latency_count.max_acq_lat); //print max
  //
  //fprintf(latency_stats_fd, "#---------------- RELEASES ---------------\n");
  //for(i = 0; i < LATENCY_BUCKETS; ++i)
  //  fprintf(latency_stats_fd, "releases: %d, %d\n",i * (MAX_LATENCY / LATENCY_BUCKETS), latency_count.releases[i]);
  //fprintf(latency_stats_fd, "releases: -1, %d\n",latency_count.releases[LATENCY_BUCKETS]); //print outliers
  //fprintf(latency_stats_fd, "releases-hl: %d\n", latency_count.max_rel_lat); //print max
  //
  //fprintf(latency_stats_fd, "#---------------- READS --------------\n");
  //for(i = 0; i < LATENCY_BUCKETS; ++i)
  //  fprintf(latency_stats_fd, "reads: %d, %d\n", i * (MAX_LATENCY / LATENCY_BUCKETS), latency_count.reads[i]);
  //fprintf(latency_stats_fd, "reads: -1, %d\n", latency_count.acquires[LATENCY_BUCKETS]); //print outliers
  //fprintf(latency_stats_fd, "reads-hl: %d\n", latency_count.max_acq_lat); //print max
  //
  //fprintf(latency_stats_fd, "#---------------- WRITES ---------------\n");
  //for(i = 0; i < LATENCY_BUCKETS; ++i)
  //  fprintf(latency_stats_fd, "writes: %d, %d\n",i * (MAX_LATENCY / LATENCY_BUCKETS), latency_count.writes[i]);
  //fprintf(latency_stats_fd, "writes: -1, %d\n",latency_count.releases[LATENCY_BUCKETS]); //print outliers
  //fprintf(latency_stats_fd, "writes-hl: %d\n", latency_count.max_rel_lat); //print max
  //
  //fprintf(latency_stats_fd, "#---------------- RMWs ---------------\n");
  //for(i = 0; i < LATENCY_BUCKETS; ++i)
  //  fprintf(latency_stats_fd, "rmws: %d, %d\n",i * (MAX_LATENCY / LATENCY_BUCKETS), latency_count.rmws[i]);
  //fprintf(latency_stats_fd, "rmws: -1, %d\n",latency_count.releases[LATENCY_BUCKETS]); //print outliers
  //fprintf(latency_stats_fd, "rmws-hl: %d\n", latency_count.max_rel_lat); //print max



  fclose(latency_stats_fd);

  printf("Latency stats saved at %s\n", filename);
}