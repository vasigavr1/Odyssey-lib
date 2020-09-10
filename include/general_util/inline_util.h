//
// Created by vasilis on 30/06/20.
//

#ifndef INLINE_UTIL_H
#define INLINE_UTIL_H

#include "client_if_util.h"
#include "debug_util.h"
#include "latency_util.h"
#include "rdma_gen_util.h"
#include "generic_inline_util.h"
#include "config_util.h"
#include "netw_func.h"
#include "network_context.h"
#include "fifo.h"
#include "multicast.h"


static inline bool all_sessions_are_stalled(context_t *ctx,
                                            bool all_sessions_stalled,
                                            uint32_t *stalled_sessions_dbg_counter)
{
  // if there are clients the "all_sessions_stalled" flag is not used,
  // so we need not bother checking it
  if (!ENABLE_CLIENTS && all_sessions_stalled) {
    stalled_sessions_dbg_counter++;
    if (ENABLE_ASSERTIONS) {
      if (*stalled_sessions_dbg_counter == MILLION) {
        //my_printf(red, "Wrkr %u, all sessions are stalled \n", ctx->t_id);
        *stalled_sessions_dbg_counter = 0;
      }
    }
    return true;
  } else if (ENABLE_ASSERTIONS) *stalled_sessions_dbg_counter = 0;
  return false;
}


static inline bool find_starting_session(context_t *ctx,
                                         uint16_t last_session,
                                         bool* stalled,
                                         int* working_session)
{
  for (uint16_t i = 0; i < SESSIONS_PER_THREAD; i++) {
    uint16_t sess_i = (uint16_t)((last_session + i) % SESSIONS_PER_THREAD);
    if (pull_request_from_this_session(stalled[sess_i], sess_i, ctx->t_id)) {
      *working_session = sess_i;
      break;
    }
  }
  if (ENABLE_CLIENTS) {
    if (*working_session == -1) return false;
  }
  else if (ENABLE_ASSERTIONS) assert(*working_session != -1);
  return true;
}


//static inline void create_inputs_of_op(uint8_t **value_to_write, uint8_t **value_to_read,
//                                       uint32_t *real_val_len, uint8_t *opcode,
//                                       uint32_t *index_to_req_array,
//                                       mica_key_t *key, uint8_t *op_value, trace_t *trace,
//                                       int working_session, uint16_t t_id)
//{
//  client_op_t *if_cl_op = NULL;
//  if (ENABLE_CLIENTS) {
//    uint32_t pull_ptr = interface[t_id].wrkr_pull_ptr[working_session];
//    if_cl_op = &interface[t_id].req_array[working_session][pull_ptr];
//    (*index_to_req_array) = pull_ptr;
//    check_client_req_state_when_filling_op(working_session, pull_ptr,
//                                           *index_to_req_array, t_id);
//
//    (*opcode) = if_cl_op->opcode;
//    (*key) = if_cl_op->key;
//    (*real_val_len) = if_cl_op->val_len;
//    (*value_to_write) = if_cl_op->value_to_write;
//    (*value_to_read) = if_cl_op->value_to_read;
//  }
//  else {
//    (*opcode) = trace->opcode;
//    *(uint64_t *) (key) = *(uint64_t *) trace->key_hash;
//    (*real_val_len) = (uint32_t) VALUE_SIZE;
//    (*value_to_write) = op_value;
//    (*value_to_read) = op_value;
//    if (*opcode == FETCH_AND_ADD) *(uint64_t *) op_value = 1;
//  }
//}


#endif //INLINE_UTIL_H
