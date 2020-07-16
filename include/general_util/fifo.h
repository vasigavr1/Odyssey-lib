//
// Created by vasilis on 16/07/20.
//

#ifndef ODYSSEY_FIFO_H
#define ODYSSEY_FIFO_H


#include "top.h"

typedef struct per_fifo_slot_meta {
  uint16_t coalesce_num;
  uint32_t byte_size;
  uint32_t resp_size;
  uint32_t backward_ptr; // ptr to a different fifo
} slot_meta_t;

typedef struct fifo {
  void *fifo;
  slot_meta_t *slot_meta;
  void* ctx;
  uint32_t push_ptr;
  uint32_t pull_ptr;
  uint32_t max_size; //in slots
  uint32_t max_byte_size;
  uint32_t capacity;
  uint32_t net_capacity;
  uint32_t slot_size;
  uint16_t mes_header;
} fifo_t;


/*---------------------------------------------------------------------
 * -----------------------FIFO Handles------------------------------
 * ---------------------------------------------------------------------*/

static inline void check_fifo(fifo_t *fifo)
{
  if (ENABLE_ASSERTIONS) {
    assert(fifo != NULL);
    assert(fifo->fifo != NULL);
    assert(fifo->max_size > 0);
    assert(fifo->slot_size > 0);
    assert(fifo->capacity <= fifo->max_size);
    assert(fifo->slot_size * fifo->max_size == fifo->max_byte_size);
    assert(fifo->push_ptr < fifo->max_size);
    assert(fifo->pull_ptr < fifo->max_size);
  }
}

static inline void check_slot_and_slot_meta(fifo_t *fifo, uint32_t slot_no)
{
  if (ENABLE_ASSERTIONS) {
    assert(slot_no < fifo->max_size);
    assert(fifo->slot_meta != NULL);
  }
}

static inline fifo_t *fifo_constructor(uint32_t max_size,
                                       uint32_t slot_size,
                                       bool alloc_slot_meta,
                                       uint16_t mes_header)
{
  fifo_t * fifo = calloc(1, (sizeof(fifo_t)));
  fifo->max_size = max_size;
  fifo->slot_size = slot_size;
  fifo->max_byte_size = max_size * slot_size;
  fifo->fifo = calloc(fifo->max_byte_size, sizeof(uint8_t));

  if (alloc_slot_meta) {
    fifo->slot_meta = calloc(max_size, sizeof(slot_meta_t));
    fifo->slot_meta[0].byte_size = mes_header;
  }
  fifo->mes_header = mes_header;
  check_fifo(fifo);
  return fifo;
}

/// FIFO SLOT
static inline void* get_fifo_slot(fifo_t *fifo, uint32_t slot, uint32_t offset)
{
  check_fifo(fifo);
  if (ENABLE_ASSERTIONS) {
    assert(slot < fifo->max_size);
    assert((fifo->slot_size * slot) + offset < fifo->max_byte_size);
  }
  return fifo->fifo + (fifo->slot_size * slot) + offset;
}


static inline void* get_fifo_push_slot(fifo_t *fifo)
{
  return get_fifo_slot(fifo, fifo->push_ptr, 0);
}

static inline void* get_fifo_pull_slot(fifo_t *fifo)
{
  return get_fifo_slot(fifo, fifo->pull_ptr, 0);
}

static inline void* get_fifo_specific_slot(fifo_t *fifo, uint32_t slot_no)
{
  return get_fifo_slot(fifo, slot_no, 0);
}


static inline void* get_fifo_push_slot_with_offset(fifo_t *fifo, uint32_t offset)
{
  return get_fifo_slot(fifo, fifo->push_ptr, offset);
}

static inline void* get_fifo_pull_slot_with_offset(fifo_t *fifo, uint32_t offset)
{
  return get_fifo_slot(fifo, fifo->pull_ptr, offset);
}

