#include "node.h"
#include "channel.h" // calculate channel
#include <random> // uniform_real_distribution, default_random_engine, accumulate (????)
#include <list> // std::list
#include <algorithm> //std::sort
#include <iostream>
#include <fstream> // ofstream
#include <chrono> // seed, steady_clock, duration_cast
#include <math.h> // log2, power, floor
#include "math.c" // erfinv
#include "resource_allocation.h" // struct mod scheme

void printSch(const mod_scheme* m_sch){
    #ifdef DEBUG

    std::cout<<"[ ";
    if (m_sch != NULL)
        for(int m:m_sch->modes_each_layer)
            std::cout<<m<<' ';
    std::cout<<"]";

    #endif // DEBUG
}

/** used to sort candidate in ASCENDING order of their NOMA order... */
struct sort_candidate{
    sort_candidate(const int& AP_id){AP=node::transmitter[AP_id];};
    const node* AP;
    bool operator() (const int& a, const int& b){
        return AP->get_sorting_order(a) <= AP->get_sorting_order(b);
    }
};

double node::calculate_ICI(const int& UE_id){
    /*--- case: this AP has no serving UEs ---*/
    if (this->connected.size() == 0) return 0.0;

    int order = this->get_sorting_order(UE_id);
    double channel_AP_UE = node::channel[this->id][UE_id];
    double ICI = channel_AP_UE*channel_AP_UE*g_P_max;
    if (order==-1){ // if UE not associated with this AP
        return ICI;
    } else {
        return ICI * order / this->connected.size();
    }
}

/**
* \brief calculate nearest lower rate constraint level
* \return int j_star: nearest lower rate constraint level
*/
int jFloor(double const& rateConstraintForPrevLayer){
    // if R_j - r < 0, t_m = 0
    if (rateConstraintForPrevLayer<0) return 0;
    int j_star = floor(rateConstraintForPrevLayer / g_R_max * g_J);
    /* actually belongs to j_star+1 level, but round down to j_star according to algorithm */
    return j_star;
}

double SINR_single_layer(const int& m){
    double SINR = 0.0;
    SINR = g_BER;
    SINR = SINR * m * pow(2,m);
    SINR = SINR / (pow(2,m) - 1);
    SINR = 1 - SINR;
    SINR = erfinv(SINR);
    SINR = pow(SINR,2);
    SINR = SINR / 3 * 2 * ( pow(2,2*m) - 1);
    return SINR;
}

double node::calculate_achievable_rate_single_layer(const int& m, const int& l){
    /* BER(m) -> SINR */
    double SINR = SINR_single_layer(m);

    /* SINR(B,beta_l,SINR) -> rate */
    double rate = 0.0;
    rate = (double)g_bandwidth_per_rb / pow(2,l) * log2(1 + SINR);
    return rate;
}

