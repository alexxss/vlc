
#include "ns3/trace-helper.h"
#include "vlc-channel-helper.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("VLCCHANNELHELPER");

VlcChannelHelper::VlcChannelHelper() {
	//NS_LOG_FUNCTION(this);
	m_queueFactory.SetTypeId("ns3::DropTailQueue");
	m_channelFactory.SetTypeId("VlcChannel");
	m_remoteChannelFactory.SetTypeId("ns3::PointToPointRemoteChannel");
}

void VlcChannelHelper::CreateChannel(std::string channelName) {
	//NS_LOG_FUNCTION(this);
	this->m_channel[channelName] = CreateObject<VlcChannel>();
}

void VlcChannelHelper::SetChannelParameter(std::string chName,
		std::string paramName, double value) {
	if (paramName == "TEMP") {
		this->m_channel[chName]->SetTemperature(value);
	} else if (paramName == "ElectricNoiseBandWidth") {
		this->m_channel[chName]->SetElectricNoiseBandWidth(value);
	}
}

void VlcChannelHelper::SetPropagationLoss(std::string channelName,
		std::string propagationLossType) {
	//NS_LOG_FUNCTION(this);
	if (propagationLossType == "VlcPropagationLoss") {
		this->m_channel[channelName]->SetPropagationLossModel(
				CreateObject<VlcPropagationLossModel>());
	}
}

void VlcChannelHelper::SetPropagationDelay(std::string channelName,
		double value) {
	//NS_LOG_FUNCTION(this);
	this->m_channel[channelName]->SetPropagationDelay(value);

}

double VlcChannelHelper::GetChannelSNR(std::string chName) {
	ns3::Ptr < VlcRxNetDevice > rx = DynamicCast < VlcRxNetDevice
			> (m_channel[chName]->GetDevice(1));
	this->m_channel[chName]->DoCalcPropagationLossForSignal(0);
	this->m_channel[chName]->CalculateNoiseVar();
	double snr = this->m_channel[chName]->GetSNR();
	rx->SetSNRForErrorModel(snr);
	return snr;
}

void VlcChannelHelper::SetChannelWavelength(std::string channelName, int lower,
		int upper) {
	this->m_channel[channelName]->SetWavelength(lower, upper);
}

void VlcChannelHelper::AttachTransmitter(std::string chName,
		std::string TXDevName, ns3::Ptr<VlcDeviceHelper> devHelper) {
	//NS_LOG_FUNCTION(this);
	//this->m_channel[chName]->Attach(devHelper->GetTransmitter(TXDevName));
	ns3::Ptr < VlcPropagationLossModel > loss = ns3::DynamicCast
			< VlcPropagationLossModel
			> (m_channel[chName]->GetPropagationLossModel());
	loss->SetTxPowerMAX(devHelper->GetTransmitter(TXDevName)->GetTXPowerMax());
	loss->SetTXGain(devHelper->GetTransmitter(TXDevName)->GetTXGain());

	//this->m_channel[chName]->SetPropagationLossParametersFromTX(devHelper->GetTransmitter(TXDevName));
}

void VlcChannelHelper::AttachReceiver(std::string chName, std::string RXDevName,
		ns3::Ptr<VlcDeviceHelper> devHelper) {
	//NS_LOG_FUNCTION(this);
	//this->m_channel[chName]->Attach(devHelper->GetReceiver(RXDevName));

	ns3::Ptr < VlcPropagationLossModel > loss = ns3::DynamicCast
			< VlcPropagationLossModel
			> (m_channel[chName]->GetPropagationLossModel());

	loss->SetFilterGain(devHelper->GetReceiver(RXDevName)->GetFilterGain());

	loss->SetConcentratorGain(
			devHelper->GetReceiver(RXDevName)->GetConcentrationGain());

	loss->SetRXGain(devHelper->GetReceiver(RXDevName)->GetRXGain());

	loss->SetArea(devHelper->GetReceiver(RXDevName)->GetPhotoDetectorArea());

	//this->m_channel[chName]->SetPropagationLossParametersFromRX(devHelper->GetReceiver(RXDevName));
}

ns3::Ptr<VlcChannel> VlcChannelHelper::GetChannel(std::string chName) {
	NS_LOG_FUNCTION(this);
	return m_channel[chName];
}

ns3::Ptr<VlcNetDevice> VlcChannelHelper::GetDevice(std::string chName,
		uint32_t idx) {
	NS_LOG_FUNCTION(this);
	ns3::Ptr < VlcNetDevice > ans = DynamicCast < VlcNetDevice
			> (m_channel[chName]->GetDevice(idx));
	return ans;
}