static inline void* get_fifo_specific_slot_with_offset(fifo_t *fifo, uint32_t slot_no,
                                                       uint32_t offset)
{
  return get_fifo_slot(fifo, slot_no, offset);
}

/// INCREMENT PUSH PULL
static inline void fifo_incr_push_ptr(fifo_t *fifo)
{
  check_fifo(fifo);
  MOD_INCR(fifo->push_ptr, fifo->max_size);
  check_fifo(fifo);
}

static inline void fifo_incr_pull_ptr(fifo_t *fifo)
{
  check_fifo(fifo);
  MOD_INCR(fifo->pull_ptr, fifo->max_size);
  check_fifo(fifo);
}

// GET SLOT_META

static inline slot_meta_t* get_fifo_slot_meta(fifo_t *fifo, uint32_t slot_no)
{
  check_fifo(fifo);
  check_slot_and_slot_meta(fifo, slot_no);
  return &fifo->slot_meta[slot_no];
}

static inline slot_meta_t* get_fifo_slot_meta_push(fifo_t *fifo)
{
  return get_fifo_slot_meta(fifo, fifo->push_ptr);
}

static inline slot_meta_t* get_fifo_slot_meta_pull(fifo_t *fifo)
{
  return get_fifo_slot_meta(fifo, fifo->pull_ptr);
}

// GET/ SET BACKWARDS_PTRS
static inline uint32_t fifo_get_backward_ptr(fifo_t *fifo, uint32_t slot_no)
{
  check_fifo(fifo);
  check_slot_and_slot_meta(fifo, slot_no);

  return fifo->slot_meta[slot_no].backward_ptr;
}

static inline uint32_t fifo_get_push_backward_ptr(fifo_t *fifo)
{
  return fifo_get_backward_ptr(fifo, fifo->push_ptr);
}

static inline uint32_t fifo_get_pull_backward_ptr(fifo_t *fifo)
{
  return fifo_get_backward_ptr(fifo, fifo->pull_ptr);
}

static inline void fifo_set_backward_ptr(fifo_t *fifo, uint32_t slot_no, uint32_t new_ptr)
{
  check_fifo(fifo);
  if (ENABLE_ASSERTIONS) {
    assert(slot_no < fifo->max_size);
    assert(fifo->slot_meta != NULL);
  }
  fifo->slot_meta[slot_no].backward_ptr = new_ptr;
}

static inline void fifo_set_push_backward_ptr(fifo_t *fifo, uint32_t new_ptr)
{
  fifo_set_backward_ptr(fifo, fifo->push_ptr, new_ptr);
}

static inline void fifo_set_pull_backward_ptr(fifo_t *fifo, uint32_t new_ptr)
{
  fifo_set_backward_ptr(fifo, fifo->pull_ptr, new_ptr);
}


///
static inline void fifo_incr_capacity(fifo_t *fifo)
{
  check_fifo(fifo);
  fifo->capacity++;
  check_fifo(fifo);
}

static inline void fifo_decr_capacity(fifo_t *fifo)
{
  check_fifo(fifo);
  fifo->capacity--;
  check_fifo(fifo);
}

///
static inline void fifo_incr_net_capacity(fifo_t *fifo)
{
  check_fifo(fifo);
  fifo->net_capacity++;
}


///
// Set up a fresh read message to coalesce requests -- Proposes, reads, acquires
static inline void reset_fifo_slot(fifo_t *send_fifo)
{
  check_fifo(send_fifo);
  fifo_incr_push_ptr(send_fifo);
  slot_meta_t *slot_meta = get_fifo_slot_meta_push(send_fifo);

  slot_meta->byte_size = (uint16_t) send_fifo->mes_header;
  slot_meta->resp_size = 0;
  slot_meta->coalesce_num = 0;

}


