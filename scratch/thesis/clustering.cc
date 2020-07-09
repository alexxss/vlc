#ifndef CLUSTERING
#include "clustering.h"
#endif // CLUSTERING
#ifndef ENVIRONMENT
#include "global_environment.h"
#endif // ENVIRONMENT

#include<iostream>
#include<fstream> // file stream

void UE_send_requests(){
    std::cout << "1. UE -> AP (channel threshold)\n";
    for(int i=0;i<node::UE_number;i++){
        // check all APs, if channel > threshold send request
        for(int j=0; j<g_AP_number; j++){
            if (node::channel[j][i]>g_channel_threshold)
                node::transmitter[j]->receiveRequest(i);
        }
    }
}

void AP_send_requests();

void both_sides_connect(){
    std::ofstream fout("AP_UE_pair.dat");
    if (fout.is_open()){
        for(int i=0;i<g_AP_number;i++){
            for(int j=0; j<node::UE_number; j++){
                if (node::transmitter[i]->hasRequest(j) && node::receiver[j]->hasRequest(i)){
                    node::transmitter[i]->connect(j);
                    node::receiver[j]->connect(i);

                    /*--------------save to file for plot------------------
                    repeats following output for every line segment (AP-centric)
                       <AP x> <AP y>
                       <UE x> <UE y>
                       <empty line>
                     run in cmd: `gnuplot -p plot_pair.gnu` to view plot
                    -------------------------------------------------------*/
                    fout<<node::transmitter[i]->location.first<<' '<<node::transmitter[i]->location.second<<std::endl;
                    fout<<node::receiver[j]->location.first<<' '<<node::receiver[j]->location.second<<std::endl<<std::endl;
                }
            }
        }
        fout.close();
    } else
        std::cout<<"Cant open file \"AP_UE_pair.dat\""<<std::endl;
}
