//
// Created by vasilis on 20/08/20.
//

#ifdef KITE
  #include "util.h"
  #include "kite_inline_util.h"
  #define appl_init_qp_meta kite_init_qp_meta
  #define set_up_appl_ctx set_up_pending_ops
  #define main_loop main_loop
#endif

#ifdef ZOOKEEPER
  #include "zk_util.h"
  #include "zk_inline_util.h"

  #define appl_init_qp_meta zk_init_qp_meta
  #define set_up_appl_ctx set_up_zk_ctx
  #define main_loop main_loop
#endif

#ifdef DERECHO

  #include "dr_inline_util.h"
  #include "dr_util.h"
  #define appl_init_qp_meta dr_init_qp_meta
  #define set_up_appl_ctx set_up_dr_ctx
  #define main_loop main_loop
#endif

#include "init_connect.h"


void *worker(void *arg)
{
  struct thread_params params = *(struct thread_params *) arg;
  uint16_t t_id = (uint16_t) params.id;

  if (t_id == 0) {
    my_printf(yellow, "Machine-id %d \n",
              machine_id);
    if (ENABLE_MULTICAST) my_printf(cyan, "MULTICAST IS ENABLED \n");
  }



  context_t *ctx = create_ctx((uint8_t) machine_id,
                              (uint16_t) params.id,
                              (uint16_t) QP_NUM,
                              local_ip);

  appl_init_qp_meta(ctx);
  set_up_ctx(ctx);

  /// Connect with other machines and exchange qp information
  setup_connections_and_spawn_stats_thread(ctx);
  // We can set up the send work requests now that
  // we have address handles for remote machines
  init_ctx_send_wrs(ctx);

  /// Application specific context
  ctx->appl_ctx = (void*) set_up_appl_ctx(ctx);

  if (t_id == 0)
    my_printf(green, "Worker %d  reached the loop \n", t_id);

  ///
  main_loop(ctx);


  return NULL;
};