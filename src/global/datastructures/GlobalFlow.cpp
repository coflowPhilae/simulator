/*
 * GlobalFlow.cpp
 *
 *      Author: 
 */

#include "GlobalFlow.h"
#include "GlobalJob.h"

GlobalFlow::GlobalFlow(flowSizeT sizeInBytes, unsigned int executionMode, flowIdT flowId) : executionMode(executionMode),id(flowId) {
    commonInitialization();
    setSizeInBytes(sizeInBytes);
    bytesSentSoFar = 0;
//	cout <<"flow constructor fid: "<<id<<" execMode: "<<executionMode<<"\n";
}

void GlobalFlow::commonInitialization() {
    setRate(0);
    setOtherIds();
    markNonProbe();
}

void GlobalFlow::setSizeInBytes(flowSizeT size) {
    totalSizeInBytes = size;
    sizeRemaining = size;
}


flowSizeT GlobalFlow::getSizeInBytes() {
    return totalSizeInBytes;
}

flowSizeT GlobalFlow::getRemainingSize() {
    if (executionMode == Utils::simulation) {
        return sizeRemaining;
    } else {
        Utils::utilError("Called simulation function, getRemainingSize, from Global flow in non simulation mode");
        return Utils::invalidValue;
    }
}

flowSizeT GlobalFlow::getBytesSentSoFar() {
    return bytesSentSoFar;
}

bool GlobalFlow::finished() {    //	return true if flows is finished
//	if(executionMode == Utils::simulation){
    if (sizeRemaining <= 0) {
        return true;
    } else {
        return false;
    }
//	}else{
//		Utils::utilError("Called simulation function, finished, from Global flow in non simulation Mode");
//		cout <<"executionMode: "<<executionMode<<"\n";
//		return false;
//	}
}

void GlobalFlow::updateFlowBytesSentOnFlowCompletion(){	// this function should only be called when size is not being updated by any other means.
    bytesSentSoFar += getSizeInBytes();
    sizeRemaining -= getSizeInBytes();
}

flowSizeT GlobalFlow::simulate(timeT simTime) {
    rateT rate = getRate();
    flowSizeT maxCanSend = rate * simTime;
    if (sizeRemaining < maxCanSend) {
        maxCanSend = sizeRemaining;
    }
    bytesSentSoFar += maxCanSend;
    sizeRemaining -= maxCanSend;
    return maxCanSend;
}

flowSizeT GlobalFlow::decrementSizeRemaining(flowSizeT upto) {
    if (sizeRemaining < upto) {
        upto = sizeRemaining;
    }
    sizeRemaining -= upto;
    return upto;
}

void GlobalFlow::setRate(rateT newRate) {
//	cout<<"GF SR Resetting Rate: "<<id<<" rate: "<<getRate()<<"\n";
    rate = newRate;
//	cout << "GF SR id: "<<id<<" rate: "<<rate<<" newRate: "<<newRate<<"\n";
}

rateT GlobalFlow::getRate() {
    return rate;
}

void GlobalFlow::setOtherIds() {
    jobId = Utils::getJobIdFromFlowId(id);
    mapId = Utils::getMapPortNumFromFlowId(id);
    redId = Utils::getRedPortNumFromFlowId(id);
}

bool GlobalFlow::markProbe() {
    if (isProbe()) {
        return false;
    } else {
//		cout<< "marked as probe: "<<id<<"\n";
        probe = true;
    }
    return true;
}

void GlobalFlow::markNonProbe() {
    probe = false;
}

bool GlobalFlow::isProbe() {
    return probe;
}


Task* GlobalFlow::getRedTaskPtr() {
    if (jobPtr->redTasksMap.find(Utils::getRedTaskIdFromFlowId(id))==jobPtr->redTasksMap.end()){
        cout << "Couldn't find redTask for fid: "<<id<<endl;
    } else {
        Task *taskPtr = jobPtr->redTasksMap.find(Utils::getRedTaskIdFromFlowId(id))->second;
        return taskPtr;
    }
    return nullptr;
}

Task* GlobalFlow::getMapTaskPtr() {
    if (jobPtr->mapTasksMap.find(Utils::getMapTaskIdFromFlowId(id))==jobPtr->mapTasksMap.end()){
        cout << "Couldn't find mapTask for fid: "<<id<<endl;
    } else {
        Task *taskPtr = jobPtr->mapTasksMap.find(Utils::getMapTaskIdFromFlowId(id))->second;
        return taskPtr;
    }
    return nullptr;
}

GlobalFlow::~GlobalFlow() {
}