/** \brief first tier RA </br>
*
* First Tier RA of UE <candidate-id>. Logs are saved to `./log/<candidate-id>_first_tier`.
* for each l:1-L
*    for each j:1-J
*       for each m:1-|M|
*          [1] calculate achievable rate using m for layer l: r
*          [2] calculate achievate rate at rate constraint (R_j - r) using layers 1 to (l-1): T
*          [3] t_m = T + r
*       obtain ahievable rate at rate constraint R_j using layers 1 to l: max(t_m)
*
* \return list of mod schemes that satisfy min rate
*/
std::list<mod_scheme*> ra_first_tier(const int& candidate_id){
    node* node_UE = node::receiver[candidate_id];
    std::list<mod_scheme*> candidate_schemes;
    //std::cout<<"-- 1st Tier RA for UE "<<node_UE->id<<'\n';
    struct mod_scheme* my_mod_schemes[g_L][g_J]; // used to store mod schemes for all l and j

    std::string filepath = "./log/UE_" + std::to_string(node_UE->id) + "_first_tier";
    std::ofstream fout(filepath);
    if(!fout.is_open()) {
        std::cout<<"Can't Open "<<filepath<<", exiting now";
        exit(1);
    }

    /*-- for each l, for each j, calculate max achievable rate and respective mod scheme --*/
    for(int l = 1; l<=g_L; l++){
        /* using l layers: */
        fout<<"l = "<<l<<", "<<'\n';

        for(int j = 1; j<=g_J; j++){
            /* under rate constraint R_j: */
            fout<<"-- j = "<<j<<", r|t_m for m=1~5:";

            /* initialize mod_scheme */
            my_mod_schemes[l-1][j-1] = new mod_scheme;
            mod_scheme* current_mod_scheme = my_mod_schemes[l-1][j-1];
            current_mod_scheme->sum_throughput = 0.0;

            for(int m = 1; m<=g_M; m++){
                /*-- [1] t_m = T_(k,l-1)(R_j-r(m,k,l)) + r(m,k,l) --*/
                /*--     calculate r(m,k,l) --*/
                double r_m = node_UE->calculate_achievable_rate_single_layer(m,l);
                fout<<"\t"<<r_m;

                /*--     calculate t_m = T + r    --*/
                /* initialize mod scheme combination */
                std::list<int> current_mod_combi;
                /* if >1st layer, t_m initialize as T_(k,l-1)(R_j-r) */
                double t_m = 0.0;
                double R_j = (double)g_R_max / g_J * j;
                if (l>1)  {
                    R_j = R_j-r_m;
                    if (R_j < 0) {// if R_j - r < 0, t_m = 0.
                        t_m = 0;
                        current_mod_combi = my_mod_schemes[l-2][j-1]->modes_each_layer;
                        current_mod_combi.push_back(0);// this mod CANNOT be chosen.
                    }
                    else {       // if R_j - r >= 0, can at least afford this layer.
                        t_m = r_m;
                        if (jFloor(R_j)>0) {// remaining Rate enough for prev (l-1) layers
                            t_m = my_mod_schemes[l-2][jFloor(R_j)-1]->sum_throughput + r_m; // l-2 because l starts at 1 but index starts at 0
                            current_mod_combi = my_mod_schemes[l-2][jFloor(R_j)-1]->modes_each_layer;
                        }
                        else {
                            current_mod_combi = std::list<int>(l-1, 0); // fills previous l-1 layers with "-1"
                        }
                        current_mod_combi.push_back(m);
                    }

                } else { // first layer
                    if (R_j >= r_m){
                        t_m = r_m;
                        current_mod_combi.push_back(m);
                    } else
                        current_mod_combi.push_back(0);
                }
                fout<<"|"<<t_m;

                /*-- [2] max t_m --*/
                if (t_m >= current_mod_scheme->sum_throughput){
                    current_mod_scheme->sum_throughput = t_m;
                    current_mod_scheme->modes_each_layer = current_mod_combi;
                }
            }

            /*-- save mod scheme using l layers under constraint R_j:
             *-  mod used for each layer, total achievable rate, total required power
             *-  struct defined in "resource_allocation.h"
             */
            fout<<"\t=> "<<current_mod_scheme->sum_throughput<<", \t";
            //fout<<"                          \t";
            for(int i:current_mod_scheme->modes_each_layer) fout<<i<<' ';
            fout<<'\n';

            /* only save schemes that produces sum rate within acceptable range */
            if (current_mod_scheme->sum_throughput >= node_UE->min_required_rate && current_mod_scheme->sum_throughput < g_R_max){
                candidate_schemes.push_back(current_mod_scheme);
            }
        }
    }
    fout.close();
    return candidate_schemes;
}


