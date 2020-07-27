#include "node.h"
#include "channel.h" // calculate channel
#include "resource_allocation.h" // struct mod scheme
#include <list> // std::list
#include <algorithm> //std::sort
#include <iostream>
#include <random> // uniform_real_distribution, default_random_engine, accumulate (????)
#include <chrono> // seed, steady_clock, duration_cast
#include <unordered_map>

/////////////////////////////////////////////DATA STRUCTURES///////////////////////////////////////////////////////////////////
/** used to sort candidate in ASCENDING order of their NOMA order... */
struct sort_candidate{
    sort_candidate(const int& AP_id){AP=node::transmitter[AP_id];};
    const node* AP;
    bool operator() (const int& a, const int& b){
        return AP->get_sorting_order(a) <= AP->get_sorting_order(b);
    }
};

struct LayerInfo {
    int m;
    double power;
    double rate;
    LayerInfo(){
        m=0;
        power=0;
        rate=0;
    }
    LayerInfo(const LayerInfo &src){ // copy constructor
        m=src.m;
        power=src.power;
        rate=src.rate;
    }
    LayerInfo(const int &m, const double &p, const double& r){ // value (?) constructor
        this->m=m;
        power=p;
        rate=r;
    }
};

double SINR_const[g_M];

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
* UE needs THIS much power from AP to use m in layer l alone
* \return double power
*/
double powerPerLayer(const int& AP_id, const int &UE_id, const int& m, const int& l, const double& powerPrevUEs){
     /* SINR of this layer = single layer SINR using BPSK*/
    double SINR = SINR_const[m-1]; // REMEMBER ARRAY STARTS AT 0!!
    /* beta_l (subcarrier ratio) of this layer */
    double beta_l = (double) 1 / std::pow(2,l);
    /* interference from prev UEs (1 to k-1) */
    double intra_I = powerPrevUEs * std::pow(node::channel[AP_id][UE_id],2);
    /* AWGN */
    double AWGN = g_total_bandwidth * g_N_0;
    /* inter-cell-interference I_n,k */
    double ICI = 0.0;
    for(int f = 0; f<g_AP_number; f++){
        if (f==AP_id) continue;
        else {
            /* check if same RB */
            int thisRB = node::transmitter[AP_id]->get_resource_block();
            int thatRB = node::transmitter[f]->get_resource_block();
            if (thisRB == thatRB) /* ICI ONLY if use same RB */
                ICI += node::transmitter[f]->calculate_ICI(UE_id);
        }
    }

    /* power for this layer */
    double power = SINR * beta_l * (intra_I + AWGN + ICI) / std::pow(node::channel[AP_id][UE_id],2);

    return power;
}

double minPower(const int &AP_id, const int &UE_id){
     if (SINR_const[0]==0) // initialize if not yet init'd
        for(int m=1;m<=g_M;m++) SINR_const[m-1] = SINR_single_layer(m);
    /* UE needs THIS much power for mod 1 in layer 4. *
     * if this is higher than Pmax, then it is impossible *
     * for this AP to support this UE. */

    return powerPerLayer(AP_id, UE_id,1,4,0);
}

double sumPowerAcrossLayers(const LayerInfo myLayers[g_L]){
    double normalSum = 0.0, rmsSum = 0.0;
    for (int l=1;l<=g_L;l++){
        const LayerInfo &layer = myLayers[l-1];
        if (layer.m == 0) continue;
            normalSum += layer.power;
            rmsSum += std::sqrt(layer.power);
    }
    return normalSum*(M_PI-1)/M_PI + std::pow(rmsSum,2)/M_PI;
}

/** modulation mode assignment for ONE UE *
  * \param AP id
  * \param UEid
  * \param prevUEPower: for Intra-cell interference
  * \param myPmax: available power for this UE to attempt mod assignment
  * \param UEPowerMap, UEmodSchMap, UESumRateMap: used for saving result
  * \return final required power. caller use this to accumulate UE powers.
  */
