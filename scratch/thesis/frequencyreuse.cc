#ifndef FREQUENCYREUSE
#include "frequencyreuse.h"
#endif // FREQUENCYREUSE

#include<list>
#include<fstream>
#include<iostream>
#include<algorithm> // for sort
#include<random> // for random.. duh

/**
z[i][j] = true means AP i and AP j associate to at least one same UE.
z[i][j] = false means otherwise.
z[i][i] is undefined. pls dont use.
*/
bool z[g_AP_number][g_AP_number]={false};

/* print z[][] array */
void print(){
    for(int i=0; i<g_AP_number;i++){
        for(int j=0; j<g_AP_number;j++){
            //std::cout<<z[i][j]<<' ';
        }
        //std::cout<<std::endl;
    }
}

/* print overlapped APs of each UE */
void print(const std::list<int> &overlapped_APs){
    auto it = overlapped_APs.begin();
    while(it!=overlapped_APs.end()){
        //std::cout<<*it++<<' ';
    }
}

/* print transmitter id & degree in graph */
void print(const std::list<fr_node*> graph_nodes){
    //std::cout<<"AP nodes in FR graph: ";
    auto it = graph_nodes.begin();
    while(it!=graph_nodes.end()){
        //std::cout<<std::endl<<(*it)->transmitter->id<<' '<<(*it)->degree;
        it++;
    }
    //std::cout<<std::endl;
}

/* save frequency reuse data to file */
void save_fr_relationship(node* transmitter[]){
    #ifdef DEBUG
    std::cout<<"Overlapping nodes relationship saved at FR_graph.dat\n";
    #endif // DEBUG
    std::ofstream fout("FR_graph.dat");
    if(fout.is_open()){
        for(int i=0;i<g_AP_number;i++){
            for(int j=i+1;j<g_AP_number;j++){
                if (z[i][j]==false) continue;
                fout<<transmitter[i]->location.first<<' '<<transmitter[i]->location.second<<std::endl;
                fout<<transmitter[j]->location.first<<' '<<transmitter[j]->location.second<<std::endl<<std::endl;
            }
        }
        fout.close();
    }
}

/* save RB assignment data to file */
void save_RB_assignment(node* transmitter[g_AP_number]){
    #ifdef DEBUG
    std::cout<<"Resource block assignment saved at RB_assignment.dat\n";
    #endif // DEBUG
    std::ofstream fout("RB_assignment.dat");
    if(fout.is_open()){
        for(int i=0; i<g_AP_number; i++){
            fout<<transmitter[i]->location.first<<' '<<transmitter[i]->location.second<<' '<<transmitter[i]->get_resource_block()<<std::endl;
        }
        fout.close();
    }
}

void set_relationship_true(const std::list<int> &overlapped_APs){
    // std::cout<<"setting overlapped APs: ";
    //print(overlapped_APs);
    //std::cout<<std::endl;

    for(int i : overlapped_APs){
        for(int j : overlapped_APs){
            if (i==j || z[i][j]) continue;
            else z[i][j]=true;
        }
    }
}

void construct_graph(std::list<fr_node*> graph_nodes){
    /* for all UE, set all of its connected APs as overlapped */
    //std::cout<<"Setting overlapped APs relationship in z[][]..."<<std::endl;
    for(int i=0; i<node::UE_number;i++){
        set_relationship_true(node::receiver[i]->get_connected());
    }
    /* at here, relationship between APs (z[][]) should be OK. */
    // print(); //print z[][]

    /* update degree and neighbours of graph nodes according to z[][] */
    //std::cout<<"Updating graph nodes..."<<std::endl;
    for(fr_node* curNode : graph_nodes){
        int id = curNode->transmitter->id;
        curNode->degree = 0;
        auto it = graph_nodes.begin();
        for(int i=0; i<g_AP_number; i++){
            if(z[id][i]==true){
                curNode->degree ++;
                curNode->neighbors.push_back(*it);
            }
            it++;
        }
    }
}

