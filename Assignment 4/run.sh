if [ ! -d "Traces" ]; then
  mkdir Traces
fi
./waf --run "scratch/script TcpNewReno"
./waf --run "scratch/script TcpHybla"
./waf --run "scratch/script TcpWestwood"
./waf --run "scratch/script TcpScalable"
./waf --run "scratch/script TcpVegas"
	
