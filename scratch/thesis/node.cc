#ifndef NODE
#include "node.h"
#endif // NODE
#ifndef ENVIRONMENT
#include "global_environment.h"
#endif // ENVIRONMENT

#include <iostream>
#include <math.h>
#include <random>
#include <algorithm> // std::sort
#include <chrono> // seed

node::node(int id){
    this->id = id;
    double x = ((id%g_AP_per_row)+1)*(g_room_dim/(g_AP_per_row+1));
    double y = (g_AP_per_row-(id/g_AP_per_row))*(g_room_dim/(g_AP_per_row+1));
    this->location = std::pair<double,double>(x,y);
    this->resource_block_id = -1;
    this->sum_throughput = 0.0;
    /* generate random minimum rate */
    std::uniform_real_distribution<double> unif(0.0, g_R_min);
    std::default_random_engine re(std::chrono::system_clock::now().time_since_epoch().count());
    this->min_required_rate = unif(re);
}

node::node(int id, double x, double y){
    this->id = id;
    this->location = std::pair<double,double>(x,y);
    this->resource_block_id = -1;
    this->sum_throughput = 0.0;
    /* generate random minimum rate */
    std::uniform_real_distribution<double> unif(0.0, g_R_min);
    std::default_random_engine re(std::chrono::system_clock::now().time_since_epoch().count());
    this->min_required_rate = unif(re);
}

void node::receiveRequest(int srcNodeId){
    this->receivedRequests.push_back(srcNodeId);
}

double node::plane_distance(const node* nodeB){
    double x = this->location.first - nodeB->location.first;
    double y = this->location.second - nodeB->location.second;
    double dist = std::pow(x,2) + std::pow(y,2);
    dist = std::sqrt(dist);
    return dist;
}

double node::euclidean_distance(const node* nodeB){
    const double height_diff = g_AP_height - g_UE_height;
    const double plane_diff = this->plane_distance(nodeB);
    return std::sqrt(std::pow(height_diff,2)+std::pow(plane_diff,2));
}

bool node::hasRequest(const int srcNodeId){
    auto it = this->receivedRequests.begin();
    while(it != this->receivedRequests.end())
        if (*it++ == srcNodeId) return true;
    return false;
}

void node::connect(const int srcNode){
    this->receivedRequests.remove(srcNode);
    this->connected.push_back(srcNode);
    this->OnOff = true;
}

std::list<int> node::get_connected(){
    return this->connected;
}

struct UE_compare_channel_desc{
    UE_compare_channel_desc(const int& AP_id){this->AP_id = AP_id;}
    bool operator() (const int& UE_a, const int& UE_b) {
        const double& channel_AP_a = node::channel[AP_id][UE_a];
        const double& channel_AP_b = node::channel[AP_id][UE_b];
        return channel_AP_a >= channel_AP_b;
    }
    int AP_id;
};

void node::NOMA_sort_UE_desc(){
    std::list<int> UE_list = this->get_connected(); // copies list of all connected UEs
    UE_list.sort(UE_compare_channel_desc(this->id));
    this->sorted_UE = UE_list;
}

int node::get_sorting_order (const int& UE_id) const {
    std::list<int> sortedUEList = this->sorted_UE;
    std::list<int>::iterator it = std::find(sortedUEList.begin(),sortedUEList.end(),UE_id);
    if (it==sortedUEList.end()) {
        return -1; // UE_id is not in the sorted_UE list!!!
    }
    int order = std::distance(sortedUEList.begin(),it);
    return order;
}

void node::set_resource_block(const int& rb_id){
    this->resource_block_id = rb_id;
}

int node::get_resource_block(){
    return this->resource_block_id;
}

//void node::tdma_scheduling(){}

//void node::tdma_time_allocation(){}

//void node::tdma(){}

/** @brief (one liner)
*
* (documentation goes here)
*/
void node::dropRelationship(const int& nodeId)
{
    this->connected.remove(nodeId);
    if (this->connected.size()==0)
        this->OnOff = false;
}

