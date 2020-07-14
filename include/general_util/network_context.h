//
// Created by vasilis on 14/07/20.
//

#ifndef ODYSSEY_NETWORK_CONTEXT_H
#define ODYSSEY_NETWORK_CONTEXT_H

#include <multicast.h>
#include <hrd.h>
#include "top.h"



typedef enum{CREDIT, UNICAST, UNICAST_TO_LDR, BROADCAST} send_type_t;


typedef struct per_qp_meta {
  struct ibv_send_wr *send_wr;
  struct ibv_sge *send_sgl;
  struct ibv_sge *recv_sgl;
  struct ibv_wc *recv_wc;
  struct ibv_recv_wr *recv_wr;
  struct ibv_mr *mr;

  uint32_t send_wr_num;
  uint32_t recv_wr_num;

  uint32_t send_q_depth;
  uint32_t recv_q_depth;
  send_type_t send_type;

  uint32_t receipient_num;
  uint32_t remote_senders_num;
  uint8_t leader_m_id; // if there exist

  uint32_t ss_batch;
  uint16_t credits;

  fifo_t *recv_fifo;
  bool has_recv_fifo;
  fifo_t *send_fifo;
  bool has_send_fifo;
  uint32_t recv_buf_size;
  uint32_t recv_buf_slot_num;

  uint32_t pull_ptr;
  uint32_t push_ptr;

  struct ibv_qp *send_qp;
  struct ibv_qp *recv_qp; //send and recv qps are the same except if we are using multicast
  struct ibv_cq *recv_cq;
  struct ibv_cq *send_cq;
  recv_info_t *recv_info;
  uint32_t recv_size;
  uint32_t send_size;
  bool hw_multicast;
  uint16_t mcast_qp_id;
  bool enable_inlining;

  uint64_t sent_tx; //how many messages have been sent


} per_qp_meta_t;

typedef struct context {
  hrd_ctrl_blk_t *cb;
  mcast_cb_t *mcast_cb;
  per_qp_meta_t *qp_meta;
  uint16_t qp_num;
  uint8_t m_id;
  uint16_t t_id;
} context_t;

static void create_per_qp_meta(per_qp_meta_t* qp_meta,
                        uint32_t send_wr_num,
                        uint32_t recv_wr_num,
                        send_type_t send_type,
                        uint32_t receipient_num,
                        uint32_t remote_senders_num,
                        uint32_t recv_fifo_slot_num,
                        uint32_t recv_size,
                        uint32_t send_size,
                        bool hw_multicast,
                        uint16_t mcast_qp_id,
                        uint8_t leader_m_id,
                        uint32_t send_fifo_slot_num)
{
  qp_meta->send_wr = malloc(send_wr_num * sizeof(struct ibv_send_wr));

  if (send_type == BROADCAST)
    qp_meta->send_sgl = malloc(MAX_BCAST_BATCH * sizeof(struct ibv_sge));
  else if (send_type == CREDIT)
    qp_meta->send_sgl = malloc(sizeof(struct ibv_sge));
  else qp_meta->send_sgl = malloc(send_wr_num * sizeof(struct ibv_sge));

  if (recv_wr_num > 0) {
    qp_meta->recv_wr = malloc(recv_wr_num * sizeof(struct ibv_recv_wr));
    qp_meta->recv_sgl = malloc(recv_wr_num * sizeof(struct ibv_sge));
    qp_meta->recv_wc = malloc(recv_wr_num * sizeof(struct ibv_wc));
  }

  qp_meta->send_wr_num = send_wr_num;
  qp_meta->recv_wr_num = recv_wr_num;
  qp_meta->send_type = send_type;
  qp_meta->receipient_num = receipient_num;
  qp_meta->remote_senders_num = remote_senders_num;
  qp_meta->pull_ptr = 0;
  qp_meta->push_ptr = 0;
  qp_meta->leader_m_id = leader_m_id;

  qp_meta->recv_q_depth = recv_wr_num + 3;
  if (send_type == BROADCAST) {
    qp_meta->ss_batch = (uint32_t) MAX((MIN_SS_BATCH / (receipient_num)), (MAX_BCAST_BATCH + 2));
    qp_meta->send_q_depth = ((qp_meta->ss_batch * receipient_num) + 10);

  }
  else {
    qp_meta->ss_batch = (uint32_t) MAX(MIN_SS_BATCH, (send_wr_num + 2));
    qp_meta->send_q_depth = qp_meta->ss_batch + 3;
  }


  qp_meta->recv_buf_slot_num = recv_fifo_slot_num;
  qp_meta->recv_buf_size = recv_fifo_slot_num * recv_size;
  qp_meta->recv_size = recv_size;
  qp_meta->send_size = send_size;
  qp_meta->hw_multicast = hw_multicast;
  qp_meta->enable_inlining = send_size <= MAXIMUM_INLINE_SIZE;

  //if (recv_fifo_slot_num > 0 ) {
  qp_meta->has_recv_fifo = true;
  qp_meta->recv_fifo = calloc(1, sizeof(fifo_t));
  qp_meta->recv_fifo->fifo = NULL; // will be filled after initializing the hrd_cb
  qp_meta->recv_fifo->max_size = recv_fifo_slot_num;
  qp_meta->recv_fifo->max_byte_size = recv_fifo_slot_num * recv_size;
  //}
  //else qp_meta->has_recv_fifo = false;
  //qp_meta->recv_fifo->max_size_in_bytes = recv_fifo_slot_num * recv_size;

  if (send_fifo_slot_num > 0) {
    qp_meta->has_send_fifo = true;
    qp_meta->send_fifo = calloc(1, sizeof(fifo_t));
    qp_meta->send_fifo->fifo = calloc(send_fifo_slot_num, send_size);
    qp_meta->send_fifo->max_size = send_fifo_slot_num;
    qp_meta->send_fifo->max_byte_size = send_fifo_slot_num * send_size;
    qp_meta->send_fifo->backwards_ptrs =
      malloc(qp_meta->send_fifo->max_size * sizeof(uint32_t));
  }
  else qp_meta->has_send_fifo = false;
}


