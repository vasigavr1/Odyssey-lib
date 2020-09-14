//
// Created by vasilis on 14/09/20.
//

#ifndef ODYSSEY_STATS_PROT_SEL_H
#define ODYSSEY_STATS_PROT_SEL_H


#ifdef ZOOKEEPER
  #include "zk_util.h"
  #define appl_stats zk_stats
#endif

#ifdef KITE
  #include <latency_util.h>
  #include "util.h"
  #define appl_stats kite_stats
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



#endif //ODYSSEY_STATS_PROT_SEL_H
