//
// Created by vasilis on 22/05/20.
//

#ifndef KITE_STATS_H
#define KITE_STATS_H


//LATENCY Measurements
#include <stdint.h>
#include <time.h>
#include "generic_opcodes.h"


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
  c_stats_t *curr_c_stats;
  c_stats_t *prev_c_stats;
} stats_ctx_t;


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






#endif //KITE_STATS_H
