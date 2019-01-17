#ifndef UTILITY_HPP_
#define UTILITY_HPP_

#include <iostream>
#include <fstream>
#include <sstream>
#include <string.h>   //strlen
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <vector>
#include <cassert>
#include <climits>
#include <iostream>
#include <iomanip>
#include <string>
#include <map>
#include <random>
#include <cmath>

using namespace std;

class GlobalJob;
typedef unsigned long long jobSizeT;
typedef unsigned long jobIdT;
typedef unsigned long long flowSizeT;
typedef long long int impactFactorT;
typedef int contentionFactorT;
typedef unsigned long long int flowIdT;
typedef unsigned long mapIdT;    // will be used for mapperNum too.
typedef unsigned long redIdT;    // will be used for receiverNum too.
typedef unsigned long portIdT;    // will used for portNum too.
typedef unsigned int rateT;        // used for rate and capacity
typedef unsigned long int timeT;
typedef unsigned long long taskSizeT;
typedef unsigned long long taskIdT;
typedef unsigned long long mapTaskIdT;
typedef unsigned long long redTaskIdT;

namespace Utils {
    //	constants
    const unsigned int b = 1;    //	bits
    const unsigned int Kb = 1024 * b;
    const unsigned int Mb = 1024 * Kb;
    const unsigned int Gb = 1024 * Mb;
    const unsigned int B = 1;    //	bytes
    const unsigned int KB = 1024 * B;
    const unsigned int MB = 1024 * KB;
    const unsigned int GB = 1024 * MB;
    const unsigned int bitsPerByte = 8;
    const int invalidValue = -100;
    const unsigned int minCapacityToBeAllocated = 1;
    //	connection informations
    const unsigned int coordinatorPort = 8888;
    const unsigned int coordinatorListenerBacklog = 300;
    const unsigned int receiverBaseListnerPort = 9999;
    const unsigned int receiverListenerBacklog = 300;
    //	trace info - DEPRICATED - use variables totalPorts and totalJobs defined in the class Coordinator.
    const unsigned int numPorts = 12;
    const unsigned int numCoFlows = 527;
    //	daemon types
    const unsigned int daemonTypeCoordinatorSim = 1;
    const unsigned int daemonTypeSenderPort = 2;
    const unsigned int daemonTypeReceiverPort = 3;
    const unsigned int readConfig = 5;
    //	task types
    const unsigned int taskTypeMap = daemonTypeSenderPort;
    const unsigned int taskTypeRed = daemonTypeReceiverPort;
    //	fabric parameters
    const int orderNumberOfPorts = 100000;    // if this is 1000 you can have atmost 999 ports if this is 10000 you can have atmost 9999 ports.... you see the pattern.
    const int orderNumberOfJobs = 100000;    // if this is 1000 you can have atmost 999 ports if this is 10000 you can have atmost 9999 ports.... you see the pattern.
    const rateT portBitsPerSec = 1 * Gb;	// make it 0.5 for testbed -- for test trace data is also reduced to half and queuelimits also need to be made half this way everything relatively remains same. This has to be done as we are using 1 machine to emulate 2 ports on it.
    const rateT portBytesPerSec = portBitsPerSec / bitsPerByte;
    const rateT portBitsPerMSec = 0.001 * portBitsPerSec;
    const rateT portBytesPerMSec = portBitsPerMSec / bitsPerByte;
    const unsigned int maxNumOfAllowedFlows = 32768;
    //	coordinators parameter
    const unsigned int rateUpdateQuantaInMSec = 8;
    const unsigned int dataSentUpdateQuantaInMSec = 8;
//    const unsigned int simQuantaInMSec;
    const unsigned int numJobQueues = 10;    // DEPRICATED - use virtual function getNumJobQueues in define in the class Coordinators
    const unsigned int q0Limit = portBytesPerMSec * rateUpdateQuantaInMSec;    // The value in the variable is in Bytes.
    //	scheduling policies
    const unsigned int schedulingPolicyPHILAE = 7;
    // starvation freedom selection method and related parameters
    const unsigned int starvationVulnerable = 0;
    const unsigned int starvationFreedomTimeSlot = 1;
    const unsigned int starvationFreedomDeadline = 2;
    // pilot flow selection method
    const unsigned int randomPilotFlows = 0;
    const unsigned int leastContentionPilotFlows = 1;

