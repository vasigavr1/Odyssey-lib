//
// Created by vasilis on 10/09/20.
//

#include "netw_func.h"

inline void ctx_refill_recvs(context_t *ctx,
                             uint16_t qp_id)
{
  per_qp_meta_t *qp_meta = &ctx->qp_meta[qp_id];
  if (qp_meta->recv_wr_num > qp_meta->recv_info->posted_recvs)
    post_recvs_with_recv_info(qp_meta->recv_info,
                              qp_meta->recv_wr_num - qp_meta->recv_info->posted_recvs);
}

/* ---------------------------------------------------------------------------
//------------------------------ INSERT --------------------------------
//---------------------------------------------------------------------------*/

forceinline void ctx_insert_mes(context_t *ctx, uint16_t qp_id,
                                uint32_t send_size,
                                uint32_t recv_size,
                                bool break_message,
                                void* source,
                                uint32_t source_flag,
                                uint16_t fifo_i)
{
  per_qp_meta_t *qp_meta = &ctx->qp_meta[qp_id];
  fifo_t* send_fifo = &qp_meta->send_fifo[fifo_i];
  void *ptr = get_send_fifo_ptr(send_fifo, send_size, recv_size,
                                break_message, ctx->t_id);

  qp_meta->mfs->insert_helper(ctx, ptr, source, source_flag);

  slot_meta_t *slot_meta = get_fifo_slot_meta_push(send_fifo);


  if (slot_meta->coalesce_num == 1)
    fifo_increm_capacity(send_fifo);

  fifo_incr_net_capacity(send_fifo);
}

/* ---------------------------------------------------------------------------
//------------------------------ BROADCASTS --------------------------------
//---------------------------------------------------------------------------*/

static inline void ctx_forge_bcast_wr(context_t *ctx,
                                      uint16_t qp_id,
                                      uint16_t br_i)
{
  per_qp_meta_t *qp_meta = &ctx->qp_meta[qp_id];
  struct ibv_sge *send_sgl = qp_meta->send_sgl;

  fifo_t *send_fifo = qp_meta->send_fifo;
  send_sgl[br_i].length = get_fifo_slot_meta_pull(send_fifo)->byte_size;
  send_sgl[br_i].addr = (uintptr_t) get_fifo_pull_slot(send_fifo);

  form_bcast_links(&qp_meta->sent_tx, qp_meta->ss_batch, ctx->q_info, br_i,
                   qp_meta->send_wr, qp_meta->send_cq, qp_meta->send_string, ctx->t_id);
}


inline void ctx_send_broadcasts(context_t *ctx, uint16_t qp_id)
{
  per_qp_meta_t *qp_meta = &ctx->qp_meta[qp_id];
  per_qp_meta_t *recv_qp_meta = &ctx->qp_meta[qp_meta->recv_qp_id];
  uint16_t br_i = 0, mes_sent = 0, available_credits = 0;
  fifo_t *send_fifo = qp_meta->send_fifo;
  if (send_fifo->net_capacity == 0) return;
  else if (!check_bcast_credits(qp_meta->credits, ctx->q_info,
                                &qp_meta->time_out_cnt,
                                &available_credits, 1,
                                ctx->t_id)) return;


  while (send_fifo->net_capacity > 0 && mes_sent < available_credits) {

    qp_meta->mfs->send_helper(ctx);
    ctx_forge_bcast_wr(ctx, qp_id, br_i);
    fifo_send_from_pull_slot(send_fifo);
    br_i++;
    mes_sent++;


    if (br_i == MAX_BCAST_BATCH) {
      post_quorum_broadasts_and_recvs(recv_qp_meta->recv_info,
                                      recv_qp_meta->recv_wr_num - recv_qp_meta->recv_info->posted_recvs,
                                      ctx->q_info, br_i, qp_meta->sent_tx, qp_meta->send_wr,
                                      qp_meta->send_qp, qp_meta->enable_inlining);
      br_i = 0;
    }
  }
  if (br_i > 0) {
    post_quorum_broadasts_and_recvs(recv_qp_meta->recv_info,
                                    recv_qp_meta->recv_wr_num - recv_qp_meta->recv_info->posted_recvs,
                                    ctx->q_info, br_i, qp_meta->sent_tx, qp_meta->send_wr,
                                    qp_meta->send_qp, qp_meta->enable_inlining);
  }
  if (ENABLE_ASSERTIONS) assert(recv_qp_meta->recv_info->posted_recvs <= recv_qp_meta->recv_wr_num);
  if (mes_sent > 0) decrease_credits(qp_meta->credits, ctx->q_info, mes_sent);
}

