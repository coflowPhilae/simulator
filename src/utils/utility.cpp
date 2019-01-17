/*
 * utility.cpp
 *
 *      Author: 
 */

#include "utility.hpp"

namespace Utils{

	void utilError(string msg){
		cerr << msg+"\n";
	}

	void utilMsg(string msg){
		cout << msg+"\n";
	}
	
	flowIdT makeFlowId(jobIdT jobId, mapIdT mapId, redIdT redId){

		flowIdT toReturn = (flowIdT)jobId*((flowIdT)Utils::orderNumberOfPorts*(flowIdT)Utils::orderNumberOfPorts)+(flowIdT)redId*(flowIdT)Utils::orderNumberOfPorts+(flowIdT)mapId;
		return toReturn;
	}

	jobIdT getJobIdFromFlowId(flowIdT flowId){
		return flowId/(flowIdT)((flowIdT)Utils::orderNumberOfPorts*(flowIdT)Utils::orderNumberOfPorts);
	}

	mapIdT getMapPortNumFromFlowId(flowIdT flowId){
		return flowId%(flowIdT)Utils::orderNumberOfPorts;
	}

	redIdT getRedPortNumFromFlowId(flowIdT flowId){
		return (flowId%((flowIdT)((flowIdT)Utils::orderNumberOfPorts*(flowIdT)Utils::orderNumberOfPorts)))/(flowIdT)Utils::orderNumberOfPorts;
	}

	taskIdT makeTaskId(jobIdT jobId, portIdT portNum, int taskType){
		taskIdT toReturn = (taskIdT)jobId*((taskIdT)Utils::orderNumberOfPorts*10)+(taskIdT)portNum*10+(taskIdT)taskType;
		return toReturn;
	}

	mapTaskIdT getMapTaskIdFromFlowId(flowIdT flowId){
		return makeTaskId(getJobIdFromFlowId(flowId), getMapPortNumFromFlowId(flowId), taskTypeMap);
	}

	redTaskIdT getRedTaskIdFromFlowId(flowIdT flowId){
		return makeTaskId(getJobIdFromFlowId(flowId), getRedPortNumFromFlowId(flowId), taskTypeRed);
	}

	mapTaskIdT getMapTaskIdFromFlowIdAndJobId(flowIdT flowId, jobIdT jobId){
		return makeTaskId(jobId, getMapPortNumFromFlowId(flowId), taskTypeMap);
	}

	redTaskIdT getRedTaskIdFromFlowIdAndJobId(flowIdT flowId, jobIdT jobId){
		return makeTaskId(jobId, getRedPortNumFromFlowId(flowId), taskTypeRed);
	}

	mapTaskIdT getMapTaskIdFromPortIdAndJobId(portIdT pNum, jobIdT jobId){
		return makeTaskId(jobId, pNum, taskTypeMap);
	}

	redTaskIdT getRedTaskIdFromPortIdAndJobId(portIdT pNum, jobIdT jobId){
		return makeTaskId(jobId, pNum, taskTypeRed);
	}

	long long int minInt(long long int int1, long long int int2){
		if(int1>int2){
			return int2;
		}else{
			return int1;
		}
	}

	long long int maxInt(long long int int1, long long int int2){
		if(int1<int2){
			return int2;
		}else{
			return int1;
		}
	}

    	vector<int> randomInteger(int min, unsigned long max, int size) {
        	vector<int> toReturn(size);
        	for (int i = 0; i < size; i++) {
        	    std::mt19937 rng;
        	    rng.seed(std::random_device()());
        	    std::uniform_int_distribution<std::mt19937::result_type> rrr(min, max); // distribution in range [1, 6]]
        	    toReturn[i] = rrr(rng);
        	}
        	return toReturn;
    	}
}
