#define CLUSTERING

#ifndef NODE
#include "node.h"
#endif // NODE
#ifndef ENVIRONMENT
#include "global_environment.h"
#endif // ENVIRONMENT

/*
UE_send_requests and AP_send_requests are currently unused :P
*/
void UE_send_requests();
void AP_send_requests();

/**
* \brief connects AP and UE if requests are sent and received by each other
* \param transmitter array of transmitters
* \param receiver array of receivers
*/
void both_sides_connect();
