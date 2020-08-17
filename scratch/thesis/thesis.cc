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
static const uint64_t totalTxBytes = 8000; //  orig is 80000
static uint32_t currentTxBytes[g_AP_number] = {0};

// Perform series of 1040 byte writes (this is a multiple of 26 since
// we want to detect data splicing in the output stream)
static const uint32_t writeSize = 2048; //2048??? // MSS=588len+header 2byte, max payload=536
uint8_t data[writeSize];

//const int g_max_epoch = 3;

// These are for starting the writing process, and handling the sending
// socket's notification upcalls (events).  These two together more or less
// implement a sending "Application", although not a proper ns3::Application
// subclass.

void StartFlow(Ptr<Socket>, Ipv4Address, uint16_t);

void WriteUntilBufferFull(Ptr<Socket>, uint32_t);

//std::vector<double> Received(1, 0);
std::vector<double> Sent(1,0);
//std::vector<double> theTime(1, 0);
uint64_t iReceived=0.0; double iTime=0.0;
//////////////////////////////////////
//Function to generate signals.
std::vector<double>& GenerateSignal(int size, double dutyRatio);

static void RxEnd(Ptr<const Packet> p) { // used for tracing and calculating throughput

	//PrintPacketData(p,p->GetSize());
//	std::cout<<"Received packet "<<p->GetUid()<<" size "<<p->GetSize()<<" at "<<Simulator::Now().GetSeconds()<<'\n';
//	Received.push_back(Received.back() + p->GetSize()); // appends on the received packet to the received data up until that packet and adds that total to the end of the vector
//	theTime.push_back(Simulator::Now().GetSeconds()); // keeps track of the time during simulation that a packet is received
	//NS_LOG_UNCOND("helooooooooooooooooo RxEnd");

	iReceived += p->GetSize();
	iTime = Simulator::Now().GetSeconds();
}

static void TxEnd(Ptr<const Packet> p) { // also used as a trace and for calculating throughput

//	std::cout<<"Transmit packet "<<p->GetUid()<<" size "<<p->GetSize()<<" at "<<Simulator::Now().GetSeconds()<<'\n';
	Sent.push_back(Sent.back() + p->GetSize()); // same as for the RxEnd trace
//	theTime.push_back(Simulator::Now().GetSeconds()); 	//
	//NS_LOG_UNCOND("helooooooooooooooooo TxEnd");
}

static void CwndTracer(uint32_t oldval, uint32_t newval) {
	NS_LOG_INFO("Moving cwnd from " << oldval << " to " << newval);
}

std::string makeName(const std::string& devType, const int& APid, const int& UEid){
    std::string DeviceName = devType + "-" + std::to_string(APid) + "-" + std::to_string(UEid);
    return DeviceName;
}

void openStream(std::ofstream& ofs, const std::string &filepath, const std::ofstream::openmode &open_mode){
    ofs.open(filepath,open_mode);
    if (!ofs.is_open()){
        std::cout<<"Can't open "<<filepath<<"!! Abort :(\n";
        exit(-1);
    }
}

/*          init node static member       */
int node::UE_number = 50; // default UE_number
node* node::transmitter[g_AP_number] = {0};
node* node::receiver[g_UE_max] = {0};
double node::channel[g_AP_number][g_UE_max] = {0};
double node::SINR[g_AP_number][g_UE_max] = {0};
/*         init algorithm static member         */
bool algorithm::RBmode = false, algorithm::TDMAmode = false, algorithm::RAmode = false;
int algorithm::paramX = g_param_X; // default paramX
double algorithm::shannon = 0.0;
int algorithm::poolDropTriggerCnt = 0;
int algorithm::tdmaOldBreakTriggerCnt = 0;

