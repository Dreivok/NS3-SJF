WAVE models
-----------
WAVE is a system architecture for wireless-based vehicular communications, 
specified by the IEEE.  This chapter documents available models for WAVE
within |ns3|.  The focus is on the MAC layer and MAC extension layer
defined by [ieee80211p]_, and on the multichannel operation defined in
[ieee1609.4]_.

.. include:: replace.txt

.. heading hierarchy:
   ------------- Chapter
   ************* Section (#.#)
   ============= Subsection (#.#.#)
   ############# Paragraph (no number)

Model Description
*****************

WAVE is an overall system architecture for vehicular communications.
The standards for specifying WAVE include a set of extensions to the IEEE
802.11 standard, found in IEEE Std 802.11p-2010 [ieee80211p]_, and
the IEEE 1609 standard set, consisting of four documents:  
resource manager:  IEEE 1609.1 [ieee1609.1]_,
security services:  IEEE 1609.2 [ieee1609.2]_,
network and transport layer services:  IEEE 1609.3 [ieee1609.3]_,
and multi-channel coordination:  IEEE 1609.4 [ieee1609.4]_.

In |ns3|, the focus of the ``wave`` module is on the MAC layer.
The key design aspect of WAVE-compilant MACs is that they allow
communications outside the context of a basic service set (BSS).
The literature uses the acronym OCB to denote "outside the context
of a BSS", and the class ``ns3::OcbWifiMac`` models this in |ns3|. 
Many management frames will not be used; the BSSID field needs to set 
a wildcard BSSID value. 
Management information is transmitted by what is called **vendor specific 
action frame**. 
With these changes, the packet transmissions (for a moving vehicle) can
be fast with small delay.  At the physical layer, the biggest difference is 
to use the 5.9 GHz band with a channel bandwidth of 10 MHz.  These physical
layer changes can make the wireless signal relatively more stable,
without degrading throughput too much (ranging from 3 Mbps to 27 Mbps), 
although 20 MHz channel bandwidth is still supported.
As declared in IEEE Std 1609.4-2010 [ieee1609.4]_: "WAVE devices must support 
a control channel with multiple service channels.To operate over multiple wireless 
channels while in operation with OCBEnabled, there is a need to perform channel coordination."
The class ``ns3::WaveNetDevice`` models a set of extensions of 802.11p MAC layer, 
not including data plane, but also management plane features. The data plane includes 
Channel coordination, Channel routing and User priority; and the management plane 
includes Multi-channel synchronization, Channel access, Vendor Specific Action frames 
and Readdressing.

The source code for the WAVE MAC models lives in the directory
``src/wave``.


Design
======

In |ns3|, support for 802.11p involves the MAC and PHY layers.
To use a 802.11p netdevice, Wifi80211pHelper is suggested.

MAC layer
#########
Here are OrganizationIdentifier, VendorSpecificActionHeader, HigherDataTxVectorTag,
WaveMacLow and OcbWifiMac.
The OrganizationIdentifier and VendorSpecificActionHeader are used to support 
send Vendor Specific Action frame. The HigherDataTxVectorTag and WaveMacLow are 
used to support higher control tx parameters. These classes are all used in OcbWifiMac.
OcbWifiMac is very similar to AdhocWifiMac, with some modifications. 
(|ns3| AdhocWifiMac class is implemented very close to 802.11p OCB mode rather than 
real 802.11 ad-hoc mode. The AdhocWifiMac has no BSS context that is defined in 802.11 
standard, so it will not take time to send beacon and authenticate.)
1. SetBssid, GetBssid, SetSsid, GetSsid
these methods are related to 802.11 BSS context which is unused in OCB context.
2. SetLinkUpCallback, SetLinkDownCallback
WAVE device can send packets directly, so wifi link is never down.
3. SendVsc, AddReceiveVscCallback
WAVE management information shall be sent by vendor specific action frame, 
and it will be called by upper layer 1609.4 standard to send WSA
(WAVE Service Advertisement) packets or other vendor specific information.
4. SendTimingAdvertisement (not implemented)
although Timing Advertisement is very important and specific defined in 
802.11p standard, it is useless in simulation environment. Every node in |ns3| is 
assumed already time synchronized,
which we can imagine that every node has a GPS device installed. 
5. ConfigureEdca
This method will allow user set EDCA parameters of WAVE channeles including 
CCH ans SCHs. And OcbWifiMac itself also use this method to configure default 
802.11p EDCA parameters.
6. WILDCARD BSSID
the WILDCARD BSSID is set to "ff:ff:ff:ff:ff:ff".
As defined in 802.11-2007, a wildcard BSSID shall not be used in the
BSSID field except for management frames of subtype probe request. But Adhoc 
mode of ns3 simplify this mechanism, when stations receive packets, packets 
regardless of BSSID will forward up to higher layer. This process is very close 
to OCB mode as defined in 802.11p-2010 which stations use the wildcard BSSID 
to allow higher layer of other stations can hear directly.
7. Enqueue, Receive
The most important methods are send and receive methods. According to the 
standard, we should filter frames that are not permitted. However here we 
just idetify the frames we care about, the other frames will be disgard.
8. NotifyBusy and SetWaveEdcaQueue
These methods are private, and can only be used by friend class ChannelSceduler
to provide some special features of MAC layer required by 1609.4 standard.

PHY layer
#########
Actually, no modification or extension happen in ns3 PHY layer.
In the 802.11p standard, the PHY layer wireless technology is still 80211a OFDM with 10MHz,
so Wifi80211pHelper will only allow user set the standard WIFI_PHY_STANDARD_80211_10MHZ
(WIFI_PHY_STANDARD_80211a with 20MHz is supported, but not recommended.)
The maximum station transmit power and maximum permited EIRP defined in 802.11p is larger 
than wifi, so transmit range can normally become longer than usual wifi. But this feature will 
not be implemented. Users who want to get longer range should configure atrributes "TxPowerStart", 
"TxPowerEnd" and "TxPowerLevels" of YansWifiPhy class by themself.

In |ns3|, support for 1609.4 involves the MAC extension layers.
To use a WAVE netdevice, WaveHelper is suggested.
MAC extension
#############
Although 1609.4 is still in MAC layer, the implemention approach here will 
not do modification directly in source code of wifi module. Instead, if some 
feature is related to wifi MAC classes, then a relevant subclass and new tag will 
be defined; if some features has no relation with wifi MAC classess, then  
new class will be defined. And all these classes will work and called by WaveNetDevice.
WaveNetDevice is a subclass inherting from WifiNetDeivce, composed of the objects of 
ChannelScheduler, ChannelManager, ChannelCoordinator and VsaRepeater classess 
to provide the features of 1609.4. The main work of WaveNetDevice is to create objects, 
configure, check arguments and provide new methods for multiple channel operation.
1. SetChannelManager, SetChannelScheduler, SetChannelCoordinator.
These methods are used when users want to use customized channel operations or 
want to debug state information of internal classes.
3. StartSch and StopSch
Different from 802.11p device can sent packets after constructing, a wave device 
should request to assign channel access for sending packets. The methods will use 
ChannelScheduler to assign and free channel resource.
4. SendX
After access is assigned, we can send WSMP packets or other packets by SendX method. 
We should specify the parameters of packets, e.g. which channel the packets will be 
sent. Beside that, we can also control data rate and tx power level, if we not set, 
then the data rate and tx power level will be set by RemoteStationManager automatically.
5. RegisterTxProfile and UnregisterTxProfile
If access is assigned and we want to send IP-based packets by Send method, we must 
register a tx profile to specify tx parameter then all packets sent by Send method 
will use the same channel number. If the adapate parameter is false, data rate and 
tx power level will be same for sending, otherwise data rate and tx power level will 
be set automatically by RemoteStationManager.
6. Send, 
Only access is assigned and tx profile is registered, we can send IP-based packets 
or other packets. And no matter data packets is sent by Send method or SendX method, 
the received data packets will forward up to higher layer by setted ReceiveCallback.
7. StartVsa, StopVsa and SetVsaReceiveCallback
VSA frames is useful for 1609.3 to send WSA managemnt information. By StartVsa method 
we can specify transmit parameters.

Channel Coordination
####################
ns3::ChannelCoodrdiator defines CCH Interval, SCH Interval and GuardInteval. Users can 
use this class to get which interval is now or after duration. If current channel access 
is alternating, channel interval event will be notified repeatedly. Current default 
parameter is CCHI with 50ms, SCHI with 50ms, and GuardI with 4ms. Users can change these 
value by controlling attributes. 

Channel routing
###############
Channel routind indicated different transmission apporach for WSMP data, IP datagrams and management data.
For WSMP data, WaveNetDevice::SendX method implements the service primitive MA-UNITDATAX, and users can 
set transmission parameter for each individual packet. The parameter here includes channel number, priority,
data rate, tx power level and expire time. 
For IP datagrams, WaveNetDevice::Send method is virtual method from WifiNetDevice::Send, so users should add 
qos tag into packets by themselves if they want use qos. And an important point is they should register a tx 
profile which contains SCH number, data rate, power level and adaptable status before Send method is called. 
For management frames, users can use StartVsa to send VSA frames, and the tx information is already configured 
in ns3::ChannelManager which contains data rate, power level and adaptable status.
To queue a packet under multiple channel environment, ns3::WaveEdcaTxopN is defined to inhert from EdcaTxopN to 
change internal single mac queue to multiple mac queue. When a packet is sent to MAC layer, first decide which AC 
is by ns3::QosTag, then get EdcaTxopN object(actually is WaveEdcaTxopN object) by AC value, then this WaveEdcaTxopN 
decides which internal queue is by ns3::ChannelTag, at last the packet will be queued to the proper queue.


User priority and Multi-channel synchronization
###############################################
Since wifi module is already implemented qos mechanism, wave module will do nothing except that VSA frames 
will be assigned the highest AC according to the standard. The feature that allows higher layer set new EDCA 
parameters for SCH is not implemented.
Multipl-channel synchronization is indeed very important in practice for devices without local timing source. 
However in simulation every node is supposed to have the same system clock which could be achieved by GPS in 
real environment, so this feature will not be implemented. During the guard interval, the device state will 
be neither receive nor send even real channel switch duration of PHY layer is smaller than 1ms.
A important point is the channel access should be assigned before routing packets. WaveNetDevice will check 
channel state by ns3::ChannelManager to make sure channel access status.

Channel access
##############
The channel access assignment is done by ns3::ChannelScheduler to assign ChannelContinuousAccess, ExtendedAccess 
and AlternatingAccess. Besides that, immediate access is achieved by enable or disbale "immediate" value. If 
immediate enabled, channel access will switch channel immediately, then assign relevant channel access. The 
channel access here is in the context of single-phy device. So if channel access is already assigned for other 
request, current request will fail until different channel access is released.
To support alternating access switch channel at the guard interval, ns3::WaveEdcaTxopN will not do the action of 
flushing queue, instead, the class will only switch current channel queue to another channel queue and a MAC busy 
event will be notified to suspend MAC activities. When releasing the alternating access, the real queue flush action 
will be done at this moment. 
The StartSch method and StopSch method are actually implementd by calling ns3::ChannelScheduler.

Vendor Specific Action frames
#############################
When users want to send VSA repeatedlt by calling WaveNetDevice::StartVsa, VSA will be send repeatedly by 
ns3::VsaRepeater. An point is that if peer mac address is a unicast address, the VSA inorges repeat request and 
send only once. The tx parameters are configured in ns3::ChannelManager.

Scope and Limitations
=====================

1. Is this model going to involve vehicular mobility of some sort?

Vehicular networks involve not only communication protocols, but also 
communication environment 
including vehicular mobility and propagation models. Because of specific 
features of the latter, the protocols need to change. The MAC layer model 
in this 
project just adopts MAC changes to vehicular environment. However this model 
does not involves any
vehicular mobility with time limit. Users can use any mobility model in |ns3|, 
but should know these models are not real vehicular mobility. 

2. Is this model going to use different propagation models?

Referring to the first issue, some more realistic propagation models 
for vehicualr environment
are suggested and welcome. And some existed propagation models in |ns3| are 
also suitable. 
Normally users can use Friis,Two-Ray Ground and Nakagami model. This 
model will not care about special propagation models.

3. are there any vehicular application models to drive the code?

About vehicular application models, SAE J2375 depends on WAVE architecture and
is an application message set in US; CAM and DENM in Europe between network 
and application layer, but is very close to application model. The BSM in 
J2375 and CAM send alert messages that every 
vehicle node will sent periodicity about its status information to 
cooperate with others. 
Fow now, there is no plan to develop a vehicular application model. 
But to drive WAVE MAC layer 
code, a simple application model will be tried to develop. 

4. any previous code you think you can leverage ?

This project will provide two NetDevices. 

First is normal 802.11p netdevice of WifiNetDevice which uses new implemented OcbWifiMac. 
This is very useful 
for those people who only want to simulate routing protocols or upper layer
protocols for vehicular 
environment and do not need the whole WAVE architecture.
For any previous code, my suggestion is to use Wifi80211phelper with 
little modification 
on previous code or just use ad-hoc mode with 802.11a 10MHz (this is enough 
to simulate the main characteristic of 802.11p). 

The second NetDevices is a 1609.4 netdevice of WaveNetDevice which provides some methods 
to deal with multi-channel operation. This is part of the whole WAVE 
architecture and provides service for upper 1609.3 standards, which could 
leverage no previous codes. 

5 Why here are two kinds of NetDevice?
The reason to provide a special 802.11p device is that considering the fact many 
researchers are interested in route protocol or other aspects on vehicular 
environment of single channel, so they need neither multi-channel operation 
nor WAVE architectures. Besides that, the European standard could use 802.11p 
device in an modified ITS-G5 implementation (maybe could named ITSG5NetDevice).

References
==========

.. [ieee80211p] IEEE Std 802.11p-2010 *IEEE Standard for Information 
technology-- Local and metropolitan area networks-- Specific requirements-- 
Part 11: Wireless LAN Medium Access Control (MAC) and Physical Layer (PHY) 
Specifications Amendment 6: Wireless Access in Vehicular Environments*
.. [ieee1609.1] IEEE Std 1609.1-2010 *IEEE Standard for Wireless Access in 
Vehicular Environments (WAVE) - Resource Manager, 2010*
.. [ieee1609.2] IEEE Std 1609.2-2010 *IEEE Standard for Wireless Access in 
Vehicular Environments (WAVE) - Security Services for Applications and Management Messages, 2010*
.. [ieee1609.3] IEEE Std 1609.3-2010 *IEEE Standard for Wireless Access in 
Vehicular Environments (WAVE) - Networking Services, 2010*
.. [ieee1609.4] IEEE Std 1609.4-2010 *IEEE Standard for Wireless Access in 
Vehicular Environments (WAVE) - Multi-Channel Operation, 2010*

Usage
*****

Helpers
=======
The helpers include ns3::NqosWaveMacHelper, ns3::QosWaveMacHelper, 
ns3::Wifi80211pHelper and ns3::WaveHelper. Wifi80211pHelper is used to create 
802.11p devices that follow the 802.11p-2010 standard. WaveHelper is 
used to create WAVE devices that follow the 802.11p-2010 and 1609.4-2010 
standards which are the MAC and PHY layers of the WAVE architecture. 
The relation of them is described as below:

::
    WifiHelper ----------use---------->  WifiMacHelper
     ^      ^                             ^       ^
     |      |                             |       |
     |      |                          inherit  inherit
     |      |                             |       |
     |      |                 QosWifiMacHelper  NqosWifiMacHelper
     |      |                             ^       ^
     |      |                             |       |
 inherit    inherit                     inherit  inherit
     |      |                             |       |
WaveHelper Wifi80211pHelper     QosWaveMacHelper  NqosWaveHelper

Although Wifi80211Helper can use any subclasses inheriting from 
WifiMacHelper, we force user to use subclasses inheriting from 
QosWaveMacHelper or NqosWaveHelper.
Although WaveHelper can use any subclasses inheriting from WifiMacHelper, 
we force user to use subclasses inheriting from QosWaveMacHelper. 
NqosWaveHelper is also not permitted, because 1609.4 standard requires the 
support for priority. 

Users can use Wifi80211pHelper to create "real" wifi 802.11p device.
Although the functions of wifi 802.11p device can be achieved by WaveNetDevice's 
ContinuousAccess assignment, Wifi80211pHelper ia more recommened if there is no 
need for multiple channel operation.
Usage as follows:

::
    NodeContainer nodes;
    NetDeviceContainer devices;
    nodes.Create (2);
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());
    NqosWave80211pMacHelper wifi80211pMac = NqosWaveMacHelper::Default();
    Wifi80211pHelper 80211pHelper = Wifi80211pHelper::Default ();
    devices = 80211pHelper.Install (wifiPhy, wifi80211pMac, nodes);

Users can use WaveHelper to create wave devices to deal with multiple
channel operations. The devices are WaveNetDevice objects. It's useful 
when users want to research on multi-channel environment or use WAVE 
architecture.
Usage as follows:

::
    NodeContainer nodes;
    NetDeviceContainer devices;
    nodes.Create (2);
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());
    QosWaveMacHelper waveMac = QosWaveMacHelper::Default ();
    WaveHelper waveHelper = WaveHelper::Default ();
    devices = waveHelper.Install (wifiPhy, waveMac, nodes);
