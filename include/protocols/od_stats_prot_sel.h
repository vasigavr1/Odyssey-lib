//
// Created by vasilis on 14/09/20.
//

#ifndef ODYSSEY_OD_STATS_PROT_SEL_H
#define ODYSSEY_OD_STATS_PROT_SEL_H


#ifdef ZOOKEEPER
  #include "zk_util.h"
  #define appl_stats zk_stats
#endif

#ifdef KITE
  #include "kt_util.h"
  #define appl_stats kite_stats
#endif

#ifdef PAXOS
  #include "cp_util.h"
  #define appl_stats cp_stats
#endif

#ifdef DERECHO
  #include "dr_util.h"
  #define appl_stats dr_stats
#endif

#ifdef HERMES
  #include "hr_util.h"
  #define appl_stats hr_stats
#endif

#ifdef CHT
  #include "cht_util.h"
  #define appl_stats cht_stats
#endif

#ifdef CRAQ
  #include "cr_util.h"
  #define appl_stats cr_stats
#endif


#endif //ODYSSEY_OD_STATS_PROT_SEL_H
