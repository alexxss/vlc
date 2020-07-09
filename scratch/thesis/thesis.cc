/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

//
// Network topology
//
//           10Mb/s, 10ms       10Mb/s, 10ms
//       n0-----------------n1-----------------n2
//
//
// - Tracing of queues and packet receptions to file
//   "tcp-large-transfer.tr"
// - pcap traces also generated in the following files
//   "tcp-large-transfer-$n-$i.pcap" where n and i represent node and interface
// numbers respectively
//  Usage (e.g.): ./waf --run tcp-large-transfer
#include <ctype.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cassert>

#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
//#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/vlc-channel-helper.h"
#include "ns3/vlc-device-helper.h"
#include "ns3/netanim-module.h"

#include "global_environment.h"
#include "algorithm.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VisibleLightCommunication");

void PrintPacketData(Ptr<const Packet> p, uint32_t size);

// The number of bytes to send in this simulation.
static const uint32_t totalTxBytes = 80000;
static uint32_t currentTxBytes = 0;

// Perform series of 1040 byte writes (this is a multiple of 26 since
// we want to detect data splicing in the output stream)
static const uint32_t writeSize = 2048;
uint8_t data[writeSize];

// These are for starting the writing process, and handling the sending
// socket's notification upcalls (events).  These two together more or less
// implement a sending "Application", although not a proper ns3::Application
// subclass.

void StartFlow(Ptr<Socket>, Ipv4Address, uint16_t);

void WriteUntilBufferFull(Ptr<Socket>, uint32_t);

std::vector<double> Received(1, 0);
std::vector<double> theTime(1, 0);
//////////////////////////////////////
//Function to generate signals.
std::vector<double>& GenerateSignal(int size, double dutyRatio);

static void RxEnd(Ptr<const Packet> p) { // used for tracing and calculating throughput

	//PrintPacketData(p,p->GetSize());

	Received.push_back(Received.back() + p->GetSize()); // appends on the received packet to the received data up until that packet and adds that total to the end of the vector
	theTime.push_back(Simulator::Now().GetSeconds()); // keeps track of the time during simulation that a packet is received
	//NS_LOG_UNCOND("helooooooooooooooooo RxEnd");
}

static void TxEnd(Ptr<const Packet> p) { // also used as a trace and for calculating throughput

	Received.push_back(Received.back() + p->GetSize()); // same as for the RxEnd trace
	theTime.push_back(Simulator::Now().GetSeconds()); 	//
	//NS_LOG_UNCOND("helooooooooooooooooo TxEnd");
}

static void CwndTracer(uint32_t oldval, uint32_t newval) {
	NS_LOG_INFO("Moving cwnd from " << oldval << " to " << newval);
}

/*          init node static member       */
int node::UE_number = 0;
node* node::transmitter[g_AP_number] = {0};
node* node::receiver[g_UE_max] = {0};
double node::channel[g_AP_number][g_UE_max] = {0};

