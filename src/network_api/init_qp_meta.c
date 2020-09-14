//
// Created by vasilis on 10/09/20.
//

#include "network_context.h"


void allocate_work_requests(per_qp_meta_t* qp_meta)
{
  qp_meta->send_wr = malloc(qp_meta->send_wr_num * sizeof(struct ibv_send_wr));
  switch (qp_meta->flow_type) {
    case SEND_CREDITS_LDR_RECV_NONE:
      qp_meta->send_sgl = malloc(sizeof(struct ibv_sge));
      break;
    case SEND_BCAST_LDR_RECV_UNI:
    case SEND_BCAST_RECV_UNI:
    case SEND_BCAST_RECV_BCAST:
      qp_meta->send_sgl = malloc(MAX_BCAST_BATCH * sizeof(struct ibv_sge));
      break;
    case RECV_CREDITS:
      break;
    case SEND_UNI_REQ_RECV_UNI_REQ:
    case SEND_UNI_REQ_RECV_LDR_REP:
    case SEND_UNI_REP_RECV_LDR_BCAST:
    case SEND_UNI_REP_TO_BCAST:
    case SEND_UNI_REP_LDR_RECV_UNI_REQ:
    case SEND_UNI_REP_RECV_UNI_REQ:
    case SEND_UNI_REP_RECV_UNI_REP:
    case SEND_ACK_RECV_ACK:
      qp_meta->send_sgl = malloc(qp_meta->send_wr_num * sizeof(struct ibv_sge));
      break;
    default: assert(false);


  }
  if (qp_meta->recv_wr_num > 0) {
    qp_meta->recv_wr = malloc(qp_meta->recv_wr_num * sizeof(struct ibv_recv_wr));
    qp_meta->recv_sgl = malloc(qp_meta->recv_wr_num * sizeof(struct ibv_sge));
    qp_meta->recv_wc = malloc(qp_meta->recv_wr_num * sizeof(struct ibv_wc));
  }
}

uint16_t get_credit_rows(per_qp_meta_t* qp_meta)
{
  switch (qp_meta->flow_type){
    case SEND_BCAST_LDR_RECV_UNI:
    case SEND_BCAST_RECV_UNI:
    case SEND_BCAST_RECV_BCAST:
    case SEND_UNI_REQ_RECV_UNI_REQ:
    case SEND_UNI_REP_TO_BCAST:
    case SEND_UNI_REP_RECV_UNI_REQ:
    case SEND_UNI_REP_RECV_UNI_REP:
      assert(qp_meta->receipient_num == REM_MACH_NUM);
      return MACHINE_NUM;
    case SEND_UNI_REQ_RECV_LDR_REP:
    case SEND_UNI_REP_RECV_LDR_BCAST:
      assert(qp_meta->receipient_num == 1);
      return 1;
    case SEND_UNI_REP_LDR_RECV_UNI_REQ:
    case RECV_CREDITS:
    case SEND_CREDITS_LDR_RECV_NONE:
    case SEND_ACK_RECV_ACK:
    default: assert(false);
  }
}

void qp_meta_ss_batch_q_depth(per_qp_meta_t* qp_meta)
{
  switch (qp_meta->flow_type){
    case SEND_BCAST_LDR_RECV_UNI:
    case SEND_BCAST_RECV_UNI:
    case SEND_BCAST_RECV_BCAST:
      qp_meta->ss_batch = (uint32_t) MAX((MIN_SS_BATCH / (qp_meta->receipient_num)), (MAX_BCAST_BATCH + 2));
      qp_meta->send_q_depth = ((2 * qp_meta->ss_batch * qp_meta->receipient_num) + 10);
      break;
    case RECV_CREDITS:
      break;
    case SEND_CREDITS_LDR_RECV_NONE:
    case SEND_UNI_REQ_RECV_UNI_REQ:
    case SEND_UNI_REQ_RECV_LDR_REP:
    case SEND_UNI_REP_RECV_LDR_BCAST:
    case SEND_UNI_REP_TO_BCAST:
    case SEND_UNI_REP_RECV_UNI_REQ:
    case SEND_UNI_REP_RECV_UNI_REP:
    case SEND_UNI_REP_LDR_RECV_UNI_REQ:
    case SEND_ACK_RECV_ACK:
      qp_meta->ss_batch = (uint32_t) MAX(MIN_SS_BATCH, (qp_meta->send_wr_num + 2));
      qp_meta->send_q_depth = (2 * qp_meta->ss_batch) + 3;
      break;
    default: assert(false);
  }

  qp_meta->recv_q_depth = qp_meta->recv_wr_num + 3;
}