int main(int argc, char *argv[]) {
	// Users may find it convenient to turn on explicit debugging
	// for selected modules; the below lines suggest how to do this
	//  LogComponentEnable("TcpSocketImpl", LOG_LEVEL_ALL);
//	  LogComponentEnable("PacketSink", LOG_LEVEL_DEBUG);            // uncomment in original example
	//  LogComponentEnable("TcpLargeTransfer", LOG_LEVEL_ALL);

    //parameters:
    std::string varName;
	double PhotoDetectorArea = g_receiver_area; 	// to set the photo dectror area
	double Band_factor_Noise_Signal = (10.0); //?
//	Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue (1400));
//	Config::SetDefault("ns3::TcpSocket::SndBufSize",UintegerValue(totalTxBytes));
//	Config::SetDefault("ns3::TcpSocket::RcvBufSize",UintegerValue(totalTxBytes*2));

	std::string RBmode="GC", TDMAmode="GA", RAmode="BP";

	CommandLine cmd;
	cmd.AddValue("Var_name", "UE_number, AP_load, etc", varName); // used for making output filename
	cmd.AddValue("UE_number", "Set UE_number for current iteration" ,node::UE_number);
	cmd.AddValue("paramX", "Set paramX for current iteration", algorithm::paramX);
	cmd.AddValue("RB_mode", "Set RB_mode for current iteration (WGC/GC)", RBmode);
	cmd.AddValue("TDMA_mode", "Set TDMA_mode for current iteration (GA/RND)", TDMAmode);
	cmd.AddValue("RA_mode", "Set RA_mode for current iteration (HR/BP)", RAmode);
	cmd.Parse(argc, argv);

	std::string filepathprefix = "./log/"+varName+"-"+RBmode+"-"+TDMAmode+"-"+RAmode;
	algorithm::RBmode = (RBmode=="WGC") ? true:false;
	algorithm::TDMAmode = (TDMAmode=="GA") ? true:false;
	algorithm::RAmode = (RAmode=="HR") ? true:false;

	// initialize the tx buffer.
	for (uint32_t i = 0; i < writeSize; ++i) {
		char m = toascii(97 + i % 26);
		data[i] = m;
	}

//  for (node::UE_number = 10; node::UE_number<=g_UE_max; node::UE_number+=10) {
    if (node::UE_number > g_UE_max){
        std::cout<<"UE number "<<node::UE_number<<" > UE max "<<g_UE_max<<"! Abort.\n";
        return -1;
    } else {
//        std::cout<<"------------------------------UE number = "<<node::UE_number<<"------------------------------\n";
//        double aveSumThroughput = 0.0;
//        double aveGoodput = 0.0;
//        double aveTime = 0.0;

        for(int epoch=0; epoch<g_max_epoch; epoch++){
//            std::cout<<"\nEpoch "<<epoch<<"...";
            algorithm Algorithm;
            double algoTime = Algorithm.fullAlgorithm();
            {/* save results to csv */
                // algo time
                std::ofstream timeOfs;
                openStream(timeOfs, filepathprefix+"_time.csv", std::ofstream::app);
                timeOfs<<", "<<algoTime;
                timeOfs.close();
                // total shannon
                std::ofstream shannonOfs;
                openStream(shannonOfs, filepathprefix+"_shannon.csv", std::ofstream::app);
                shannonOfs<<", "<<algorithm::shannon;
                shannonOfs.close();
                // ave connected APs per active UE
                int activeUE = 0, connections = 0;
                for(node* nUE : node::receiver)
                    if (!nUE) break;
                    else {
                        if (nUE->getOnOff()){
                            activeUE ++;
                            connections += nUE->get_connected().size();
                        }
                    }
                if (activeUE>node::UE_number) {
                    std::cout<<"ACTIVE UE > UE NUMBER! something is wrong. abort.\n";
                    exit(1);
                }
                double aveConnNum = (activeUE>0) ? (double)connections/activeUE : 0.0;
                std::ofstream aveConnNumOfs;
                openStream(aveConnNumOfs, filepathprefix+"_aveConnNum.csv", std::ofstream::app);
                aveConnNumOfs<<", "<<aveConnNum;
                aveConnNumOfs.close();
                // drop rate
                std::ofstream dropRateOfs;
                openStream(dropRateOfs, filepathprefix+"_dropRate.csv", std::ofstream::app);;
                dropRateOfs<<", "<<node::UE_number-activeUE;
                dropRateOfs.close();
                // power
                double sumPower = 0.0;
                for(int iAP = 0; iAP<g_AP_number; iAP++){
                    for(int UEid : node::transmitter[iAP]->get_connected()){
                    sumPower += node::transmitter[iAP]->getRequiredPower(UEid);
                    }
                }
                std::ofstream powerOfs;
                openStream(powerOfs, filepathprefix+"_power.csv", std::ofstream::app);
                powerOfs<<", "<<sumPower;
                powerOfs.close();
                // three times drop trigger count
                std::ofstream dropTriggerOfs;
                openStream(dropTriggerOfs, filepathprefix+"_dropTrigCnt.csv", std::ofstream::app);
                dropTriggerOfs<<", "<<algorithm::poolDropTriggerCnt;
                dropTriggerOfs.close();
                // old break trigger count
                std::ofstream oldBreakTrigOfs;
                openStream(oldBreakTrigOfs, filepathprefix+"_oldBreakTrigCnt.csv", std::ofstream::app);
                oldBreakTrigOfs<<", "<<algorithm::tdmaOldBreakTriggerCnt;
                oldBreakTrigOfs.close();
            }

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
                    devHelperVPPM.SetTrasmitterParameter(transmitterName, "SemiAngle", g_PHI_half); // original example is 35
                    devHelperVPPM.SetTrasmitterParameter(transmitterName, "Azimuth", 0);
                    devHelperVPPM.SetTrasmitterParameter(transmitterName, "Elevation",180.0);
                    devHelperVPPM.SetTrasmitterPosition(transmitterName, nAP->location.first, nAP->location.second, g_AP_height);
                    devHelperVPPM.SetTrasmitterParameter(transmitterName, "Gain", node::channel[nAP->id][UEid]); // original example is 70
                    devHelperVPPM.SetTrasmitterParameter(transmitterName, "DataRateInMBPS",nAP->getAchievableRate(UEid)); //orig 0.3
                    devHelperVPPM.SetTrasmitterParameter(transmitterName, "TransmitPower", nAP->getRequiredPower(UEid));

                    //Creating and setting properties for the first receiver.
                    std::string receiverName = makeName("RECEIVER", nAP->id, UEid);
                    devHelperVPPM.CreateReceiver(receiverName);
                    devHelperVPPM.SetReceiverParameter(receiverName, "FilterGain", 1);
                    devHelperVPPM.SetReceiverParameter(receiverName, "RefractiveIndex", 1.5);
                    devHelperVPPM.SetReceiverParameter(receiverName, "FOVAngle", g_field_of_view); // original example is 28.5
                    devHelperVPPM.SetReceiverParameter(receiverName, "ConcentrationGain", 1); // orig 0
                    devHelperVPPM.SetReceiverParameter(receiverName, "PhotoDetectorArea",1); // orig 1.3e-5
                    devHelperVPPM.SetReceiverParameter(receiverName, "RXGain", node::channel[nAP->id][UEid]);
                    devHelperVPPM.SetReceiverParameter(receiverName, "PhotoDetectorArea",PhotoDetectorArea);
                    devHelperVPPM.SetReceiverParameter(receiverName, "Beta", 1);
                    devHelperVPPM.SetReceiverPosition(receiverName, node::receiver[UEid]->location.first, node::receiver[UEid]->location.second, g_UE_height);
                    /****************************************MODULATION SCHEME SETTINGS******************
                    *AVILABLE MODULATION SCHEMES ARE: [1]. VlcErrorModel::PAM4, [2]. VlcErrorModel::OOK [3]. VlcErrorModel::VPPM
                    *AVILABLE MODULATION SCHEMES ARE: [4]. VlcErrorModel::PSK4 [5]. VlcErrorModel::PSK16. [6]. VlcErrorModel::QAM4 [7]. VlcErrorModel::QAM16.
                    ************************************************************************************/
                    devHelperVPPM.SetReceiverParameter(receiverName, "SetModulationScheme",VlcErrorModel::QAM4);
                    //devHelperVPPM.SetReceiverParameter("THE_RECEIVER1", "DutyCycle", 0.85); //Dutycycle is only valid for VPPM
                    devHelperVPPM.SetReceiverParameter(receiverName, "DataRateInMBPS",nAP->getAchievableRate(UEid)); //orig 0.3

                    std::string channelName = makeName("CHANNEL",nAP->id, UEid);
                    chHelper.CreateChannel(channelName);
                    chHelper.SetPropagationLoss(channelName, "VlcPropagationLoss");
                    chHelper.SetPropagationDelay(channelName, 2);
                    chHelper.AttachTransmitter(channelName, transmitterName,&devHelperVPPM);
                    chHelper.AttachReceiver(channelName, receiverName, &devHelperVPPM);
                    chHelper.SetChannelParameter(channelName, "TEMP", 295);
                    chHelper.SetChannelParameter(channelName, "BAND_FACTOR_NOISE_SIGNAL",Band_factor_Noise_Signal );
                    chHelper.SetChannelWavelength(channelName, 380, 430); // orig 380, 780
                    chHelper.SetChannelParameter(channelName, "ElectricNoiseBandWidth",3 * 1e5);
                    chHelper.SetChannelSNR(channelName,node::SINR[nAP->id][UEid]);
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
//                    std::cout<<"Schedule AP "<<APn<<" ("<<ipInterfs.GetAddress(i-1)<<") with addr "<<ipInterfs.GetAddress(i)<<'\n';
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
            apps.Stop(Seconds(6.0));

            /*--- set up callback trace ---*/
            for(node* nAP : node::transmitter){
                for(int iUE : nAP->get_connected()){
                    std::string receiverName = makeName("RECEIVER",nAP->id,iUE);
                    Ptr<VlcRxNetDevice> rxPtr = devHelperVPPM.GetReceiver(receiverName);
                    rxPtr->TraceConnectWithoutContext("PhyRxEnd", MakeCallback(&RxEnd)); //traces to allow us to see what and when data is sent through the network
//
//                    std::string transmitterName = makeName("TRANSMITTER", nAP->id, iUE);
//                    Ptr<VlcTxNetDevice> txPtr = devHelperVPPM.GetTransmitter(transmitterName);
//                    txPtr->TraceConnectWithoutContext("PhyTxEnd", MakeCallback(&TxEnd));
                }
            }


            //Ask for ASCII and pcap traces of network traffic
//            AsciiTraceHelper ascii;
//            chHelper.EnableAsciiAll (ascii.CreateFileStream ("./log/trace/tcp-large-transfer.tr"));
//            chHelper.EnablePcapAll ("./log/trace/tcp-large-transfer"); // call VlcChannelHelper::EnablePcapAll
//            chHelper.EnablePcap("./log/trace/tcp-large-transfer",allUE);


    //		 netAnim is shitty as FUCK i'd rather use gnuplot if i dont need animation. or maybe im just dumb.
    //		AnimationInterface anim("../dat/visible-light-communication.xml");
            Simulator::Stop(Seconds(1.5));
            Simulator::Run();

//            std::cout<<"Sent "<<Sent.size()<<" segments ("<<Sent.back()<<" bytes), received "<<Received.size()<<" segments ("<<Received.back()<<" bytes).\n";
            std::cout<<"Simulation time "<<iTime<<" seconds\n";
            double throughput = (iReceived * 8 * 1e-6) / iTime; // Mbps
            std::cout << "throughput value is " << throughput << "Mbps.\n";
            double goodput = 0.0;
            for(int APi = 0; APi<g_AP_number; APi++){
                for(int UEi : node::transmitter[APi]->get_connected()){
                    std::string receiverName = makeName("RECEIVER",APi,UEi);
                    Ptr<VlcRxNetDevice> rxHandle = devHelperVPPM.GetReceiver(receiverName);
//                    std::cout<<"Good pkt # from AP "<<APi<<" to UE "<<UEi<<" = "<<rxHandle->ComputeGoodPut()<<" bytes.\n";
                    goodput += rxHandle->ComputeGoodPut();
                }
            }
            goodput *= 8 * 1e-6 / iTime;
            std::cout<<"Total goodput = "<<goodput<<"Mbps.\n";

            {  /* save result to csv */
                std::ofstream goodputOfs;
                openStream(goodputOfs, filepathprefix+"_goodput.csv" , std::ofstream::app);
                goodputOfs<<", "<<goodput;
                goodputOfs.close();
            }

//            aveGoodput += goodput/g_max_epoch;
//            aveSumThroughput += throughput/g_max_epoch;
//            aveTime += algoTime/g_max_epoch;

            Simulator::Destroy();


            iTime=0.0;
            iReceived=0.0;
            Sent.clear(); Sent.push_back(0);
//            currentTxBytes = 0;
            memset(currentTxBytes,0,sizeof(currentTxBytes));
            for(node* n : node::transmitter) delete n;
            for(node* n : node::receiver) if (n) delete n;

        }
//        std::cout<<"\nAverage throughput is "<<aveSumThroughput<<"Mbps.\n";
//        std::cout<<"Average goodput is "<<aveGoodput<<"Mbps.\n";
//        std::cout<<"Average algorithm time is "<<aveTime/1000<<" seconds.\n";
	} // end of UE_number if-block

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
	int iAP = localSocket->GetNode()->GetId();

	while (currentTxBytes[iAP] < totalTxBytes && localSocket->GetTxAvailable() > 0) {

		uint32_t left = totalTxBytes - currentTxBytes[iAP];
		uint32_t dataOffset = currentTxBytes[iAP] % writeSize;
		uint32_t toWrite = writeSize - dataOffset;
		toWrite = std::min (toWrite, left);
		toWrite = std::min (toWrite, localSocket->GetTxAvailable ());

		Ptr<Packet> p = Create<Packet>(&data[dataOffset], toWrite);
//		Ptr<Node> startingNode = localSocket->GetNode();
//		Ptr<VlcTxNetDevice> txOne = DynamicCast<VlcTxNetDevice>(startingNode->GetDevice(0) );
        Ptr<VlcTxNetDevice> txOne = DynamicCast<VlcTxNetDevice>(localSocket->GetBoundNetDevice());
		txOne->EnqueueDataPacket(p);
//		std::cout<<"AP "<<iAP<<" enqueue packet "<<p->GetUid()<<" size " << p->GetSize()<<" at "<<Simulator::Now().GetSeconds()<<'\n';

//		int amountSent = localSocket->Send (&data[dataOffset], toWrite, 0);
        int amountSent = localSocket->Send(p);
		if(amountSent < 0)
		{
			// we will be called again when new tx space becomes available.
			NS_LOG_DEBUG(" buffer full. Wait. \n");
			return;
		}
        NS_LOG_DEBUG(amountSent<<" bytes... ");
		currentTxBytes[iAP] += amountSent;
	}
	NS_LOG_DEBUG("complete. \n");
//	localSocket->Close();
//	std::cout<<localSocket->GetBoundNetDevice()->GetNode()->GetId()<<" called socket->Close(). "
//             <<"Remaining "<<totalTxBytes - currentTxBytes[iAP]<<" bytes, available buffer "<<localSocket->GetTxAvailable()<<" bytes.\n";
    if (currentTxBytes[iAP]>=totalTxBytes) currentTxBytes[iAP]=0;
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