/* ---------------------------------------------------------------------------
//------------------------------ UNICASTS --------------------------------
//---------------------------------------------------------------------------*/

static forceinline void ctx_forge_unicast_wr(context_t *ctx,
                                             uint16_t qp_id,
                                             uint16_t fifo_i,
                                             uint16_t mes_i)
{
  per_qp_meta_t *qp_meta = &ctx->qp_meta[qp_id];
  struct ibv_sge *send_sgl = qp_meta->send_sgl;
  struct ibv_send_wr *send_wr = qp_meta->send_wr;
  fifo_t *send_fifo = &qp_meta->send_fifo[fifo_i];

  if (ENABLE_ASSERTIONS)
    assert(mes_i < qp_meta->send_wr_num);
  //
  send_sgl[mes_i].length = get_fifo_slot_meta_pull(send_fifo)->byte_size;
  send_sgl[mes_i].addr = (uintptr_t) get_fifo_pull_slot(send_fifo);
  send_sgl[mes_i].lkey = qp_meta->send_mr[fifo_i]->lkey;

  //printf("%u/%p \n", send_sgl[mes_i].length, (void*)send_sgl[mes_i].addr );

  //if (qp_meta->receipient_num > 1) {
  uint8_t rm_id = get_fifo_slot_meta_pull(send_fifo)->rm_id;
  if (ENABLE_ASSERTIONS) {
    assert(rm_id < MACHINE_NUM);
    assert(rm_id != ctx->m_id);
  }
  send_wr[mes_i].wr.ud.ah = rem_qp[rm_id][ctx->t_id][qp_id].ah;
  send_wr[mes_i].wr.ud.remote_qpn = (uint32_t) rem_qp[rm_id][ctx->t_id][qp_id].qpn;
  //}


  selective_signaling_for_unicast(&qp_meta->sent_tx, qp_meta->ss_batch, send_wr,
                                  mes_i, qp_meta->send_cq, qp_meta->enable_inlining,
                                  qp_meta->send_string, ctx->t_id);
  // Have the last message point to the current message
  if (mes_i > 0) send_wr[mes_i - 1].next = &send_wr[mes_i];

}


static forceinline bool ctx_can_send_unicasts (per_qp_meta_t *qp_meta,
                                               uint16_t fifo_i)
{
  fifo_t *send_fifo = &qp_meta->send_fifo[fifo_i];
  if (qp_meta->needs_credits)
    return send_fifo->capacity > 0 && *qp_meta->credits > 0;
  else return send_fifo->capacity > 0;

}

inline void ctx_check_unicast_before_send(context_t *ctx,
                                          uint8_t rm_id,
                                          uint16_t qp_id)
{
  if (ENABLE_ASSERTIONS) {
    per_qp_meta_t *qp_meta = &ctx->qp_meta[qp_id];
    assert(qp_meta->send_qp == ctx->cb->dgram_qp[qp_id]);
    assert(qp_meta->send_wr[0].sg_list == &qp_meta->send_sgl[0]);
    if (qp_meta->receipient_num == 0)
      assert(qp_meta->send_wr[0].wr.ud.ah == rem_qp[rm_id][ctx->t_id][qp_id].ah);
    assert(qp_meta->send_wr[0].opcode == IBV_WR_SEND);
    assert(qp_meta->send_wr[0].num_sge == 1);
    if (!qp_meta->enable_inlining) {
      //assert(qp_meta->send_wr[0].sg_list->lkey == qp_meta->send_mr->lkey);
      //assert(qp_meta->send_wr[0].send_flags == IBV_SEND_SIGNALED);
      assert(!qp_meta->enable_inlining);
    }
  }
}


