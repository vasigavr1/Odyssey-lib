//
// Created by vasilis on 11/05/20.
//

#ifndef KITE_INTERFACE_H
#define KITE_INTERFACE_H

#include <city.h>
#include "top.h"






/* ----------------------------------POLLING API-----------------------------------------------*/

// Blocking call
void poll_all_reqs(uint16_t session_id);

// returns whether it managed to poll a request
bool poll_a_req_async(uint16_t session_id, uint64_t target);

// returns after it managed to poll a request
void poll_a_req_blocking(uint16_t session_id, uint64_t target);

// Blocks until it can poll one request. Useful when you need to issue a request
void poll_one_req_blocking(uint16_t session_id);

bool is_polled(uint16_t session_id, uint64_t target);

/* --------------------------------------------------------------------------------------
 * ----------------------------------ASYNC API----------------------------------------
 * --------------------------------------------------------------------------------------*/

/*------------------------STRONG------------------------------------------------------*/
// Async strong functions will block until the request is *issued* (i.e. block polling for a free slot)

//
int async_read_strong(uint32_t key_id, uint8_t *value_to_read,
                      uint32_t val_len, uint16_t session_id);

//
int async_write_strong(uint32_t key_id, uint8_t *value_to_write,
                       uint32_t val_len, uint16_t session_id);

//
int async_acquire_strong(uint32_t key_id, uint8_t *value_to_read,
                         uint32_t val_len, uint16_t session_id);

//
int async_release_strong(uint32_t key_id, uint8_t *value_to_write,
                         uint32_t val_len, uint16_t session_id);

//
int async_cas_strong(uint32_t key_id, uint8_t *expected_val,
                     uint8_t *desired_val, uint32_t val_len,
                     bool *cas_result, bool rmw_is_weak,
                     uint16_t session_id);

//
int async_faa_strong(uint32_t key_id, uint8_t *value_to_read,
                     uint8_t *argument_val, uint32_t val_len,
                     uint16_t session_id);
/*------------------------WEAK------------------------------------------------------*/
// Async weak functions will not block, but may return that the request was not issued

//
int async_read_weak(uint32_t key_id, uint8_t *value_to_read,
                    uint32_t val_len, uint16_t session_id);

//
int async_write_weak(uint32_t key_id, uint8_t *value_to_write,
                     uint32_t val_len, uint16_t session_id);

//
int async_acquire_weak(uint32_t key_id, uint8_t *value_to_read,
                       uint32_t val_len, uint16_t session_id);

//
int async_release_weak(uint32_t key_id, uint8_t *value_to_write,
                       uint32_t val_len, uint16_t session_id);

//
int async_cas_weak(uint32_t key_id, uint8_t *expected_val,
                   uint8_t *desired_val, uint32_t val_len,
                   bool *cas_result, bool rmw_is_weak,
                   uint16_t session_id);

//
int async_faa_weak(uint32_t key_id, uint8_t *value_to_read,
                   uint8_t *argument_val, uint32_t val_len,
                   uint16_t session_id);



/* --------------------------------------------------------------------------------------
 * ----------------------------------BLOCKING API----------------------------------------
 * --------------------------------------------------------------------------------------*/

//
int blocking_read(uint32_t key_id, uint8_t *value_to_read,
                  uint32_t val_len, uint16_t session_id);

//
int blocking_write(uint32_t key_id, uint8_t *value_to_write,
                   uint32_t val_len, uint16_t session_id);

//
int blocking_acquire(uint32_t key_id, uint8_t *value_to_read,
                     uint32_t val_len, uint16_t session_id);

//
int blocking_release(uint32_t key_id, uint8_t *value_to_write,
                     uint32_t val_len, uint16_t session_id);

//
int blocking_cas(uint32_t key_id, uint8_t *expected_val,
                 uint8_t *desired_val, uint32_t val_len,
                 bool *cas_result, bool rmw_is_weak,
                 uint16_t session_id);

//
int blocking_faa(uint32_t key_id, uint8_t *value_to_read,
                 uint8_t *argument_val, uint32_t val_len,
                 uint16_t session_id);


/* ------------------------------------------------------------------------------------------------------------------- */
/* ------------------------------------USER INTERFACE----------------------------------------------------------------- */
/* ------------------------------------------------------------------------------------------------------------------- */
// allow the user to check the API from the console
void user_interface();




#endif //KITE_INTERFACE_H
