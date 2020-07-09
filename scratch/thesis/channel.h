#define CHANNEL

#ifndef NODE
#include "node.h"
#endif // NODE

/**
* \brief calculates the channel betwen an AP and a UE
*
* gets the angle of incidence first, and checks if it is within FOV.
* Note: alpha (order of Lambertian emission) would be negative so I used abs().
* (but I have never seen any paper use abs() when calculating order of Lambertian emission...)
*
* \return the channel
*/
double calculate_one_channel(node* nodeAP, node* nodeUE);

/**
* \brief calculates all the channel between every AP-UE pair
* \param transmitter
* \param receiver
* \param channel passed as reference
*/
void calculate_all_channel();