void qp_meta_set_strings(per_qp_meta_t* qp_meta,
                                const char *send_string,
                                const char *recv_string)
{
  if (send_string != NULL) {
    qp_meta->send_string = malloc(strlen(send_string) + 1);
    strcpy(qp_meta->send_string, send_string);
  }

  if (recv_string != NULL) {
    qp_meta->recv_string = malloc(strlen(recv_string) + 1);
    strcpy(qp_meta->recv_string, recv_string);
  }
}

void qp_meta_set_up_credits(per_qp_meta_t* qp_meta, uint16_t credits)
{
  qp_meta->max_credits = credits;
  if (credits > 0) {
    qp_meta->needs_credits = true;
    int credit_rows = get_credit_rows(qp_meta);
    qp_meta->credits = malloc(credit_rows * sizeof(uint16_t));
    for (int cr_i = 0; cr_i < credit_rows; ++cr_i) {
      qp_meta->credits[cr_i] = credits;
    }
  }
}


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
                        const char *recv_string)
{
  qp_meta->send_wr_num = send_wr_num;
  qp_meta->recv_wr_num = recv_wr_num;
  qp_meta->flow_type = flow_type;
  qp_meta->recv_type = recv_type;
  qp_meta->receipient_num = receipient_num;
  qp_meta->leader_m_id = leader_m_id;

  qp_meta->recv_buf_slot_num = recv_fifo_slot_num;
  qp_meta->recv_buf_size = recv_fifo_slot_num * recv_size;
  qp_meta->recv_size = recv_size;
  qp_meta->send_size = send_size;
  qp_meta->mcast_send = mcast_send;
  qp_meta->mcast_recv = mcast_recv;
  qp_meta->mcast_qp_id = mcast_qp_id;
  qp_meta->completed_but_not_polled = 0;
  qp_meta->recv_qp_id = recv_qp_id;

  qp_meta_set_strings(qp_meta, send_string, recv_string);
  qp_meta_ss_batch_q_depth(qp_meta);

  qp_meta_set_up_credits(qp_meta, credits);

  qp_meta->enable_inlining = send_size <= MAXIMUM_INLINE_SIZE;
  qp_meta->recv_fifo = calloc(1, sizeof(fifo_t));
  qp_meta->recv_fifo->fifo = NULL; // will be filled after initializing the hrd_cb
  qp_meta->recv_fifo->max_size = recv_fifo_slot_num;
  qp_meta->recv_fifo->max_byte_size = recv_fifo_slot_num * recv_size;
  qp_meta->recv_fifo->slot_size = recv_size;

  if (send_fifo_slot_num > 0) {
    qp_meta->send_fifo_num = (uint16_t) (flow_type == SEND_UNI_REQ_RECV_UNI_REQ ? receipient_num : 1);
    if (ENABLE_ASSERTIONS) printf("Setting up %u send_fifo(s) with %u slots, %u size\n",
                                  qp_meta->send_fifo_num, send_fifo_slot_num, send_size);
    qp_meta->send_fifo = fifo_constructor(send_fifo_slot_num,
                                          send_size, true, mes_header,
                                          qp_meta->send_fifo_num);
  }
  else qp_meta->send_fifo_num = 0;
  allocate_work_requests(qp_meta);
  if (qp_meta->mfs == NULL) qp_meta->mfs = malloc(sizeof(mf_t));
}


void crate_ack_qp_meta(per_qp_meta_t* qp_meta,
                              uint16_t recv_qp_id,
                              uint32_t receipient_num,
                              uint32_t remote_senders_num,
                              uint16_t max_remote_credits)
{


  create_per_qp_meta(qp_meta, receipient_num + 1,
                     max_remote_credits * remote_senders_num,
                     SEND_ACK_RECV_ACK, RECV_ACK,
                     recv_qp_id, receipient_num, remote_senders_num,
                     max_remote_credits * remote_senders_num,
                     sizeof(struct ctx_ack_message_ud_req),
                     sizeof(ctx_ack_mes_t),
                     false, false, 0, 0, receipient_num + 1, 0, 0,
                     "send acks", "recv acks");

  ctx_ack_mes_t *ack_send_buf = (ctx_ack_mes_t *) qp_meta->send_fifo->fifo;
  for (int m_i = 0; m_i <= receipient_num; m_i++) {
    qp_meta->send_sgl[m_i].addr =
      (uintptr_t) &ack_send_buf[m_i];
  }
}
