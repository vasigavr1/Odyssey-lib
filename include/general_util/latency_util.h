//
// Created by vasilis on 22/05/20.
//

#ifndef KITE_LATENCY_UTIL_H
#define KITE_LATENCY_UTIL_H

#include "top.h"
#include "generic_inline_util.h"

/* ---------------------------------------------------------------------------
//------------------------------ LATENCY MEASUREMENTS-------------------------
//---------------------------------------------------------------------------*/

static inline char *latency_req_to_str(req_type rt)
{
  switch (rt){
    case NO_REQ:
      return "NO_REQ";
    case RELEASE:
      return "RELEASE";
    case ACQUIRE:
      return "ACQUIRE";
    case WRITE_REQ:
      return "WRITE_REQ";
    case READ_REQ:
      return "READ_REQ";
    case RMW_REQ:
      return "RMW_REQ";
    case WRITE_REQ_BEFORE_CACHE:
      return "WRITE_REQ_BEFORE_CACHE";
    default: assert(false);
  }
}


//Add latency to histogram (in microseconds)
static inline void bookkeep_latency(uint64_t useconds, req_type rt){
  uint32_t** latency_counter;
  switch (rt){
    case ACQUIRE:
      latency_counter = &latency_count.acquires;
      if (useconds > latency_count.max_acq_lat) {
        latency_count.max_acq_lat = (uint32_t) useconds;
        //my_printf(green, "Found max acq latency: %u/%d \n",
        //             latency_count.max_acq_lat, useconds);
      }
      break;
    case RELEASE:
      latency_counter = &latency_count.releases;
      if (useconds > latency_count.max_rel_lat) {
        latency_count.max_rel_lat = (uint32_t) useconds;
        //my_printf(yellow, "Found max rel latency: %u/%d \n", latency_count.max_rel_lat, useconds);
      }
      break;
    case READ_REQ:
      latency_counter = &latency_count.reads;
      if (useconds > latency_count.max_read_lat) latency_count.max_read_lat = (uint32_t) useconds;
      break;
    case WRITE_REQ:
      latency_counter = &latency_count.writes;
      if (useconds > latency_count.max_write_lat) latency_count.max_write_lat = (uint32_t) useconds;
      break;
    case RMW_REQ:
      latency_counter = &latency_count.rmws;
      break;
    default: assert(0);
  }
  latency_count.total_measurements++;


  if (useconds > MAX_LATENCY)
    (*latency_counter)[LATENCY_BUCKETS]++;
  else
    (*latency_counter)[useconds / (MAX_LATENCY / LATENCY_BUCKETS)]++;
}

//
static inline void report_latency(latency_info_t* latency_info)
{
  struct timespec end;
  clock_gettime(CLOCK_MONOTONIC, &end);
  uint64_t useconds = (uint64_t)((end.tv_sec - latency_info->start.tv_sec) * MILLION) +
                 ((end.tv_nsec - latency_info->start.tv_nsec) / 1000);  //(end.tv_nsec - start->tv_nsec) / 1000;
  if (ENABLE_ASSERTIONS) assert(useconds > 0);

  if (DEBUG_LATENCY) {
    printf("Latency of a req of type %s is %lu us, sess %u , measured reqs: %lu \n",
           latency_req_to_str(latency_info->measured_req_flag),
           useconds, latency_info->measured_sess_id,
           (latency_count.total_measurements + 1));
    //if (useconds > 1000)


  }
  bookkeep_latency(useconds, latency_info->measured_req_flag);
  latency_info->measured_req_flag = NO_REQ;
}

static inline req_type map_opcodes_to_req_type(uint8_t opcode)
{
  switch(opcode){
    case OP_RELEASE:
      return RELEASE;
    case OP_ACQUIRE:
      return ACQUIRE;
    case KVS_OP_GET:
      return READ_REQ;
    case KVS_OP_PUT:
      return WRITE_REQ;
    case FETCH_AND_ADD:
    case COMPARE_AND_SWAP_WEAK:
    case COMPARE_AND_SWAP_STRONG:
      return RMW_REQ;
    default: if (ENABLE_ASSERTIONS) assert(false);
  }
}

// Necessary bookkeeping to initiate the latency measurement
static inline void start_measurement(latency_info_t* latency_info, uint32_t sess_id,
                                     uint8_t opcode, uint16_t t_id)
{
  if (ENABLE_ASSERTIONS)assert(latency_info->measured_req_flag == NO_REQ);


  latency_info->measured_req_flag = map_opcodes_to_req_type(opcode);
  latency_info->measured_sess_id = sess_id;
  if (DEBUG_LATENCY)
    my_printf(green, "Measuring a req , opcode %s, sess_id %u,  flag %s op_i %d \n",
  					 opcode_to_str(opcode), sess_id, latency_req_to_str(latency_info->measured_req_flag),
            latency_info->measured_sess_id);
  if (ENABLE_ASSERTIONS) assert(latency_info->measured_req_flag != NO_REQ);

  clock_gettime(CLOCK_MONOTONIC, &latency_info->start);


}

//// A condition to be used to trigger periodic (but rare) measurements
//static inline bool trigger_measurement(uint16_t local_client_id)
//{
//  return t_stats[local_client_id].cache_hits_per_thread % K_32 > 0 &&
//         t_stats[local_client_id].cache_hits_per_thread % K_32 <= 500 &&
//         local_client_id == 0 && machine_id == MACHINE_NUM -1;
//}


#endif //KITE_LATENCY_UTIL_H
