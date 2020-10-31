#include "frequencyreuse.h"

#include<iostream>
#include<random> // for random
#include<chrono> // for seed

double wz[g_AP_number][g_AP_number] = {-1};

/* print wz[][] array */
void printwz(){
    for(int i=0; i<g_AP_number;i++){
        for(int j=0; j<g_AP_number;j++){
            std::cout<<wz[i][j]<<' ';
        }
        std::cout<<std::endl;
    }
}

/** overlap severity SV of UE k between AP n & f */
/** if INRkn>0 and INRkf>0, SV = (|Dkn - Dkf| + 1) * |INRkn - INRkf|/(INRkn + INRkf) */
/** else                  , SV = -1 */
double calculate_OverlapSeverity(const int& n, const int& f, const int& k){
    node* nAP = node::transmitter[n];
    node* fAP = node::transmitter[f];
    node* kUE = node::receiver[k];
    double distDiff = std::abs(nAP->euclidean_distance(kUE) - fAP->euclidean_distance(kUE)) + 1;
    std::cout<< nAP->get_sorting_order(k)<<'/'<<nAP->get_connected().size()<<',';
    double INRkn = ((double) nAP->get_sorting_order(k) ) / nAP->get_connected().size();
    double INRkf = ((double) fAP->get_sorting_order(k) ) / fAP->get_connected().size();
    ///////////////// sanity check ///////////////
    if (INRkn==-1 || INRkf==-1) {
        std::cout<<"Why is INR -1??? Abort.\n";
        exit(1);
    }
    //////////////////////////////////////////////
    double INRdiff = std::abs(INRkn - INRkf);
    double INRadd = INRkn + INRkf;

    if ((INRkn and INRkf) == 0) return  -1;
    return distDiff * (INRdiff + 1) / (INRadd + 1);
}

void set_relationship_weight(const int& UEid, const std::list<int> &overlapped_APs){
    for(int nAP : overlapped_APs){
        for(int fAP : overlapped_APs){
            if (nAP!=fAP) {
                double overlapSeverity = calculate_OverlapSeverity(nAP, fAP, UEid);
                if (overlapSeverity == -1) continue;
                if (wz[nAP][fAP]==-1) wz[nAP][fAP] = overlapSeverity;
                else wz[nAP][fAP] = std::min(wz[nAP][fAP], overlapSeverity);
                if (wz[fAP][nAP]==-1) wz[fAP][nAP] = overlapSeverity;
                else wz[fAP][nAP] = std::min(wz[fAP][nAP], overlapSeverity);
            }
        }
    }
}

void construct_weighted_graph(std::list<weighted_frNode*> graph_nodes){
    /* for all UE, set all of its connected APs as overlapped */
    //std::cout<<"Setting overlapped APs relationship in z[][]..."<<std::endl;
    for(int i=0; i<node::UE_number;i++){
        set_relationship_weight(i, node::receiver[i]->get_connected());
    }
    /* at here, relationship between APs (z[][]) should be OK. */
     printwz(); //print z[][]

    /* update degree and neighbours of graph nodes according to z[][] */
    //std::cout<<"Updating graph nodes..."<<std::endl;
    for(weighted_frNode* curNode : graph_nodes){
        int nAP = curNode->transmitter->id;
        for (weighted_frNode* otherNode : graph_nodes){
            int fAP = otherNode->transmitter->id;
            if (wz[nAP][fAP]>=0){
                curNode->degree++;
                curNode->neighbors.push_back(otherNode);
                if (wz[nAP][fAP]<curNode->weight){
                    curNode->weight = wz[nAP][fAP];
                    curNode->importantNeighbor = otherNode;
                }
            }
        }
    }
}

bool compare_fr_node(const weighted_frNode* nodeA, const weighted_frNode* nodeB){
    if (nodeA->degree != nodeB->degree)
        return (nodeA->degree > nodeB->degree);
    else
        return (nodeA->weight < nodeB->weight);
}

int giveRB(weighted_frNode* curNode){
    std::uniform_int_distribution<int> unif(0,g_frequency_reuse_factor-1);
    std::default_random_engine re(std::chrono::system_clock::now().time_since_epoch().count());
    // check neighbour rbs
    bool availability[g_frequency_reuse_factor] = {true}; // true = unused, false=used
    for(weighted_frNode* otherNode : curNode->neighbors ){
        int neighborRb = otherNode->transmitter->get_resource_block();
        if (neighborRb!=-1) availability[neighborRb] = false;
    }
    for(int rb = 0; rb<g_frequency_reuse_factor;rb++){
        if (availability[rb]) {// has available rb, rand() until ok
            int myRB = unif(re);
            while(!availability[myRB]) myRB=unif(re);
            return myRB;
        }
    }
    // no more available rb, just rand() one for it
    return unif(re);
}

void weighted_frequency_reuse(){
    // initialize wz
    for(int i=0;i<g_AP_number;i++)
        for(int j=0;j<g_AP_number;j++)
            wz[i][j]=-1;

    // make Graph Node for each AP
    std::list<weighted_frNode*> graph_nodes;
    for(int i=0; i<g_AP_number; i++){
        weighted_frNode* tmpNode = new weighted_frNode();
        tmpNode->transmitter = node::transmitter[i];
        graph_nodes.push_back(tmpNode);
    }
    // construct Graph:
    // 1. build (weight) adjacency matrix z
    // 2. give Graph Nodes weight
    construct_weighted_graph(graph_nodes);

    save_fr_relationship();

    // 3. sort Graph Nodes by degree and weight
    graph_nodes.sort(compare_fr_node);
    // 4. rand RB

    std::list<weighted_frNode*>::iterator it = graph_nodes.begin();
    while(it != graph_nodes.end()){
        // give rb to this node
        weighted_frNode* curNode = *it;
        int myRB = giveRB(curNode);
        curNode->transmitter->set_resource_block(myRB);
        // peek next, if same degree then give RB to this node's importantNeighbor
        it++;
        if (it==graph_nodes.end()) break;
        if ((*it)->degree == curNode->degree){
            weighted_frNode* nextNode = curNode->importantNeighbor;
            if (nextNode==NULL) continue;
            myRB = giveRB(nextNode);
            nextNode->transmitter->set_resource_block(myRB);
        }
    }

    save_RB_assignment();

    for(weighted_frNode* n : graph_nodes) delete n;
}
