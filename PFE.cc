/*
installing ns3

$ wget https://www.nsnam.org/releases/ns-allinone-3.44.tar.bz2
$ tar xfj ns-allinone-3.44.tar.bz2
$ cd ns-allinone-3.44/ns-3.44
./ns3 build
--------------------------------------------------------------------
$ git clone https://gitlab.com/nsnam/ns-3-dev.git
$ cd ns-3-dev
./ns3 build

EVS: https://github.com/vipchengrui/EVS-codec?tab=readme-ov-file
*/

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config-store-module.h"
#include "ns3/buildings-module.h"
#include "ns3/building.h"
#include "ns3/buildings-helper.h"
#include "ns3/hybrid-buildings-propagation-loss-model.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstdlib>

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("My_PFE_Essay");

float lossper = 20;
float jit = 0;
double lat = 0;
double prevlat = 0;
double prevdelay = 0;
float recpack = 0;
float sentpack = 0;

double totalDelay = 0.0;

std::string dir = "/home/crow/ns-allinone-3.44/ns-3.44/scratch/";
std::ofstream delayLog("/home/crow/ns-allinone-3.44/ns-3.44/scratch/delay_log.txt", std::ios::out);

class MyTag : public Tag
{
public:
  MyTag() : m_data(0) {}

  void SetData(double data) { m_data = data; }
  double GetData() const { return m_data; }

  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("MyTag")
      .SetParent<Tag>()
      .AddConstructor<MyTag>();
    return tid;
  }

  virtual TypeId GetInstanceTypeId(void) const override
  {
    return GetTypeId();
  }

  virtual void Serialize(TagBuffer i) const override
  {
    i.WriteDouble(m_data);
  }

  virtual void Deserialize(TagBuffer i) override
  {
    m_data = i.ReadDouble();
  }

  virtual uint32_t GetSerializedSize() const override
  {
    return 8;
  }

  virtual void Print(std::ostream &os) const override
  {
    os << "MyTag, Data: " << m_data;
  }

private:
  double m_data;
};

void buffer(Ptr<Packet> packet){
    std::ofstream out(dir + "recieved.bit", std::ios::binary | std::ios::app);
    uint32_t size = packet->GetSize();
    std::vector<uint8_t> buffer(size);
    packet->CopyData(buffer.data(), size);
    out.write(reinterpret_cast<char*>(buffer.data()), size);
    out.close();
}
void ReceivePacket(Ptr<Socket> socket) {
    Ptr<Packet> packet = socket->Recv();
    recpack++; // Already counting received packets

    MyTag tag;
    if (packet->PeekPacketTag(tag)) {
        double sendTime = tag.GetData();
        double recvTime = Simulator::Now().GetSeconds();
        double delay = recvTime - sendTime;
        totalDelay += delay;

         delayLog << recvTime << "," << (delay * 1000.0) << std::endl;

        NS_LOG_INFO("packet sent at: " << sendTime
                     << " seconds ----- received at: " << recvTime
                     << " seconds");

        if(prevdelay != 0){
            jit = jit + abs(delay - prevdelay);
        }
        prevdelay = delay;
    }
}

void ReceivePacketm(Ptr<Socket> socket) {
    Ptr<Packet> packet = socket->Recv();
    recpack++;

    MyTag tag;
    if (packet->PeekPacketTag(tag)) {
        NS_LOG_INFO("packet sent at: " << tag.GetData() << " seconds-----received at: " << Simulator::Now().GetSeconds() << " seconds");
        double sendTime = tag.GetData();
        double recvTime = Simulator::Now().GetSeconds();
        double delay = recvTime - sendTime;
            jit = jit + abs(delay - prevdelay);

        prevdelay = delay;
    }



    std::ofstream out(dir + "recieved.bit", std::ios::binary | std::ios::app);
    uint32_t size = packet->GetSize();
    std::vector<uint8_t> buffer(size);
    packet->CopyData(buffer.data(), size);
    out.write(reinterpret_cast<char*>(buffer.data()), size);
    out.close();
}
void SendPacket(Ptr<Socket> socket, Address address, double sendtime, std::vector<uint16_t> mes) {
    std::vector<uint8_t> byteBuffer(mes.size() * sizeof(uint16_t));
    std::memcpy(byteBuffer.data(), mes.data(), byteBuffer.size());

    Ptr<Packet> packet = Create<Packet>(byteBuffer.data(), byteBuffer.size());

    MyTag tag;
    tag.SetData(sendtime);
    packet->AddPacketTag(tag);
    socket->SendTo(packet, 0, address);
}
void ReceivePacketsip(Ptr<Socket> socket) {
    Ptr<Packet> packet = socket->Recv();
}
void SendPacketsip(Ptr<Socket> socket, Address address, uint32_t size, std::string message) {
    Ptr<Packet> packet = Create<Packet>(size);
    socket->SendTo(packet, 0, address);
    NS_LOG_INFO( message );
}

