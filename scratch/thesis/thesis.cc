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

std::string makeName(const std::string& devType, const int& APid, const int& UEid){
    std::string DeviceName = devType + "-" + std::to_string(APid) + "-" + std::to_string(UEid);
    return DeviceName;
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
//	  LogComponentEnable("PacketSink", LOG_LEVEL_ALL);            // uncomment in original example
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

  	for (node::UE_number = 20; node::UE_number<=g_UE_max; node::UE_number+=10) {

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

        ///////////// location for UE ///////////////
        for(node* n : node::receiver){
            if (!n) break;
            double x = n->location.first;
            double y = n->location.second;
            m_listPosition->Add(Vector(x,y,g_UE_height));
        }

        MobilityHelper mobility;
		mobility.SetPositionAllocator(m_listPosition);
		mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel"); // all nodes are constant
		mobility.Install(allAP);
		mobility.Install(allUE);

		VlcDeviceHelper devHelperVPPM; //VLC Device Helper to manage the VLC Device and device properties.
        VlcChannelHelper chHelper;

        NodeContainer UEcluster[g_AP_number];
  		NetDeviceContainer dev[g_AP_number]; // all dev (tx & rx) in APn's cluster
        /*--- setup transmitter, receiver, channel for every paired AP & UE ---*/
		for(node* nAP : node::transmitter){
            for(int UEid : nAP->get_connected()){
                UEcluster[nAP->id].Add(allUE.Get(UEid)); // add this UE into AP's cluster container

                std::string transmitterName = makeName("TRANSMITTER", nAP->id, UEid); // device name: "TRANSMITTER-<APid>-<UEid>"
                devHelperVPPM.CreateTransmitter(transmitterName);
                /****************************************TX-SIGNAL************************************
                 * 1000-SIGNAL SIZE, 0.5-DUTY RATIO, 0-BIAS, 9.25E-5-PEAK VOLTAGE, 0-MINIMUM VOLTAGE
                 ************************************************************************************/
                devHelperVPPM.SetTXSignal(transmitterName, 1000, 0.5, 0, 9.25e-5, 0);
                devHelperVPPM.SetTrasmitterParameter(transmitterName, "Bias", 0);//SETTING BIAS VOLTAGE MOVES THE SIGNAL VALUES BY THE AMOUNT OF BIASING
                devHelperVPPM.SetTrasmitterParameter(transmitterName, "SemiAngle", 35);
                devHelperVPPM.SetTrasmitterParameter(transmitterName, "Azimuth", 0);
                devHelperVPPM.SetTrasmitterParameter(transmitterName, "Elevation",180.0);
                devHelperVPPM.SetTrasmitterPosition(transmitterName, 0.0, 0.0, 0.0);
                devHelperVPPM.SetTrasmitterParameter(transmitterName, "Gain", 70);
                devHelperVPPM.SetTrasmitterParameter(transmitterName, "DataRateInMBPS",0.3);
                //Creating and setting properties for the first receiver.
                std::string receiverName = makeName("RECEIVER", nAP->id, UEid);
                devHelperVPPM.CreateReceiver(receiverName);
                devHelperVPPM.SetReceiverParameter(receiverName, "FilterGain", 1);
                devHelperVPPM.SetReceiverParameter(receiverName, "RefractiveIndex", 1.5);
                devHelperVPPM.SetReceiverParameter(receiverName, "FOVAngle", 100); // original example is 28.5
                devHelperVPPM.SetReceiverParameter(receiverName, "ConcentrationGain", 0);
                devHelperVPPM.SetReceiverParameter(receiverName, "PhotoDetectorArea",1.3e-5);
                devHelperVPPM.SetReceiverParameter(receiverName, "RXGain", 0);
                devHelperVPPM.SetReceiverParameter(receiverName, "PhotoDetectorArea",PhotoDetectorArea);
                devHelperVPPM.SetReceiverParameter(receiverName, "Beta", 1);
                devHelperVPPM.SetReceiverPosition(receiverName, 0.0, 0.0, 0.0);
                /****************************************MODULATION SCHEME SETTINGS******************
                *AVILABLE MODULATION SCHEMES ARE: [1]. VlcErrorModel::PAM4, [2]. VlcErrorModel::OOK [3]. VlcErrorModel::VPPM
                *AVILABLE MODULATION SCHEMES ARE: [4]. VlcErrorModel::PSK4 [5]. VlcErrorModel::PSK16. [6]. VlcErrorModel::QAM4 [7]. VlcErrorModel::QAM16.
                ************************************************************************************/
                devHelperVPPM.SetReceiverParameter(receiverName, "SetModulationScheme",VlcErrorModel::QAM4);
                //devHelperVPPM.SetReceiverParameter("THE_RECEIVER1", "DutyCycle", 0.85); //Dutycycle is only valid for VPPM

                std::string channelName = makeName("CHANNEL",nAP->id, UEid);
                chHelper.CreateChannel(channelName);
                chHelper.SetPropagationLoss(channelName, "VlcPropagationLoss");
                chHelper.SetPropagationDelay(channelName, 2);
                chHelper.AttachTransmitter(channelName, transmitterName,&devHelperVPPM);
                chHelper.AttachReceiver(channelName, receiverName, &devHelperVPPM);
                chHelper.SetChannelParameter(channelName, "TEMP", 295);
                chHelper.SetChannelParameter(channelName, "BAND_FACTOR_NOISE_SIGNAL",Band_factor_Noise_Signal );
                chHelper.SetChannelWavelength(channelName, 380, 780);
                chHelper.SetChannelParameter(channelName, "ElectricNoiseBandWidth",3 * 1e5);
                dev[nAP->id].Add(chHelper.Install(allAP.Get(nAP->id), allUE.Get(UEid),
                                &devHelperVPPM, &chHelper, transmitterName, receiverName, channelName)) ;
            }
		}

        // Now add ip/tcp stack to all nodes.
        InternetStackHelper internet;
        internet.InstallAll();

        // add IP addresses
        Ipv4AddressHelper ipv4;
        Ipv4InterfaceContainer ipInterfs;
        ApplicationContainer apps;
        for(int APn=0; APn<g_AP_number; APn++){
            if (dev[APn].GetN()==0) continue;
            std::string addressStr = "192.168." + std::to_string(APn) + ".0"; // base addr: 192.168.<APid>.0
            ipv4.SetBase(addressStr.c_str(),"255.255.255.0");
            ipInterfs = ipv4.Assign(dev[APn]);

            // Create a source to send packets from n0.  Instead of a full Application
            // and the helper APIs you might see in other example files, this example
            // will use sockets directly and register some socket callbacks as a sending
            // "Application".
            // Create and bind the socket...
//            Ptr < Socket > localSocket = Socket::CreateSocket(allAP.Get(APn), TcpSocketFactory::GetTypeId());
//            Socket::Bind
//            localSocket->Bind();

//            LogComponentEnable("TcpSocketBase",LOG_FUNCTION);
            /*--- setup sink ---*/
            uint16_t servPort = 4000;
            PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(),servPort));
            apps.Add(sink.Install(UEcluster[APn]));

            /*--- setup socket (to send data???) ---*/
            for(int i = 1; i<ipInterfs.GetN(); i+=2){
                std::cout<<"Schedule AP "<<APn<<" ("<<ipInterfs.GetAddress(i-1)<<") with addr "<<ipInterfs.GetAddress(i)<<'\n';
                Ptr < Socket > localSocket = Socket::CreateSocket(allAP.Get(APn), TcpSocketFactory::GetTypeId());
                localSocket->Bind();
                localSocket->BindToNetDevice(dev[APn].Get(i-1));
                // ...and schedule the sending "Application"; This is similar to what an
                // ns3::Application subclass would do internally.
                Simulator::ScheduleNow(&StartFlow, localSocket, ipInterfs.GetAddress(i), servPort);
            }
//            LogComponentDisable("TcpSocketBase",LOG_FUNCTION);
        }

        // setup ip routing table
        Ipv4GlobalRoutingHelper::PopulateRoutingTables();

        // setup packet sink
