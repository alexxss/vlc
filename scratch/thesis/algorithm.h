#include "channel.h"
#include "clustering.h"
#include "frequencyreuse.h"
#include "global_environment.h"
#include "node.h"
#include "resource_allocation.h"

class algorithm {
public:
    /**
    mode = true: use proposed algo;
    mode = false: use reference algo
    */
    static bool RBmode;
    static bool TDMAmode;
    static bool RAmode;

    /** does the algorithm
    * \return double total time(ms) taken from clustering to RA
    */
    double fullAlgorithm();

    static double shannon;
};
