//
// Created by vasilis on 14/07/20.
//

#ifndef ODYSSEY_NETWORK_CONTEXT_H
#define ODYSSEY_NETWORK_CONTEXT_H

#include <multicast.h>
#include <hrd.h>
#include <rdma_gen_util.h>
#include "top.h"
#include "fifo.h"
#include "generic_inline_util.h"
#include "ctx_templates.h"
typedef struct context context_t;

typedef void (*insert_helper_t) (context_t *, void*, void *, uint32_t);
typedef bool (*recv_handler_t)(context_t *);
typedef void (*send_helper_t)(context_t *);
typedef void (*recv_kvs_t)(context_t *);
typedef void (*polling_debug_t)(context_t *, uint16_t, int);


typedef enum{
  SEND_ACK_RECV_ACK,

  SEND_CREDITS_LDR_RECV_NONE,
  RECV_CREDITS,

  SEND_UNI_REQ_RECV_REP,
  SEND_UNI_REP_RECV_UNI_REQ,

  SEND_UNI_REQ_RECV_LDR_REP,
  SEND_UNI_REP_LDR_RECV_UNI_REQ,

  SEND_BCAST_LDR_RECV_UNI,
  SEND_UNI_REP_RECV_LDR_BCAST,

  SEND_UNI_REP_TO_BCAST,
  SEND_BCAST_RECV_UNI,

  SEND_BCAST_RECV_BCAST,
  SEND_UNI_REP_RECV_UNI_REP

} flow_type_t;

typedef enum {
  RECV_NOTHING,
  RECV_ACK,
  RECV_REPLY,
  RECV_REQ,
  RECV_SEC_ROUND

} recv_type_t;

typedef struct qp_meta_mfs {
  recv_handler_t recv_handler;
  send_helper_t send_helper;
  recv_kvs_t recv_kvs;
  insert_helper_t insert_helper;
  polling_debug_t polling_debug;
} mf_t;


typedef struct per_qp_meta {
  struct ibv_send_wr *send_wr;
  struct ibv_sge *send_sgl;
  struct ibv_sge *recv_sgl;
  struct ibv_wc *recv_wc;
  struct ibv_recv_wr *recv_wr;
  struct ibv_mr *send_mr;

  uint32_t send_wr_num;
  uint32_t recv_wr_num;

  uint32_t send_q_depth;
  uint32_t recv_q_depth;
  flow_type_t flow_type;
  recv_type_t recv_type;
  uint16_t recv_qp_id;

  uint32_t receipient_num;
  uint8_t leader_m_id; // if there exist

  uint32_t ss_batch;

  // flow control
  uint16_t max_credits;
  uint16_t *credits; //[MACHINE_NUM]
  bool needs_credits;
  fifo_t *mirror_remote_recv_fifo;

  // send-recv fifos
  fifo_t *recv_fifo;
  fifo_t *send_fifo;
  bool has_send_fifo;
  uint32_t recv_buf_size;
  uint32_t recv_buf_slot_num;

  struct ibv_qp *send_qp;
  struct ibv_qp *recv_qp; //send and recv qps are the same except if we are using multicast
  struct ibv_cq *recv_cq;
  struct ibv_cq *send_cq;
  recv_info_t *recv_info;
  uint32_t recv_size;
  uint32_t send_size;
  bool mcast_send;
  bool mcast_recv;
  uint16_t mcast_qp_id;
  bool enable_inlining;

  uint64_t sent_tx; //how many messages have been sent

  uint32_t completed_but_not_polled;
  uint32_t polled_messages;

  // debug info
  uint32_t outstanding_messages;
  uint32_t wait_for_reps_ctr;

  uint32_t time_out_cnt;

  char *send_string;
  char *recv_string;
  mf_t *mfs;

} per_qp_meta_t;

typedef struct rdma_context {
  struct ibv_context *ibv_ctx;
  struct ibv_mr *recv_mr;
  struct ibv_pd *pd;
  uint16_t *local_id;
  int dev_port_id;
} rdma_context_t;


typedef struct context {
  hrd_ctrl_blk_t *cb;
  mcast_cb_t *mcast_cb;
  per_qp_meta_t *qp_meta;
  quorum_info_t *q_info;

  uint16_t qp_num;
  uint8_t m_id;
  uint16_t t_id;
  uint32_t total_recv_buf_size;
  void *recv_buffer;
  rdma_context_t *rdma_ctx;
  char* local_ip;
  void* appl_ctx;
  uint8_t *tmp_val;


} context_t;

static void check_ctx(context_t *ctx)
{
  for (int qp_i = 0; qp_i < ctx->qp_num; ++qp_i) {
    per_qp_meta_t *qp_meta = &ctx->qp_meta[qp_i];
    assert(qp_meta->send_wr != NULL);
    assert(qp_meta->send_qp != NULL);
    assert(qp_meta->recv_qp != NULL);
  }

  if (ENABLE_ASSERTIONS && ctx->t_id == 0) my_printf(green, "CTX checked \n");

}


///-----------------------------------------------------------------------
///------------------------QP_META----------------------------------------
///-----------------------------------------------------------------------

void create_per_qp_meta(per_qp_meta_t *qp_meta,
                        uint32_t send_wr_num,
                        uint32_t recv_wr_num,
                        flow_type_t flow_type,
                        recv_type_t recv_type,

                        uint16_t recv_qp_id,

                        uint32_t receipient_num,
                        uint32_t remote_senders_num,
                        uint32_t recv_fifo_slot_num,

                        uint32_t recv_size,
                        uint32_t send_size,
                        bool mcast_send,
                        bool mcast_recv,

                        uint16_t mcast_qp_id,
                        uint8_t leader_m_id,
                        uint32_t send_fifo_slot_num,
                        uint16_t credits,
                        uint16_t mes_header,
                        const char *send_string,
                        const char *recv_string);


void crate_ack_qp_meta(per_qp_meta_t* qp_meta,
                       uint16_t recv_qp_id,
                       uint32_t receipient_num,
                       uint32_t remote_senders_num,
                       uint16_t max_remote_credits);



///-----------------------------------------------------------------------
///------------------------CTX-------------------------------------------
///-----------------------------------------------------------------------
void ctx_set_qp_meta_mfs(context_t *ctx,
                         mf_t *mf);

void ctx_qp_meta_mirror_buffers(per_qp_meta_t *qp_meta,
                                uint32_t max_size,
                                uint16_t fifo_num);


void init_ctx_send_wrs(context_t *ctx);

void set_up_ctx(context_t *ctx);

context_t *create_ctx(uint8_t m_id, uint16_t t_id,
                      uint16_t qp_num, char* local_ip);




#endif //ODYSSEY_NETWORK_CONTEXT_H
