#ifndef CHANNEL
#include "channel.h"
#endif
#ifndef ENVIRONMENT
#include "global_environment.h"
#endif // ENVIRONMENT

#define PI 3.14159265359

#include<math.h> // for math stuff..

// <math.h> trigonometry functions accept angles in RADIAN!!!!
// remember to convert
double RtoD(const double& radian){
    return radian * 180 / PI;
}
double DtoR(const double& degree){
    return degree * PI / 180;
}

/**
* \brief calculates angle of incidence between an AP and a UE
* \return the angle of incidence in RADIAN
*/
double get_angle_of_incidence(node* nodeAP, node* nodeUE){
    const double height_diff = g_AP_height - g_UE_height;
    const double plane_dist = nodeAP->plane_distance(nodeUE);
    const double hypotenuse = nodeAP->euclidean_distance(nodeUE);

    // angle of incidence = angle between <height diff> and <hypotenuse>
    const double angle = acos((pow(height_diff,2) + pow(hypotenuse,2) - pow(plane_dist,2)) / (2*height_diff*hypotenuse));
    return angle;
}

/**
* \brief calculates the channel betwen an AP and a UE
*
* gets the angle of incidence first, and checks if it is within FOV.
* Note: alpha (order of Lambertian emission) would be negative so I used abs().
* (but I have never seen any paper use abs() when calculating order of Lambertian emission...)
*
* \return the channel
*/
double calculate_one_channel(node* nodeAP, node* nodeUE){
    const double angle_of_incidence = get_angle_of_incidence(nodeAP,nodeUE); // IMPORTANT: this is in RADIAN
    // no channel if exceeds FOV
    if (RtoD(angle_of_incidence)>=(g_field_of_view/2))
        return 0.0;

    // TODO (alex#2#): alpha should be global constant to reduce computation.
    const double alpha = abs(log(2)/log(cos(DtoR(g_PHI_half))));
    const double angle_of_irradiance = angle_of_incidence;
    double ch = (alpha + 1) * g_receiver_area / (2 * PI * pow(nodeAP->euclidean_distance(nodeUE),2)); // first term
    ch = ch * pow(cos(angle_of_irradiance),alpha);// second term
    ch = ch * g_filter_gain * g_concentrator_gain; // third and fourth term
    ch = ch * cos(angle_of_incidence); // last term
    return ch;
}

void calculate_all_channel(){
    for(int i=0; i<g_AP_number;i++){
        for(int j=0; j<node::UE_number;j++){
            node::channel[i][j]=calculate_one_channel(node::transmitter[i],node::receiver[j]);
        }
    }
}