APIs
====
The 802.11p device can send data packets which is the same as normal wifi device
1. already create a Node object and WifiNetdevice 802.11p object
2(a). if we want to send UDP packets over 802.11p device, we can create socket

::
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> recvSink = Socket::CreateSocket (c.Get (0), tid);
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 80);
  recvSink->Bind (local);
  recvSink->SetRecvCallback (MakeCallback (&ReceivePacket));
  
  Ptr<Socket> source = Socket::CreateSocket (c.Get (1), tid);
  InetSocketAddress remote = InetSocketAddress (Ipv4Address ("255.255.255.255"), 80);
  source->SetAllowBroadcast (true);
  source->Connect (remote);
  socket->Send (Create<Packet> (pktSize));
  
2(b). if we want to send packets directly or create new protocol over 802.11p device

:: 
Ptr<NetDevice> device;
device->SetReceiveCallback (MakeCallback (&ReceivePacket));
device->Send (packet, dest, protocolNumber);

The 802.11p device can allow the upper layer to send management information
over Vendor Specific Action management frames by using different
OrganizationIdentifier fields to identify differences.

1. already create a Node object and WifiNetdevice 802.11p device object
2. define an OrganizationIdentifier

::
   uint8_t oi_bytes[5] = {0x00, 0x50, 0xC2, 0x4A, 0x40};
   OrganizationIdentifier oi(oi_bytes,5);

