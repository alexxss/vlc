#define FREQUENCYREUSE
#ifndef NODE
#include "node.h"
#endif // NODE

#include<list>

/*----------------------------------------
1. make graph
2. sort node according to degree
3. assign RBs to highest degree nodes
4. remaining nodes rand() a RB that is different from neighbor nodes
-----------------------------------------*/

struct fr_node{
    node* transmitter;
    std::list<fr_node*> neighbors;
    int degree;
    bool if_RB_repeat(const int& rb_id);
    std::list<int> get_rb_candidate();
};

void frequency_reuse(node* transmitter[g_AP_number]);
