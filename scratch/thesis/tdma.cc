#include "node.h"
#include <vector>
#include <list>
#include <iostream>
#include <algorithm> // random_shuffle

/*---------scheduling----------------
1. attempt to support K UEs via RA;
   - K=min(|remaining UE|, Gamma)
2. successful UEs removed from pool;
3. if no successful UEs, exit;
4. if no remaing UEs, exit;
5. repeat from 1.
-------------------------------------*/

void node::tdma_scheduling(){
    this->servedUE_cnt = 0;
    std::list<std::list<int>> schedule;
    /* copy connected UEs into pool */
    std::list<int> pool;
    for(int i : this->get_connected()) pool.push_back(i);

    //std::uniform_int_distribution<int> unif(0,pool.size());

    while(!pool.empty()){
        int K = pool.size() < g_maximum_load_per_AP ? pool.size() : g_maximum_load_per_AP;

        /* rand select K UEs from pool */
        //std::cout<<"-- Selecting "<<K<<" UEs from pool { "; for(int i:pool) std::cout<<i<<' '; std::cout<<"}\n";

        // copy pool into candidate
        std::vector<int> candidate;
        for(int i:pool)candidate.push_back(i);
        // shuffle candidate
        std::random_shuffle(candidate.begin(),candidate.end());
        // drop candidate items after K
        candidate.erase(candidate.begin()+K,candidate.end());
        #ifdef DEBUG
        std::cout<<"-- Candidate: "; for(int i:candidate)std::cout<<i<<' '; std::cout<<'\n';
        #endif // DEBUG

        /* try to RA */
        std::list<int> accepted = this->dynamic_resource_allocation(candidate);
        //std::cout<<"-- Accepted: "; for(int i:accepted)std::cout<<i<<' '; std::cout<<'\n';

        /* remove successful UE from pool */
        if (accepted.empty()) break;
        for(int i:accepted) pool.remove(i);

        /* store successful UE in this time slot */
        schedule.push_back(accepted);
        this->servedUE_cnt += accepted.size();
    }
    this->time_slot_schedule = schedule;
    /* drop the UEs remaining in pool */
    for(int UE_id : pool){
        this->dropRelationship(UE_id);
        node::receiver[UE_id]->dropRelationship(this->id);
        #ifdef DEBUG
        std::cout<<"UE "<<UE_id<<" dropped from AP "<<this->id<<"\'s cluster.\n";
        #endif // DEBUG
    }
}

/*-----------time ra-----------------
- if no unsupported UE:
  t_g = time_slot_UE_ratio * T;
- else:
  t_g = (time_slot_UE_ratio + time_slot_UE_ratio*rejected_UE_ratio) * T;
-------------------------------------*/
void node::tdma_time_allocation(bool smartMode){
    if (smartMode) {
        for (std::list<int> slot : this->time_slot_schedule){
            if (this->servedUE_cnt==0) {
                #ifdef DEBUG
                std::cout<<"There\'s no served UEs, but there is time slot? \n";
                #endif // DEBUG
                system("pause");
            }
            double t_g = g_total_time * slot.size() / this->servedUE_cnt;
            this->time_allocation.push_back(t_g);
        }
    } else {
        this->time_allocation.clear();
        int servedUE_cnt = this->servedUE_cnt, connectedUE_cnt = (this->get_connected()).size();

        if (connectedUE_cnt==0) {
            //std::cout<<"-- no clustered UE.\n";
            return;
        }
        if (servedUE_cnt==0) {
            //std::cout<<"-- no served UE.\n";
            return;
        }

        if (servedUE_cnt == connectedUE_cnt){
            /* no rejected UE */
            //std::cout<<"-- all UEs accepted\n";
            for(std::list<int> slot:this->time_slot_schedule){
                double t_g = (double)slot.size()/connectedUE_cnt * g_total_time;
                this->time_allocation.push_back(t_g);
            }
        } else if (servedUE_cnt < connectedUE_cnt){
            /* has rejected UE */
            int rejectedUE_cnt = connectedUE_cnt - servedUE_cnt;
            //std::cout<<"-- rejected "<< rejectedUE_cnt <<" UEs\n";
            for(std::list<int> slot:this->time_slot_schedule){
                double t_g = ((double)slot.size()/connectedUE_cnt) + ((double)rejectedUE_cnt * slot.size())/(connectedUE_cnt * servedUE_cnt);
                t_g = t_g * g_total_time;
                this->time_allocation.push_back(t_g);
            }
        } else {
            #ifdef DEBUG
            std::cout<<"-- served UE > clustered UE! impossible. something is wrong.\n";
            #endif // DEBUG
            exit(1);
        }
    }
}

void node::tdma(){
    #ifdef DEBUG
    std::cout<<"\nTDMA for transmitter "<<this->id<<std::endl;
    #endif // DEBUG
    this->tdma_scheduling();
    #ifdef DEBUG
    this->printme(2); // print schedule
    #endif
    this->tdma_time_allocation(true);
    #ifdef DEBUG
    this->printme(3); // print allocated time resource
    #endif // DEBUG
}
