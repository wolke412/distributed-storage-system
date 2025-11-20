#ifndef STATEMACHINE_H
#define STATEMACHINE_H


#include "server/server.h"
#include "fileserver/fs.h"




/**
 * PROCEDURES
 * ------------------------------------------------------------
 */
int xprocedure_send_request_fragment( Server *sv, Address *to , int file_id, int fragment_id, Address *deliver_to );

int xprocedure_send_fragment( Server *sv, xFileServer *fs, int file_id, int fragment_id, Address *deliver_to ) ;


#endif