/**calculate required power of given UE using given candidate modulation scheme,
* needs to know which AP is calling this function.
* \param UE_id id of UE k
* \param candidate_scheme scheme to be used in calculation
* \param prev_UEs_power total power required by UEs 1 to (k-1), determined in caller function
* \return double required power
*/
double power_required(const int& AP_id, const int& UE_id, const mod_scheme &candidate_scheme, const double &prev_UEs_power){
    double totalPower = 0.0;

    /* interference from prev UEs (1 to k-1) */
    double intra_I = prev_UEs_power * std::pow(node::channel[AP_id][UE_id],2);
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

    /* for each layer used in candidate_scheme: calculate power required for that layer */
    int l=0;
    std::list<double> powerSqrt_eachLayer;
    for(int m : candidate_scheme.modes_each_layer){
        l++;
        if (m==0) continue;
        /* SINR of this layer */
        double SINR = SINR_single_layer(m);
        /* beta_l (subcarrier ratio) of this layer */
        double beta_l = (double) 1 / std::pow(2,l);
        /* power for this layer */
        double power = SINR * beta_l * (intra_I + AWGN + ICI) / std::pow(node::channel[AP_id][UE_id],2);

        totalPower += power;
        powerSqrt_eachLayer.push_back(std::sqrt(power));
    }

    /* total power required by this scheme */
    totalPower = (M_PI - 1)/M_PI * totalPower; // first half
    totalPower = totalPower + (1/M_PI * std::pow(std::accumulate(powerSqrt_eachLayer.begin(),powerSqrt_eachLayer.end(),0.0),2));

    return totalPower;
}

