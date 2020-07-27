#include "algorithm.h"
#include <chrono> // for time taken & rand seed
#include <random> // uniform_real_distribution
#include <fstream>
#include <iostream>

void save_throughputUE(){
    /*--------------save to file for plot------------------
     columns: <x> <y> <throughput>
     run in cmd: `gnuplot -p          .gnu` to view plot
    -------------------------------------------------------*/
    std::ofstream fout("./log/UE_throughput.dat");
    if (fout.is_open()){
        for(node* n : node::receiver){
            if (!n) break;
              fout<<n->location.first<<' '<<n->location.second<<' '<<n->sum_throughput<<'\n';
        }
        fout.close();
    } else std::cout<<"Can\'t open ./log/UE_throughput.dat :\(\n";
}

void save_idleNode(){
    /*--------------save to file for plot------------------
     columns: <x> <y>
     run in cmd: `gnuplot -p plot_pairWithOutage.gnu` to view plot
    -------------------------------------------------------*/
    std::ofstream fout("./log/UE_off.dat");
    if (fout.is_open()){
        for(node* n : node::receiver)
            if (!n) break;
            else if (n->get_connected().size()==0)
                fout<<n->location.first<<' '<<n->location.second<<'\n';
        fout.close();
    } else std::cout<<"Can\'t open ./log/UE_off.dat :\(\n";

    fout.open("./log/AP_off.dat");
    if (fout.is_open()){
        for(node* n : node::transmitter){
            if (n->get_connected().size()==0)
                fout<<n->location.first<<' '<<n->location.second<<'\n';
        }
        fout.close();
    } else std::cout<<"Can\'t open ./log/AP_off.dat :\(\n";
}

void save_finalClusterRelationship(){
    /*--------------save to file for plot------------------
    repeats following output for every line segment
       <AP x> <AP y>
       <UE x> <UE y>
       <empty line>
     run in cmd: `gnuplot -p plot_pairWithOutage.gnu` to view plot
    -------------------------------------------------------*/
    std::ofstream fout("./log/AP_UE_pairFinal.dat");
    if (fout.is_open()){
        for(node* AP : node::transmitter){
            for(int UEid : AP->get_connected()){
                fout<<AP->location.first<<' '<<AP->location.second<<'\n';
                node* UE = node::receiver[UEid];
                fout<<UE->location.first<<' '<<UE->location.second<<"\n\n";
            }
        }
        fout.close();
    } else std::cout<<"Can\'t open ./log/AP_UE_pairFinal.dat :\(\n";
}

/**
\brief saves UE & AP location to file for plot
\param receiver array
\param transmitter array
*/
void save_room_data(){
    /*--------------save to file for plot------------------
     columns: <node id> <x> <y>
     run in cmd: `gnuplot -p plot_room.gnu` to view plot
    -------------------------------------------------------*/
    std::ofstream fout("./log/UE_point.dat");
    if(fout.is_open()){
        for(int i=0;i<node::UE_number;i++)
            fout<<i<<' '<<node::receiver[i]->location.first<<' '<<node::receiver[i]->location.second<<'\n';
        fout.close();
    }
    fout.open("./log/AP_point.dat");
    if(fout.is_open()){
        for(int i=0;i<g_AP_number;i++){
            fout<<i<<' '<<node::transmitter[i]->location.first<<' '<<node::transmitter[i]->location.second<<'\n';
        }
        fout.close();
    }

}

double algorithm::fullAlgorithm(){

    std::chrono::steady_clock::time_point beginTime = std::chrono::steady_clock::now();
    /*-------------initialization----------------*/

    // generate AP node
    for (int i=0; i<g_AP_number; i++){
        node::transmitter[i] = new node(i);
    }
    // generate UE node
    std::uniform_real_distribution<double> unif(0.0, g_room_dim);
    std::default_random_engine re(std::chrono::system_clock::now().time_since_epoch().count());
    for (int i=0; i<node::UE_number; i++){
        node::receiver[i] = new node(i,unif(re), unif(re)); // UE
    }

    /*--------------save to file for plot------------------
     run in cmd: `gnuplot -p plot_room.gnu` to view plot
    -------------------------------------------------------*/
    save_room_data();

    /*-----------calculate channel---------------*/
    calculate_all_channel();

//    std::cout<<"-----------------CLUSTERING------------------\n";
    /*---- for each UE, send request to APs if channel > threshold ----*/
//    std::cout<<"1. UE->AP (channel threshold="<<g_channel_threshold<<")\n";
    for(int i=0;i<node::UE_number;i++){
        // check all APs, if channel > threshold send request
        for(int j=0; j<g_AP_number; j++){
            if (node::channel[j][i]>g_channel_threshold)
                node::transmitter[j]->receiveRequest(i);
        }
    }

    /*---- for each AP, send request to UEs if distance < threshold ----*/
//    std::cout<<"2. UE<-AP (distance threshold="<<g_distance_threshold<<")\n";
    for(int i=0; i<g_AP_number; i++){
        // check all UEs, if distance<threshold send request
        for(int j=0; j<node::UE_number; j++){
            if (node::transmitter[i]->euclidean_distance(node::receiver[j])<g_distance_threshold)
                node::receiver[j]->receiveRequest(i);
        }
    }

//    std::cout<<"3. Confirmation\n";
    both_sides_connect();

    for(node* n : node::transmitter){
        n->NOMA_sort_UE_desc();
    }

//  ---------------FREQUENCY REUSE---------------
    frequency_reuse(node::transmitter);

//  -------------------TDMA----------------------
    for(node* n : node::transmitter){
        n->tdma(algorithm::TDMAmode, algorithm::RAmode);
    }

    save_idleNode();
    save_finalClusterRelationship();
    save_throughputUE();

    std::chrono::steady_clock::time_point endTime = std::chrono::steady_clock::now();
    std::cout<<"\nResource Allocation complete. \nTotal time taken = "
             << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - beginTime).count() << "ms = "
             << (double) std::chrono::duration_cast<std::chrono::milliseconds>(endTime - beginTime).count()/1000 <<"s = "
             << (double) std::chrono::duration_cast<std::chrono::milliseconds>(endTime - beginTime).count()/60000 << "min\n";
    double sum_throughput = 0.0;
    for(node* n : node::receiver) if (!n) break; else sum_throughput += n->sum_throughput;
    std::cout<<"Achievable sum throughput = "<<sum_throughput<<"Mbps\n";
    algorithm::shannon=sum_throughput;

    /*--------- create solution struct to return ---------------*/
    /* [1] location data */
    /* [2] clustering data */
    /* [3] resource block */
    /* [4] mod scheme & power */

    for(node* nAP : node::transmitter)
        nAP->calculateSINR();

    std::cout<<"SINR matrix\n";
    for(int APid = 0; APid<g_AP_number; APid++){
        for(int UEid=0; UEid<node::UE_number; UEid++){
            std::cout<<node::SINR[APid][UEid]<<'\t';
        }
        std::cout<<'\n';
    }
    std::cout<<"Shannon matrix\n";
    for (node* nAP : node::transmitter){
        for (int u=0; u<node::UE_number; u++){
            std::cout<<nAP->getAchievableRate(u)<<'\t';
        }
        std::cout<<'\n';
    }

    return (double)std::chrono::duration_cast<std::chrono::milliseconds>(endTime-beginTime).count();
}
