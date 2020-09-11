//
// Created by vasilis on 11/05/20.
//

#ifndef KITE_CLIENT_IF_UTIL_H
#define KITE_CLIENT_IF_UTIL_H

#include "debug_util.h"

/*-------------------------------- CLIENT REQUEST ARRAY ----------------------------------------*/
// signal completion of a request to the client
void signal_completion_to_client(uint32_t sess_id,
                                 uint32_t req_array_i,
                                 uint16_t t_id);

// signal that the request is being processed to tne client
void signal_in_progress_to_client(uint32_t sess_id,
                                  uint32_t req_array_i,
                                  uint16_t t_id);

// Returns whether a certain request is active, i.e. if the client has issued a request in a slot
bool is_client_req_active(uint32_t sess_id,
                          uint32_t req_array_i,
                          uint16_t t_id);

// is any request of the client request array active
bool any_request_active(uint16_t sess_id, uint32_t req_array_i,
                        uint16_t t_id);

//
void fill_req_array_when_after_rmw(uint16_t sess_id, uint32_t req_array_i,
                                   uint8_t  opcode, uint8_t* value_to_read,
                                   bool rmw_is_successful, uint16_t t_id);

void fill_req_array_on_rmw_early_fail(uint32_t sess_id, uint8_t* value_to_read,
                                      uint32_t req_array_i, uint16_t t_id);


// Returns true if it's valid to pull a request for that session
bool pull_request_from_this_session(bool stalled,
                                    uint16_t sess_i,
                                    uint16_t t_id);





#endif //KITE_CLIENT_IF_UTIL_H
