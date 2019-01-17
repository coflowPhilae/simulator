#include "GlobalSenderPort.h"

GlobalSenderPort::GlobalSenderPort(portIdT portNum) : GlobalPort(portNum, Utils::daemonTypeSenderPort) {
    setCurrentAllocatedFlow(NULL);
    lastRateSentTime = -1 * Utils::rateUpdateQuantaInMSec;
}

void GlobalSenderPort::resetAllocations() {
    setCapacity(Utils::portBytesPerMSec);
    //flowRates.clear();
}

void GlobalSenderPort::setCurrentAllocatedFlow(GlobalFlow *gf) {
    currentAllocatedFlow = gf;
}

GlobalFlow *GlobalSenderPort::getCurrentAllocatedFlow() {
    return currentAllocatedFlow;
}

bool GlobalSenderPort::removeFlow(flowIdT fid, jobIdT jid) {    // returns true if job no more exists on the port
    GlobalFlow *gf = getCurrentAllocatedFlow();
    if (gf != NULL) {
        if (gf->id == fid) {
            setCurrentAllocatedFlow(NULL);
        }
    }
    return GlobalPort::removeFlow(fid, jid);
}

GlobalSenderPort::~GlobalSenderPort() {
}