double randomdelay() {
    return ((rand() % 50) / 1000.0);

}

int main (int argc, char *argv[]) {
    LogComponentEnable("My_PFE_Essay", LOG_LEVEL_INFO);

    // Creation of the LTE module (E-UTRAN and EPC)
    Ptr<LteHelper> E_UTRAN = CreateObject<LteHelper> ();
    Ptr<PointToPointEpcHelper> EPC = CreateObject<PointToPointEpcHelper> ();
    E_UTRAN->SetEpcHelper(EPC);


    // Create and configure EPC elements
    Ptr<Node> PGW = EPC->GetPgwNode();
    Ptr<Node> SGW = EPC->GetSgwNode();
    Ptr<Node> MME = CreateObject<Node>();
    Ptr<Node> IMS = CreateObject<Node>();

    // Create devices (eNB, UE1, UE2)
    int users = 10;
    NodeContainer eNBN, UE1N, UE2N;
    eNBN.Create(2);
    UE1N.Create(users);
    UE2N.Create(users);

    MobilityHelper position;
    position.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    position.Install(eNBN);
    position.Install(UE1N);
    position.Install(UE2N);
    eNBN.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(-20000.0, 0.0, 0.0));
    eNBN.Get(1)->GetObject<MobilityModel>()->SetPosition(Vector(20000.0, 0.0, 0.0));

    for(int i=0; i<users;i++){
        UE1N.Get(i)->GetObject<MobilityModel>()->SetPosition(Vector(-25000 + 200*i, 0.0 - 50*i, 0.0 + 100*i));
        UE2N.Get(i)->GetObject<MobilityModel>()->SetPosition(Vector(25000 - 200*i, 0.0 + 100*i, 0.0 +50*i));
    }


    // Install internet stack
    InternetStackHelper internet;
    internet.Install(IMS);
    internet.Install(UE1N);
    internet.Install(UE2N);

    // Install LTE devices
    NetDeviceContainer eNB;//This one will store the eNB devices (LTE base stations).
    // Install eNB devices individually
    Ptr<NetDevice> enbDev0 = E_UTRAN->InstallEnbDevice(eNBN.Get(0)).Get(0);
    Ptr<NetDevice> enbDev1 = E_UTRAN->InstallEnbDevice(eNBN.Get(1)).Get(0);
    eNB.Add(enbDev0);
    eNB.Add(enbDev1);//dding the two eNB devices into the eNB container
