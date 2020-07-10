//
// Created by vasilis on 10/07/20.
//

#ifndef ODYSSEY_MULTICAST_H
#define ODYSSEY_MULTICAST_H


#include "top.h"

// This helps us set up the necessary rdma_cm_ids for the multicast groups
struct cm_qps
{
  struct rdma_cm_id **cma_id;
  struct ibv_pd* pd;
  struct ibv_cq** cq;
};


// This helps us set up the multicasts
typedef struct mcast_init
{
  //int	t_id;
  struct rdma_event_channel *channel;
  struct sockaddr_storage *dst_in;
  struct sockaddr **dst_addr;
  struct sockaddr_storage src_in;
  struct sockaddr *src_addr;
  struct cm_qps *cm_qp;
  //Send-only stuff
  struct rdma_ud_param *mcast_ud_param;

} mcast_init_t;

typedef struct mcast_context {
  mcast_init_t *init;
  uint16_t groups_num;
  uint16_t machine_num;
  uint32_t *recv_q_depth;
  uint16_t *group_to_send_to;
  void *buf;
  size_t buff_size;
  uint16_t flow_num;
  uint16_t *groups_per_flow;
  uint16_t send_qp_num;
  uint16_t recv_qp_num;
  uint16_t t_id;
} mcast_context_t;


// this contains all data we need to perform our mcasts
typedef struct mcast_cb {
  struct ibv_cq **recv_cq;
  struct ibv_qp **recv_qp;
  struct ibv_mr *recv_mr;
  struct ibv_ah **send_ah;
  uint32_t *qpn;
  uint32_t *qkey;
} mcast_cb_t;



// wrapper around getaddrinfo socket function
static int get_addr(char *dst, struct sockaddr *addr)
{
  struct addrinfo *res;
  int ret;
  ret = getaddrinfo(dst, NULL, NULL, &res);
  if (ret) {
    printf("getaddrinfo failed - invalid hostname or IP address %s\n", dst);
    return ret;
  }
  memcpy(addr, res->ai_addr, res->ai_addrlen);
  freeaddrinfo(res);
  return ret;
}

//Handle the addresses
static void resolve_addresses(mcast_context_t* mcast_cont)
{
  mcast_init_t *init = mcast_cont->init;
  int ret, i, t_id = mcast_cont->t_id;
  char mcast_addr[40];
  // Source addresses (i.e. local IPs)
  init->src_addr = (struct sockaddr*)&init->src_in;
  ret = get_addr(local_ip, ((struct sockaddr *)&init->src_in)); // to bind
  if (ret) printf("Client: failed to get src address \n");
  for (i = 0; i < mcast_cont->groups_num; i++) {
    ret = rdma_bind_addr(init->cm_qp->cma_id[i], init->src_addr);
    if (ret) perror("Client: address bind failed");
  }
  // Destination addresses(i.e. multicast addresses)
  for (i = 0; i < mcast_cont->groups_num; i ++) {
    init->dst_addr[i] = (struct sockaddr*)&init->dst_in[i];
    int m_cast_group_id = (t_id * mcast_cont->groups_num + i);
    sprintf(mcast_addr, "224.0.%d.%d", m_cast_group_id / 256, m_cast_group_id % 256);
    //printf("Thread %u mcast addr %d: %s\n", t_id, i, mcast_addr);
    ret = get_addr((char*) &mcast_addr, ((struct sockaddr *)&init->dst_in[i]));
    if (ret) printf("Client: failed to get dst address \n");
  }
}

// Set up the Send and Receive Qps for the multicast
static void set_up_mcast_qps(mcast_context_t* mcast_cont)
{
  struct cm_qps *qps = mcast_cont->init->cm_qp;
  uint32_t *max_recv_q_depth = mcast_cont->recv_q_depth;

  int ret, i, recv_q_depth;
  qps->pd = ibv_alloc_pd(qps->cma_id[0]->verbs);

  // Create the recv QPs that are needed (1 for each flow we are listening to) +
  // "useless" QPs

  for (i = 0; i < mcast_cont->recv_qp_num; i++) {
    recv_q_depth = i < mcast_cont->recv_qp_num ? max_recv_q_depth[i] : 1;
    assert(qps->cma_id[i] != NULL);
    assert(&qps->cq[i] != NULL);
    assert(&qps != NULL);
    qps->cq[i] = ibv_create_cq(qps->cma_id[i]->verbs, recv_q_depth, &qps, NULL, 0);
    struct ibv_qp_init_attr init_qp_attr;
    memset(&init_qp_attr, 0, sizeof init_qp_attr);
    init_qp_attr.cap.max_send_wr = 1;
    init_qp_attr.cap.max_recv_wr = (uint32_t) recv_q_depth;
    init_qp_attr.cap.max_send_sge = 1;
    init_qp_attr.cap.max_recv_sge = 1;
    init_qp_attr.qp_context = qps;
    init_qp_attr.sq_sig_all = 0;
    init_qp_attr.qp_type = IBV_QPT_UD;
    init_qp_attr.send_cq = qps->cq[i];
    init_qp_attr.recv_cq = qps->cq[i];
    ret = rdma_create_qp(qps->cma_id[i], qps->pd, &init_qp_attr);
    if (ret) printf("unable to create QP \n");
  }
}