3. define a Callback for the defined OrganizationIdentifier

::
   VscCallback vsccall = MakeCallback (&VsaExample::GetWsaAndOi, this);

4. OcbWifiMac of 802.11p device regists this identifier and function

::
      Ptr<WifiNetDevice> device1 = DynamicCast<WifiNetDevice>(nodes.Get (i)->GetDevice (0));
      Ptr<OcbWifiMac> ocb1 = DynamicCast<OcbWifiMac>(device->GetMac ());
      ocb1->AddReceiveVscCallback (oi, vsccall);

5. now one can send management packets over VSA frames

::
      Ptr<Packet> vsc = Create<Packet> ();
      ocb2->SendVsc (vsc, Mac48Address::GetBroadcast (), m_16093oi);

6. then registered callbacks in other devices will be called.

The wave device can support send data packets by inherited method 
NetDevice::Send. But before sending,we must assign channel access for devices,
and register a tx profile to indicate tx parameters.

1. create a Node object and WaveNetdevice wave device object
2. assigne channel access for sender

::
  const SchInfo schInfo = SchInfo (CCH, false, 0xff);
  Simulator::Schedule (Seconds (1.0),&WaveNetDevice::StartSch,sender,schInfo);
   
   assigne channel access for receiver
   
 ::
  const SchInfo schInfo = SchInfo (CCH, false, 0xff);
  Simulator::Schedule (Seconds (1.0),&WaveNetDevice::StartSch,receiver,schInfo);
  