bool compare_fr_node(const fr_node* nodeA, const fr_node* nodeB){
    return (nodeA->degree > nodeB->degree);
}

std::list<int> fr_node::get_rb_candidate(){
    bool rb_occupied[g_frequency_reuse_factor] = {false};
    std::list<int> rb_candidate;
    for(int i=0; i<g_frequency_reuse_factor; i++) rb_candidate.push_back(i);
    for(fr_node* n : this->neighbors){
        int rb = n->transmitter->get_resource_block();
        if (rb!=-1 && rb_occupied[rb]==false){
            rb_occupied[rb]=true;
            rb_candidate.remove(rb);
            if (rb_candidate.size()==0) {
                //std::cout<<this->transmitter->id<<"'s neighbors occupied all RBs"<<std::endl<<"-- ";
                return rb_candidate;
            }
        }
    }
    return rb_candidate;
}

bool fr_node::if_RB_repeat(const int& rb_id){
    if(this->degree==0) return false;

    for(fr_node* n : this->neighbors){
        if(n->transmitter->get_resource_block()==rb_id){
            //std::cout<<n->transmitter->id<<" has already occupied RB #"<<rb_id<<std::endl<<"-- ";
            return true;
        }
    }
    return false;
}

void assign_rb(std::list<fr_node*> graph_nodes){

    //print(graph_nodes);
    //std::cout<<"Sorting graph nodes by degree..."<<std::endl;
    graph_nodes.sort(compare_fr_node);

    //print(graph_nodes);

    /* first g_frequency_reuse_factor nodes get assigned RBs first */
    int reuse_factor=0;
    auto it = graph_nodes.begin();
    while(it!=graph_nodes.end()){
        if(reuse_factor >= g_frequency_reuse_factor) break;
        //std::cout<<(*it)->transmitter->id<<" gets RB #"<<reuse_factor<<std::endl;
        (*it++)->transmitter->set_resource_block(reuse_factor++);
    }

    /* remaining nodes rand() for non-repeating RB */
    std::uniform_int_distribution<int> unif(0,g_frequency_reuse_factor-1);
    std::default_random_engine re; // TODO (alex#2#): remember to seed
    while(it!=graph_nodes.end()){
        int rb_id = unif(re);
        std::list<int> rb_candidate = (*it)->get_rb_candidate();
        /* if no valid candidate, there is bound to repeat, no need to check/rand() for non-repeating RB */
        if (rb_candidate.size() > 0) {
              /* check if repeat, rand() until no longer while */
//            std::uniform_int_distribution<int>::param_type parm = std::uniform_int_distribution<int>::param_type(0,rb_candidate.size());
//            unif.param(parm);
            //std::cout<<(*it)->transmitter->id<<" has to check for repeating RB"<<std::endl<<"-- ";
            while((*it)->if_RB_repeat(rb_id)) rb_id = unif(re);
        }
        (*it)->transmitter->set_resource_block(rb_id);
        //std::cout<<(*it)->transmitter->id<<" gets RB #"<<rb_id<<std::endl;
        it++;
    }
}

void frequency_reuse(node* transmitter[g_AP_number]){
    #ifdef DEBUG
    std::cout<<"Build graph...\n";
    #endif // DEBUG
    std::list<fr_node*> graph_nodes;
    for(int i=0; i<g_AP_number; i++){
        fr_node* tmpNode = new fr_node;
        tmpNode->transmitter = node::transmitter[i];
        graph_nodes.push_back(tmpNode);
    }

    construct_graph(graph_nodes);

    save_fr_relationship(transmitter);

    #ifdef DEBUG
    std::cout<<"Assign RB...\n";
    #endif // DEBUG
    assign_rb(graph_nodes);

    save_RB_assignment(transmitter);

    for(fr_node* n : graph_nodes) delete n;
}