double mod_mode_assignment(const int& APid, const int& UEid, const double& powerPrevUEs, const double &myPmax,
                         std::unordered_map<int,double> &UEPowerMap, std::unordered_map<int,LayerInfo[g_L]> &UEmodSchMap, std::unordered_map<int,double> &UESumRateMap){
    LayerInfo myLayers[g_L] = {LayerInfo()}; // REMEMBER ARRAY STARTS FROM 0!!
    double sumRate = 0.0, sumPower = 0.0;
    double Rmin = node::receiver[UEid]->min_required_rate;
//    std::cout<<UEid<<" allocated power "<<myPmax<<" ("<<UEPowerMap[UEid]<<"), Rmin "<<Rmin<<". Power of prev UEs "<<powerPrevUEs<<'\n';
    for (int l = 1; l<=g_L; l++){
        for (int m = 1; m<=g_M; m++){ // iterate through mods to find affordable m

            double p_ml = powerPerLayer(APid, UEid,m,l,powerPrevUEs);
            double r_ml = (double)g_bandwidth_per_rb / pow(2,l) * log2(1 + SINR_const[m-1]);
//            std::cout<<UEid<<" using "<<m<<" on layer "<<l<<" needs "<<p_ml<<", rate profit = "<<r_ml<<'\n';

            LayerInfo oldLayerl = LayerInfo(myLayers[l-1]); // save info of prev m, in case need to revert
            /*--- check if this m is affordable? ---*/
            myLayers[l-1] = LayerInfo(m, p_ml, r_ml);
            if (sumPowerAcrossLayers(myLayers)>myPmax) {
                // cannot afford this m, revert back to prev m, then break
                myLayers[l-1] = LayerInfo(oldLayerl);
                std::cout<<"Cannot afford! Revert to "<<m-1<<"\n";
                break;
            } else if (sumRate+r_ml >= Rmin) {
                // is this layer overkill?
                if (m==1 && Rmin-sumRate < r_ml/2 && l!=g_L) {
//                    std::cout<<"Overkill, go to lower layer\n";
                    myLayers[l-1] = LayerInfo();
                    break;
                }
                // can afford this m, and already satisfy Rmin, break
//                std::cout<<"Rmin is satisfied. "<<m<<" is chosen.\n";
                break;
            } // can afford this m, but Rmin still not satisfied
        }

        if (myLayers[l-1].m == 0) {      // no mods affordable for this layer
            myLayers[l-1] = LayerInfo(); // reinitialize to zero, just in case ...
        } else {
            // sanity check
            if (myLayers[l-1].rate == 0 || myLayers[l-1].power == 0) {
//                std::cout<<"Layer "<<l<<" selected mod "<<myLayers[l-1].m<<" but rate or power = 0! Something's wrong...\n";
                exit(1);
            }
            // update this UE's current sumRate and sumPower
            sumRate += myLayers[l-1].rate;
            sumPower = sumPowerAcrossLayers(myLayers);
        }
//        std::cout<<"---"<<UEid<<" chose "<<myLayers[l-1].m<<" on layer "<<l<<" needs "<<myLayers[l-1].power<<", rate "<<myLayers[l-1].rate
//                 <<", total power "<<sumPower<<"\n";
        // TODO (alex#1#): check if can afford cheapest mod in next layer, break if can't
        if (sumRate >= Rmin) {
//            std::cout<<"Sum rate "<<sumRate<<" >= Rmin "<<Rmin<<'\n';
            break;
        }
    }

    if (sumRate<Rmin){
        sumPower = sumRate = 0.0;
        for(int l=1;l<=g_L;l++) myLayers[l-1]=LayerInfo();
    }

    UEPowerMap[UEid] = sumPower;
    UESumRateMap[UEid] = sumRate;
    for(int l=1; l<=g_L; l++) // update layer info individually because c++ is dumb /s
        (UEmodSchMap[UEid])[l-1] = LayerInfo(myLayers[l-1]);

//    std::cout<<UEid<<" mod scheme result: [";
//    for(int l=1; l<=g_L; l++) std::cout<<' '<<(UEmodSchMap[UEid])[l-1].m;
//    std::cout<<" ]\n";

    return sumPower;

}

