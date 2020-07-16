# About
This project is based on [Aldalbahias\/VLC-ns3-v3.25](https://github.com/Aldalbahias/VLC-ns3-v3.25).\

## Simulation
Algorithm is based on [alexxss/thesis-simulation](https://github.com/alexxss/thesis-simulation) which is an implementaion of *S. Feng, et al,* "Multiple Access Design for Ultra-Dense VLC Networks: Orthogonal vs Non-Orthogonal," *IEEE Transactions on Communications*, Vol. 67, No. 3, pp. 2218-2231, Mar. 2019.  
Algorithm code has been modified since last commit in the above repo.  

### Automation
The script file `wafshellRun.sh` automatically repeats 20 iterations for increment of UE number from 10 to 70 (step size 10). To use the script file, under the `ns-3.25` directory, do the following:  
```bash
$ ./waf shell # you will now enter waf's shell
$ ./wafshellRun.sh
```
Outputs will be in `ns-3.25/log` and `ns-3.25/log/RA`, be sure that these directories exist!
> TODO: support automation of other variables such as AP load, RB number, etc.

### Structure
`./wafshellRun.sh` outputs results and logs to `ns-3.25/log`.  
single iters built in codeblocks will output results and logs to `ns-3.25/build/log`. `debug` and `release` profiles have their own respective folder for `.so``.o` files etc.  

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
