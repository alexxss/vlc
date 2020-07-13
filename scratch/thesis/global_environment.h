#ifndef ENVIRONMENT
#define ENVIRONMENT

constexpr int g_max_epoch = 1;

/*---------------------------------------------------
                ENVIRONMENT CONSTANTS
room size 15*15*3
+ AP
  - number 8*8=64
  - height 2.5m
+ UE
  - number 10-70 step 10
  - height 0.85m
---------------------------------------------------*/
constexpr double g_room_dim = 15;
constexpr double g_AP_height = 2.5;
constexpr double g_UE_height = 0.85;

constexpr int g_UE_max = 70; // range 10-70 step 10
constexpr int g_AP_number = 64; // should be 8*8=64
constexpr int g_AP_per_row = 8;

constexpr double g_channel_threshold = 0.000001;
constexpr double g_distance_threshold = 5.0;

/*---------------------------------------------------
                CHANNEL CONSTANTS
FOV range 70-130 step 10
semi-angle at half-illumination (phi_1/2) 60
gain of optical filter (g_of(psi))         1
gain of optical concentrator (g_oc(psi))   1
physical area for PD receiver              1 cm^2 = 0.0001 m^2
reflection efficiency (rho)                0.75
---------------------------------------------------*/
constexpr double g_field_of_view = 100;
constexpr int g_PHI_half = 60;
constexpr int g_filter_gain = 1;
constexpr int g_concentrator_gain = 1;
constexpr double g_receiver_area = 0.0001;
constexpr double g_reflect_efficiency = 0.75;

/*---------------------------------------------------
           FREQUENCY REUSE CONSTANTS (?)
reuse factor (rho, number of resource blocks)    1-4 step 1
total bandiwdth (B)                              50 MHz
---------------------------------------------------*/

constexpr int g_frequency_reuse_factor = 2; // RB id range from 0 to g_frequency_reuse_factor-1
constexpr double g_total_bandwidth = 50;
constexpr double g_bandwidth_per_rb = g_total_bandwidth / g_frequency_reuse_factor;

/*---------------------------------------------------
                TDMA CONSTANT
maximum load per AP (Gamma)    2-5 step 1
---------------------------------------------------*/
constexpr int g_maximum_load_per_AP = 3;
constexpr double g_total_time = 1.0; // unit is "second"

/*---------------------------------------------------
              Resource Allocation CONSTANT
power constraint                             10 W per AP
minimum rate requirement                     0-150 Mbits/s
backhaul constraint                          1 Gbit/s = 1e3Mbit/s = 1e9 bit/s
number of rate limit levels (J)              100
number of power limit levels (I)             20
number of layers (L)                         4
number of available modulation modes         5 (BPSK, 4-QAM, 16-QAM, 64-QAM, 256-QAM)
BER                                          0.00001
AWGN power spectral density N_0              10^-22 A2/Hz = 10^-16 A2/MHz
----------------------------------------------------*/
constexpr double g_P_max = 10; // W
constexpr double g_R_max = 1000; // Mbit per second
constexpr double g_R_min = 150;
constexpr int g_J = 100;
constexpr int g_I = 20;
constexpr int g_L = 4;
constexpr int g_M = 5;
constexpr double g_BER = 0.00001;
constexpr double g_N_0 = 0.0000000000000001;
#endif //ENVIRONMENT