// Initial function to call to setup multicast, this calls the rest of the relevant functions
static void setup_multicast(mcast_context_t* mcast_cont)
{
  int ret;
  uint16_t t_id = mcast_cont->t_id;
  static enum rdma_port_space port_space = RDMA_PS_UDP;
  mcast_init_t *init = mcast_cont->init;
  // Create the channel
  init->channel = rdma_create_event_channel();
  if (!init->channel) {
    printf("Client %d :failed to create event channel\n", t_id);
    exit(1);
  }
  // Set up the cma_ids
  for (int i = 0; i < mcast_cont->groups_num; i++ ) {
    ret = rdma_create_id(init->channel, &init->cm_qp->cma_id[i],
                         &init->cm_qp, port_space);
    if (ret) printf("Client %d :failed to create cma_id\n", t_id);
  }
  // deal with the addresses
  resolve_addresses(mcast_cont);
  // set up the qps
  set_up_mcast_qps(mcast_cont);

  struct rdma_cm_event* event = malloc(sizeof(struct rdma_cm_event));
  int qp_i = 0, group_i = 0;
  for (int flow_i = 0; flow_i < mcast_cont->flow_num; ++flow_i) {
    for ( int sub_group_i = 0; sub_group_i < mcast_cont->groups_per_flow[flow_i]; sub_group_i++) {
      ret = rdma_resolve_addr(init->cm_qp->cma_id[group_i], init->src_addr, init->dst_addr[group_i], 20000);
      if (ret) printf("Client %d: failed to resolve address: %d, qp_i %d \n", t_id, sub_group_i, qp_i);
      if (ret) perror("Reason");

      bool is_sender = false;

      if (mcast_cont->send_qp_num > 0) {
        if (sub_group_i == mcast_cont->group_to_send_to[flow_i]) {
          is_sender = true;
        }
      }

      while (rdma_get_cm_event(init->channel, &event) == 0) {
        bool joined = false;
        switch (event->event) {
          case RDMA_CM_EVENT_ADDR_RESOLVED:
            if (flow_i >= mcast_cont->recv_qp_num) {
              qp_i = mcast_cont->recv_qp_num; // this means we are not interested to receive from this flow
            }
              // We do not want to register the recv_qp that we will use to the mcast group that we own
              // (sub_group_i.e. sub_group_i == machine_id)
            else if (flow_i < mcast_cont->send_qp_num &&  sub_group_i == machine_id) {
              qp_i = mcast_cont->recv_qp_num; // this is a useless qp
            }
            else qp_i = flow_i; // this recv_qp will be used
            //printf("Worker %u uses recvp qp %u in group %u \n", t_id, qp_i, group_i);

            ret = rdma_join_multicast(init->cm_qp->cma_id[qp_i], init->dst_addr[group_i], init);
            if (ret) printf("unable to join multicast \n");
            break;
          case RDMA_CM_EVENT_MULTICAST_JOIN:
            joined = true;
            if (is_sender) init->mcast_ud_param[flow_i] = event->param.ud;

            //printf("RDMA JOIN MUlTICAST EVENT %d \n", sub_group_i);
            break;
          case RDMA_CM_EVENT_MULTICAST_ERROR:
          default:
            break;
        }
        rdma_ack_cm_event(event);
        if (joined) break;
      }
      group_i ++;
    }
  }
}


static mcast_cb_t *construct_mcast_cb(mcast_context_t* mcast_cont)
{
  mcast_cb_t* mcast_cb = (mcast_cb_t*) malloc(sizeof(mcast_cb_t));

  mcast_cb->recv_cq = calloc(mcast_cont->recv_qp_num, sizeof(struct ibv_cq*));
  mcast_cb->recv_qp = calloc(mcast_cont->recv_qp_num, sizeof(struct ibv_qp*));
  mcast_cb->send_ah = calloc(mcast_cont->send_qp_num, sizeof(struct ibv_ah*));
  mcast_cb->qpn = calloc(mcast_cont->send_qp_num, sizeof(uint32_t));
  mcast_cb->qkey = calloc(mcast_cont->send_qp_num, sizeof(uint32_t));

  return mcast_cb;
}