double required_power(const int& APid, const int& UEid, const double& prevUEsPower, LayerInfo myLayers[g_L]){
    ///// REMEMBER!! myLayers[] is passed by value here!!!
    ///// Changing its content is only for the purpose of calculating power
    ///// Won't affect the actual recorded power!
    for (int l=1; l<=g_L; l++){
        double power = powerPerLayer(APid,UEid,myLayers[l-1].m, l, prevUEsPower);
        myLayers[l-1].power = power;
    }
    return sumPowerAcrossLayers(myLayers);
}

/** calculate data rate change rate for a layer */
double layerRateGradient(const int& APid, const int& UEid, const double& prevUEsPower, const LayerInfo& aLayer, const int l){
    /* interference from prev UEs (1 to k-1) */
    double intra_I = prevUEsPower * std::pow(node::channel[APid][UEid],2);
    /* AWGN */
    double AWGN = g_total_bandwidth * g_N_0;
    /* inter-cell-interference I_n,k */
    double ICI = 0.0;
    for(int f = 0; f<g_AP_number; f++){
        if (f==APid) continue;
        else {
            /* check if same RB */
            int thisRB = node::transmitter[APid]->get_resource_block();
            int thatRB = node::transmitter[f]->get_resource_block();
            if (thisRB == thatRB) /* ICI ONLY if use same RB */
                ICI += node::transmitter[f]->calculate_ICI(UEid);
        }
    }
    double NoiseAndInterference = intra_I + AWGN + ICI;
    /* beta_l (subcarrier ratio) of this layer */
    double beta_l = (double) 1 / std::pow(2,l);

    double gradient = g_bandwidth_per_rb / std::log10(2) / (NoiseAndInterference/(std::pow(node::channel[APid][UEid],2)) + aLayer.power/beta_l);
    return gradient;
}

double SINR_single_layer(const int& APid, const int& UEid, const double& prevUEsPower, const double& myPower){
     /* interference from prev UEs (1 to k-1) */
    double intra_I = prevUEsPower * std::pow(node::channel[APid][UEid],2);
    /* AWGN */
    double AWGN = g_total_bandwidth * g_N_0;
    /* inter-cell-interference I_n,k */
    double ICI = 0.0;
    for(int f = 0; f<g_AP_number; f++){
        if (f==APid) continue;
        else {
            /* check if same RB */
            int thisRB = node::transmitter[APid]->get_resource_block();
            int thatRB = node::transmitter[f]->get_resource_block();
            if (thisRB == thatRB) /* ICI ONLY if use same RB */
                ICI += node::transmitter[f]->calculate_ICI(UEid);
        }
    }
    double SINR = std::pow(node::channel[APid][UEid],2) * myPower / (intra_I + AWGN + ICI);
    return SINR;
}

