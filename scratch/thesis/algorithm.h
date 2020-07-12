#include "channel.h"
#include "clustering.h"
#include "frequencyreuse.h"
#include "global_environment.h"
#include "node.h"
#include "resource_allocation.h"

class algorithm {
public:
    /** does the algorithm
    * \return double total time(ms) taken from clustering to RA
    */
    double fullAlgorithm();
};