static inline void* get_send_fifo_ptr(fifo_t* send_fifo,
                                      uint32_t new_size,
                                      uint32_t resp_size,
                                      uint16_t t_id)
{
  slot_meta_t *slot_meta = get_fifo_slot_meta_push(send_fifo);
  slot_meta->resp_size += resp_size;


  bool new_message_because_of_r_rep = slot_meta->resp_size > MTU;
  bool new_message = (slot_meta->byte_size + new_size) > send_fifo->slot_size ||
                     new_message_because_of_r_rep;

  if (new_message) {
    reset_fifo_slot(send_fifo);
    slot_meta = get_fifo_slot_meta_push(send_fifo);
  }

  slot_meta->coalesce_num++;
  uint32_t inside_r_ptr = slot_meta->byte_size;
  slot_meta->byte_size += new_size;
  if (ENABLE_ASSERTIONS) {
    assert(slot_meta->byte_size <= send_fifo->slot_size);
    assert(slot_meta->byte_size <= MTU);
  }
  return get_fifo_push_slot_with_offset(send_fifo, inside_r_ptr);
}



/*------------------------------------------------------
 * ----------------BUFFER MIRRORING------------------------
 * ----------------------------------------------------*/

/// Sometimes it's hard to infer or even send credits.
/// For example a zk-follower infers the credits to send writes to the leader
// by examining the incoming commits.
/// In this case it is useful to mirror the buffer of remote machines,
/// to infer their buffer availability

// Generic function to mirror buffer spaces--used when elements are added
static inline void add_to_the_mirrored_buffer(struct fifo *mirror_buf, uint8_t coalesce_num,
                                              uint16_t number_of_fifos,
                                              uint32_t max_size,
                                              quorum_info_t *q_info)
{
  for (uint16_t i = 0; i < number_of_fifos; i++) {
    uint32_t push_ptr = mirror_buf[i].push_ptr;
    uint16_t *fifo = (uint16_t *) mirror_buf[i].fifo;
    fifo[push_ptr] = (uint16_t) coalesce_num;
    MOD_INCR(mirror_buf[i].push_ptr, max_size);
    mirror_buf[i].capacity++;
    if (mirror_buf[i].capacity > max_size) {
      // this may noy be an error if there is a failure
      assert(q_info != NULL);
      assert(q_info->missing_num > 0);
    }
    //if (ENABLE_ASSERTIONS) assert(mirror_buf[i].capacity <= max_size);
  }
}

// Generic function to mirror buffer spaces--used when elements are removed
static inline uint16_t remove_from_the_mirrored_buffer(struct fifo *mirror_buf_, uint16_t remove_num,
                                                       uint16_t t_id, uint8_t fifo_id, uint32_t max_size)
{
  struct fifo *mirror_buf = &mirror_buf_[fifo_id];
  uint16_t *fifo = (uint16_t *) mirror_buf->fifo;
  uint16_t new_credits = 0;
  if (ENABLE_ASSERTIONS && mirror_buf->capacity == 0) {
    my_printf(red, "remove_num %u, ,mirror_buf->w_pull_ptr %u fifo_id %u  \n",
              remove_num, mirror_buf->pull_ptr, fifo_id);
    assert(false);
  }
  while (remove_num > 0) {
    uint32_t pull_ptr = mirror_buf->pull_ptr;
    if (fifo[pull_ptr] <= remove_num) {
      remove_num -= fifo[pull_ptr];
      MOD_INCR(mirror_buf->pull_ptr, max_size);
      if (ENABLE_ASSERTIONS && mirror_buf->capacity == 0) {
        my_printf(red, "remove_num %u, ,mirror_buf->w_pull_ptr %u fifo_id %u  \n",
                  remove_num, mirror_buf->pull_ptr, fifo_id);
        assert(false);
      }
      mirror_buf->capacity--;
      new_credits++;
    }
    else {
      fifo[pull_ptr] -= remove_num;
      remove_num = 0;
    }
  }
  return new_credits;
}


#endif //ODYSSEY_FIFO_H
