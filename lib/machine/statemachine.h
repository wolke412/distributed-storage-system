#ifndef STATEMACHINE_H
#define STATEMACHINE_H


#include "../server/server.h"
#include "../fileserver/fs.h"


void server_healthcheck(Server *sv);



node_id_t xprocedure_wait_identification(Server *sv, int c);

/**
 * PROCEDURES
 * ------------------------------------------------------------
 */


void xprocedure_check_peer_b(Server *sv, xFileInNetwork *fni); 

int xprocedure_send_request_fragment( Server *sv, Address *to , int file_id, int fragment_id, Address *deliver_to );

int xprocedure_send_use_local( Server *sv, int fragment_id, Address *deliver_to ) ;

int xprocedure_send_fragment( Server *sv, xFileServer *fs, int file_id, int fragment_id, Address *deliver_to ) ;




int xprocedure_save_file_to_index( Server *sv, xFileServer *fs, xFileNetworkIndex *fnetidx, xReportFileKnowledge *r, int c);

int xprocedure_peer_died( Server *sv ) ;

#endif