inline void ctx_send_unicasts(context_t *ctx,
                              uint16_t qp_id)
{
  struct ibv_send_wr *bad_send_wr;
  uint16_t mes_i = 0;
  per_qp_meta_t *qp_meta = &ctx->qp_meta[qp_id];
  for (uint16_t fifo_i = 0; fifo_i < qp_meta->send_fifo_num; ++fifo_i) {
    fifo_t *send_fifo = &qp_meta->send_fifo[fifo_i];
    while (ctx_can_send_unicasts(qp_meta, fifo_i)) {

      if (qp_meta->mfs->send_helper != NULL) {
        ctx->ctx_tmp->counter = (uint64_t) fifo_i;
        qp_meta->mfs->send_helper(ctx);
      }
      ctx_forge_unicast_wr(ctx, qp_id, fifo_i, mes_i);

      fifo_send_from_pull_slot(send_fifo);

      // Credit management
      if (qp_meta->needs_credits)
        (*qp_meta->credits)--; // TODO decrement the correct counter
      mes_i++;
    }
  }

    if (mes_i > 0) {
      ctx_refill_recvs(ctx, qp_meta->recv_qp_id);
      //if (qp_meta->recv_wr_num > qp_meta->recv_info->posted_recvs)
      //  post_recvs_with_recv_info(qp_meta->recv_info,
      //                            qp_meta->recv_wr_num - qp_meta->recv_info->posted_recvs);
      //printf("Mes_i %u length %u, address %p/ %p \n", mes_i, qp_meta->send_wr[0].sg_list->length,
      //       (void *) qp_meta->send_wr->sg_list->addr, send_fifo->fifo);
      qp_meta->send_wr[mes_i - 1].next = NULL;
      ctx_check_unicast_before_send(ctx, qp_meta->leader_m_id, qp_id);
      int ret = ibv_post_send(qp_meta->send_qp, qp_meta->send_wr, &bad_send_wr);
      CPE(ret, "Unicast ibv_post_send error", ret);
    }

}


inline void ctx_poll_incoming_messages(context_t *ctx, uint16_t qp_id)
{
  per_qp_meta_t *qp_meta = &ctx->qp_meta[qp_id];
  fifo_t *recv_fifo = qp_meta->recv_fifo;
  int completed_messages =
    find_how_many_messages_can_be_polled(qp_meta->recv_cq, qp_meta->recv_wc,
                                         &qp_meta->completed_but_not_polled,
                                         qp_meta->recv_buf_slot_num, ctx->t_id);
  if (completed_messages <= 0) {
    if (qp_meta->recv_type == RECV_REPLY) {
      if (qp_meta->outstanding_messages > 0) qp_meta->wait_for_reps_ctr++;
    }
    return;
  }
  //printf("completed %d \n", completed_messages);
  qp_meta->polled_messages = 0;

  // Start polling
  while (qp_meta->polled_messages < completed_messages) {

    if (!qp_meta->mfs->recv_handler(ctx)) break;

    fifo_incr_pull_ptr(recv_fifo);
    //printf("Qp %u pull_ptr %u/%u \n", qp_id, recv_fifo->pull_ptr, recv_fifo->max_size);
    qp_meta->polled_messages++;
  }
  qp_meta->completed_but_not_polled = completed_messages - qp_meta->polled_messages;
  //printf("polled %d , completed not polled %d \n", qp_meta->polled_messages, qp_meta->completed_but_not_polled);
  //zk_debug_info_bookkeep(ctx, qp_id, completed_messages, qp_meta->polled_messages);
  if (ENABLE_ASSERTIONS) {
    if (qp_meta->mfs->polling_debug != NULL)
      qp_meta->mfs->polling_debug(ctx, qp_id, completed_messages);
  }
  qp_meta->recv_info->posted_recvs -= qp_meta->polled_messages;


  if (qp_meta->polled_messages > 0 && qp_meta->mfs->recv_kvs != NULL)
    qp_meta->mfs->recv_kvs(ctx);

}


/* ---------------------------------------------------------------------------
//------------------------------ ACKS --------------------------------
//---------------------------------------------------------------------------*/

inline uint32_t ctx_find_when_the_ack_points_acked(ctx_ack_mes_t *ack,
                                                          fifo_t *rob,
                                                          uint64_t pull_lid,
                                                          uint32_t *ack_num)
{
  if (pull_lid >= ack->l_id) {
    (*ack_num) -= (pull_lid - ack->l_id);
    if (ENABLE_ASSERTIONS) assert(*ack_num > 0 && *ack_num <= rob->max_size);
    return rob->pull_ptr;
  }
  else { // l_id > pull_lid
    return (uint32_t) (rob->pull_ptr + (ack->l_id - pull_lid)) % rob->max_size;
  }
}

///
inline void ctx_increase_credits_on_polling_ack(context_t *ctx,
                                                       uint16_t qp_id,
                                                       ctx_ack_mes_t *ack)
{
  per_qp_meta_t *qp_meta = &ctx->qp_meta[qp_id];
  ctx->qp_meta[qp_meta->recv_qp_id].credits[ack->m_id] += ack->credits;
  if (ctx->qp_meta[qp_meta->recv_qp_id].credits[ack->m_id] > ctx->qp_meta[qp_meta->recv_qp_id].max_credits) {
    if (ENABLE_ASSERTIONS) assert(ctx->qp_meta[qp_meta->recv_qp_id].mcast_send);
    ctx->qp_meta[qp_meta->recv_qp_id].credits[ack->m_id] = ctx->qp_meta[qp_meta->recv_qp_id].max_credits;
  }
}

