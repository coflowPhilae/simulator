#include <iomanip>
#include "CoordinatorSim.h"

CoordinatorSim::CoordinatorSim(char *traceFilePath, string schedulingPolicyName, string outputFileDiscriminator,
                               int probeNum, int probeSelection, int OneProbeFlowPerMapper,
                               int starvationFreedomPolicy)
        : Coordinator(traceFilePath, schedulingPolicyName, Utils::simulation, outputFileDiscriminator, probeSelection,
                      probeNum, OneProbeFlowPerMapper, starvationFreedomPolicy, nullptr) {
    makePorts();
    setTime();
    dataSentLast = 0;
    cycle = 0;
    run();
}

CoordinatorSim::CoordinatorSim(nlohmann::json config) : Coordinator(config){
    makePorts();
    setTime();
    dataSentLast = 0;
    cycle = 0;
    run();
}



void CoordinatorSim::run() {
    started = true;
    while (!canExit) { // Main Loop
        if (getTimeMillis() % binningPeriod == 0) {
            (this->*(Coordinator::updateActiveJobsPtr))();
        }
        // Decide time slot's queue. -- starvation freedom
        if(starvationFreedomPolicy != Utils::starvationFreedomDeadline){
                setWorkingQueue();
        }
        // Assign rates
        (this->*(Coordinator::setRatesPtr))();
        calculateSimTime();
        unsigned long int nextSimTime = getSimTime();
        simulatePorts(nextSimTime);
        cycle ++;
    }
}

void CoordinatorSim::simulatePorts(unsigned long int simTime) {
vector<jobIdT> finishedJobsThisTime;
    for (int i = ((this->*(Coordinator::getNumJobQueuesPtr))() - 1); i >= 0; i--) { // Iterate over all queues
        for (auto job : activeJobs[i]) { // Iterate over all jobs
            vector<flowIdT> finishedFlows;
            finishedFlows = job->simulate(simTime, assignRateScheme);
            for (auto finishedFlow : finishedFlows){ // Take care of all finished flows
                if ((this->*(Coordinator::flowFinishPtr))(finishedFlow)) {
                    finishedJobsThisTime.push_back(Utils::getJobIdFromFlowId(finishedFlow));
                }
            }
        }
    }
    incrementTimeBy(simTime);
    for (auto finishedJobId: finishedJobsThisTime){
        GlobalJob * job = getActiveJobPointerByJobId(finishedJobId);
        if (schedulingPolicy == Utils::schedulingPolicyPHILAE && !job->probeFinished()){
            probeFinishedPHILAE(job);
        }
        jobFinish(finishedJobId);
    }
}

bool CoordinatorSim::makePorts() {
    for (unsigned int i = 0; i < totalPorts; i++) {
        portIdT portNum = i;
        GlobalReceiverPort *grp = new GlobalReceiverPort(portNum);
        receiversMap.insert(std::make_pair<redIdT, GlobalReceiverPort *>(std::move(portNum), std::move(grp)));
        receiversVec[portNum] = grp;
        GlobalSenderPort *gsp = new GlobalSenderPort(portNum);
        sendersMap.insert(std::make_pair<mapIdT, GlobalSenderPort *>(std::move(portNum), std::move(gsp)));
        sendersVec[portNum] = gsp;
    }
    return true;
}

void CoordinatorSim::setTime() {
    startTimeMillis = 0;
    currentTimeMillis = 0;
    cout << "+++++++++++++++++++++++++CurrentTimeMillis set to: " << currentTimeMillis << "+++++++++++++++++++++++++\n";
    started = true;
}

unsigned long int CoordinatorSim::getTimeMillis() {
    return currentTimeMillis;
}

void CoordinatorSim::incrementTimeBySimQuanta() {
    incrementTimeBy(simQuantaInMSec);
}

void CoordinatorSim::incrementTimeBy(unsigned long int incr) {
    currentTimeMillis += incr;
}

void CoordinatorSim::endEmulation() {
    canExit = true;
    cout << "Emulation ended\n";
}

void CoordinatorSim::calculateSimTime() {
    unsigned long int timeToSet;
    unsigned long int minFCT = getMinTime();
    unsigned long int nextBinTime = binningPeriod - getTimeMillis() % binningPeriod;
    timeToSet = minFCT;
    if (startedJobs < totalJobs) {
        if (timeToSet > nextBinTime) {
            timeToSet = nextBinTime;
        }
    }
    int remainder = timeToSet % simQuantaInMSec;
    timeToSet -= remainder;
    if (remainder != 0) {
        timeToSet += simQuantaInMSec;
    }
    setSimTime(timeToSet);
}

unsigned long int CoordinatorSim::getSimTime() {
    return simQuantaInMSec;
}

void CoordinatorSim::setSimTime(unsigned long int simTimeNew) {
    unsigned long temp = simTimeNew - simQuantaInMSec;
    if (temp !=0) {    // because setRateCycle reflects number of cycles, in multiple of Utils::simQuanta passed. So if we are merging some cycles we need to increment setRateCycle also appropriately
        setRateCycle += (temp / simQuantaInMSec);
    }
    simTime = simTimeNew;
}

CoordinatorSim::~CoordinatorSim() {
}
