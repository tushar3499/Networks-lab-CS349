//Standard C++ libraries
#include <fstream>
#include<bits/stdc++.h>

//Importing ns3 modules required
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/stats-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor-module.h"

#define Mp std::map<FlowId, FlowMonitor::FlowStats>

//We will use both C++ and ns3 functions
using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("CongestionWindow_PacketDrop_Study");

//Defining our application class derived from ns3 Application class
class MyApplication : public Application{
	
	public:
		MyApplication();
		virtual ~MyApplication();

		static TypeId GetTypeId(void){
			static TypeId tid = TypeId("MyApplication").SetParent<Application>().SetGroupName("Tutorial").AddConstructor<MyApplication>();
			return tid;
		}
		// Setup connection by initializing the member variables
		void Setup(Ptr<Socket> socket, Address address, uint32_t size_packet, uint32_t nPackets, DataRate dataRate){
			socket_app = socket;
			peer_app = address;
			packetSize_app = size_packet;
			maxPackets_app = nPackets;
			dataRate_app = dataRate;
		}
		
	private:
	
		// Starts the application by initial binding
		void StartApplication(void){
			isRunning_app = true;
			packetSent_app = 0;
			
			// Ipv4/Ipv6 binding
			if(InetSocketAddress::IsMatchingType(peer_app)) socket_app->Bind();
			else socket_app->Bind6();
			socket_app->Connect(peer_app);
			SendPacket();
		}

		// Create and send a packet
		void SendPacket(void){
			Ptr<Packet> packet = Create<Packet>(packetSize_app);
			socket_app->Send(packet);
			
			//If max packets not sent, schedule next transmission
			if(++packetSent_app < maxPackets_app) Schedule_next();
		}

		// schedule next transmission
		void Schedule_next(void){
			if(isRunning_app)
			{
				Time next_time(Seconds(packetSize_app * 8 / static_cast<double>(dataRate_app.GetBitRate())));
				sendEvent_app = Simulator::Schedule(next_time, &MyApplication::SendPacket, this);
			}
		}

		// Stops generating packets by cancelling any pending send events then closes the socket
		void StopApplication(void){
			isRunning_app = false;
			if(sendEvent_app.IsRunning()) Simulator::Cancel(sendEvent_app);
			if(socket_app) socket_app->Close();
		}

		Ptr<Socket> socket_app;
		Address peer_app;
		uint32_t packetSize_app;
		uint32_t maxPackets_app;
		DataRate dataRate_app;
		EventId sendEvent_app;
		bool isRunning_app;
		uint32_t packetSent_app;
};

			// Constructor for intialising values
	   MyApplication::MyApplication(): socket_app(0),peer_app(), packetSize_app(0), maxPackets_app(0), dataRate_app(0), sendEvent_app(), isRunning_app(false), packetSent_app(0)
		{

		}
		// Remove socket after closing the application
	    MyApplication::~MyApplication()
		{
		socket_app = 0;
		}

// Flow Monitor for stats
Ptr<FlowMonitor> flow_monitor;
FlowMonitorHelper flowHelper;

// Store packet drop and time
vector< pair <float,int> > packets_drop;

// Generate Constant Bit-Rate UDP traffic within the given start_time and stop_time 
void simulateCBR(NodeContainer &nodes, OnOffHelper &onOff, uint64_t start_time, uint64_t stop_time)
{
	//Set the data rate,start and end time for the simulation
	onOff.SetAttribute("DataRate", StringValue("250Kbps"));
	onOff.SetAttribute("StartTime", TimeValue(MilliSeconds(start_time)));
	onOff.SetAttribute("StopTime", TimeValue(MilliSeconds(stop_time)));

	//Application to be alloted to node 0
	ApplicationContainer cbr;
	cbr.Add(onOff.Install(nodes.Get(0)));

	//Simulating CBR traffic
	cbr.Start(MilliSeconds(start_time));
	cbr.Stop(MilliSeconds(stop_time));
}

// Information about packets drop
void tracePacketDrop()
{
	Mp stats = flow_monitor->GetFlowStats();
	float cur_time = Simulator::Now().GetSeconds();
	int num_packets = 0;
	// Count the TCP packet drops which are in Channel 1 & 2 only i.e. TCP Tx and Rx
	for(int j = 1; j <= 2; j++){
		if(stats[j].packetsDropped.size() >= 5){
			num_packets+=stats[j].packetsDropped[3]+stats[j].packetsDropped[4];
		}
	}

	packets_drop.push_back({cur_time, num_packets});
	Simulator::Schedule(Seconds(0.01), &tracePacketDrop);
}

