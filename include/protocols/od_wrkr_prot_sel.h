//
// Created by vasilis on 14/09/20.
//


#ifdef KITE
  #include "kt_util.h"
  #include "kt_inline_util.h"
  #define appl_init_qp_meta kite_init_qp_meta
  #define set_up_appl_ctx set_up_pending_ops
  #define main_loop kite_main_loop
#endif

#ifdef PAXOS
  #include "cp_util.h"
  #include "cp_inline_util.h"
  #define appl_init_qp_meta cp_init_qp_meta
  #define set_up_appl_ctx cp_set_up_pending_ops
  #define main_loop cp_main_loop
#endif

#ifdef ZOOKEEPER
  #include "zk_util.h"
  #include "zk_inline_util.h"

  #define appl_init_qp_meta zk_init_qp_meta
  #define set_up_appl_ctx set_up_zk_ctx
  #define main_loop zk_main_loop
#endif

#ifdef DERECHO
  #include "dr_inline_util.h"
  #include "dr_util.h"
  #define appl_init_qp_meta dr_init_qp_meta
  #define set_up_appl_ctx set_up_dr_ctx
  #define main_loop dr_main_loop
#endif

#ifdef HERMES
  #include "hr_inline_util.h"
  #include "hr_util.h"
  #define appl_init_qp_meta hr_init_qp_meta
  #define set_up_appl_ctx set_up_hr_ctx
  #define main_loop hr_main_loop
#endif

#ifdef CHT
  #include "cht_inline_util.h"
  #include "cht_util.h"
  #define appl_init_qp_meta cht_init_qp_meta
  #define set_up_appl_ctx set_up_cht_ctx
  #define main_loop cht_main_loop
#endif

#ifdef CRAQ
  #include "cr_inline_util.h"
  #include "cr_util.h"
  #define appl_init_qp_meta cr_init_qp_meta
  #define set_up_appl_ctx set_up_cr_ctx
  #define main_loop cr_main_loop
#endif




#ifndef ODYSSEY_WRKR_PROT_SEL_H
#define ODYSSEY_WRKR_PROT_SEL_H

#endif //ODYSSEY_WRKR_PROT_SEL_H
