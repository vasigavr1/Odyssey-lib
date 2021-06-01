//
// Created by vasilis on 14/09/20.
//

#ifndef ODYSSEY_TOP_INCLUDE_H
#define ODYSSEY_TOP_INCLUDE_H



typedef enum {
  error_sys,
  kite_sys = 141,
  zookeeper_sys = 142,
  derecho_sys,
  hermes_sys,
  cht_sys,
  craq_sys,
  paxos_sys
} system_t;

//#define COMPILED_SYSTEM zookeeper_sys



#ifdef KITE
  #define COMPILED_SYSTEM kite_sys
#endif

#ifdef ZOOKEEPER
  #define COMPILED_SYSTEM zookeeper_sys
#endif

#ifdef DERECHO
  #define COMPILED_SYSTEM derecho_sys
#endif

#ifdef HERMES
  #define COMPILED_SYSTEM hermes_sys
#endif

#ifdef CHT
  #define COMPILED_SYSTEM cht_sys
#endif

#ifdef CRAQ
  #define COMPILED_SYSTEM craq_sys
#endif

#ifdef PAXOS
  #define COMPILED_SYSTEM paxos_sys
#endif

///Default for the IDE
#ifndef KITE
  #ifndef ZOOKEEPER
    #ifndef DERECHO
      #ifndef HERMES
        #ifndef CHT
          #ifndef CRAQ
            #ifndef PAXOS
              #define ZOOKEPER
              #define COMPILED_SYSTEM zookeeper_sys
            #endif
          #endif
        #endif
      #endif
    #endif
  #endif
#endif

#endif //ODYSSEY_TOP_INCLUDE_H