//        uint16_t servPort = 4000;
//        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), servPort));
//        ApplicationContainer apps = sink.Install(allUE);

        apps.Start(Seconds(0.0));
        apps.Stop(Seconds(4.0));

        /*--- set up callback trace ---*/
        for(node* nAP : node::transmitter){
            for(int iUE : nAP->get_connected()){
                std::string receiverName = makeName("RECEIVER",nAP->id,iUE);
                Ptr<VlcRxNetDevice> rxPtr = devHelperVPPM.GetReceiver(receiverName);
                rxPtr->TraceConnectWithoutContext("PhyRxEnd", MakeCallback(&RxEnd)); //traces to allow us to see what and when data is sent through the network
            }
        }

        //Ask for ASCII and pcap traces of network traffic
        AsciiTraceHelper ascii;

        chHelper.EnableAsciiAll (ascii.CreateFileStream ("./log/tcp-large-transfer.tr"));
        chHelper.EnablePcapAll ("./log/tcp-large-transfer"); // call VlcChannelHelper::EnablePcapAll

		// netAnim is shitty as FUCK i'd rather use gnuplot if i dont need animation. or maybe im just dumb.
		//AnimationInterface anim("../dat/visible-light-communication.xml");

		Simulator::Stop(Seconds(5.0));
		Simulator::Run();

        double throughput = ((Received.back() * 8)) / theTime.back(); // bps
        std::cout << "throughput value is " << throughput << std::endl;

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
	NS_LOG_DEBUG("AP "<<localSocket->GetNode()->GetId()<<" Start flow to "<<servAddress<<'\n');

	//NS_LOG_UNCOND("helooooooooooooooooo StartFlow");
	if (localSocket->Connect(InetSocketAddress(servAddress, servPort)) == -1) std::cout<<"Failed to connect!\n"; //connect

	// tell the tcp implementation to call WriteUntilBufferFull again
	// if we blocked and new tx buffer space becomes available
	localSocket->SetSendCallback(MakeCallback(&WriteUntilBufferFull));
	WriteUntilBufferFull(localSocket, localSocket->GetTxAvailable());
}

