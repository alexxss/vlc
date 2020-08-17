#include "node.h"
#include <vector>
#include <list>
#include <iostream>
#include <algorithm> // random_shuffle
#include <map>
#include "algorithm.h"

/** used to sort candidate in ASCENDING order of their NOMA order... */
struct sortByMinPower{
    sortByMinPower(const int& AP_id){AP=node::transmitter[AP_id];};
    const node* AP;
    bool operator() (const int& a, const int& b){
//        return AP->get_sorting_order(a) <= AP->get_sorting_order(b);
        int APid = AP->id;
        return minPower(APid, a) <= minPower(APid,b);
    }
};

void prune(const int& APid){
    std::cout<<"Pruning AP "<<APid<<"...\n";
    for(int UEid : node::transmitter[APid]->get_connected()){
        std::cout<<"Minimum required power to serve UE "<<UEid<<" is "<< minPower(APid,UEid)<<"\n";
        if (minPower(APid,UEid) > g_P_max){
            node::transmitter[APid]->dropRelationship(UEid);
            node::receiver[UEid]->dropRelationship(APid);
            #ifdef DEBUG
            std::cout<<", UE "<<UEid<<" dropped from AP "<<APid<<"\'s cluster.\n";
            #endif // DEBUG
        }
    }
    std::cout<<"Done.\n";
}

std::vector<int> chooseCandidate(const std::list<int> &pool, const int &K, const int& APid, const bool TDMAmode, int paramX){
    std::vector<int> candidate;
    if (!TDMAmode){
    /*----------------------multiple access method----------------------------------------*/
        for(int i:pool)candidate.push_back(i);
        // shuffle candidate
        std::random_shuffle(candidate.begin(),candidate.end());
        // drop candidate items after K
        candidate.erase(candidate.begin()+K,candidate.end());
    } else {
    /*--------------------------my method--------------------------------------------------*/
        for(int i:pool)candidate.push_back(i);
        // pick guaranteed admission
        //1. sort by channel gain
        std::cout<<"Sort...\n";
        std::sort(candidate.begin(),candidate.end(),sortByMinPower(APid));
        std::cout<<"Sort OK, now: ";
        for(int i:candidate)std::cout<<i<<' '; std::cout<<"\n";
        // fill in remaining seats by random sel
        //1. shuffle from X+1 to |candidate|
        if (paramX<0) {
            std::cout<<"Something is wrong with param X: "<<paramX<<" Abort.\n";
            exit(1);
        }
        std::random_shuffle(candidate.begin()+std::min(paramX,K), candidate.end());
        std::cout<<"Shuffle OK\n";
        //2. drop candidate items after K
        candidate.erase(candidate.begin()+K, candidate.end());
        std::cout<<"Erase OK\n";
    }

    return candidate;
}

/*---------scheduling----------------
1. attempt to support K UEs via RA;
   - K=min(|remaining UE|, Gamma)
2. successful UEs removed from pool;
3. if no successful UEs, exit;
4. if no remaing UEs, exit;
5. repeat from 1.
-------------------------------------*/

void node::tdma_scheduling(bool TDMAmode, bool RAmode, int paramX){
    this->servedUE_cnt = 0;
    std::list<std::list<int>> schedule;

    if (TDMAmode || RAmode) {
        prune(this->id);
    }

    /* copy connected UEs into pool */
    std::list<int> pool;
    std::map<int,int> checked;
    int checkedCount = 0;
    for(int i : this->get_connected()) {
        pool.push_back(i);
        checked.insert(std::pair<int,bool>(i,0));
    }

    //std::uniform_int_distribution<int> unif(0,pool.size());

    while(!pool.empty()){
        int K = pool.size() < g_maximum_load_per_AP ? pool.size() : g_maximum_load_per_AP;

        /* rand select K UEs from pool */
        //std::cout<<"-- Selecting "<<K<<" UEs from pool { "; for(int i:pool) std::cout<<i<<' '; std::cout<<"}\n";

        // determine candidate
        std::vector<int> candidate = chooseCandidate(pool,K,this->id,TDMAmode,paramX);
        #ifdef DEBUG
        std::cout<<"-- Candidate: "; for(int i:candidate)std::cout<<i<<' '; std::cout<<'\n';
        #endif // DEBUG

        /* try to RA */
        std::list<int> accepted;
        if (RAmode)
            accepted = this->heuristic_resource_allocation(candidate);
        else
            accepted = this->dynamic_resource_allocation(candidate);

        // mark candidates that have been checked
        for(int i : candidate) {
            if (checked[i]==0) {
                checkedCount++;
            }
            checked[i]++;
            if (checked[i]==3 && (find(accepted.begin(),accepted.end(),i) == accepted.end())){
                std::cout<<"UE "<<i<<" selected third time and failed. Dropped from pool and AP.\n";
                pool.remove(i);
                this->dropRelationship(i);
                node::transmitter[i]->dropRelationship(this->id);
                algorithm::poolDropTriggerCnt++;
            }
        }

        //std::cout<<"-- Accepted: "; for(int i:accepted)std::cout<<i<<' '; std::cout<<'\n';

        /* remove successful UE from pool */
        if (accepted.empty() && checkedCount==this->get_connected().size())
            break;
        else if (accepted.empty()){
            std::cout<<"All failed but there are unchecked, continue...\n";
            algorithm::tdmaOldBreakTriggerCnt++;
        }
        else if (!accepted.empty()){
            for(int i:accepted) pool.remove(i);

            /* store successful UE in this time slot */
            schedule.push_back(accepted);
            this->servedUE_cnt += accepted.size();
        }
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
                char c = getchar();
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

void node::tdma(bool TDMAmode, bool RAmode, int paramX){
    #ifdef DEBUG
    std::cout<<"\nTDMA for transmitter "<<this->id<<std::endl;
    #endif // DEBUG

    this->tdma_scheduling(TDMAmode, RAmode, paramX);

    #ifdef DEBUG
    this->printme(2); // print schedule
    #endif

    this->tdma_time_allocation(true);

    #ifdef DEBUG
    this->printme(3); // print allocated time resource
    #endif // DEBUG
}