int main(int argc, char *argv[]) {
	// Users may find it convenient to turn on explicit debugging
	// for selected modules; the below lines suggest how to do this
	//  LogComponentEnable("TcpSocketImpl", LOG_LEVEL_ALL);
	//  LogComponentEnable("PacketSink", LOG_LEVEL_ALL);            // uncomment in original example
	//  LogComponentEnable("TcpLargeTransfer", LOG_LEVEL_ALL);

    //parameters:
	double PhotoDetectorArea = g_receiver_area; 	// to set the photo dectror area
	double Band_factor_Noise_Signal = (10.0); //?

	CommandLine cmd;
	cmd.Parse(argc, argv);

	// initialize the tx buffer.
	for (uint32_t i = 0; i < writeSize; ++i) {
		char m = toascii(97 + i % 26);
		data[i] = m;
	}

  	for (node::UE_number = 10; node::UE_number<=g_UE_max; node::UE_number+=10) {

        algorithm Algorithm;
        Algorithm.fullAlgorithm();

		// Here, we will explicitly create three nodes.  The first container contains
		// nodes 0 and 1 from the diagram above, and the second one contains nodes
		// 1 and 2.  This reflects the channel connectivity, and will be used to
		// install the network interfaces and connect them with a channel.
        NodeContainer allAP;
        allAP.Create(g_AP_number);
        NodeContainer allUE;
        allUE.Create(node::UE_number);

		/////////////// location for AP ///////////////
		Ptr < ListPositionAllocator > m_listPosition = CreateObject< ListPositionAllocator >();
		for(node* n : node::transmitter){
            double x = n->location.first;
            double y = n->location.second;
            m_listPosition->Add(Vector(x,y,g_AP_height));
		}

        /*--- read from .dat version ---*/
        /*std::ifstream fin("../dat/AP_point.dat");
        if(fin.is_open()){
            int cnt=0;
            while(cnt<g_AP_number-1 && fin){
                double x,y;
                fin>>cnt>>x>>y;
                m_listPosition->Add(Vector(x,y, g_AP_height));
            }
            if (cnt!=g_AP_number-1) {
                std::cout<<"AP location not enough? Should get "<<g_AP_number-1<<", got "<<cnt<<" instead";
                exit(1);
            }
            fin.close();
        } else {
            std::cout<<"CANT OPEN AP_POINT FILE";
            exit(1);
        }*/

        ///////////// location for UE ///////////////
        for(node* n : node::receiver){
            if (!n) break;
            double x = n->location.first;
            double y = n->location.second;
            m_listPosition->Add(Vector(x,y,g_UE_height));
        }
        /*--- read from dat version ---*/
        /*std::ifstream fin("../dat/UE_point.dat");
        fin.open("../dat/UE_point.dat");
        if(fin.is_open()){
            int cnt=0;
            while(cnt<node::UE_number-1 && fin){
                double x,y;
                fin>>cnt>>x>>y;
                m_listPosition->Add(Vector(x,y, g_UE_height));
            }
            if (cnt!=node::UE_number-1) {
                std::cout<<"UE location not enough? Should get "<<node::UE_number-1<<", got "<<cnt<<" instead";
                exit(1);
            }
            fin.close();
        } else {
            std::cout<<"CANT OPEN UE_POINT FILE";
            exit(1);
        }*/

        MobilityHelper mobility;
		mobility.SetPositionAllocator(m_listPosition);
		mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel"); // all nodes are constant
		mobility.Install(allAP);
		mobility.Install(allUE);

		VlcDeviceHelper devHelperVPPM; //VLC Device Helper to manage the VLC Device and device properties.

        devHelperVPPM.CreateTransmitter("THE_TRANSMITTER1");
		/****************************************TX-SIGNAL************************************
		 * 1000-SIGNAL SIZE, 0.5-DUTY RATIO, 0-BIAS, 9.25E-5-PEAK VOLTAGE, 0-MINIMUM VOLTAGE
		 ************************************************************************************/
		devHelperVPPM.SetTXSignal("THE_TRANSMITTER1", 1000, 0.5, 0, 9.25e-5, 0);
		devHelperVPPM.SetTrasmitterParameter("THE_TRANSMITTER1", "Bias", 0);//SETTING BIAS VOLTAGE MOVES THE SIGNAL VALUES BY THE AMOUNT OF BIASING
		devHelperVPPM.SetTrasmitterParameter("THE_TRANSMITTER1", "SemiAngle", 35);
		devHelperVPPM.SetTrasmitterParameter("THE_TRANSMITTER1", "Azimuth", 0);
		devHelperVPPM.SetTrasmitterParameter("THE_TRANSMITTER1", "Elevation",180.0);
		devHelperVPPM.SetTrasmitterPosition("THE_TRANSMITTER1", 0.0, 0.0, 52.0);
		devHelperVPPM.SetTrasmitterParameter("THE_TRANSMITTER1", "Gain", 70);
		devHelperVPPM.SetTrasmitterParameter("THE_TRANSMITTER1", "DataRateInMBPS",0.3);


		// netAnim is shitty as FUCK i'd rather use gnuplot if i dont need animation. or maybe im just dumb.
		AnimationInterface anim("../dat/visible-light-communication.xml");

		Simulator::Stop(Seconds(5.0));
		Simulator::Run();
        Simulator::Destroy();

        node::UE_number=80;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//begin implementation of sending "Application"
void StartFlow(Ptr<Socket> localSocket, Ipv4Address servAddress,
		uint16_t servPort) {
	//NS_LOG_UNCOND("helooooooooooooooooo StartFlow");
	localSocket->Connect(InetSocketAddress(servAddress, servPort)); //connect

	// tell the tcp implementation to call WriteUntilBufferFull again
	// if we blocked and new tx buffer space becomes available
	localSocket->SetSendCallback(MakeCallback(&WriteUntilBufferFull));
	WriteUntilBufferFull(localSocket, localSocket->GetTxAvailable());
}

void WriteUntilBufferFull(Ptr<Socket> localSocket, uint32_t txSpace) {
	//NS_LOG_UNCOND("helooooooooooooooooo WriteUntilBufferFull");
	while (currentTxBytes < totalTxBytes && localSocket->GetTxAvailable() > 0) {

		uint32_t left = totalTxBytes - currentTxBytes;
		uint32_t dataOffset = currentTxBytes % writeSize;
		uint32_t toWrite = writeSize - dataOffset;
		toWrite = std::min (toWrite, left);
		toWrite = std::min (toWrite, localSocket->GetTxAvailable ());

		Ptr<Packet> p = Create<Packet>(&data[dataOffset], toWrite);
		Ptr<Node> startingNode = localSocket->GetNode();
		Ptr<VlcTxNetDevice> txOne = DynamicCast<VlcTxNetDevice>(startingNode->GetDevice(0) );
		txOne->EnqueueDataPacket(p);

		int amountSent = localSocket->Send (&data[dataOffset], toWrite, 0);
		if(amountSent < 0)
		{
			// we will be called again when new tx space becomes available.
			return;
		}

		currentTxBytes += amountSent;
	}

	localSocket->Close();
}
/*
std::vector<double>& GenerateSignal(int size, double dutyRatio) {
	std::vector<double> *result = new std::vector<double>();
	result->reserve(size);

	double bias = 0;
	double Vmax = 4.5;
	double Vmin = 0.5;

	for (int i = 0; i < size; i++) {
		if (i < size * dutyRatio) {
			result->push_back(Vmax + bias);
		} else {
			result->push_back(Vmin + bias);
		}
	}

	return *result;
}

void PrintPacketData(Ptr<const Packet> p, uint32_t size) {
	uint8_t *data = new uint8_t[size];

	p->CopyData(data, size);

	for (uint32_t i = 0; i < size; i++) {
		std::cout << (int) data[i] << " ";
	}

	std::cout << std::endl;

	delete[] data;

}
*/