/*
    // Set different DL Tx power levels
    Ptr<LteEnbNetDevice> enb0 = enbDev0->GetObject<LteEnbNetDevice>();
    Ptr<LteEnbNetDevice> enb1 = enbDev1->GetObject<LteEnbNetDevice>();
    enb0->GetPhy()->SetTxPower(10); // weak signal
    enb1->GetPhy()->SetTxPower(40); // strong signal;
*/

    NetDeviceContainer UE1 = E_UTRAN->InstallUeDevice (UE1N);
    NetDeviceContainer UE2 = E_UTRAN->InstallUeDevice (UE2N);
    Ipv4StaticRoutingHelper Routeuripv4;

    // Set up point-to-point connections
    PointToPointHelper bigligne;
    bigligne.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Gb/s")));
    bigligne.SetDeviceAttribute("Mtu", UintegerValue(1500));
    bigligne.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    bigligne.Install(eNBN.Get(0), MME);
    //bigligne.Install(eNBN.Get(0), eNBN.Get(1));
    bigligne.Install(eNBN.Get(1), MME);
    bigligne.Install(eNBN.Get(0), SGW);
    bigligne.Install(eNBN.Get(1), SGW);
    bigligne.Install(MME, SGW);
    bigligne.Install(SGW, PGW);

    NetDeviceContainer IMSDevice;
    PointToPointHelper IMStoPGW;
    IMStoPGW.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Gb/s")));
    IMStoPGW.SetDeviceAttribute("Mtu", UintegerValue(1500));
    IMStoPGW.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    IMSDevice = IMStoPGW.Install(IMS, PGW);

    //set up the ip addresses and the routing
    Ipv4InterfaceContainer UE1NIP = EPC->AssignUeIpv4Address(UE1);
    Ipv4InterfaceContainer UE2NIP = EPC->AssignUeIpv4Address(UE2);
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer IMSIP = ipv4.Assign(IMSDevice);

    std::vector<Ptr<Ipv4StaticRouting>> RoutageUE1(users);
    std::vector<Ptr<Ipv4StaticRouting>> RoutageUE2(users);

    for(int i=0; i<users; i++){
        RoutageUE1[i] = Routeuripv4.GetStaticRouting(UE1N.Get(i)->GetObject<Ipv4>());
        RoutageUE1[i]->SetDefaultRoute(EPC->GetUeDefaultGatewayAddress(), 1);
        RoutageUE2[i] = Routeuripv4.GetStaticRouting(UE2N.Get(i)->GetObject<Ipv4>());
        RoutageUE2[i]->SetDefaultRoute(EPC->GetUeDefaultGatewayAddress(), 1);
    }

    Ptr<Ipv4> ipv4IMS = IMS->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> staticRoutingIMS = Routeuripv4.GetStaticRouting(ipv4IMS);
    staticRoutingIMS->SetDefaultRoute(IMSIP.GetAddress(1), 1);

    // Attach UE devices to eNBs
    E_UTRAN->Attach(UE1, eNB.Get(0));
    E_UTRAN->Attach(UE2, eNB.Get(1));

    //JITTER CONFIGURARTIONS i thought putting them all here is better for us in the future

    // Set up communication bearers and QoS for EVS
    EpsBearer::Qci volteQci = EpsBearer::GBR_CONV_VOICE;  //this is anti jitter and and anti packet loss
    GbrQosInformation qos;
    qos.gbrDl = 5900;  // GBR downlink for WB EVS
    qos.gbrUl = 5900;  // GBR uplink for WB EVS
    qos.mbrDl = 128000;  // MBR downlink for WB EVS
    qos.mbrUl = 128000;  // MBR uplink for WB EVS
    EpsBearer bearer(volteQci, qos);

    E_UTRAN->ActivateDedicatedEpsBearer(UE1, bearer, EpcTft::Default()); //this is anti jitter and and anti packet loss
    E_UTRAN->ActivateDedicatedEpsBearer(UE2, bearer, EpcTft::Default()); //this is anti jitter and and anti packet loss

    // Configure the carrier and MAC scheduler for voice optimization
    E_UTRAN->SetSchedulerType("ns3::TtaFfMacScheduler"); // helps with jitter
    Config::SetDefault("ns3::LteEnbRrc::EpsBearerToRlcMapping", EnumValue(LteEnbRrc::RLC_UM_ALWAYS)); //helps with jitter and volte and all
    Config::Set("/ChannelList/0/Delay", TimeValue(MilliSeconds(20)));//this sets fixed delay for the core network, helps with delay


    //EVS LINUX CONFIGURAION
    std::string cmd =
        "rm -f "+dir+"in.32k "
        " "+dir+"in.bit "
        " "+dir+"recieved.bit "
        " "+dir+"out.32k "
        " "+dir+"out.wav";
    std::system(cmd.c_str());

    std::string ffmpegCmd = "ffmpeg -y -i " + dir + "in.wav -f s16le -acodec pcm_s16le -ar 32000 -ac 1 " + dir + "in.32k";
    std::string encodeCmd = dir + "EVS_cod 32000 32 " + dir + "in.32k " + dir + "in.bit";
    std::system(ffmpegCmd.c_str());
    std::system(encodeCmd.c_str());

    std::string createrec = "touch "+dir+"recieved.bit";
    std::system(createrec.c_str());


    // Set up SIP packet exchange
    std::vector<Ptr<Socket>> recsipUE1(users);
    std::vector<Ptr<Socket>> recsipUE2(users);
    std::vector<Ptr<Socket>> UE1socket(users);
    std::vector<Ptr<Socket>> UE2socket(users);

    for(int i=0; i<users; i++){
        recsipUE1[i] = Socket::CreateSocket(UE1N.Get(i), UdpSocketFactory::GetTypeId());
        recsipUE1[i]->Bind(InetSocketAddress(UE1NIP.GetAddress(i), 7000 + i));
        recsipUE1[i]->SetRecvCallback(MakeCallback(&ReceivePacketsip));

        recsipUE2[i] = Socket::CreateSocket(UE2N.Get(i), UdpSocketFactory::GetTypeId());
        recsipUE2[i]->Bind(InetSocketAddress(UE2NIP.GetAddress(i), 7050 + i));
        recsipUE2[i]->SetRecvCallback(MakeCallback(&ReceivePacketsip));

        UE1socket[i] = Socket::CreateSocket(UE1N.Get(i), UdpSocketFactory::GetTypeId());
        UE1socket[i]->Bind();
        UE2socket[i] = Socket::CreateSocket(UE2N.Get(i), UdpSocketFactory::GetTypeId());
        UE2socket[i]->Bind();
    }
    Ptr<Socket> recIMS = Socket::CreateSocket(IMS, UdpSocketFactory::GetTypeId());
    recIMS->Bind(InetSocketAddress(IMSIP.GetAddress(0), 7003));
    recIMS->SetRecvCallback(MakeCallback(&ReceivePacketsip));
    Ptr<Socket> IMSsocket = Socket::CreateSocket(IMS, UdpSocketFactory::GetTypeId());
    IMSsocket->Bind();

    //manual sockets for control over jitter,
    std::vector<Ptr<Socket>> recsipUE1c(users);
    std::vector<Ptr<Socket>> recsipUE2c(users);
    std::vector<Ptr<Socket>> UE1socketc(users);
    std::vector<Ptr<Socket>> UE2socketc(users);

        recsipUE1c[0] = Socket::CreateSocket(UE1N.Get(0), UdpSocketFactory::GetTypeId());
        recsipUE1c[0]->Bind(InetSocketAddress(UE1NIP.GetAddress(0), 5000));
        recsipUE1c[0]->SetRecvCallback(MakeCallback(&ReceivePacketm));
        recsipUE2c[0] = Socket::CreateSocket(UE2N.Get(0), UdpSocketFactory::GetTypeId());
        recsipUE2c[0]->Bind(InetSocketAddress(UE2NIP.GetAddress(0), 6000 ));
        recsipUE2c[0]->SetRecvCallback(MakeCallback(&ReceivePacket));
        UE1socketc[0] = Socket::CreateSocket(UE1N.Get(0), UdpSocketFactory::GetTypeId());
        UE1socketc[0]->Bind();
        UE2socketc[0] = Socket::CreateSocket(UE2N.Get(0), UdpSocketFactory::GetTypeId());
        UE2socketc[0]->Bind();

    for(int i=1; i<users; i++){
        recsipUE1c[i] = Socket::CreateSocket(UE1N.Get(i), UdpSocketFactory::GetTypeId());
        recsipUE1c[i]->Bind(InetSocketAddress(UE1NIP.GetAddress(i), 5000 + i));
        recsipUE1c[i]->SetRecvCallback(MakeCallback(&ReceivePacket));

        recsipUE2c[i] = Socket::CreateSocket(UE2N.Get(i), UdpSocketFactory::GetTypeId());
        recsipUE2c[i]->Bind(InetSocketAddress(UE2NIP.GetAddress(i), 6000 + i));
        recsipUE2c[i]->SetRecvCallback(MakeCallback(&ReceivePacket));

        UE1socketc[i] = Socket::CreateSocket(UE1N.Get(i), UdpSocketFactory::GetTypeId());
        UE1socketc[i]->Bind();
        UE2socketc[i] = Socket::CreateSocket(UE2N.Get(i), UdpSocketFactory::GetTypeId());
        UE2socketc[i]->Bind();
    }

    //start convo with sip
    for(int i=0; i<users; i++){
        Simulator::Schedule(Seconds(1.1), &SendPacketsip, UE1socket[i], InetSocketAddress(IMSIP.GetAddress(0), 7003), 600, "sending SIP invite"); // INVITE
        Simulator::Schedule(Seconds(1.2), &SendPacketsip, IMSsocket, InetSocketAddress(UE2NIP.GetAddress(i), 7050 + i), 600, "forwarding SIP invite"); // FORWARD
        Simulator::Schedule(Seconds(1.3), &SendPacketsip, UE2socket[i], InetSocketAddress(IMSIP.GetAddress(0), 7003), 400, "sending SIP okay");  // OK
        Simulator::Schedule(Seconds(1.4), &SendPacketsip, IMSsocket, InetSocketAddress(UE1NIP.GetAddress(i), 7000 + i), 400, "forwarding SIP okay"); // 200 OK
    }

    std::ifstream file(dir+"in.bit", std::ios::binary);
    double EVS_interval = 0.02;
    double sendtime = 2;
    uint16_t word;
    std::vector<uint16_t> mes;
    int crowcount = 0;
    float rand_val;
    //float lossper = 20;
    while (file.read(reinterpret_cast<char*>(&word), sizeof(word))){

        if(word == 0x6B21 || word == 0x6b20){
            crowcount = crowcount + 1;
        }
        if (crowcount == 1){
            mes.push_back(word);
        }
        else if(crowcount == 2){
            for(int j=0; j<users; j++){
                rand_val = rand() % 99;
                if(rand_val >=lossper){
                    Simulator::Schedule(Seconds(sendtime + randomdelay()), &SendPacket, UE1socketc[j], InetSocketAddress(UE2NIP.GetAddress(j), 6000 + j), sendtime, mes);
                    Simulator::Schedule(Seconds(sendtime + randomdelay()), &SendPacket, UE2socketc[j], InetSocketAddress(UE1NIP.GetAddress(j), 5000 + j), sendtime, mes);
                }
                sentpack = sentpack + 2;
            }
            mes.clear();
            file.seekg(-2, std::ios::cur);
            crowcount = 0;
            sendtime = sendtime + EVS_interval;
        }
    }
    if (crowcount == 1 ) {
        for(int j=0; j<users; j++){
            rand_val = rand() % 99;
            if(rand_val >=lossper){
                Simulator::Schedule(Seconds(sendtime + randomdelay()), &SendPacket, UE1socketc[j], InetSocketAddress(UE2NIP.GetAddress(j), 6000 + j), sendtime, mes);
                Simulator::Schedule(Seconds(sendtime + randomdelay()), &SendPacket, UE2socketc[j], InetSocketAddress(UE1NIP.GetAddress(j), 5000 + j), sendtime, mes);
            }
            sentpack = sentpack + 2;
        }
    }

    //end convo with sip
    float end = sendtime +1;
    for(int i=0; i<users; i++){
        Simulator::Schedule(Seconds(end+0.01), &SendPacketsip, UE1socket[i], InetSocketAddress(IMSIP.GetAddress(0), 7003), 600, "sending SIP bye"); //
        Simulator::Schedule(Seconds(end+0.02), &SendPacketsip, IMSsocket, InetSocketAddress(UE2NIP.GetAddress(i), 7050 + i), 600, "forwarding SIP bye"); // FORWARD
        Simulator::Schedule(Seconds(end+0.03), &SendPacketsip, UE2socket[i], InetSocketAddress(IMSIP.GetAddress(0), 7003), 400, "sending SIP okay");  // OK
        Simulator::Schedule(Seconds(end+0.04), &SendPacketsip, IMSsocket, InetSocketAddress(UE1NIP.GetAddress(i), 7000 + i), 400, "forwarding SIP okay"); // 200 OK
    }
    file.close();

    Simulator::Stop(Seconds(end+1));
    Simulator::Run();

    std::string decodeCmd = dir + "EVS_dec 32 " + dir + "recieved.bit " + dir + "out.32k";
    std::system(decodeCmd.c_str());

    std::string ffmpegCmdout = "ffmpeg -f s16le -ar 32000 -ac 1 -i " + dir +"out.32k " + dir +"out.wav";
    std::system(ffmpegCmdout.c_str());

    NS_LOG_INFO("   ");
    NS_LOG_INFO("------------------------------------------------------------------------------------------------------------------------------------");
    NS_LOG_INFO("   ");
    NS_LOG_INFO("jitter: " << (jit/(recpack-1))*1000 << " ms");
    NS_LOG_INFO("sent packets " << sentpack);
    NS_LOG_INFO("received packets " << recpack);
    NS_LOG_INFO("lost packets " << sentpack-recpack);
    NS_LOG_INFO("packet loss percentage " <<(sentpack-recpack)*100 / sentpack << "%");

    Simulator::Destroy();
    return 0;
}
