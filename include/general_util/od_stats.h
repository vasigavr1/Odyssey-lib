//
// Created by vasilis on 22/05/20.
//

#ifndef OD_STATS_H
#define OD_STATS_H


//LATENCY Measurements
#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include "od_generic_opcodes.h"


#define MAX_LATENCY 400 //in us
#define LATENCY_BUCKETS 200 //latency accuracy

// Store statistics from the workers, for the stats thread to use
typedef struct thread_stats t_stats_t;
typedef struct stats all_stats_t;
typedef struct client_stats c_stats_t;


typedef struct stats_ctx {
  double seconds;
  uint16_t print_count;
  t_stats_t *curr_w_stats;
  t_stats_t *prev_w_stats;
  t_stats_t *all_per_t;
  t_stats_t *all_aggreg;
  c_stats_t *curr_c_stats;
  c_stats_t *prev_c_stats;
} stats_ctx_t;

typedef struct od_qp_stats{
  uint64_t sent;
  uint64_t mes_sent;
  uint64_t received;
  uint64_t mes_received;
} od_qp_stats_t;


typedef struct client_stats {
  uint64_t microbench_pushes;
  uint64_t microbench_pops;
  //uint64_t ms_enqueues;
  // uint64_t ms_dequeues;
} c_stats_t;

// For latency measurements
typedef enum {
  RELEASE_REQ = 0,
  ACQUIRE_REQ = 1,
  WRITE_REQ = 2,
  READ_REQ = 3,
  RMW_REQ = 4,
  NO_REQ
} req_type_t;

#define LATENCY_TYPE_NUM 5

typedef struct latency_flags {
  req_type_t measured_req_flag;
  uint32_t measured_sess_id;
  //struct key* key_to_measure;
  struct timespec start;
} latency_info_t;




struct latency_counters {

  uint32_t** requests;
  uint64_t total_measurements;
  uint32_t *max_req_lat;
  uint32_t *req_meas_num;

};


struct local_latency {
  int measured_local_region;
  uint8_t local_latency_start_polling;
  char* flag_to_poll;
};



static inline void stats_per_thread(const uint64_t *cur,
                                    const uint64_t *prev,
                                    uint64_t *all_aggreg,
                                    uint64_t *new_stats,
                                    uint16_t size)
{
  uint16_t number_of_stats = size / sizeof(uint64_t);
  for (uint16_t i = 0; i < number_of_stats; i ++) {
    new_stats[i] =  (cur[i] - prev[i]);
    all_aggreg[i] += new_stats[i];
  }
}

static inline double per_sec(stats_ctx_t *ctx,
                             uint64_t stat)
{
  double seconds = ctx->seconds * MILLION;
  return stat == 0 ? (double) 0 :
                     (double) stat / seconds;

}

static inline double get_batch(stats_ctx_t *ctx,
                               od_qp_stats_t *stats)
{
  double seconds = ctx->seconds * MILLION;
  return (double) (stats->sent) /
         (double) (stats->mes_sent);
}

#endif //OD_STATS_H