static mcast_context_t* construct_mcast_cont(uint16_t flow_num, uint16_t recv_qp_num,
                                             uint16_t send_num,
                                             uint16_t *active_bcast_machine_num,
                                             uint32_t *recv_q_depth,
                                             uint16_t *group_to_send_to,
                                             void *buf, size_t buff_size,
                                             uint16_t t_id)
{
  mcast_context_t* mcast_cont = (mcast_context_t*) malloc(sizeof(mcast_context_t));

  assert(flow_num > 0);
  assert(recv_qp_num > 0 || send_num > 0);
  assert(recv_qp_num <= flow_num);
  assert(send_num <= flow_num);
  for (int i = 0; i < recv_qp_num; ++i) {
    assert((recv_q_depth + i) != NULL);
    assert(recv_q_depth[i] > 1);
  }
  for (int i = 0; i < flow_num; ++i) {
    assert((active_bcast_machine_num + i) != NULL);
    assert(active_bcast_machine_num[i] > 0);
  }

  if (send_num > 0) {
    int correct_groups_found = 0;
    for (int i = 0; i < flow_num; ++i) {
      assert((group_to_send_to + i) != NULL);
      if (group_to_send_to[i] < active_bcast_machine_num[i])
        correct_groups_found++;
    }
    assert(correct_groups_found == send_num);
  }

  mcast_cont->flow_num = flow_num;

  mcast_cont->recv_qp_num = recv_qp_num; // how many flows you want to receive from
  mcast_cont->send_qp_num = send_num; // in how many flows you are an active broadcaster
  mcast_cont->buf = buf;
  mcast_cont->buff_size = buff_size;
  mcast_cont->t_id = t_id;


  mcast_cont->recv_q_depth = recv_q_depth;
  mcast_cont->groups_per_flow = active_bcast_machine_num;
  mcast_cont->group_to_send_to = group_to_send_to;

  mcast_cont->groups_num = 0;
  for (int j = 0; j < flow_num; ++j) {
    mcast_cont->groups_num += mcast_cont->groups_per_flow[j];
  }

  // Set up the init structure
  mcast_cont->init = (mcast_init_t*) malloc(sizeof(mcast_init_t));
  mcast_cont->init->dst_in = calloc(mcast_cont->groups_num, sizeof(struct sockaddr_storage));
  mcast_cont->init->dst_addr = calloc(mcast_cont->groups_num, sizeof(struct sockaddr *));
  mcast_cont->init->cm_qp = calloc(1, sizeof(struct cm_qps));
  mcast_cont->init->mcast_ud_param = calloc(mcast_cont->send_qp_num, sizeof(struct rdma_ud_param));

  // Set up cm_qp
  struct cm_qps *cm = mcast_cont->init->cm_qp;

  // In the case where we do no want to receive from a group, we still join it
  // (so that we can send in it)
  // To be able to rdma_join_multicast we need to provide a cma_id
  // Since we do not care about the group, we can t give a cma_id with a recv_qp we are using
  // Therefore in the case where we do not already have redundant cma_ids, we have to allocate a redundant one
  uint16_t cma_id_num = (uint16_t) (MAX(mcast_cont->groups_num, (mcast_cont->recv_qp_num + 1)));
  cm->cma_id = calloc(cma_id_num, sizeof(struct rdma_cm_id*));
  cm->cq = calloc(mcast_cont->groups_num, sizeof(struct ibv_cq*));

  return mcast_cont;

}


static void destroy_mcast_cont(mcast_context_t* mcast_cont)
{
  free(mcast_cont->init->dst_in);
  free(mcast_cont->init->dst_addr);
  free(mcast_cont->init->mcast_ud_param);

  // Set up cm_qp
  struct cm_qps *cm = mcast_cont->init->cm_qp;
  free(cm->cma_id);
  free(cm->cq);

  free(mcast_cont->init->cm_qp);
  free(mcast_cont->init);
  free(mcast_cont);
}