NetDeviceContainer VlcChannelHelper::Install(std::string chName, Ptr<Node> a,
		Ptr<Node> b) {
	NS_LOG_FUNCTION(this);
	NetDeviceContainer container;
	Ptr < VlcChannel > ch = this->m_channel[chName];

	Ptr < VlcTxNetDevice > devTX = DynamicCast < VlcTxNetDevice
			> (ch->GetDevice(0));
	Ptr < VlcRxNetDevice > devRX = DynamicCast < VlcRxNetDevice
			> (ch->GetDevice(1));

	devTX->SetAddress(Mac48Address::Allocate());
	a->AddDevice(devTX);
	Ptr < Queue > queueA = m_queueFactory.Create<Queue>();
	devTX->SetQueue(queueA);

	devRX->SetAddress(Mac48Address::Allocate());
	b->AddDevice(devRX);
	Ptr < Queue > queueB = m_queueFactory.Create<Queue>();
	devRX->SetQueue(queueB);

	devTX->AttachChannel(ch);
	devRX->AttachChannel(ch);

	container.Add(devTX);
	container.Add(devRX);

	return container;
}

NetDeviceContainer VlcChannelHelper::Install(Ptr<Node> a, Ptr<Node> b,
		Ptr<VlcTxNetDevice> tx, Ptr<VlcRxNetDevice> rx) {
	NetDeviceContainer container;

	tx->SetAddress(Mac48Address::Allocate());
	a->AddDevice(tx);
	Ptr < Queue > queueA = m_queueFactory.Create<Queue>();
	tx->SetQueue(queueA);

	rx->SetAddress(Mac48Address::Allocate());
	b->AddDevice(rx);
	Ptr < Queue > queueB = m_queueFactory.Create<Queue>();
	rx->SetQueue(queueB);

	bool useNormalChannel = true;
	Ptr < VlcChannel > channel = 0;

	if (useNormalChannel) {
		channel = m_channelFactory.Create<VlcChannel>();
	}

	tx->AttachChannel(channel);
	rx->AttachChannel(channel);
	container.Add(tx);
	container.Add(rx);

	return container;
}

NetDeviceContainer VlcChannelHelper::Install(Ptr<Node> a, Ptr<Node> b,
		Ptr<VlcDeviceHelper> devHelper, Ptr<VlcChannelHelper> chHelper,
		std::string txName, std::string rxName, std::string chName) {

	NetDeviceContainer container;
	devHelper->GetTransmitter(txName)->SetAddress(Mac48Address::Allocate());

	a->AddDevice(devHelper->GetTransmitter(txName));
	Ptr < Queue > queueA = m_queueFactory.Create<Queue>();
	devHelper->GetTransmitter(txName)->SetQueue(queueA);
	devHelper->GetReceiver(rxName)->SetAddress(Mac48Address::Allocate());

	b->AddDevice(devHelper->GetReceiver(rxName));
	Ptr < Queue > queueB = m_queueFactory.Create<Queue>();
	devHelper->GetReceiver(rxName)->SetQueue(queueB);

	bool useNormalChannel = true;

	Ptr < VlcChannel > channel = 0;
	if (useNormalChannel) {
		channel = chHelper->GetChannel(chName);
		m_subChannel = CreateObject<PointToPointChannel>();
	}

	devHelper->GetTransmitter(txName)->AttachChannel(channel);
	devHelper->GetTransmitter(txName)->Attach(m_subChannel);
	channel->Attach(devHelper->GetTransmitter(txName));

	devHelper->GetReceiver(rxName)->AttachChannel(channel);
	devHelper->GetReceiver(rxName)->Attach(m_subChannel);
	channel->Attach(devHelper->GetReceiver(rxName));

	container.Add(devHelper->GetTransmitter(txName));
	container.Add(devHelper->GetReceiver(rxName));

	return container;
}

VlcChannelHelper::~VlcChannelHelper() {
	m_channel.clear();
}

void
VlcChannelHelper::EnablePcapInternal (std::string prefix, Ptr<NetDevice> nd, bool promiscuous, bool explicitFilename)
{
  //
  // All of the Pcap enable functions vector through here including the ones
  // that are wandering through all of devices on perhaps all of the nodes in
  // the system.  We can only deal with devices of type PointToPointNetDevice.
  //
  Ptr<PointToPointNetDevice> device = nd->GetObject<PointToPointNetDevice> ();
  if (device == 0)
    {
      NS_LOG_INFO ("PointToPointHelper::EnablePcapInternal(): Device " << device << " not of type ns3::PointToPointNetDevice");
      return;
    }

  PcapHelper pcapHelper;

  std::string filename;
  if (explicitFilename)
    {
      filename = prefix;
    }
  else
    {
      filename = pcapHelper.GetFilenameFromDevice (prefix, device);
    }

  Ptr<PcapFileWrapper> file = pcapHelper.CreateFile (filename, std::ios::out,
                                                     PcapHelper::DLT_PPP);
  pcapHelper.HookDefaultSink<PointToPointNetDevice> (device, "PromiscSniffer", file);
}

