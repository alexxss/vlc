# About
This project is based on [Aldalbahias\/VLC-ns3-v3.25](https://github.com/Aldalbahias/VLC-ns3-v3.25).\

## Simulation
Algorithm is based on [alexxss/thesis-simulation](https://github.com/alexxss/thesis-simulation) which is an implementaion of *S. Feng, et al,* "Multiple Access Design for Ultra-Dense VLC Networks: Orthogonal vs Non-Orthogonal," *IEEE Transactions on Communications*, Vol. 67, No. 3, pp. 2218-2231, Mar. 2019.  
Algorithm code has been modified since last commit in the above repo.  

## Tracing Support
Below are general description of changes I made to support PCAP and ASCII tracing. See commit difference for details.
### PCAP Support
This method produces one file for every device on every node: `<filename-you-provided>-<node-id>-<device-id>.pcap`
- `vlc-channel-helper.h`
    -  ~~`class VlcChannelHelper : public Object`~~\
      \+`class VlcChannelHelper : public PcapHelperForDevice, public Object`
    - \+`virtual void EnablePcapInternal (std::string prefix, Ptr<NetDevice> nd, bool promiscuous, bool explicitFilename);`\
    (From `ns3/point-to-point/helper/point-to-point-helper.cc`)
- `vlc-channel-helper.cc`
    - \+ `#include ns3/trace-helper.h`
    - \+ `void VlcChannelHelper::EnablePcapInternal (std::string prefix, Ptr<NetDevice> nd, bool promiscuous, bool explicitFilename)`\
    (From `ns3/point-to-point/helper/point-to-point-helper.cc`)
- `vlc_example.cc`
    - \+ `chHelper.EnablePcapAll("tcp-large-transfer");`

> [time=Aug 15, 2019]
 
### ASCII Tracing Support
This method only produces ONE `.tr` file for the whole simulation.
- `vlc-channel-helper.h`
    - ~~`class VlcChannelHelper : public PcapHelperForDevice, public Object`~~
      \+ `class VlcChannelHelper : public PcapHelperForDevice, public AsciiTraceHelperForDevice, public Object`
    - \+ `virtual void EnableAsciiInternal (Ptr<OutputStreamWrapper> stream,std::string prefix,Ptr<NetDevice> nd,bool explicitFilename);`
- `vlc-channel-helper.cc`
    - \+ `void VlcChannelHelper::EnableAsciiInternal (Ptr<OutputStreamWrapper> stream,std::string prefix,Ptr<NetDevice> nd,bool explicitFilename);`
- `vlc_example.cc`
    - \+ `AsciiTraceHelper ascii;`
    - \+ `chHelper.EnableAsciiAll (ascii.CreateFileStream ("tcp-large-transfer.tr"));`

> [time=Sep 4, 2019]

###### tags: `ns3` `vlc3.25`