/*------------------------MULTICAST EXPLANATION-----------------
 * We will create N multicast groups in the switch.
 * For each group we need a unique IP that will be in mcast_cont->init->dst_in.
 * We create the different addresses in resolve_addresses()
 * For each group we need one struct rdma_cm_id (i.e.mcast_cont->init->cm_qp->cma_id).
 * rdma_resolve_addr() will associate the unique dst_in with the unique rdma_cm_ids
 * ---
 * This is where it gets tricky. The expected patter is that with each rdma_cm_id, we create one
 * QP, such that we have one QP per multicast group.
 * However, this is not what we want.
 * The reason is that ALL messages that go to a multicast group are forwarded to all receive QPs that are registered
 * This means that if we were to associate one multicast group with one logical flow (e.g. broadcasting writes),
 * then we would be receiving the messages we are broadcasting.
 *
 * To avoid this we do something different:
 * For each logical flow, we create one multicast group for each active broadcaster.
 * For 5 machines all broadcasting writes we must do the following:
 * 1) Create 5 multicast groups
 * Each machine broadcasts to one group
 * Each machine receives from all groups except one -- the one it broadcasts to
 * (so that it does not receive its own messages)
 *
 * Therefore, we do not need one QP per rdma_cm_id.
 * This is essentially an anti-pattern and creates the complexity.
 *
 * To Receive from a broadcast group we must:
 * -- Create a qp (rdma_create_qp) and a cq(ibv_create_cq) and associated with an rdma_cm_id.
 * -- To register then to the multicast group we must rdma_join_multicast()
 *    with the dst_address using that rdma_cm_id
 *
 * To Send to broadcast group we need not make use of the QPs (!)
 * We need only use the parameters of the group (rdma_cm_event->param.ud) to set up our regular WRs
 * I.e. we will use whatever send QP we do for unicasts,
 * but we will simply change the parameters of the send_Wrs
 * Then the sent messages will find the multicast group in the switch
 * We get the rdma_cm_event->param.ud when rdma_cm_event->event == RDMA_CM_EVENT_MULTICAST_JOIN
 * Therefore, we need to save that, for the multicast group we are interested in
 *
 * After calling setuo_multicast(), we simply pass iny the useful attributes to mcast_cb,
 * Finally, we reigster the buffer, where received messages will arrive, this could be done
 * by the client directly, using their own protection domain (pd)
 *
 * HOW TO USE THE CB:
 * -- Receive:
 * Replace your regualar recv cq and qp with the ones int he qp
 * Use the mcast_cb->recv_mr->lkey when posting receives
 * -- Send
 * Use the send_ah, qpn and_qkey to set up your send work requests
 *
 *
 * Input explanation:
 *  flow_num : how many distinct logical multicast flows (e.g. prepares and commits for Zookeeper)
 *  recv_qp_num : in how many flows you want to receive from
 *
 * */

// TODO add a bool recieve_per_flow input to allow to not receive in some flow

static mcast_cb_t* create_mcast_cb(uint16_t flow_num,
                                   uint16_t recv_qp_num,
                                   uint16_t send_num,
                                   uint16_t *active_bcast_machine_num,
                                   uint32_t *recv_q_depth,
                                   uint16_t *group_to_send_to,
                                   void *buf, size_t buff_size,
                                   uint16_t t_id)
{


  mcast_context_t* mcast_cont = construct_mcast_cont(flow_num, recv_qp_num, send_num,
                                                     active_bcast_machine_num,
                                                     recv_q_depth, group_to_send_to,
                                                     buf, buff_size, t_id);
  setup_multicast(mcast_cont);

  mcast_cb_t* mcast_cb = construct_mcast_cb(mcast_cont);
  // Fill cb from the mcast-context
  mcast_init_t* init = mcast_cont->init;
  for (uint16_t i = 0; i < mcast_cont->recv_qp_num; i++) {
    mcast_cb->recv_cq[i] = init->cm_qp->cq[i];
    mcast_cb->recv_qp[i] = init->cm_qp->cma_id[i]->qp;
  }

  for (uint16_t i = 0; i < mcast_cont->send_qp_num; i++) {
    mcast_cb->send_ah[i] = ibv_create_ah(init->cm_qp->pd, &(init->mcast_ud_param[i].ah_attr));
    mcast_cb->qpn[i] = init->mcast_ud_param[i].qp_num;
    mcast_cb->qkey[i] = init->mcast_ud_param[i].qkey;
  }
  if (recv_qp_num > 0)
    mcast_cb->recv_mr = ibv_reg_mr(init->cm_qp->pd, mcast_cont->buf,
                                   mcast_cont->buff_size, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
                                                        IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC);

  destroy_mcast_cont(mcast_cont);


  return mcast_cb;
}





#endif //ODYSSEY_MULTICAST_H