3. register a tx profile

::
  const TxProfile txProfile = TxProfile (SCH1);
  Simulator::Schedule (Seconds (2.0),&WaveNetDevice::RegisterTxProfile,
                       sender,txProfile);
4. send packets

::
  Simulator::Schedule (Seconds (3.0),&WaveNetDevice::Send, sender,
                       packet, dest, ipv4_protocol);
                       
The wave device can also support send data packets by new 
method WaveNetDevice::SendX. And before sending, we must 
assign channel access for devices.
1. create a Node object and WaveNetdevice wave device object
2. assigne channel access for sender

::
  const SchInfo schInfo = SchInfo (CCH, false, 0xff);
  Simulator::Schedule (Seconds (1.0),&WaveNetDevice::StartSch,sender,schInfo);
   
   assigne channel access for receiver
   
 ::
  const SchInfo schInfo = SchInfo (CCH, false, 0xff);
  Simulator::Schedule (Seconds (1.0),&WaveNetDevice::StartSch,receiver,schInfo);
  
3. send data packets

::
  const TxInfo txInfo = TxInfo (CCH);
  Ptr<Packet> packet = Create<Packet> (100);
  const Address dest = receiver->GetAddress ();
  Simulator::Schedule (Seconds (2.0),&WaveNetDevice::SendX, sender,
                       packet, dest, WSMP_PROT_NUMBER, txInfo);
 

Attributes
==========

**What classes hold attributes, and what are the key ones worth mentioning?**

Output
======

**What kind of data does the model generate?  What are the key trace
sources?   What kind of logging output can be enabled?**

Advanced Usage
==============

**Go into further details (such as using the API outside of the helpers)
in additional sections, as needed.**

Examples
========

**What examples using this new code are available?  Describe them here.**

Troubleshooting
===============

**Add any tips for avoiding pitfalls, etc.**

Validation
**********

**Describe how the model has been tested/validated.  What tests run in the
test suite?  How much API and code is covered by the tests?  Again,
references to outside published work may help here.**