void max_mod_mode_assignment(const int& APid, const int& UEid, const double& prevUEsPower, const double& powerAllowance,
                             std::unordered_map<int, double>& UEPowerMap, std::unordered_map<int, LayerInfo[g_L]>& UEmodSchMap, std::unordered_map<int, double>& UESumRateMap){
    LayerInfo myLayers[g_L] = { LayerInfo() };
//    std::cout<<"Max mode assignment: "<<UEid<<" tries to get as high as possible under "<<powerAllowance<<'\n';
    double sumRate = 0.0, sumPower=0.0;
    for(int l=1; l<=g_L; l++){
        for (int m=g_M; m>=0; m--){
            myLayers[l-1].m = m;
            if (m>0){
                myLayers[l-1].power = powerPerLayer(APid,UEid,m,l,prevUEsPower);
                if (sumPowerAcrossLayers(myLayers) <= powerAllowance) break;
            }
        }
        if (myLayers[l-1].m == 0){
            myLayers[l-1] = LayerInfo();
        } else {
            myLayers[l-1].rate = (double)g_bandwidth_per_rb / pow(2,l) * log2(1 + SINR_const[(myLayers[l-1].m)-1]);
            sumRate += myLayers[l-1].rate;
        }
    }

    // still have remaining allowance
//    std::cout<<"Still have remaining allowance! Give to best layer...\n";
    sumPower = sumPowerAcrossLayers(myLayers);
    if (powerAllowance>sumPower){
        // find which layer's r/p is highest
        int highestGradientLayer = 0; double highestGradient = 0.0;
        for(int l=1; l<=g_L; l++){
            if (myLayers[l-1].m==0) continue;
            double thisLayerRateGradient = layerRateGradient(APid, UEid, prevUEsPower, myLayers[l-1], l);
            if (thisLayerRateGradient > highestGradient) {
                highestGradient = thisLayerRateGradient;
                highestGradientLayer = l;
            }
        }
        // give (powerAllowance-sumPower) to this layer
        if (highestGradientLayer != 0){
//            std::cout<<"Layer "<<highestGradientLayer<<" increase power until sumPower "<<sumPower<<" reaches "<<powerAllowance<<"\n";
            sumRate -= myLayers[highestGradientLayer-1].rate;
            while(sumPower<=powerAllowance){
                myLayers[highestGradientLayer-1].power += 0.5;
                sumPower = sumPowerAcrossLayers(myLayers);
//                std::cout<<"sumPower now "<<sumPower<<'\n';
            }
            while(sumPower>powerAllowance){
                myLayers[highestGradientLayer-1].power -= 0.5;
                sumPower = sumPowerAcrossLayers(myLayers);
            }
            UEPowerMap[UEid] = sumPowerAcrossLayers(myLayers);
            myLayers[highestGradientLayer-1].rate = g_bandwidth_per_rb / std::pow(2,highestGradientLayer) * log2( 1 + SINR_single_layer(APid, UEid, prevUEsPower, myLayers[highestGradientLayer-1].power) );
            sumRate += myLayers[highestGradientLayer-1].rate;
        }
    }

    if (sumRate>UESumRateMap[UEid]){
        UESumRateMap[UEid] = sumRate;
        UEPowerMap[UEid] = sumPowerAcrossLayers(myLayers);
        for(int l=1; l<=g_L; l++)
            (UEmodSchMap[UEid])[l-1] = myLayers[l-1];
    }
}

