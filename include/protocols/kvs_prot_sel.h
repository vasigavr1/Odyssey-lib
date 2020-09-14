//
// Created by vasilis on 14/09/20.
//

#ifndef ODYSSEY_KVS_PROT_SEL_H
#define ODYSSEY_KVS_PROT_SEL_H

#ifdef KITE
  #include "config.h"
#endif

#ifdef DERECHO
  #include "dr_config.h"
#endif

#ifdef ZOOKEEPER
  #include "zk_config.h"
#endif

#ifdef HERMES
  #include "hr_config.h"
#endif

#ifdef CHT
  #include "cht_config.h"
#endif

#ifndef KITE
  #ifndef ZOOKEEPER
    #ifndef DERECHO
      #ifndef HERMES
        #ifndef CHT
          #include "zk_config.h"
        #endif
      #endif
    #endif
  #endif
#endif

#endif //ODYSSEY_KVS_PROT_SEL_H