void
VlcChannelHelper::EnableAsciiInternal (
  Ptr<OutputStreamWrapper> stream,
  std::string prefix,
  Ptr<NetDevice> nd,
  bool explicitFilename)
{
  //
  // All of the ascii enable functions vector through here including the ones
  // that are wandering through all of devices on perhaps all of the nodes in
  // the system.  We can only deal with devices of type PointToPointNetDevice.
  //
  Ptr<PointToPointNetDevice> device = nd->GetObject<PointToPointNetDevice> ();
  if (device == 0)
    {
      NS_LOG_INFO ("VlcChannelHelper::EnableAsciiInternal(): Device " << device <<
                   " not of type ns3::PointToPointNetDevice");
      return;
    }

  //
  // Our default trace sinks are going to use packet printing, so we have to
  // make sure that is turned on.
  //
  Packet::EnablePrinting ();

  //
  // If we are not provided an OutputStreamWrapper, we are expected to create
  // one using the usual trace filename conventions and do a Hook*WithoutContext
  // since there will be one file per context and therefore the context would
  // be redundant.
  //
  if (stream == 0)
    {
      //
      // Set up an output stream object to deal with private ofstream copy
      // constructor and lifetime issues.  Let the helper decide the actual
      // name of the file given the prefix.
      //
      AsciiTraceHelper asciiTraceHelper;

      std::string filename;
      if (explicitFilename)
        {
          filename = prefix;
        }
      else
        {
          filename = asciiTraceHelper.GetFilenameFromDevice (prefix, device);
        }

      Ptr<OutputStreamWrapper> theStream = asciiTraceHelper.CreateFileStream (filename);

      //
      // The MacRx trace source provides our "r" event.
      //
      asciiTraceHelper.HookDefaultReceiveSinkWithoutContext<PointToPointNetDevice> (device, "MacRx", theStream);

      //
      // The "+", '-', and 'd' events are driven by trace sources actually in the
      // transmit queue.
      //
      Ptr<Queue> queue = device->GetQueue ();
      asciiTraceHelper.HookDefaultEnqueueSinkWithoutContext<Queue> (queue, "Enqueue", theStream);
      asciiTraceHelper.HookDefaultDropSinkWithoutContext<Queue> (queue, "Drop", theStream);
      asciiTraceHelper.HookDefaultDequeueSinkWithoutContext<Queue> (queue, "Dequeue", theStream);

      // PhyRxDrop trace source for "d" event
      asciiTraceHelper.HookDefaultDropSinkWithoutContext<PointToPointNetDevice> (device, "PhyRxDrop", theStream);

      return;
    }

  //
  // If we are provided an OutputStreamWrapper, we are expected to use it, and
  // to providd a context.  We are free to come up with our own context if we
  // want, and use the AsciiTraceHelper Hook*WithContext functions, but for
  // compatibility and simplicity, we just use Config::Connect and let it deal
  // with the context.
  //
  // Note that we are going to use the default trace sinks provided by the
  // ascii trace helper.  There is actually no AsciiTraceHelper in sight here,
  // but the default trace sinks are actually publicly available static
  // functions that are always there waiting for just such a case.
  //
  uint32_t nodeid = nd->GetNode ()->GetId ();
  uint32_t deviceid = nd->GetIfIndex ();
  std::ostringstream oss;

  oss << "/NodeList/" << nd->GetNode ()->GetId () << "/DeviceList/" << deviceid << "/$ns3::PointToPointNetDevice/MacRx";
  Config::Connect (oss.str (), MakeBoundCallback (&AsciiTraceHelper::DefaultReceiveSinkWithContext, stream));

  oss.str ("");
  oss << "/NodeList/" << nodeid << "/DeviceList/" << deviceid << "/$ns3::PointToPointNetDevice/TxQueue/Enqueue";
  Config::Connect (oss.str (), MakeBoundCallback (&AsciiTraceHelper::DefaultEnqueueSinkWithContext, stream));

  oss.str ("");
  oss << "/NodeList/" << nodeid << "/DeviceList/" << deviceid << "/$ns3::PointToPointNetDevice/TxQueue/Dequeue";
  Config::Connect (oss.str (), MakeBoundCallback (&AsciiTraceHelper::DefaultDequeueSinkWithContext, stream));

  oss.str ("");
  oss << "/NodeList/" << nodeid << "/DeviceList/" << deviceid << "/$ns3::PointToPointNetDevice/TxQueue/Drop";
  Config::Connect (oss.str (), MakeBoundCallback (&AsciiTraceHelper::DefaultDropSinkWithContext, stream));

  oss.str ("");
  oss << "/NodeList/" << nodeid << "/DeviceList/" << deviceid << "/$ns3::PointToPointNetDevice/PhyRxDrop";
  Config::Connect (oss.str (), MakeBoundCallback (&AsciiTraceHelper::DefaultDropSinkWithContext, stream));
}


} /* namespace vlc */