// Returns true if the insert is successful,
// if not  (it's probably because the machine lost some messages)
// the old ack needs to be sent, before we try again to insert
forceinline bool ctx_ack_insert(context_t *ctx,
                                uint16_t qp_id,
                                uint8_t mes_num,
                                uint64_t l_id,
                                const uint8_t m_id)
{
  per_qp_meta_t *qp_meta = &ctx->qp_meta[qp_id];
  ctx_ack_mes_t *acks = (ctx_ack_mes_t *) qp_meta->send_fifo->fifo;
  ctx_ack_mes_t *ack = &acks[m_id];
  if (ENABLE_ASSERTIONS && ack->opcode != OP_ACK) {
    if(unlikely(ack->l_id) + ack->ack_num != l_id) {
      my_printf(red, "Wrkr %u: Adding to existing ack for machine %u  with l_id %lu, "
                  "ack_num %u with new l_id %lu, coalesce_num %u, opcode %u\n", ctx->t_id, m_id,
                ack->l_id, ack->ack_num, l_id, mes_num, ack->opcode);
      //assert(false);
      return false;
    }
  }
  if (ack->opcode == OP_ACK) {// new ack
    //if (ENABLE_ASSERTIONS) assert((ack->l_id) + ack->ack_num == l_id);
    memcpy(&ack->l_id, &l_id, sizeof(uint64_t));
    ack->credits = 1;
    ack->ack_num = mes_num;
    ack->opcode = ACK_NOT_YET_SENT;
    if (DEBUG_ACKS) my_printf(yellow, "Create an ack with l_id  %lu \n", ack->l_id);
  }
  else {
    if (ENABLE_ASSERTIONS) {
      assert(ack->l_id + ((uint64_t) ack->ack_num) == l_id);
      //assert(W_CREDITS > 1);
      //if (ack->credits > W_CREDITS) {
      //  printf("attempting to put %u credits in ack (W_credits = %u)\n",
      //         ack->credits, W_CREDITS);
      //  assert(ENABLE_MULTICAST);
      //}
    }
    ack->credits++;
    ack->ack_num += mes_num;
  }
  return true;
}



forceinline void ctx_send_acks(context_t *ctx, uint16_t qp_id)
{
  per_qp_meta_t *qp_meta = &ctx->qp_meta[qp_id];
  per_qp_meta_t *recv_qp_meta = &ctx->qp_meta[qp_meta->recv_qp_id];
  //p_ops_t *p_ops = (p_ops_t *) ctx->appl_ctx;
  ctx_ack_mes_t *acks = (ctx_ack_mes_t *) qp_meta->send_fifo->fifo;
  uint8_t ack_i = 0, prev_ack_i = 0, first_wr = 0;
  struct ibv_send_wr *bad_send_wr;
  uint32_t recvs_to_post_num = 0;

  for (uint8_t m_i = 0; m_i <= qp_meta->receipient_num; m_i++) {
    if (acks[m_i].opcode == OP_ACK) continue;
    //checks_stats_prints_when_sending_acks(acks, m_i, ctx->t_id);
    acks[m_i].opcode = OP_ACK;

    selective_signaling_for_unicast(&qp_meta->sent_tx, qp_meta->ss_batch, qp_meta->send_wr,
                                    m_i, qp_meta->send_cq, true, qp_meta->send_string, ctx->t_id);
    if (ack_i > 0) {
      if (DEBUG_ACKS)
        my_printf(yellow, "Wrkr %u, ack %u points to ack %u \n", ctx->t_id, prev_ack_i, m_i);
      qp_meta->send_wr[prev_ack_i].next = &qp_meta->send_wr[m_i];
    }
    else first_wr = m_i;

    recvs_to_post_num += acks[m_i].credits;
    ack_i++;
    prev_ack_i = m_i;
  }

  if (ack_i > 0 && qp_meta->mfs->send_helper != NULL)
    qp_meta->mfs->send_helper(ctx);

  //Post receives
  if (recvs_to_post_num > 0) {
    post_recvs_with_recv_info(recv_qp_meta->recv_info, recvs_to_post_num);
    //checks_when_posting_write_receives(qp_meta->recv_info, recvs_to_post_num, ack_i);
  }
  // SEND the acks
  if (ack_i > 0) {
    if (DEBUG_ACKS) printf("Wrkr %u send %u acks, last recipient %u, first recipient %u \n",
                           ctx->t_id, ack_i, prev_ack_i, first_wr);
    qp_meta->send_wr[prev_ack_i].next = NULL;
    int ret = ibv_post_send(qp_meta->send_qp, &qp_meta->send_wr[first_wr], &bad_send_wr);
    if (ENABLE_ASSERTIONS) CPE(ret, "ACK ibv_post_send error", ret);
  }
}