static void CwndChange(Ptr<OutputStreamWrapper> stream, uint32_t old_Cwnd, uint32_t new_Cwnd)
{
	*stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << old_Cwnd << "\t" << new_Cwnd << '\n';
}
    
// configure Tcp to given input
void setup(int x)
{
	if(x == 0){
		Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId())); 
	}else if(x == 1){
		Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpHybla::GetTypeId()));
	}else if(x == 2){
		Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpWestwood::GetTypeId()));
		//Config::SetDefault("ns3::TcpWestwood::FilterType", EnumValue(TcpWestwood::TUSTIN));
	}else if(x == 3){
		Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpScalable::GetTypeId()));
	}else{
		Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpVegas::GetTypeId()));
	}
}
    
double calculate(int t,double l,double f)
{
	return t * 8.0 / ( l- f) / 1000000 ;
}

int main(int argc, char *argv[])
{
	// Take arguments from command line and get the transport protocol
	CommandLine cmd;
	cmd.Parse(argc, argv);
	string tcp_version = argv[1];

	// Create nodesfor the simulation of point to point network
	NodeContainer nodes;
	nodes.Create(2);

	// Create Link between 2 nodes and set speed and delay and add drop tail queue
	PointToPointHelper tcp_connection;
	tcp_connection.SetDeviceAttribute("DataRate", StringValue("1Mbps"));					
	tcp_connection.SetChannelAttribute("Delay", StringValue("10ms"));
	
	//The maxsize has to be greater than bandwidth delay products
	tcp_connection.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("1500B"));

	// Connect link with nodes
	NetDeviceContainer devices;
	devices = tcp_connection.Install(nodes);

	//Configuring the requirements for different TCP versions 
	vector <string> v= {"TcpNewReno","TcpHybla","TcpWestwood","TcpScalable","TcpVegas"};
	int x;
	for(x = 0; x < 5; x++){
		if(tcp_version == v[x]){
			setup(x);
			break;
		}
	}
	
	//If the TCP version does not match the given ones
	if(x == 5){
		cout<<"This Tcp version is not supported"<<endl;
		cout<<"These versions of Tcp are available"<<endl;
		cout<<"1.TcpNewReno"<<endl;
		cout<<"2.TcpHybla"<<endl;
		cout<<"3.TcpWestWood"<<endl;
		cout<<"4.TcpScalable"<<endl;
		cout<<"5.TcpVegas"<<endl;
		NS_LOG_DEBUG("Invalid TCP version");
		exit(1);
	}


	// Set up IpV4 routing and related objects installation
	InternetStackHelper stack;
	stack.Install(nodes);
	
	// Create base IP address and assign IP addresses to the nodes
	NS_LOG_INFO("Assign IP Addresses.");
	Ipv4AddressHelper address;
	address.SetBase("10.1.1.0", "255.255.255.0");
	Ipv4InterfaceContainer interfaces = address.Assign(devices);
	
	//Port number and Address of receiver, i.e Node 1
	uint16_t sink_port = 8080;
	Ipv4Address temp = interfaces.GetAddress(1);
	Address sink_address = InetSocketAddress(temp, sink_port);;
	Address any_address = InetSocketAddress(Ipv4Address::GetAny(), sink_port);;
	
	//Packets tracing which are transmitted by Node0
	string probe_type = "ns3::Ipv4PacketProbe";
	string path_trace = "/NodeList/*/$ns3::Ipv4L3Protocol/Tx";

	// Initializing packet size and max number of packets which will be transmitted from Node 0 to Node 1
	uint16_t size_packet = 512;
	uint32_t max_packets = 100000;

	// Simulating receving of packets at node 1
	PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", any_address);
	ApplicationContainer packets_sink = packetSinkHelper.Install(nodes.Get(1));
	//packets_sink.Start(MilliSeconds(0.));
	packets_sink.Start(MilliSeconds(0));
	packets_sink.Stop(MilliSeconds(1800));

	// Create the TCP socket and set the sender as Node 0
	Ptr<Socket> tcp_socket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());

	// Create the MyApplication object for the FTP connection
	Ptr<MyApplication> ftp_app = CreateObject<MyApplication>();
	ftp_app->Setup(tcp_socket, sink_address, size_packet, max_packets, DataRate("1Mbps"));

	// Install the simulated Application to Node0
	nodes.Get(0)->AddApplication(ftp_app);
	ftp_app->SetStartTime(MilliSeconds(0));
	ftp_app->SetStopTime(MilliSeconds(1800));

	// Assigning port for CBR
	uint16_t cbr_port = 8000;
	temp=interfaces.GetAddress(1);
	OnOffHelper onOff("ns3::UdpSocketFactory", InetSocketAddress(temp, cbr_port));
	onOff.SetAttribute("OnTime",  StringValue("ns3::ConstantRandomVariable[Constant=1]"));
	onOff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

	//Start and stop times for the CBR to be simulated
	vector <int64_t> start_times{200, 400, 600, 800, 1000};
	vector <int64_t> stop_times{1800, 1800, 1200, 1400, 1600};

	for(int i=0;i<5;i++){
		simulateCBR(nodes, onOff, start_times[i], stop_times[i]);
	}

	// Create a packet sink to receive the packets
	PacketSinkHelper udp_sink("ns3::UdpSocketFactory",InetSocketAddress(Ipv4Address::GetAny(), cbr_port));

	// Simulate an Application to receive the UDP traffic in Node1
	ApplicationContainer cbr_sink = udp_sink.Install(nodes.Get(1));
	cbr_sink.Start(MilliSeconds(0));
	cbr_sink.Stop(MilliSeconds(1800));

	// Store the congestion window data
	AsciiTraceHelper asciiTraceHelper;
	Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("./Traces/"+tcp_version+".cwnd");
	tcp_socket->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback(&CwndChange, stream));

	// Use FileHelper to write out the packet byte count over time
	FileHelper fileHelper;

	// Configure the file to be written, and the formatting of output data.
	fileHelper.ConfigureFile("./Traces/"+tcp_version+"byteCount", FileAggregator::FORMATTED);

	// Set the labels for this formatted output file.
	fileHelper.Set2dFormat("%.3e\t%.0f");

	// Specify the probe type, trace source path(in configuration namespace), and
	// probe output trace source("OutputBytes") to write.
	fileHelper.WriteProbe(probe_type, path_trace, "OutputBytes");

	// Flow monitor
	flow_monitor = flowHelper.InstallAll();

	// Run simulation.
	Simulator::Schedule(Seconds(0.01), &tracePacketDrop);
	Simulator::Stop(MilliSeconds(1800));
	Simulator::Run();

	Mp stats = flow_monitor->GetFlowStats();

	int transmitted_packets = stats[1].txPackets;
	int transmitted_Bytes = stats[1].txBytes;
	double Lasttransmitted_packet = stats[1].timeLastTxPacket.GetSeconds();
	double Firsttransmitted_packet = stats[1].timeFirstTxPacket.GetSeconds();
	double offer_ld=calculate(transmitted_Bytes,Lasttransmitted_packet,Firsttransmitted_packet);
	int recieved_packet=stats[1].rxPackets;
	int recieved_Bytes=stats[1].rxBytes;
	double Lastrecieved_packet = stats[1].timeLastRxPacket.GetSeconds();
	double firstrecieved_packet = stats[1].timeFirstRxPacket.GetSeconds();
	double throughput=calculate(recieved_Bytes,Lastrecieved_packet,firstrecieved_packet);

	cout << '\n' << "Flow monitor output:" << '\n';
	cout << "Tx Packets:   " << transmitted_packets << '\n';
	cout << "Tx Bytes:     " << transmitted_Bytes << '\n';
	cout << "Offered Load: " << offer_ld << " Mbps" << '\n';
	cout << "Rx Packets:   " << recieved_packet << '\n';
	cout << "Rx Bytes:     " << recieved_Bytes << '\n';
	cout << "Throughput:   " << throughput << " Mbps" << '\n';
	cout << "Mean delay:   " << stats[1].delaySum.GetSeconds() / stats[1].rxPackets << '\n';
	cout << "Mean jitter:  " << stats[1].jitterSum.GetSeconds() /(stats[1].rxPackets - 1) << '\n';

	// Write the Flow Monitor data to file
	flow_monitor->SerializeToXmlFile("./Traces/"+tcp_version + ".flow_monitor", true, true);

	for(Mp::const_iterator i = stats.begin(); i != stats.end(); ++i)
	{
		cout << " --------------------------------- " << '\n';
		cout << "Flow Id: " << i->first << '\n';
		cout << "Tx Bytes: " << i->second.txBytes  << '\n';
		cout << "Drop Packet Count: " << i->second.lostPackets << '\n';
	}

	Simulator::Destroy();
	NS_LOG_INFO("Done.");

	// Write the data of the packets dropped into file
	ofstream fileOutput;
	fileOutput.open("./Traces/"+tcp_version+".droppacket");
	std::vector<std::pair <float, int> > :: iterator itr;
	for(itr = packets_drop.begin(); itr != packets_drop.end(); ++itr){
	  fileOutput << itr->first << " " << itr->second << '\n';
	}
    return 0;
}