    // number of pilot flows selection method
    const unsigned int minMapRedPilotFlows = 0;
    const unsigned int constantPilotFlows = 1;
    const unsigned int proportionalMapperPilotFlows = 3;
    const unsigned int proportionalMINMRPilotFlows = 2;
    const unsigned int proportionalMCrossRPilotFlows = 4;
    const float proportionalPilotFlowsRatio = 0.05;
    const unsigned int constantPilotFlowsValue = 5;
    // thin limit
    const unsigned int thinProbingLimit = 7;
    //	job, flow, port transition types
    const int transitionTypeArrival = 1;
    const int transitionTypeDeparture = -1 * transitionTypeArrival;
    //	executionType
    const unsigned int simulation = daemonTypeCoordinatorSim;
    //	Scheduling Policy specific parameters
    const unsigned int totalAdditionalQueuesPHILAE = 2;	// this the total number of non-mainJob queues, like thin queue, probe queue etc. 
	const unsigned int firstMainQIndexPHILAE = totalAdditionalQueuesPHILAE; 
	const unsigned int philaeNonProbingOracle = 2;
    const unsigned int probeQPhilae = 1;
    const unsigned int thinQPhilae = 0;
    //	scheduler configuration parameters
    const unsigned int jobSizeMetricTotalData = 1;    //	whenever changing this update actOnLocal.py file goo
    const unsigned int jobSizeMetricLongestFlowLength = 2;    //	whenever changing this update actOnLocal.py file too
    const unsigned int jobSizeMetricEstimatedFlowLength = 3;    //	whenever changing this update actOnLocal.py file too
    const unsigned int jobSizeMetricNoUpdate = 4;    //	whenever changing this update actOnLocal.py file too
    const unsigned int rateAssignmentOrderNsquared = 1;
    const unsigned int rateAssignmentOrderN = 2;
    const unsigned int orderP = 1;
    const unsigned int orderP2 = 0;
    const unsigned int LRIFPeriod = 1000;
    const unsigned int longestRemainingJobWiseImpact = 0;
    const unsigned int initialSizeImpact = 1;
    const unsigned int remainingSizeImpact = 2;
    const unsigned int simpleContentionImpact = 3;
    const unsigned int simpleJobWiseImpact = 4;
    const unsigned int maxPortWiseImpact = 5;
    const unsigned int sumPortWiseImpact = 6;
    const unsigned int bottleneckImpact = 7;
    const unsigned int TestingNewThings = 100;
    //	approximation parameters
    const int maxRateAssignLapse = 0;
    const flowSizeT minAllowedFlowLength = 1;
    const int totalTimeSlotNum = 256;
    const int timeSplittingFactor = 10;//change timeSplittingFactor to change the ratio of bandwidth share between different queues. If it is 10 then sharing is like for 3 queues 100, 10, 1.
    int getReceiverListnerPort(redIdT receiverPortNum);
    vector<string> getPortNumIpVector(char *filePath);

    void utilError(string msg);

    void utilMsg(string msg);

    flowIdT makeFlowId(jobIdT jobId, mapIdT mapId, redIdT redId);

    jobIdT getJobIdFromFlowId(flowIdT flowId);

    mapIdT getMapPortNumFromFlowId(flowIdT flowId);

    redIdT getRedPortNumFromFlowId(flowIdT flowId);

    taskIdT makeTaskId(jobIdT jobId, portIdT portNum, int taskType);

    mapTaskIdT getMapTaskIdFromFlowId(flowIdT flowId);

    redTaskIdT getRedTaskIdFromFlowId(flowIdT flowId);

    mapTaskIdT getMapTaskIdFromFlowIdAndJobId(flowIdT flowId, jobIdT jobId);

    redTaskIdT getRedTaskIdFromFlowIdAndJobId(flowIdT flowId, jobIdT jobId);

    mapTaskIdT getMapTaskIdFromPortIdAndJobId(portIdT pNum, jobIdT jobId);

    redTaskIdT getRedTaskIdFromPortIdAndJobId(portIdT pNum, jobIdT jobId);

    vector<int> randomInteger(int min, unsigned long max, int size);

    long long int minInt(long long int int1, long long int int2);

    long long int maxInt(long long int int1, long long int int2);
}

#endif /* UTILITY_HPP_ */