/** second tier resource allocation
* can only be called by AP node!!
* \return list of accept UEs' id
*/
mod_scheme_combi node::ra_second_tier(const std::list<std::pair<int, std::list<mod_scheme*>>>& all_candidate_mod_scheme_set){
    #ifdef DEBUG
    std::cout<<"-- Second tier...\n";
    #endif // DEBUG

    const int AP_id = this->id;
    mod_scheme_combi* mod_combi_each_constraint_up_to_prev_UE[g_I]; // each l-th element = scheme combi from row(UE) 1 to prev under constraint l
    for(int i=1;i<=g_I;i++) {  // init
        mod_combi_each_constraint_up_to_prev_UE[i-1] = new mod_scheme_combi;
    }

    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    /* from UE 1 to K, determine mod scheme selection for each power constraint */
    for(std::pair<int, std::list<mod_scheme*>> candidate_scheme_set : all_candidate_mod_scheme_set){
        #ifdef DEBUG
        std::cout<<"Evaluating UE "<<candidate_scheme_set.first<<": "<<candidate_scheme_set.second.size()<<" candidate schemes\n";
        /*-- for logging */
        std::string filepath = "./log/AP_" + std::to_string(AP_id) + "_UE_" + std::to_string(candidate_scheme_set.first) +"_second_tier";
        std::ofstream fout(filepath);
        if(!fout.is_open()) {
            std::cout<<"Can't Open "<<filepath<<", exiting now";
            exit(1);
        }
        #endif // DEBUG

        int UE_id = candidate_scheme_set.first;
        mod_scheme_combi* selected_schemes_for_this_UE[g_I] = {NULL};

        for(int i = 1; i<=g_I; i++){ // i is power constraint level
            double P_i = g_P_max * i / g_I;
            #ifdef DEBUG
            std::cout<<"p_"<<i<<" = "<<P_i<<"  \t";
            fout<<"p_"<<i<<" = " <<P_i<<"----------------------------------------------------------------\n";
            #endif // DEBUG

            /*--- determine best scheme under P_i ---*/
            // calculate required power & sum_throughput for each candidate scheme...
            for(mod_scheme* scheme : candidate_scheme_set.second){
                #ifdef DEBUG
                fout<<"Mod scheme [";
                for(int m:scheme->modes_each_layer)fout<<m<<","; fout<<"]\t";
                #endif // DEBUG

                //... under different power constraint for prev UE
                for(int j=1; j<=i; j++){
                    // power and throughput for THIS scheme only
                    double power = power_required(AP_id,UE_id,*scheme,mod_combi_each_constraint_up_to_prev_UE[j-1]->required_power);
                    double throughput = scheme->sum_throughput;
                    // select scheme (highest sumRate, sumPower within constraint)
                    if (power + mod_combi_each_constraint_up_to_prev_UE[j-1]->required_power <= P_i) { // power within this constraint?
                        if (selected_schemes_for_this_UE[i-1]==NULL) { // no scheme selected yet for this constraint
                            selected_schemes_for_this_UE[i-1] = new mod_scheme_combi;
                        }
                        if (throughput + mod_combi_each_constraint_up_to_prev_UE[j-1]->sum_throughput>=selected_schemes_for_this_UE[i-1]->sum_throughput){ // replace prev selected scheme
                            selected_schemes_for_this_UE[i-1]->sum_throughput = throughput + mod_combi_each_constraint_up_to_prev_UE[j-1]->sum_throughput;
                            selected_schemes_for_this_UE[i-1]->required_power = power + mod_combi_each_constraint_up_to_prev_UE[j-1]->required_power;
                            selected_schemes_for_this_UE[i-1]->mod_schemes_each_UE = mod_combi_each_constraint_up_to_prev_UE[j-1]->mod_schemes_each_UE;
                            scheme->required_power = power;
                            selected_schemes_for_this_UE[i-1]->mod_schemes_each_UE.push_back(scheme);
                        }
                    }
                    #ifdef DEBUG
                     fout<< power <<"|"<<throughput<<"\t";
                    #endif // DEBUG

                }
                #ifdef DEBUG
                fout<<"\n";
                if (selected_schemes_for_this_UE[i-1]!=NULL) {
                    fout<<"Cur best scheme: power="<<selected_schemes_for_this_UE[i-1]->required_power<<"|rate="<<selected_schemes_for_this_UE[i-1]->sum_throughput<<"\n";
                }
                else {
                    fout<<"this scheme cannot be selected for this constraint\n";
                }
                #endif // DEBUG
            }
            // WHAT IF NO SCHEME AVAILABLE FOR THIS CONSTRAINT?!
            if (selected_schemes_for_this_UE[i-1]==NULL) {
                    selected_schemes_for_this_UE[i-1] = new mod_scheme_combi;
                    selected_schemes_for_this_UE[i-1]->copyFrom(mod_combi_each_constraint_up_to_prev_UE[i-1]);
                    selected_schemes_for_this_UE[i-1]->mod_schemes_each_UE.push_back(NULL);
            }

            #ifdef DEBUG
            // cout which scheme chosen for this constraint
            if (selected_schemes_for_this_UE[i-1]==NULL) { // NO scheme selected for this constraint
                std::cout<<"All schemes unaffordable.\n";
            } else { // has selected scheme for this constraint
                for(mod_scheme* m_sch:selected_schemes_for_this_UE[i-1]->mod_schemes_each_UE){
                    printSch(m_sch); std::cout<<" ";
                }
                std::cout<<"Required power="<<selected_schemes_for_this_UE[i-1]->required_power<<"\t| ";
                std::cout<<"Sum rate="<<selected_schemes_for_this_UE[i-1]->sum_throughput<<"\n";
            }
            #endif // DEBUG
        }
        /*--- save scheme ---*/
        for(int i=1; i<=g_I; i++) {
            mod_combi_each_constraint_up_to_prev_UE[i-1]->copyFrom(selected_schemes_for_this_UE[i-1]);
            delete selected_schemes_for_this_UE[i-1];
        }
        //delete[] selected_schemes_for_this_UE;
        #ifdef DEBUG
        fout.close();
        #endif // DEBUG

    }

    /*--- all K UEs are done ---*/
    // select combi with highest throughput
    mod_scheme_combi solution;
    for(int i=1; i<=g_I; i++){
        if(mod_combi_each_constraint_up_to_prev_UE[i-1]->sum_throughput >= solution.sum_throughput) {
            solution = *(mod_combi_each_constraint_up_to_prev_UE[i-1]);
        } else {

        }
    }
    //delete[] mod_combi_each_constraint_up_to_prev_UE;

    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    #ifdef DEBUG
    std::cout<<"complete. "<< "Time taken = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "ms\n";
    #else
    std::cout<<"Second tier complete. Time taken = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "ms\n";
    #endif // DEBUG
    return solution;
}


