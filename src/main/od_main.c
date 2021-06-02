//
// Created by vasilis on 20/08/20.
//

#include "od_kvs.h"


#include "od_main_prot_sel.h"

#include <od_init_func.h>



//Global Vars
int bqr_read_buffer_size, bqr_is_remote;
int is_roce, machine_id, num_threads;
int write_ratio = WRITE_RATIO;
struct latency_counters latency_count;
t_stats_t t_stats[WORKERS_PER_MACHINE];
c_stats_t c_stats[CLIENTS_PER_MACHINE];
remote_qp_t ***rem_qp;  //[MACHINE_NUM][WORKERS_PER_MACHINE][QP_NUM];
atomic_bool qps_are_set_up;
atomic_bool print_for_debug;
FILE* client_log[CLIENTS_PER_MACHINE];
struct wrk_clt_if interface[WORKERS_PER_MACHINE];
uint64_t last_pulled_req[SESSIONS_PER_MACHINE];
uint64_t last_pushed_req[SESSIONS_PER_MACHINE];
uint64_t time_approx;
all_qp_attr_t *all_qp_attr;
atomic_uint_fast32_t workers_with_filled_qp_attr;



int main(int argc, char *argv[])
{
  appl_init_func(argc, argv);
  struct thread_params *param_arr;

  num_threads =  WORKERS_PER_MACHINE;
  param_arr = malloc(TOTAL_THREADS * sizeof(struct thread_params));
  pthread_t * thread_arr = malloc(TOTAL_THREADS * sizeof(pthread_t));

  pthread_attr_t attr;
  cpu_set_t pinned_hw_threads;
  pthread_attr_init(&attr);
  bool occupied_cores[TOTAL_CORES] = { 0 };
  char node_purpose[15];
  sprintf(node_purpose, "Worker");

  for(uint16_t i = 0; i < TOTAL_THREADS; i++) {
    if (i < WORKERS_PER_MACHINE) {
      #ifdef KITE
        // PAXOS VERIFIER
        if (VERIFY_PAXOS || PRINT_LOGS || COMMIT_LOGS) {
          char fp_name[40];
          sprintf(fp_name, "../PaxosVerifier/thread%d.out", GET_GLOBAL_T_ID(machine_id, i));
          rmw_verify_fp[i] = fopen(fp_name, "w+");
        }
      #endif

      od_spawn_threads(param_arr, i, node_purpose, &pinned_hw_threads,
                       &attr, thread_arr, worker, occupied_cores);
    }
    else  {
      #ifdef ZOOKEEPER
        if (machine_id != LEADER_MACHINE && MAKE_FOLLOWERS_PASSIVE)
          continue;
      #endif
      assert(ENABLE_CLIENTS);
      od_fopen_client_logs(i);
      od_spawn_threads(param_arr, i, "Client", &pinned_hw_threads,
                       &attr, thread_arr, client, occupied_cores);
    }
  }


  for(uint16_t i = 0; i < TOTAL_THREADS; i++)
    pthread_join(thread_arr[i], NULL);

  return 0;
}