// Set up the receive info
static recv_info_t* cust_init_recv_info(uint32_t lkey, per_qp_meta_t *qp_meta,
                                        struct ibv_qp *recv_qp)
{
  recv_info_t* recv = (recv_info_t*) malloc(sizeof(recv_info_t));
  recv->push_ptr = qp_meta->push_ptr;
  recv->buf_slots = qp_meta->recv_buf_slot_num;
  recv->slot_size = qp_meta->recv_size;
  recv->posted_recvs = 0;
  recv->recv_qp = recv_qp;
  recv->buf = qp_meta->recv_fifo->fifo;
  assert(recv->buf != NULL);
  recv->recv_wr = qp_meta->recv_wr;
  recv->recv_sgl = qp_meta->recv_sgl;

  for (int i = 0; i < qp_meta->recv_wr_num; i++) {
    // It can be that incoming messages have no payload,
    // and thus sgls (which store buffer pointers) are not useful
    if (recv->buf_slots == 0) {
      recv->recv_wr[i].sg_list = recv->recv_sgl;
      recv->recv_sgl->addr = (uintptr_t) recv->buf;
      recv->recv_sgl->length = qp_meta->recv_size;
      recv->recv_sgl->lkey = lkey;
    }
    else {
      recv->recv_wr[i].sg_list = &(recv->recv_sgl[i]);
      recv->recv_sgl[i].length = qp_meta->recv_size;
      recv->recv_sgl[i].lkey = lkey;
    }
    recv->recv_wr[i].num_sge = 1;
  }
  return recv;
}

static void init_ctx_recv_infos(context_t *ctx)
{
  for (int qp_i = 0; qp_i < ctx->qp_num; ++qp_i) {
    per_qp_meta_t *qp_meta = &ctx->qp_meta[qp_i];
    if (qp_meta->recv_wr_num == 0) continue;

    uint32_t lkey = ctx->qp_meta[qp_i].hw_multicast ?
                    ctx->mcast_cb->recv_mr->lkey :
                    ctx->cb->dgram_buf_mr->lkey;


    qp_meta->recv_info =
      cust_init_recv_info(lkey, qp_meta, ctx->cb->dgram_qp[qp_i]);
  }
}

static void init_ctx_mrs(context_t *ctx)
{
  for (int qp_i = 0; qp_i < ctx->qp_num; ++qp_i) {
    per_qp_meta_t *qp_meta = &ctx->qp_meta[qp_i];
    if (!qp_meta->has_send_fifo) continue;
    qp_meta->mr = register_buffer(ctx->cb->pd,
                                  qp_meta->send_fifo,
                                  qp_meta->send_fifo->max_byte_size);

  }
}

static void set_up_ctx_qps(context_t *ctx)
{
  for (int qp_i = 0; qp_i < ctx->qp_num; ++qp_i) {
    per_qp_meta_t *qp_meta = &ctx->qp_meta[qp_i];
    qp_meta->send_qp = ctx->cb->dgram_qp[qp_i];
    qp_meta->send_cq = ctx->cb->dgram_send_cq[qp_i];
    if (qp_meta->hw_multicast) {
      qp_meta->recv_qp = ctx->mcast_cb->recv_qp[qp_meta->mcast_qp_id];
      qp_meta->recv_cq = ctx->mcast_cb->recv_cq[qp_meta->mcast_qp_id];
    }
    else {
      qp_meta->recv_qp = ctx->cb->dgram_qp[qp_i];
      qp_meta->recv_cq = ctx->cb->dgram_recv_cq[qp_i];
    }

    //ctx
  }
}

