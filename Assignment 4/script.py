from pandas import read_csv
import numpy as np
import matplotlib.pyplot as plt
from pylab import rcParams

#exec(%matplotlib inline)

rcParams['figure.figsize'] = 20, 10

versions = ['NewReno', 'Hybla', 'Westwood', 'Scalable', 'Vegas']

for version in versions:
    data = read_csv('./Traces/Tcp'+version+'.cwnd', delimiter='\s', header=None, engine='python')
    X = data[0]
    Y = data[2]
    plt.plot(X, Y, label=version)
plt.legend()
plt.xlabel('Time (in seconds)')
plt.ylabel('CWND size')
plt.title('Comparision of Congestion Window size for various TCP versions')
plt.savefig('./Traces/CWND Comparision.png')
plt.show()

for version in versions:
    data = read_csv('./Traces/Tcp'+version+'byteCount-0.txt', delimiter='\s', header=None, engine='python')
    X = data[0]
    Y = np.cumsum(data[1])
    plt.plot(X, Y, label=version)
plt.xlabel('Time (in seconds)')
plt.ylabel('Cumulative bytes transferred')
plt.title('Cumulative bytes transferred vs Time for various TCP versions')
plt.savefig('./Traces/Bytes Transferred.png')
plt.legend()
plt.show()

for version in versions:
    data = read_csv('./Traces/Tcp'+version+'.droppacket', delimiter='\s', header=None, engine='python')
    X = data[0]
    Y = data[1]
    plt.plot(X, Y, label=version)

plt.xlabel('Time (in seconds)')
plt.ylabel('Cumulative packets dropped')
plt.title('Cumulative packets dropped vs Time for various TCP versions')
plt.savefig('./Traces/Packets dropped.png')
plt.legend()
plt.show()