/** this member function should only be called by an AP node during TDMA */
std::list<int> node::dynamic_resource_allocation(std::vector<int>& candidate){

    /* a list of pair<UE_id, list_of_candidate_schemes_of_this_UE> */
    std::list<std::pair<int, std::list<mod_scheme*>>> all_candidate_mod_scheme_set;

    // sort UEs according to channel in desc order!! (UE 1 has highest channel)
    std::sort(candidate.begin(),candidate.end(),sort_candidate(this->id));

    /* first tier RA: generate mod schemes for each UE */
    #ifdef DEBUG
    std::cout<<"-- First tier...";
    #endif // DEBUG
    for(int candidate_id:candidate){
        std::pair<int,std::list<mod_scheme*>> candidate_mod_schemes_perUE;
        candidate_mod_schemes_perUE.first = candidate_id;
        candidate_mod_schemes_perUE.second = ra_first_tier(candidate_id);
        all_candidate_mod_scheme_set.push_back(candidate_mod_schemes_perUE);
    }
    #ifdef DEBUG
    std::cout<<"complete.\n";
    #endif // DEBUG


    /* second tier RA: select scheme combination */
    mod_scheme_combi solution = this->ra_second_tier(all_candidate_mod_scheme_set);

    if (solution.mod_schemes_each_UE.size() != candidate.size()) {
        std::cout<<"Solution size "<< solution.mod_schemes_each_UE.size() <<" != candidate number "<< candidate.size() << ". Abort\n";
        exit(1);
    }

    #ifdef DEBUG
    std::cout<<"Result: ";
    auto it = candidate.begin();
    for(mod_scheme* m_sch : solution.mod_schemes_each_UE){
        if(it != candidate.end()){
           std::cout<<"\nUE "<<*it++<<"\t ";

        } else {
            std::cout<<"\nUE ??? \t ";
            exit(1);
        }
        printSch(m_sch); std::cout<<' ';
        if (m_sch) {
            std::cout<<"p="<<m_sch->required_power;
            std::cout<<"\t| r="<<m_sch->sum_throughput;
        }
    }
    std::cout<<"\nTotal power = "<<solution.required_power<<"\t| Sum rate = "<<solution.sum_throughput<<"\n";
    #endif // DEBUG

    std::list<int> accepted;
    auto UEid_it = candidate.begin(); auto solution_it = solution.mod_schemes_each_UE.begin();
    while(UEid_it!=candidate.end() && solution_it!=solution.mod_schemes_each_UE.end()){
        if (*solution_it==NULL) {
            /* UE <*UEid_it> rejected! */
            std::cout<<"UE "<<*UEid_it<<" rejected.\n";
        } else {
            /*--- save the mod scheme result ---*/
            UE_scheme curScheme;
            curScheme.UE_id = *UEid_it;
            curScheme.modulation_scheme = *solution_it;
            this->mod_scheme_assignment.push_back(curScheme);
            /*--- accumulate (shannon) throughput for UE ---*/
            node::receiver[*UEid_it]->sum_throughput += (*solution_it)->sum_throughput;

            /*--- for return ---*/
            accepted.push_back(*UEid_it);
        }
        UEid_it++;
        solution_it++;
    }

    /*------- randomly decide which UEs get accepted --------------
    std::uniform_real_distribution<double> unif(0,1.0);
    std::default_random_engine re(std::chrono::system_clock::now().time_since_epoch().count());

    std::list<int> accepted;
    for(int i:candidate) accepted.push_back(i);
    //std::cout<<"-- Accepted: "; for(int i:accepted)std::cout<<i<<' '; std::cout<<'\n';
    for(int i : candidate){
        double chance = unif(re);
        //std::cout<<"-- -- Chance="<<chance<<std::endl;
        if(chance<0.5) accepted.remove(i);
    }*/
    return accepted;
}