void WriteUntilBufferFull(Ptr<Socket> localSocket, uint32_t txSpace) {
	//NS_LOG_UNCOND("helooooooooooooooooo WriteUntilBufferFull");
	NS_LOG_DEBUG("AP "<<localSocket->GetNode()->GetId()<<" writing... ");

	while (currentTxBytes < totalTxBytes && localSocket->GetTxAvailable() > 0) {

		uint32_t left = totalTxBytes - currentTxBytes;
		uint32_t dataOffset = currentTxBytes % writeSize;
		uint32_t toWrite = writeSize - dataOffset;
		toWrite = std::min (toWrite, left);
		toWrite = std::min (toWrite, localSocket->GetTxAvailable ());

		Ptr<Packet> p = Create<Packet>(&data[dataOffset], toWrite);
//		Ptr<Node> startingNode = localSocket->GetNode();
//		Ptr<VlcTxNetDevice> txOne = DynamicCast<VlcTxNetDevice>(startingNode->GetDevice(0) );
        Ptr<VlcTxNetDevice> txOne = DynamicCast<VlcTxNetDevice>(localSocket->GetBoundNetDevice());
		txOne->EnqueueDataPacket(p);

		int amountSent = localSocket->Send (&data[dataOffset], toWrite, 0);
		if(amountSent < 0)
		{
			// we will be called again when new tx space becomes available.
			NS_LOG_DEBUG(" buffer full. Wait. \n");
			return;
		}
        NS_LOG_DEBUG(amountSent<<" bytes... ");
		currentTxBytes += amountSent;
	}
	NS_LOG_DEBUG("complete. \n");
    currentTxBytes = 0;
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
