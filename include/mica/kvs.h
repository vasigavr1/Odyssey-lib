#ifndef KITE_KVS_H
#define KITE_KVS_H


#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <generic_inline_util.h>
#include "top.h"
#include "wrkr_side_calls.h"
#include <debug_util.h>

#include "kvs_prot_sel.h"

#define KVS_NUM_BKTS (8 * 1024 * 1024)
#define KVS_LOG_CAP  (1024 * 1024 * 1024)




#define MICA_LOG_BITS 40
#define MICA_INDEX_SHM_KEY 1185
#define MICA_LOG_SHM_KEY 2185



struct mica_slot {
	uint32_t in_use	:1;
	uint32_t tag	:(64 - MICA_LOG_BITS - 1);
	uint64_t offset	:MICA_LOG_BITS;
};

struct mica_bkt {
	struct mica_slot slots[8];
};

typedef struct  {
	struct mica_bkt *ht_index;
	uint8_t *ht_log;

	/* Metadata */
	int instance_id;	/* ID of this MICA instance. Used for shm keys */
	int node_id;

	int num_bkts;	/* Number of buckets requested by user */
	int bkt_mask;	/* Mask down from a mica_key's @bkt to a bucket */

	uint64_t log_cap;	/* Capacity of circular log in bytes */
	uint64_t log_mask;	/* Mask down from a slot's @offset to a log offset */

	/* State */
	uint64_t log_head;

	/* Stats */
	long long num_get_op;	/* Number of GET requests executed */
	long long num_put_op;	/* Number of PUT requests executed */
	long long num_get_fail;	/* Number of GET requests failed */
	long long num_put_fail;	/* Number of GET requests failed */
	long long num_insert_op;	/* Number of PUT requests executed */
	long long num_index_evictions; /* Number of entries evicted from index */
} mica_kv_t;

extern mica_kv_t *KVS;



void custom_mica_init(int kvs_id);
void custom_mica_populate_fixed_len(mica_kv_t *, int n, int val_len);


/* ---------------------------------------------------------------------------
//------------------------------ KVS UTILITY GENERIC -----------------------------
//---------------------------------------------------------------------------*/


// Locate the buckets for the requested keys
static inline void KVS_locate_one_bucket(uint16_t op_i, uint *bkt, struct key *op_key,
																				 struct mica_bkt **bkt_ptr, uint *tag,
																				 mica_op_t **kv_ptr, mica_kv_t *KVS)
{
	bkt[op_i] = op_key->bkt & KVS->bkt_mask;
	bkt_ptr[op_i] = &KVS->ht_index[bkt[op_i]];
//  printf("bkt %u \n", bkt[op_i]);
	__builtin_prefetch(bkt_ptr[op_i], 0, 0);
	tag[op_i] = op_key->tag;
	kv_ptr[op_i] = NULL;
}

// Locate the buckets for the requested keys

// Locate a kv_pair inside a bucket: used in a loop for all kv-pairs
static inline void KVS_locate_one_kv_pair(int op_i, uint *tag, struct mica_bkt **bkt_ptr,
																					mica_op_t **kv_ptr, mica_kv_t *KVS)
{
	for(uint8_t j = 0; j < 8; j++) {
		if(bkt_ptr[op_i]->slots[j].in_use == 1 &&
			 bkt_ptr[op_i]->slots[j].tag == tag[op_i] ) {


			uint64_t log_offset = bkt_ptr[op_i]->slots[j].offset &
														KVS->log_mask;
			/*
               * We can interpret the log entry as mica_op, even though it
               * may not contain the full MICA_MAX_VALUE value.
               */
			kv_ptr[op_i] = (mica_op_t *) &KVS->ht_log[log_offset];

			/* Small values (1--64 bytes) can span 2 kvs lines */
			__builtin_prefetch(kv_ptr[op_i], 0, 0);
			__builtin_prefetch((uint8_t *) kv_ptr[op_i] + 64, 0, 0);

			/* Detect if the head has wrapped around for this index entry */
			if(KVS->log_head - bkt_ptr[op_i]->slots[j].offset >= KVS->log_cap) {
				kv_ptr[op_i] = NULL;	/* If so, we mark it "not found" */
			}

			break;
		}
	}
}

static inline void KVS_check_key(mica_op_t *kv_ptr, mica_key_t opkey, uint32_t op_i)
{
	if (ENABLE_ASSERTIONS && kv_ptr == NULL) assert(false);
	bool key_found = memcmp(&kv_ptr->key, &opkey, KEY_SIZE) == 0;
	if (unlikely(ENABLE_ASSERTIONS && !key_found)) {
		my_printf(red, "Kvs miss %u\n", op_i);
		cust_print_key("Op", &opkey);
		cust_print_key("KV_ptr", &kv_ptr->key);
		assert(false);
	}
}

// After locating the buckets locate all kv pairs
static inline void KVS_locate_all_kv_pairs(uint16_t op_num, uint *tag, struct mica_bkt **bkt_ptr,
																					 mica_op_t **kv_ptr, mica_kv_t *KVS)
{
	for(uint16_t op_i = 0; op_i < op_num; op_i++) {
		KVS_locate_one_kv_pair(op_i, tag, bkt_ptr, kv_ptr, KVS);
	}
}



static inline void KVS_local_read(mica_op_t *kv_ptr,
                                  uint8_t *value_to_read,
                                  uint8_t *resp_type,
                                  uint16_t t_id)
{
  if (ENABLE_ASSERTIONS) {
    assert(value_to_read != NULL);
    assert(kv_ptr != NULL);
  }
  uint32_t debug_cntr = 0;
  uint64_t tmp_lock = read_seqlock_lock_free(&kv_ptr->seqlock);
  do {
    debug_stalling_on_lock(&debug_cntr, "local read", t_id);
    memcpy(value_to_read, kv_ptr->value, (size_t) VALUE_SIZE);
  } while (!(check_seqlock_lock_free(&kv_ptr->seqlock, &tmp_lock)));
	if (resp_type != NULL)
  	*resp_type = KVS_LOCAL_GET_SUCCESS;
}

static inline void KVS_write(mica_op_t *kv_ptr, uint8_t *value_to_write)
{
	lock_seqlock(&kv_ptr->seqlock);
		memcpy(kv_ptr->value, value_to_write, (size_t) VALUE_SIZE);
	unlock_seqlock(&kv_ptr->seqlock);
}

#endif
