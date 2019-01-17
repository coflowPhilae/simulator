/*
 * Globalport.cpp
 *
 *      Author: 
 */

#include "GlobalPort.h"

GlobalPort::GlobalPort(portIdT portNum, int portType) : portNum(portNum), portType(portType) {
    commonInitialization();
    executionMode = Utils::simulation;
}

void GlobalPort::commonInitialization() {
    setCapacity(Utils::portBytesPerMSec);
    activeProbeFlowCount = 0;
}

rateT GlobalPort::getCapacity() {
    return capacity;
}

void GlobalPort::setCapacity(rateT cap) {
    capacity = cap;
}

void GlobalPort::addJob(GlobalJob *job) {
    /*	Below is a code written by iterating over map replacing it with vector	*/
    std::vector<GlobalFlow *>::iterator begin, end;
    int flowCount = 0;
    for (begin = job->flowsVec.begin(), end = job->flowsVec.end(); begin != end; begin++) {
        portIdT pNum = Utils::invalidValue;
        flowIdT fid = (*begin)->id;
        if (portType == Utils::daemonTypeSenderPort) {
            pNum = (*begin)->mapId;
        } else if (portType == Utils::daemonTypeReceiverPort) {
            pNum = (*begin)->redId;
        }
        if (pNum == portNum) {
            flowCount++;
            activeFlowsMap.insert(std::make_pair<flowIdT, GlobalFlow *>(std::move(fid), std::move((*begin))));
            activeFlowsVec.push_back((*begin));
        }
    }
    if (flowCount > 0) {
        activeJobs.insert(std::make_pair<jobIdT, int>(std::move(job->id), std::move(flowCount)));
    }
}

bool GlobalPort::removeFlow(flowIdT fid, jobIdT jid) {    // returns true if job no more exists on the port
    activeFlowsMap.erase(std::move(fid));
    std::vector<GlobalFlow *>::iterator afvib, afvie;
    for (afvib = activeFlowsVec.begin(), afvie = activeFlowsVec.end(); afvib != afvie; afvib++) {
        if ((*afvib)->id == fid) {
            activeFlowsVec.erase(afvib);
            break;
        }
    }

    std::unordered_map<jobIdT, int>::iterator it = activeJobs.find(jid);
    if (it != activeJobs.end()) {
        it->second = it->second - 1;
        if (it->second == 0) {
            removeJob(jid);
            return true;
        }
    } else {
        Utils::utilError("unexpected error in GlobalPort::removeFlow job not found");
    }
    return false;
}

void GlobalPort::removeJob(jobIdT jid) {
    activeJobs.erase(std::move(jid));
}

bool GlobalPort::addProbeFlow(flowIdT fid) {
    activeProbeFlowCount++;
    return true;
}

bool GlobalPort::removeProbeFlow(flowIdT fid) {
    activeProbeFlowCount--;
    return true;
}

int GlobalPort::getActiveProbeFlowCount() {
//	cout <<"ActiveProbeFlowCount: "<<activeProbeFlowCount<<"\n";
    return activeProbeFlowCount;
}

GlobalPort::~GlobalPort() {
}