static void const_set_up_wr(context_t *ctx, uint16_t qp_i,
                            uint16_t wr_i, uint16_t sgl_i,
                            bool last, uint16_t rm_id)
{
  per_qp_meta_t *qp_meta = &ctx->qp_meta[qp_i];
  struct ibv_send_wr* send_wr = &qp_meta->send_wr[wr_i];
  struct ibv_sge *send_sgl = &qp_meta->send_sgl[sgl_i];



  if (qp_meta->hw_multicast) {
    mcast_cb_t *mcast_cb = ctx->mcast_cb;
    send_wr->wr.ud.ah = mcast_cb->send_ah[qp_meta->mcast_qp_id];
    send_wr->wr.ud.remote_qpn = mcast_cb->qpn[qp_meta->mcast_qp_id];
    send_wr->wr.ud.remote_qkey = mcast_cb->qkey[qp_meta->mcast_qp_id];
  }
  else {
    send_wr->wr.ud.ah = rem_qp[rm_id][ctx->t_id][qp_i].ah;
    send_wr->wr.ud.remote_qpn = (uint32_t) rem_qp[rm_id][ctx->t_id][qp_i].qpn;
    send_wr->wr.ud.remote_qkey = HRD_DEFAULT_QKEY;
  }

  if (qp_meta->send_type == CREDIT) {
    send_wr->opcode = IBV_WR_SEND_WITH_IMM;
    send_wr->num_sge = 0;
    send_wr->imm_data = ctx->m_id;
  }
  else {
    send_wr->opcode = IBV_WR_SEND;
    send_wr->num_sge = 1;
  }
  send_wr->sg_list = send_sgl;
  send_wr->sg_list->length = qp_meta->send_size;
  if (qp_meta->send_type == CREDIT) assert(send_wr->sg_list->length == 0);
  if (qp_meta->enable_inlining) send_wr->send_flags = IBV_SEND_INLINE;
  else {
    send_sgl->lkey = qp_meta->mr->lkey;
    send_wr->send_flags = 0;
  }

  send_wr->next = last ? NULL : &send_wr[1];
}

static void init_ctx_send_wrs(context_t *ctx)
{
  for (uint16_t qp_i = 0; qp_i < ctx->qp_num; ++qp_i) {
    per_qp_meta_t *qp_meta = &ctx->qp_meta[qp_i];
    if (qp_meta->send_type == UNICAST_TO_LDR || qp_meta->send_type == UNICAST) {
      for (uint16_t wr_i = 0; wr_i < qp_meta->send_wr_num; ++wr_i) {
        const_set_up_wr(ctx, qp_i, wr_i, wr_i, wr_i == qp_meta->send_wr_num - 1,
                        qp_meta->leader_m_id);
      }
    }
    else if (qp_meta->send_type == CREDIT)
      for (uint16_t wr_i = 0; wr_i < qp_meta->send_wr_num; ++wr_i) {
        const_set_up_wr(ctx, qp_i, wr_i, 0, true, qp_meta->leader_m_id);
      }
    else if (qp_meta->send_type == BROADCAST) {
      for (uint16_t br_i = 0; br_i < MAX_BCAST_BATCH; br_i++) {
        for (uint16_t i = 0; i < MESSAGES_IN_BCAST; i++) {
          uint16_t rm_id = (uint16_t) (i < ctx->m_id ? i : i + 1);
          uint16_t wr_i = (uint16_t) ((br_i * MESSAGES_IN_BCAST) + i);
          bool last = (i == MESSAGES_IN_BCAST - 1);
          const_set_up_wr(ctx, qp_i, wr_i, br_i, last, rm_id);
        }

      }
    }
    else assert(false);
  }
}


static void set_per_qp_meta_recv_fifos(context_t *ctx)
{

  for (int i = 0; i < ctx->qp_num; ++i) {
    per_qp_meta_t *qp_meta = &ctx->qp_meta[i];

    if (i == 0) {
      qp_meta->recv_fifo->fifo = (void *) ctx->cb->dgram_buf;
      assert(qp_meta->recv_fifo->fifo != NULL);
    }
    else {
      per_qp_meta_t *prev_qp_meta = &ctx->qp_meta[i - 1];
      assert(prev_qp_meta->recv_fifo->fifo != NULL);
      qp_meta->recv_fifo->fifo = prev_qp_meta->recv_fifo->fifo +
        prev_qp_meta->recv_buf_slot_num;
    }
  }
}


static int *get_recv_q_depths(per_qp_meta_t* qp_meta, uint16_t qp_num)
{
  int *recv_q_depth = (int *) malloc(qp_num * sizeof(int));
  for (int i = 0; i < qp_num; ++i) {
    recv_q_depth[i] = qp_meta[i].recv_q_depth;
    if (recv_q_depth[i] == 0) recv_q_depth[i] = 1;
  }
  return recv_q_depth;
}

static int *get_send_q_depths(per_qp_meta_t* qp_meta, uint16_t qp_num)
{
  int *send_q_depth = (int *) malloc(qp_num * sizeof(int));
  for (int i = 0; i < qp_num; ++i) {
    send_q_depth[i] = qp_meta[i].send_q_depth;
  }
  return send_q_depth;
}

#endif //ODYSSEY_NETWORK_CONTEXT_H
