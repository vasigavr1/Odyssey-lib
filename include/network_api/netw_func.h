//
// Created by vasilis on 18/08/20.
//

#ifndef ODYSSEY_NETW_FUNC_H
#define ODYSSEY_NETW_FUNC_H

#include <config_util.h>
#include <rdma_gen_util.h>
#include <inline_util.h>
#include "network_context.h"


void ctx_refill_recvs(context_t *ctx,
                             uint16_t qp_id);

/* ---------------------------------------------------------------------------
//------------------------------ INSERT --------------------------------
//---------------------------------------------------------------------------*/

void ctx_insert_mes(context_t *ctx, uint16_t qp_id,
                    uint32_t send_size,
                    uint32_t recv_size,
                    bool break_message,
                    void* source,
                    uint32_t source_flag,
                    uint16_t fifo_i);

/* ---------------------------------------------------------------------------
//------------------------------ BROADCASTS --------------------------------
//---------------------------------------------------------------------------*/


void ctx_send_broadcasts(context_t *ctx, uint16_t qp_id);

/* ---------------------------------------------------------------------------
//------------------------------ UNICASTS --------------------------------
//---------------------------------------------------------------------------*/


void ctx_send_unicasts(context_t *ctx,
                       uint16_t qp_id);


/* ---------------------------------------------------------------------------
//------------------------------ POLLING --------------------------------
//---------------------------------------------------------------------------*/

void ctx_poll_incoming_messages(context_t *ctx, uint16_t qp_id);


/* ---------------------------------------------------------------------------
//------------------------------ ACKS --------------------------------
//---------------------------------------------------------------------------*/

uint32_t ctx_find_when_the_ack_points_acked(ctx_ack_mes_t *ack,
                                            fifo_t *rob,
                                            uint64_t pull_lid,
                                            uint32_t *ack_num);

///
void ctx_increase_credits_on_polling_ack(context_t *ctx,
                                         uint16_t qp_id,
                                         ctx_ack_mes_t *ack);

// Returns true if the insert is successful,
// if not  (it's probably because the machine lost some messages)
// the old ack needs to be sent, before we try again to insert
bool ctx_ack_insert(context_t *ctx,
                    uint16_t qp_id,
                    uint8_t mes_num,
                    uint64_t l_id,
                    const uint8_t m_id);



void ctx_send_acks(context_t *ctx, uint16_t qp_id);


/* ---------------------------------------------------------------------------
//------------------------------ Commits --------------------------------
//---------------------------------------------------------------------------*/

void ctx_insert_commit(context_t *ctx,
                       uint16_t qp_id,
                       uint16_t com_num,
                       uint64_t last_committed_id);

/* ---------------------------------------------------------------------------
//------------------------------  --------------------------------
//---------------------------------------------------------------------------*/

void create_inputs_of_op(uint8_t **value_to_write, uint8_t **value_to_read,
                         uint32_t *real_val_len, uint8_t *opcode,
                         uint32_t *index_to_req_array,
                         mica_key_t *key, uint8_t *op_value, trace_t *trace,
                         int working_session, uint16_t t_id);



void ctx_fill_trace_op(context_t *ctx,
                       trace_t *trace_op,
                       ctx_trace_op_t *op,
                       int working_session);

static forceinline bool ctx_find_next_working_session(context_t *ctx,
                                                      int *working_session,
                                                      bool *stalled,
                                                      uint16_t last_session,
                                                      bool *all_sessions_stalled)
{
  while (!pull_request_from_this_session(stalled[*working_session],
                                         (uint16_t) *working_session, ctx->t_id)) {

    MOD_INCR(*working_session, SESSIONS_PER_THREAD);
    if (*working_session == last_session) {

// If clients are used the condition does not guarantee that sessions are stalled
      if (!ENABLE_CLIENTS) *all_sessions_stalled = true;
      return true;
    }
  }
  return false;
}





#endif //ODYSSEY_NETW_FUNC_H