/* ---------------------------------------------------------------------------
//------------------------------ Commits --------------------------------
//---------------------------------------------------------------------------*/

forceinline void ctx_insert_commit(context_t *ctx,
                                   uint16_t qp_id,
                                   uint16_t com_num,
                                   uint64_t last_committed_id)
{
  fifo_t *send_fifo = ctx->qp_meta[qp_id].send_fifo;
  ctx_com_mes_t *commit = (ctx_com_mes_t *) get_fifo_push_prev_slot(send_fifo);

  if (send_fifo->capacity > 0)
    commit->com_num += com_num;
  else { //otherwise push a new commit
    commit->l_id = last_committed_id;
    commit->com_num = com_num;
    fifo_increm_capacity(send_fifo);
  }
  send_fifo->net_capacity += com_num;
  slot_meta_t *slot_meta = get_fifo_slot_meta_push(send_fifo);
  slot_meta->coalesce_num += com_num;
}



/* ---------------------------------------------------------------------------
//------------------------------  --------------------------------
//---------------------------------------------------------------------------*/

forceinline void create_inputs_of_op(uint8_t **value_to_write, uint8_t **value_to_read,
                                     uint32_t *real_val_len, uint8_t *opcode,
                                     uint32_t *index_to_req_array,
                                     mica_key_t *key, uint8_t *op_value, trace_t *trace,
                                     int working_session, uint16_t t_id)
{
  client_op_t *if_cl_op = NULL;
  if (ENABLE_CLIENTS) {
    uint32_t pull_ptr = interface[t_id].wrkr_pull_ptr[working_session];
    if_cl_op = &interface[t_id].req_array[working_session][pull_ptr];
    (*index_to_req_array) = pull_ptr;
    check_client_req_state_when_filling_op(working_session, pull_ptr,
                                           *index_to_req_array, t_id);

    (*opcode) = if_cl_op->opcode;
    (*key) = if_cl_op->key;
    (*real_val_len) = if_cl_op->val_len;
    (*value_to_write) = if_cl_op->value_to_write;
    (*value_to_read) = if_cl_op->value_to_read;
  }
  else {
    (*opcode) = trace->opcode;
    *(uint64_t *) (key) = *(uint64_t *) trace->key_hash;
    (*real_val_len) = (uint32_t) VALUE_SIZE;
    (*value_to_write) = op_value;
    (*value_to_read) = op_value;
    if (*opcode == FETCH_AND_ADD) *(uint64_t *) op_value = 1;
  }
}


static inline void ctx_check_op(ctx_trace_op_t *op)
{
  if (ENABLE_ASSERTIONS) {
    check_state_with_allowed_flags(3, op->opcode, KVS_OP_PUT, KVS_OP_GET);
    assert(op->real_val_len > 0);
    assert(op->index_to_req_array < PER_SESSION_REQ_NUM);
    assert(op->session_id < SESSIONS_PER_THREAD);
    assert(op->key.bkt > 0);
  }
}


forceinline void  ctx_fill_trace_op(context_t *ctx,
                                    trace_t *trace_op,
                                    ctx_trace_op_t *op,
                                    int working_session)
{
  create_inputs_of_op(&op->value_to_write, &op->value_to_read, &op->real_val_len,
                      &op->opcode, &op->index_to_req_array,
                      &op->key, ctx->ctx_tmp->tmp_val, trace_op, working_session, ctx->t_id);

  ctx_check_op(op);

  if (ENABLE_ASSERTIONS) assert(op->opcode != NOP);
  bool is_update = op->opcode == KVS_OP_PUT;
  if (WRITE_RATIO >= 1000) assert(is_update);
  op->val_len = is_update ? (uint8_t) (VALUE_SIZE >> SHIFT_BITS) : (uint8_t) 0;

  op->session_id = (uint16_t) working_session;

  if (ENABLE_CLIENTS) {
    signal_in_progress_to_client(op->session_id, op->index_to_req_array, ctx->t_id);
    if (ENABLE_ASSERTIONS) assert(interface[ctx->t_id].wrkr_pull_ptr[working_session] == op->index_to_req_array);
    MOD_INCR(interface[ctx->t_id].wrkr_pull_ptr[working_session], PER_SESSION_REQ_NUM);
  }

  if (ENABLE_ASSERTIONS == 1) {
    assert(WRITE_RATIO > 0 || is_update == 0);
    if (is_update) assert(op->val_len > 0);
  }
}