double node::calculateSINR(const int& UEid, const double& myPwr, const double& prevUEpwr){
    int APid = this->id;
    double signal = myPwr * node::channel[APid][UEid] * node::channel[APid][UEid];
    double IntraCI = prevUEpwr * node::channel[APid][UEid] * node::channel[APid][UEid];
    double AWGN = g_total_bandwidth * g_N_0;
    double ICI = 0.0;
    for(int f=0;f<g_AP_number;f++){
        if (f==APid) continue;
        if (this->get_resource_block() != node::transmitter[f]->get_resource_block()) continue;
        ICI += node::transmitter[f]->calculate_ICI(UEid);
    }
    double SINR = signal / (IntraCI + AWGN + ICI);
    return SINR;
}

void node::calculateSINR(){
    for (std::list<int> slot : this->time_slot_schedule){
        double prevUEpwr = 0.0;
        for(int UEid : slot){
            // find UEid's allocated power
            double pwr = 0.0;
            for (UE_scheme sch : this->mod_scheme_assignment) {
                if (sch.UE_id==UEid) {
                    pwr = sch.modulation_scheme->required_power;
                    break;
                }
            }

            double SINR = this->calculateSINR(UEid, pwr, prevUEpwr);
            node::SINR[this->id][UEid] = SINR;

            prevUEpwr += pwr;
        }
    }
}

double node::getRequiredPower(const int& UEid){
    for (UE_scheme sch : this->mod_scheme_assignment) {
        if (sch.UE_id==UEid) {
            return sch.modulation_scheme->required_power;
        }
    }
    return 0.0;
}

double node::getAchievableRate(const int& UEid){
    for (UE_scheme sch : this->mod_scheme_assignment){
        if (sch.UE_id==UEid){
            return sch.modulation_scheme->sum_throughput;
        }
    }
    return 0.0;
}

bool node::getOnOff () {
    return this->OnOff;
}

void node::fakesend(node* destNode){
    std::cout<<this->id<<" send to "<<destNode->id<<std::endl;
    destNode->fakereceive(this);
}

void node::fakereceive(node* srcNodeId){
    std::cout<<this->id<<" received from "<<srcNodeId->id<<std::endl;
}

void node::printme(){
    if (this->connected.empty()==true) {
        std::cout<<"node "<<this->id<<" received: ";
        auto it = this->receivedRequests.begin();
        while(it!=this->receivedRequests.end())
            std::cout<<*it++<<" ";
        std::cout<<std::endl;
    }else{
        std::cout<<"node "<<this->id<<" connected: ";
        auto it = this->connected.begin();
        while(it!=this->connected.end())
            std::cout<<*it++<<" ";
        std::cout<<std::endl;
    }
}

void node::printme(const int mode){
    switch(mode){
    case 0:
    {
        std::cout<<"node "<<this->id<<" received: ";
        auto it = this->receivedRequests.begin();
        while(it!=this->receivedRequests.end())
            std::cout<<*it++<<" ";
        std::cout<<std::endl;
        break;
    }
    case 1:
    {
        std::cout<<"node "<<this->id<<" connected: ";
        auto it = this->connected.begin();
        while(it!=this->connected.end())
            std::cout<<*it++<<" ";
        std::cout<<std::endl;
        break;
    }
    case 2:
    {
        std::cout<<"node "<<this->id<<" time slot schedule: ";
        for(std::list<int> slot : this->time_slot_schedule){
            std::cout<<"( ";
            for(int i:slot) std::cout<<i<<' ';
            std::cout<<") ";
        }
        std::cout<<std::endl;
        break;
    }
    case 3:
    {
        std::cout<<"node "<<this->id<<" time resource: ";
        for(double i:this->time_allocation) std::cout<<i<<' ';
        std::cout<<std::endl;
        break;
    }
    default:
        std::cout<<"Should not have arrived here!!"<<std::endl;
    }
}
