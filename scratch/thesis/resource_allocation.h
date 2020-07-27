#include <list>
#ifndef RESOURCE_ALLOCATION

struct mod_scheme{
	double sum_throughput;
	double required_power;
	std::list<int> modes_each_layer; // mode 0 means no mod selected for that layer!!
	mod_scheme(){};
	mod_scheme(const mod_scheme* scheme){ // copy constructor
	    this->modes_each_layer = scheme->modes_each_layer;
	    this->sum_throughput = scheme->sum_throughput;
	    this->required_power = scheme->required_power;
	}
};

struct mod_scheme_combi{
    double sum_throughput;
    double required_power;
    std::list<mod_scheme*> mod_schemes_each_UE;
    mod_scheme_combi(){ // init
        this->sum_throughput = 0.0;
        this->required_power = 0.0;
        this->mod_schemes_each_UE.clear();
    }
    void copyFrom(const mod_scheme_combi* src){
        this->sum_throughput = src->sum_throughput;
        this->required_power = src->required_power;
        this->mod_schemes_each_UE = src->mod_schemes_each_UE;
    }
};

double SINR_single_layer(const int& m);

double power_required(const int& AP_id, const int& UE_id, const mod_scheme &candidate_scheme, const double &prev_UEs_power);

double minPower(const int &AP_id, const int &UE_id);

#define RESOURCE_ALLOCATION
#endif // RESOURCE_ALLOCATION
