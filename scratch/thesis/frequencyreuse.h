#ifndef FREQUENCYREUSE
#define FREQUENCYREUSE

#include "node.h"

#include<list>

void save_fr_relationship();
void save_RB_assignment();

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

struct weighted_frNode{
    node* transmitter;
    std::list<weighted_frNode*> neighbors;
    int degree;
    double weight;
    weighted_frNode* importantNeighbor;

    weighted_frNode(){
        weight = 999999999;
        degree = 0.0;
        importantNeighbor = NULL;
    }
};

void weighted_frequency_reuse();
#endif // FREQUENCYREUSE
