//
// Created by vasilis on 18/08/20.
//

#ifndef ODYSSEY_NETW_FUNC_H
#define ODYSSEY_NETW_FUNC_H

#include <config_util.h>
#include <rdma_gen_util.h>
#include "network_context.h"





// Form the Broadcast work request for the prepare
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

// Leader Broadcasts its Prepares
static inline void ctx_send_broadcasts(context_t *ctx, uint16_t qp_id)
{
  per_qp_meta_t *qp_meta = &ctx->qp_meta[qp_id];
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
      post_quorum_broadasts_and_recvs(qp_meta->recv_info, qp_meta->recv_wr_num - qp_meta->recv_info->posted_recvs,
                                      ctx->q_info, br_i, qp_meta->sent_tx, qp_meta->send_wr,
                                      qp_meta->send_qp, qp_meta->enable_inlining);
      br_i = 0;
    }
  }
  if (br_i > 0) {
    post_quorum_broadasts_and_recvs(qp_meta->recv_info, qp_meta->recv_wr_num - qp_meta->recv_info->posted_recvs,
                                    ctx->q_info, br_i, qp_meta->sent_tx, qp_meta->send_wr,
                                    qp_meta->send_qp, qp_meta->enable_inlining);
  }
  if (ENABLE_ASSERTIONS) assert(qp_meta->recv_info->posted_recvs <= qp_meta->recv_wr_num);
  if (mes_sent > 0) decrease_credits(qp_meta->credits, ctx->q_info, mes_sent);
}

/* ---------------------------------------------------------------------------
//------------------------------ UNICASTS --------------------------------
//---------------------------------------------------------------------------*/

static inline void ctx_forge_unicast_wr(context_t *ctx,
                                        uint16_t qp_id,
                                        uint16_t mes_i)
{
  per_qp_meta_t *qp_meta = &ctx->qp_meta[qp_id];
  struct ibv_sge *send_sgl = qp_meta->send_sgl;
  struct ibv_send_wr *send_wr = qp_meta->send_wr;

  //printf("%u/%u \n", mes_i, MAX_R_REP_WRS);
  send_sgl[mes_i].length = get_fifo_slot_meta_pull(qp_meta->send_fifo)->byte_size;
  send_sgl[mes_i].addr = (uintptr_t) get_fifo_pull_slot(qp_meta->send_fifo);

  if (qp_meta->receipient_num > 1) {
    uint8_t rm_id = get_fifo_slot_meta_pull(qp_meta->send_fifo)->rm_id;
    send_wr[mes_i].wr.ud.ah = rem_qp[rm_id][ctx->t_id][qp_id].ah;
    send_wr[mes_i].wr.ud.remote_qpn = (uint32_t) rem_qp[rm_id][ctx->t_id][qp_id].qpn;
  }


  selective_signaling_for_unicast(&qp_meta->sent_tx, qp_meta->ss_batch, send_wr,
                                  mes_i, qp_meta->send_cq, qp_meta->enable_inlining,
                                  qp_meta->send_string, ctx->t_id);
  // Have the last message point to the current message
  if (mes_i > 0) send_wr[mes_i - 1].next = &send_wr[mes_i];

}





static inline bool ctx_can_send_unicasts (per_qp_meta_t *qp_meta)
{
  fifo_t *send_fifo = qp_meta->send_fifo;
  if (qp_meta->needs_credits)
    return send_fifo->capacity > 0 && *qp_meta->credits > 0;
  else return send_fifo->capacity > 0;

}

static inline void ctx_check_unicast_before_send(context_t *ctx,
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
      assert(qp_meta->send_wr[0].sg_list->lkey == qp_meta->send_mr->lkey);
      //assert(qp_meta->send_wr[0].send_flags == IBV_SEND_SIGNALED);
      assert(!qp_meta->enable_inlining);
    }
  }
}



// Send the local writes to the ldr
static inline void ctx_send_unicasts(context_t *ctx,
                                    uint16_t qp_id)
{
  struct ibv_send_wr *bad_send_wr;
  uint16_t mes_i = 0;
  per_qp_meta_t *qp_meta = &ctx->qp_meta[qp_id];
  fifo_t *send_fifo = qp_meta->send_fifo;


  while (ctx_can_send_unicasts(qp_meta)) {


    if (qp_meta->mfs->send_helper != NULL)
      qp_meta->mfs->send_helper(ctx);

    ctx_forge_unicast_wr(ctx, qp_id, mes_i);

    fifo_send_from_pull_slot(send_fifo);

    // Credit management
    if (qp_meta->needs_credits)
      (*qp_meta->credits)--;
    mes_i++;
  }

  if (mes_i > 0) {
    if (qp_meta->recv_wr_num > qp_meta->recv_info->posted_recvs)
      post_recvs_with_recv_info(qp_meta->recv_info, qp_meta->recv_wr_num - qp_meta->recv_info->posted_recvs);
    //printf("W_i %u, length %u, address %p/ %p \n", mes_i, qp_meta->send_wr[0].sg_list->length,
    //       (void *)qp_meta->send_wr[0].sg_list->addr, send_fifo->fifo);
    qp_meta->send_wr[mes_i - 1].next = NULL;
    ctx_check_unicast_before_send(ctx, qp_meta->leader_m_id, qp_id);
    int ret = ibv_post_send(qp_meta->send_qp, &qp_meta->send_wr[0], &bad_send_wr);
    CPE(ret, "Unicast ibv_post_send error", ret);
  }
}


static inline void ctx_poll_incoming_messages(context_t *ctx,
                                          uint16_t qp_id)
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



#endif //ODYSSEY_NETW_FUNC_H
