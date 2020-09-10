//
// Created by vasilis on 10/09/20.
//

#ifndef ODYSSEY_CTX_TEMPLATES_H
#define ODYSSEY_CTX_TEMPLATES_H


#define CTX_ACK_SIZE (16)
#define CTX_ACK_RECV_SIZE (GRH_SIZE + (CTX_ACK_SIZE))

// The format of an ack message
typedef struct ctx_ack_message {
  uint64_t l_id; // the first local id that is being acked
  uint32_t ack_num;
  uint16_t credits;
  uint8_t m_id;
  uint8_t opcode;
} __attribute__((__packed__)) ctx_ack_mes_t;


typedef struct ctx_ack_message_ud_req {
  uint8_t grh[GRH_SIZE];
  ctx_ack_mes_t ack;
} ctx_ack_mes_ud_t;


#define CTX_COM_SEND_SIZE (16)
#define CTX_COM_RECV_SIZE (GRH_SIZE + CTX_COM_SEND_SIZE)

// The format of a commit message
typedef struct ctx_com_message {
  uint64_t l_id;
  uint32_t com_num;
  uint8_t opcode;
  uint8_t m_id;
  uint8_t unused[2];
} __attribute__((__packed__)) ctx_com_mes_t;

// commit message plus the grh
typedef struct ctx_com_message_ud_req {
  uint8_t grh[GRH_SIZE];
  ctx_com_mes_t com;
} ctx_com_mes_ud_t;


typedef struct ctx_trace_op {
  mica_key_t key;
  uint8_t *value_to_write;
  uint8_t *value_to_read; //compare value for CAS/  addition argument for F&A
  uint32_t index_to_req_array;
  uint32_t real_val_len; // this is the value length the client is interested in
  uint16_t session_id;
  uint8_t opcode;
  uint8_t val_len; // this represents the maximum value len
} ctx_trace_op_t;

#endif //ODYSSEY_CTX_TEMPLATES_H