std::list<int> node::heuristic_resource_allocation(std::vector<int>& candidate){
    /* initialize the SINR "const" array */
    if (SINR_const[0]==0)
        for(int m=1;m<=g_M;m++) SINR_const[m-1] = SINR_single_layer(m);

    // sort UEs according to channel in desc order!! (UE 1 has highest channel)
    std::sort(candidate.begin(),candidate.end(),sort_candidate(this->id));

    /* pruning should have been done. minPower should <= P_max */
    std::unordered_map<int,double> minPowerMap;
    double minPowerTotal = 0.0;
    for (int UEid : candidate){
        double myMinPower = minPower(this->id, UEid);
//        std::cout<<"Minimum required power to serve UE "<<UEid<<" is "<< myMinPower <<'\n';
        if (myMinPower > g_P_max) {
            std::cout<<"Something is wrong.\n";
            exit(1);
        }
        minPowerMap.insert({UEid,myMinPower});
        minPowerTotal += myMinPower;
    }

    /*--- initial power allocation ---*/
    std::unordered_map<int,double> UEPowerMap;
    for(int UEid : candidate){
        double myInitPower = minPowerMap[UEid] / minPowerTotal * g_P_max;
        UEPowerMap.insert({UEid, myInitPower});
    }

    /*--- modulation mode assignment ---*/
    std::unordered_map<int,double> UESumRateMap;
    std::unordered_map<int,LayerInfo[g_L]> UEmodSchMap;
    int unsatisfiedUE = 0;
    double prevUEsPower = 0.0, leftover = 0.0; // for calculating Intra-cell Interference
    for (int UEid : candidate ){
        ///// !!!! updates to UEPowerMap, UEmodSchMap and UESumRateMap will be done in mod_mode_assignment() !!!! /////
        double myAllowance = UEPowerMap[UEid];
        prevUEsPower += mod_mode_assignment(this->id, UEid, prevUEsPower, myAllowance+leftover, UEPowerMap, UEmodSchMap, UESumRateMap);
        leftover = myAllowance + leftover - UEPowerMap[UEid];

        if (UESumRateMap[UEid] < node::receiver[UEid]->min_required_rate) unsatisfiedUE++;
    }

    std::cout<<"----- Mod mode assignment complete, total power "<<prevUEsPower<<", leftover "<<leftover<<"-----\n";
//    for(int UEid : candidate){
//        std::cout<<UEid<<": [ ";
//        for (int l=1;l<=g_L;l++) std::cout<<(UEmodSchMap[UEid])[l-1].m<<' ';
//        std::cout<<"] power="<<UEPowerMap[UEid]
//                 <<", rate="<<UESumRateMap[UEid]<<'/'<<node::receiver[UEid]->min_required_rate<<'\n';
//    }


    /*--- power allocation ---*/
    std::cout<<"----------- Power Allocation ----------\n";
    if (unsatisfiedUE>0){
        prevUEsPower = 0.0;
        for(int i=0; i<candidate.size(); i++){
            int UEid = candidate[i];
//            std::cout<<"Checking UE "<<UEid<<", current leftover "<< leftover<<'\n';
            if (UESumRateMap[UEid]<node::receiver[UEid]->min_required_rate){ // found unsatisfied UE
                /*-- temporarily store old configuration, in case need to revert ---*/
                double oldLeftover = leftover;
                double oldPrevUEsPower = prevUEsPower;
                std::unordered_map<int,double> oldUEPowerMap = UEPowerMap;
                std::unordered_map<int,double> oldUESumRateMap = UESumRateMap;
                std::unordered_map<int,LayerInfo[g_L]> oldUEmodSchMap = UEmodSchMap;
                /*-- try to support this UE with leftover power ---*
                **-- 1. try to allocate mod                        *
                **-- 2. check if UEs above are ok with this change */
                bool revertFlag = false;
                double myPower = mod_mode_assignment(this->id, UEid, prevUEsPower, leftover, UEPowerMap, UEmodSchMap, UESumRateMap);
                if (UESumRateMap[UEid]<node::receiver[UEid]->min_required_rate){
//                    std::cout<<UEid<<" still can't be supported. Reverting...\n";
                    revertFlag = true;
                } else { // 1. ok, now check 2.
                    prevUEsPower += myPower;
                    leftover -= myPower;
//                    std::cout<<UEid<<" got Rmin! Took "<<myPower<<" from leftover (now "<<leftover<<" ) Checking if UEs on top are ok with this...\n";
                    double prevUEsPowerCheck = prevUEsPower; // don't touch the actual prevPower record
                    for (int j=i+1; j<candidate.size(); j++){
                        int UEj = candidate[j];
                        // pre-calculate, this SHOULD NOT and DOES NOT change anything!
                        double UEj_newPower = required_power(this->id, UEj, prevUEsPowerCheck, UEmodSchMap[UEj]);
//                        std::cout<<UEj<<" now needs "<<UEj_newPower<<'\n';
                        if ((UEj_newPower - UEPowerMap[UEj]) > leftover) {
                            ////////BAD!! REVERT /////////////
//                            std::cout<<"Leftover cannot afford. Reverting...\n";
                            revertFlag = true;
                            break;
                        } else {
                            // update power, rate and mod doesn't change.
                            for(int l=1;l<=g_L;l++) { (UEmodSchMap[UEj])[l-1].power = powerPerLayer(this->id, UEj, (UEmodSchMap[UEj])[l-1].m, l, prevUEsPowerCheck); }
                            leftover -= UEj_newPower-UEPowerMap[UEj];
                            UEPowerMap[UEj] = UEj_newPower;
                            prevUEsPowerCheck += UEj_newPower;
//                            std::cout<<"Leftover is enough for this change.\n Remaining leftover "<< leftover-(UEj_newPower-UEPowerMap[UEj])<<'\n';
                        }
                    }
                }
                if (revertFlag) { // revert
                    leftover = oldLeftover;
                    UEPowerMap = oldUEPowerMap;
                    UESumRateMap = oldUESumRateMap;
                    UEmodSchMap = oldUEmodSchMap;
                    prevUEsPower = oldPrevUEsPower;
                } else { // this UE can take new power alloc from leftover! keep this configuration
                    unsatisfiedUE--;
                    if (unsatisfiedUE=0) break;
                }

            } else {
                prevUEsPower += UEPowerMap[UEid];
            }
        }
    }

    std::cout<<"----- UE revive complete, total power "<<prevUEsPower<<", leftover "<<leftover<<"-----\n";
//    for(int UEid : candidate){
//        std::cout<<UEid<<": [ ";
//        for (int l=1;l<=g_L;l++) std::cout<<(UEmodSchMap[UEid])[l-1].m<<' ';
//        std::cout<<"] power="<<UEPowerMap[UEid]
//                 <<", rate="<<UESumRateMap[UEid]<<'/'<<node::receiver[UEid]->min_required_rate<<'\n';
//    }
    if (prevUEsPower > g_P_max) system("pause");
    // validity check
    double sumPowerCheck = 0.0;
    for (int UEid : candidate){
        if (UEPowerMap[UEid]!=sumPowerAcrossLayers(UEmodSchMap[UEid])) {
            std::cout<<UEid<<" layer sum power != recorded sum power. Sth's wrong...\n";
            exit(1);
        }
        sumPowerCheck += UEPowerMap[UEid];
    }
    if (sumPowerCheck > prevUEsPower){
        std::cout<<"sumPowerCheck > prevUEsPower???\n";
        exit(1);
    }
    if (sumPowerCheck > g_P_max){
        std::cout<<"sumPowerCheck > g_P_max???\n";
        exit(1);
    }

//    std::cout<<"Now giving leftover power to UE with good performance...\n";
    leftover = g_P_max;
    for(int UEid : candidate) leftover -= UEPowerMap[UEid];
//    std::cout<<"Current leftover "<<leftover<<'\n';

    // FIND topmost UE
    int topMostUE = -1;
    prevUEsPower = 0.0;
    for (int i = candidate.size()-1; i>=0; i--){
        int UEid = candidate[i];
        if (UEPowerMap[UEid]==0) continue;
        else {
            topMostUE = UEid;
            prevUEsPower += UEPowerMap[UEid];
            break;
        }
    }

    if (topMostUE != -1){ // found!
        prevUEsPower -= UEPowerMap[topMostUE];
        max_mod_mode_assignment(this->id, topMostUE, prevUEsPower, leftover+UEPowerMap[topMostUE], UEPowerMap, UEmodSchMap, UESumRateMap);
//        std::cout<<"Leftover power allocation result: "<<topMostUE<<": [ ";
//        for (int l=1;l<=g_L;l++) std::cout<<(UEmodSchMap[topMostUE])[l-1].m<<' ';
//        std::cout<<"] power="<<UEPowerMap[topMostUE]
//                 <<", rate="<<UESumRateMap[topMostUE]<<'/'<<node::receiver[topMostUE]->min_required_rate<<'\n';
    } else {
//        std::cout<<"No UE available to accept leftover power.\n";
    }

    /// convert LayerInfo to mod scheme, save in AP node member var
    for(int UEid : candidate){
        if (UEPowerMap[UEid]==0) continue;
        else {
            mod_scheme* myModScheme = new mod_scheme();
            myModScheme->required_power = UEPowerMap[UEid];
            myModScheme->sum_throughput = UESumRateMap[UEid];
            const LayerInfo myLayer[g_L] = UEmodSchMap[UEid];
            for(int l=1;l<=g_L;l++) myModScheme->modes_each_layer.push_back(myLayer[l-1].m);
            UE_scheme curScheme;
            curScheme.UE_id = UEid;
            curScheme.modulation_scheme = myModScheme;
            this->mod_scheme_assignment.push_back(curScheme);
            node::receiver[UEid]->sum_throughput += UESumRateMap[UEid];
        }
    }

    std::list<int> accepted;
    for(int UEid : candidate)
        if (UESumRateMap[UEid] >= node::receiver[UEid]->min_required_rate)
            accepted.push_back(UEid);
    return accepted;



    /*------- randomly decide which UEs get accepted --------------*/
//    std::uniform_real_distribution<double> unif(0,1.0);
//    std::default_random_engine re(std::chrono::system_clock::now().time_since_epoch().count());
//
//    std::list<int> accepted;
//    for(int i:candidate) accepted.push_back(i);
//    //std::cout<<"-- Accepted: "; for(int i:accepted)std::cout<<i<<' '; std::cout<<'\n';
//    for(int i : candidate){
//        double chance = unif(re);
//        //std::cout<<"-- -- Chance="<<chance<<std::endl;
//        if(chance<0.5) accepted.remove(i);
//    }
//    return accepted;
}
