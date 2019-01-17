/*
 * Coordinator.cpp
 *
 *      Author: 
 */

#include "CoordinatorSim.h"
#include <iomanip>
#include "Coordinator.h"

Coordinator::Coordinator(nlohmann::json config):Coordinator(config.at("trace path"), config.at("scheduling policy"), config.at("daemon type"),
                                                            config.at("output file discriminator"), config.at("probeNumSelection"),
                                                            config.at("probeFlowSelection"), config.at("oneProbePerMapper"), config.at("starvation freedom policy"), config){
        cout << "probeNumSelection:" << probeNumSelection << endl << "probeFlowSelection : "
             << probeFlowSelection << endl << "starvationFreedomPolicy : " << starvationFreedomPolicy << endl;

}

Coordinator::Coordinator(string traceFilePath, string schedulingPolicyName, unsigned int executionMode,
                         string outputFileDiscriminator, int probeNum, int probeSelection, int OneProbeFlowPerMapper,
                         int starvationFreedomPolicy, nlohmann::json config)
        : traceFilePath(traceFilePath), schedulingPolicyName(schedulingPolicyName),
          executionMode(executionMode), outputFileDiscriminator(outputFileDiscriminator),
          probeFlowSelection(probeSelection), probeNumSelection(probeNum),
          restrictOnePilotFlowPerMapper(OneProbeFlowPerMapper),starvationFreedomPolicy(starvationFreedomPolicy),config(config) {
		  
	cout <<"restrictOnePilotFlowPerMapper: "<<restrictOnePilotFlowPerMapper<<"\n\n";
    toSetRate = true;
    traceIsOpen = false;
    started = false;
    startedJobs = 0;
    finishedJobs = 0;
    canExit = false;
    totalCCT = 0;
    simQuantaInMSec = Utils::invalidValue;
    setRateCycle = 0;
    lastRateSetCycleNum = -1 * Utils::maxRateAssignLapse;
    oracleSimulation = 0;
    numJobQueues = Utils::numJobQueues;
    thinProbingLimit = Utils::thinProbingLimit;
    impactCalculationSchemeSelection = Utils::invalidValue;
    timeSplitingFactorCLI = Utils::invalidValue;
    q0Limit = Utils::portBytesPerMSec * Utils::rateUpdateQuantaInMSec;
    jobSizeT qLim = Utils::invalidValue;

    if (config != nullptr){
        try {
            oracleSimulation = config.at("oracle");
            cout << "Oracle : " << oracleSimulation <<endl;
            q0Limit = config.at("q0 limit");
            qLim = config.at("q0 limit");
            cout << "Q0 Limit : " << qLim <<endl;
            qMultiplier = config.at("qMultiplier");
            cout << "qMultiplier : " << qMultiplier <<endl;
            numJobQueues = config.at("numMainJobQueues");
            cout << "numMainJobQueues : " << numJobQueues <<endl;
            impactCalculationSchemeSelection = config.at("impact calculation");
            cout << "impactCalculationSchemeSelection : " << impactCalculationSchemeSelection <<endl;
            timeSplitingFactorCLI = config.at("time split factor");
            cout << "timeSplitingFactorCLI : " << timeSplitingFactorCLI <<endl;
            thinProbingLimit = config.at("thinProbingLimit");
            cout << "thinProbingLimit : " << thinProbingLimit <<endl;
            simQuantaInMSec = config.at("synCycle");
            cout << "syn Cycle : " << simQuantaInMSec <<endl;
            if (config.find("verbose") != config.end()) { // Key word verbose found
                verbose = config.at("verbose");
                cout << "verbose mode : " << verbose <<endl;
            } else {
                verbose = 0;
            }
            if(probeNumSelection == Utils::proportionalMapperPilotFlows||probeNumSelection == Utils::proportionalMINMRPilotFlows || probeNumSelection == Utils::proportionalMCrossRPilotFlows){
	    	if (config.find("proportionalPilotFlowsRatio") != config.end()) { // Key word verbose found
            	    proportionalPilotFlowsRatio = config.at("proportionalPilotFlowsRatio");
            	} else {
            	    proportionalPilotFlowsRatio = Utils::proportionalPilotFlowsRatio;
            	    cout << "Default proportionalPilotFlowsRatio: " << Utils::proportionalPilotFlowsRatio <<endl;
            	}
	    }
            if(probeNumSelection == Utils::constantPilotFlows){
	    	if (config.find("constantPilotFlowsValue") != config.end()) { // Key word verbose found
            	    constantPilotFlowsValue = config.at("constantPilotFlowsValue");
            	} else {
            	    constantPilotFlowsValue = Utils::constantPilotFlowsValue;
            	    cout << "Default constantPilotFlowsValue" << Utils::constantPilotFlowsValue <<endl;
            	}
	    }
        }
        catch (std::out_of_range &e) {
            cerr << "Configuration read faulure";
        }
    }


    // Assign functions to function pointers here.
    schedulingPolicy = assignSchedulingPolicy(schedulingPolicyName);
	numDeadlineQueues = 0;
    	if (schedulingPolicy == Utils::schedulingPolicyPHILAE) {
        	switch(starvationFreedomPolicy){
            		case Utils::starvationFreedomTimeSlot : // Non-starvation Version
	        		setRatesPtr = &Coordinator::setRatesPHILAE;
                		getNumJobQueuesPtr = &Coordinator::getNumJobQueues;
                		updateQueuesPtr = &Coordinator::updateQueuesPHILAE;
                		numJobQueues += Utils::totalAdditionalQueuesPHILAE; 
                		break;
        	}
        updateActiveJobsPtr = &Coordinator::updateActiveJobsPHILAE;
        getImpactOfJobPtr = &Coordinator::getImpactOfJobPhilae;
        flowFinishPtr = &Coordinator::flowFinishPHILAE;
        rateAssignmentOrder = Utils::rateAssignmentOrderN;    //	Not in use currently
        jobSizeMetric = Utils::jobSizeMetricNoUpdate;    //	LocalSenderPort is not updating EstimatedLengths
    }

    activeJobs = vector<vector<GlobalJob *>>((this->*getNumJobQueuesPtr)());


    for (int i = 0; i < (this->*getNumJobQueuesPtr)(); i++) {
    	if ((schedulingPolicy == Utils::schedulingPolicyPHILAE) && i < Utils::totalAdditionalQueuesPHILAE){
    	    queueLimitsVector.push_back(0); // We don't want any CoFlow to be inserted into the additional queues like thin queues and probe queue when moving across queues so setting the limit to be zero
	    continue;
    	}
        queueLimitsVector.push_back(qLim);
        qLim = (jobSizeT) ((double)qLim *qMultiplier);
    }

    // Set upper bound for the last queue
    queueLimitsVector.back() = ULONG_LONG_MAX;

    for (int i = 0 ; i < (this->*getNumJobQueuesPtr)(); i++){
        cout << "Queue " << i << "'s upper bound is " << queueLimitsVector[i] << endl;
    }

    setTraceReader(traceFilePath);
    cout << "Initializing senderRemCap and receiversRemCap vectors for " << totalPorts << " ports with "
         << Utils::portBytesPerMSec << " bytes per milli second\n";
    receiverPortQuotaPerJob.reserve(totalPorts);
    senderPortQuotaPerJob.reserve(totalPorts);
    for (int i = 0; i < totalPorts; i++) {
        sendersRemCap.push_back(Utils::portBytesPerMSec);
        receiversRemCap.push_back(Utils::portBytesPerMSec);
        sendersVec.push_back(nullptr);
        receiversVec.push_back(nullptr);
    }
    gettimeofday(&debugTimeStruct, NULL);
    debugTimeCoordinator = debugTimeStruct.tv_sec * 1000 + debugTimeStruct.tv_usec / 1000;
    debugTimePort = debugTimeStruct.tv_sec * 1000 + debugTimeStruct.tv_usec / 1000;
}

void Coordinator::updateQueuesPHILAE() {
    //! Move finished pilot jobs to the right queue
    vector<GlobalJob *> probeFinishedJobs;
    for (auto job: activeJobs[Utils::probeQPhilae]) {// Iterate over all piloting jobs
        if (job->probeFinished()) {// When there is a piloting job finishes piloting, move this job to the right queue
            probeFinishedJobs.push_back(job);
        }
    }
    for (auto probeFinishedJob: probeFinishedJobs){
        probeFinishedPHILAE(probeFinishedJob);
    }

    //! Move each job to the right queue
    vector<std::pair<GlobalJob*, int>> jobToMoveVec;
    for (int queueItr = Utils::firstMainQIndexPHILAE; queueItr < (this->*getNumJobQueuesPtr)(); queueItr++){
        for (auto jobItr = activeJobs[queueItr].begin();jobItr!=activeJobs[queueItr].end();jobItr++){//Iterate every job in every queue
            GlobalJob * job = *jobItr;
            impactFactorT impact = (this->*getImpactOfJobPtr)(job->id);
            if (!(queueLimitsVector[queueItr-1]<= impact && impact < queueLimitsVector[queueItr])){// This job needs moving
                for (int queueToMoveItr = (this->*getNumJobQueuesPtr)() - 1; queueToMoveItr >= 1 ; --queueToMoveItr) {
                    if (queueLimitsVector[queueToMoveItr-1]<= impact && impact < queueLimitsVector[queueToMoveItr]){// If we find the right queue, we will add this job to queue to move.
                        jobToMoveVec.push_back({job, queueToMoveItr});
                    }
                }
            }
        }
    }
    for (auto jobToMoveItr : jobToMoveVec){
        auto job = jobToMoveItr.first;
        auto queueToMoveItr = jobToMoveItr.second;
        moveJobToQueue(job, queueToMoveItr);
    }
	// sorting thin queue by least contention
	sortQByCoFlowContention(Utils::thinQPhilae);
}

void Coordinator::probeFinishedPHILAE(GlobalJob *job) {
timeT currentTime = getTimeMillis();
job->setProbingEndTime(currentTime);
    if (oracleSimulation == 0) {
        job->estimateLength();
    } else{
        int numFlows = job->initialWidth;
        flowSizeT totalSize = 0;
        for (auto flow : job->flowsVec){
            totalSize += flow->sizeRemaining;
        }
        job->setEstimatedLength(totalSize/numFlows);
    	cout <<"jid: "<<job->id<<" totalSize: "<<totalSize<<"\n";
    }
    impactFactorT impact = (this->*getImpactOfJobPtr)(job->id);
    for (int queueNum = Utils::firstMainQIndexPHILAE; queueNum < queueLimitsVector.size(); queueNum++){
        if (queueLimitsVector[queueNum-1] <= impact &&  impact < queueLimitsVector[queueNum]) {
            moveJobToQueue(job,queueNum);
            break;
        }
    }
}

impactFactorT Coordinator::getImpactOfJobPhilae(jobIdT jid) {
	GlobalJob *job = getActiveJobPointerByJobId(jid);
    impactFactorT toReturn = -1;
    if (oracleSimulation == false && job->probeFinished() == false){
	    toReturn = getContentionOfJob(jid);
    } else {
        switch (impactCalculationSchemeSelection) {
            case Utils::TestingNewThings :{
                flowSizeT sentFlowSize = 0;
                //TODO: Find new methods.
                break;
            }
            case Utils::simpleJobWiseImpact :{
                toReturn = getContentionOfJob(jid) * job->getEstimatedLength();
                break;
            }
            case Utils::simpleContentionImpact :{
                toReturn = getContentionOfJob(jid);
                break;
            }
            case Utils::bottleneckImpact :{
                vector<int> bottleneck;
                job-> sortTasksBySize();
                toReturn = Utils::maxInt(job->getEstimatedLength() * job->numReceivers - job->mapTasksVec[0]->getBytesShuffled(),
                                         job->getEstimatedLength() * job->numSenders - job->redTasksVec[0]->getBytesShuffled());
                break;
            }
            case Utils::longestRemainingJobWiseImpact :{
                contentionFactorT contention = getContentionOfJob(jid);
                toReturn = Utils::maxInt(1, job->getEstimatedLength() - job->getShortestFlowLength());
                toReturn = toReturn * contention;
                break;
            }
            case Utils::maxPortWiseImpact :{
                toReturn = 0;
                // Iterate over map tasks to find the max
                for (auto mapTaskItr = (job->mapTasksVec).begin(); mapTaskItr != (job->mapTasksVec).end(); mapTaskItr++) {
                    MapTask *mapTask = *mapTaskItr;
                    if (sendersMap.find(mapTask->hostPort) == sendersMap.end()) {
                        cerr << "Task does not have a valid port.";
                        exit(-1);
                    }
                    GlobalSenderPort *senderPort = sendersMap.find(mapTask->hostPort)->second;// Get task's port
                    toReturn = Utils::maxInt(toReturn, Utils::maxInt(1, (job->getEstimatedLength() - mapTask->getShortestFlowLength())) *getPortContention(senderPort));
                }

                for (auto redTaskItr = (job->redTasksVec).begin(); redTaskItr != (job->redTasksVec).end(); redTaskItr++) {
                    RedTask *redTask = *redTaskItr;
                    if (receiversMap.find(redTask->hostPort) == receiversMap.end()) {
                        cerr << "Task does not have a valid port.";
                        exit(-1);
                    }
                    GlobalReceiverPort *receiverPort = receiversMap.find(redTask->hostPort)->second;
                    toReturn = Utils::maxInt(toReturn, Utils::maxInt(1, (job->getEstimatedLength() - redTask->getShortestFlowLength())) *getPortContention(receiverPort));
                }
                break;
            }
            case Utils::sumPortWiseImpact :{
                toReturn = 0;
                // Iterate over map tasks to find the max
                for (auto mapTaskItr = (job->mapTasksVec).begin(); mapTaskItr != (job->mapTasksVec).end(); mapTaskItr++) {
                    MapTask *mapTask = *mapTaskItr;
                    if (sendersMap.find(mapTask->hostPort) == sendersMap.end()) {
                        cerr << "Task does not have a valid port.";
                        exit(-1);
                    }
                    GlobalSenderPort *senderPort = sendersMap.find(mapTask->hostPort)->second;// Get task's port
                    toReturn += Utils::maxInt(1, (job->getEstimatedLength() - mapTask->getShortestFlowLength())) *getPortContention(senderPort);
                }

                for (auto redTaskItr = (job->redTasksVec).begin(); redTaskItr != (job->redTasksVec).end(); redTaskItr++) {
                    RedTask *redTask = *redTaskItr;
                    if (receiversMap.find(redTask->hostPort) == receiversMap.end()) {
                        cerr << "Task does not have a valid port.";
                        exit(-1);
                    }
                    GlobalReceiverPort *receiverPort = receiversMap.find(redTask->hostPort)->second;
                    toReturn += Utils::maxInt(1, (job->getEstimatedLength() - redTask->getShortestFlowLength())) * getPortContention(receiverPort);
                }
                break;
            }
            case Utils::remainingSizeImpact :{
                if (job->initialWidth*job->getEstimatedLength() >= job->bytesSent){
                    toReturn = job->initialWidth * job->getEstimatedLength() - job->bytesSent;
                } else {
                    toReturn = 1;	// this is good under the assumption that estimate does not have high error. So if bytessent is more than estimated size than that means only few bytes should have been left and so with impact as 1 it will be given high priority.
                }
                break;
            }
            case Utils::initialSizeImpact :{
                toReturn = job->initialWidth * job->getEstimatedLength();
                break;
            }
            default:
                cerr << "getImpactOfJobPhilae Error: Wrong impactCalculationSchemeSelection value.";
                break;
        }
    }
    if (toReturn < 0) {
        cerr << endl<<endl<<"getImpactOfJobPhilae ERROR: impact value is negative. Value is: "<< toReturn << "Impact calculation scheme is: "<< impactCalculationSchemeSelection<< endl<<endl;
    }
    return toReturn;
}

contentionFactorT Coordinator::getContentionOfJob(jobIdT jid) {
    auto tempJCCIterator = jobsContentionCount.find(jid);
    if (tempJCCIterator == jobsContentionCount.end()) {
        cerr << "unexpected error while getting contention for jid: " << jid << "\n";
        return Utils::invalidValue;
    }
    return tempJCCIterator->second->size();
}

void Coordinator::updateContentions(GlobalJob *job, int transitionType, int portType, portIdT portNum) {    // 3rd and 4th arguments are not valid in case of arrival; arrival movementType = 1; departure movementType = -1
    if (job == NULL) {
        cout <<"ERROR: NULL job in updateContentions\n";
	    return;
    }
    if (transitionType == Utils::transitionTypeArrival) {
        unordered_map<jobIdT, int> *innerMap = new unordered_map<jobIdT, int>();
        for (int i = 0; i < 2; i++) {    // i = 0 for senderPorts and i = 1 to handle receiverPorts
            std::vector<mapIdT>::iterator portItr, portItrEnd;
            if (i == 0) {
                portItr = job->sendersVector.begin();
                portItrEnd = job->sendersVector.end();
            } else if (i == 1) {
                portItr = job->receiversVector.begin();
                portItrEnd = job->receiversVector.end();
            }

            for (; portItr != portItrEnd; ++portItr) {//Iterate over all sender ports and receiver ports of the jobs
                GlobalPort * portPtr;
                if (i == 0) {
                    portPtr = getSPPointerByPortNum(*portItr);
                } else if (i == 1) {
                    portPtr = getRPPointerByPortNum(*portItr);
                } else {
                    Utils::utilError("Unexpected in updateContentions");
                }


                for (auto jobAtPort = portPtr->activeJobs.begin(); jobAtPort != portPtr->activeJobs.end(); ++jobAtPort) {// Iterate all jobs in the port
                    if (jobAtPort->first == job->id) { continue; } // Skip myself

                    GlobalJob * jobAtPortPtr = getActiveJobPointerByJobId(jobAtPort->first);
                    if (jobAtPortPtr->currentQueue != job->currentQueue) {
                        continue;
                    }

                    //! Construct the new contention info for `job`
                    auto innerMapItr = innerMap->find(jobAtPortPtr->id);
                    if (innerMapItr == innerMap->end()) {
                        innerMap->emplace(std::move(jobAtPortPtr->id), 1);
                    } else {
                        innerMapItr->second += 1;
                    }
                    //! Modify the contention info for `jobAtPort`
                    auto jobAtPortContentionMap = jobsContentionCount.find(jobAtPortPtr->id);
                    if (jobAtPortContentionMap == jobsContentionCount.end()) {
                        cout << "unexpected error in updateContentions upon job arrival 1\n";
                    }
                    auto jobAtPortContentionMapItr = jobAtPortContentionMap->second->find(job->id);
                    if (jobAtPortContentionMapItr == jobAtPortContentionMap->second->end()) {
                        jobAtPortContentionMap->second->emplace(std::move(job->id), 1);
                    } else {
                        jobAtPortContentionMapItr->second += 1;
                    }
                }
            }
        }
        jobsContentionCount.emplace(std::move(job->id), std::move(innerMap));

    } else if (transitionType == Utils::transitionTypeDeparture) {
        GlobalPort *gp;
        std::unordered_map<jobIdT, unordered_map<jobIdT, int> *>::iterator jobAtPortContentionItr;
        std::unordered_map<jobIdT, int>::iterator jobContentionInnerMapItr;
        if (portType == Utils::daemonTypeSenderPort) {
		gp = getSPPointerByPortNum(portNum);
        } else if (portType == Utils::daemonTypeReceiverPort) {
		gp = getRPPointerByPortNum(portNum);
        }
        for (auto jobsAtPort : gp->activeJobs) {
            if (jobsAtPort.first == job->id) {
                continue;
            }
            GlobalJob *jobAtPortItr = getActiveJobPointerByJobId(jobsAtPort.first);
            if (jobAtPortItr->currentQueue!=job->currentQueue){
                continue;
            }
            jobAtPortContentionItr = jobsContentionCount.find(jobAtPortItr->id);
            if (jobAtPortContentionItr == jobsContentionCount.end()) {
                cout << "unexpected error in updateContentions upon job departure 1\n";
            }
            jobContentionInnerMapItr = jobAtPortContentionItr->second->find(job->id);
            if (jobContentionInnerMapItr == jobAtPortContentionItr->second->end()) {
                cerr << "unexpected error in updateContentions upon job departure 2 tempJobId: " << jobAtPortItr->id
                     << " jobId: " << job->id << " when searching for port " <<jobAtPortItr->id <<"\n";
            } else {
                jobContentionInnerMapItr->second -= 1;
                if (jobContentionInnerMapItr->second == 0) {
                    jobAtPortContentionItr->second->erase(std::move(job->id));
                }
            }

            jobAtPortContentionItr = jobsContentionCount.find(job->id);
            if (jobAtPortContentionItr == jobsContentionCount.end()) {
                cout << "unexpected error in updateContentions upon job departure 3\n";
            }
            jobContentionInnerMapItr = jobAtPortContentionItr->second->find(jobAtPortItr->id);
            if (jobContentionInnerMapItr == jobAtPortContentionItr->second->end()) {
                cerr << "unexpected error in updateContentions upon job departure 4 tempJobId: " << jobAtPortItr->id
                     << " jobId: " << job->id << "\n";
            } else {
                jobContentionInnerMapItr->second -= 1;
                if (jobContentionInnerMapItr->second == 0) {
                    jobAtPortContentionItr->second->erase(std::move(jobAtPortItr->id));
                }
            }
        }
    }
}

void Coordinator::updateContentionsOnQJump(GlobalJob *job, int fromQ) {    //this function assumes that job object has already been updated to reflect newQ; currently being called from updateQueues; during call of this function job pointer is in both activeJobs[fromQ] activeJobs[newQ]
    if (job == NULL) {
        return;
    }
    if (schedulingPolicy != Utils::schedulingPolicyPHILAE) {    // contention policy check
        return;
    }

    int newQ = job->currentQueue;
    std::unordered_map<jobIdT, unordered_map<jobIdT, int> *>::iterator jobContentionItr;
    std::unordered_map<jobIdT, int>::iterator jobContentionItr2;
    std::vector<mapIdT>::iterator jobPNumsBegin, jobPNumsEnd;
    for (int i = 0; i < 2; i++) {    // i = 0 for senderPorts and i = 1 to handle receiverPorts
        if (i == 0) {
            jobPNumsBegin = job->sendersVector.begin();
            jobPNumsEnd = job->sendersVector.end();
        } else if (i == 1) {
            jobPNumsBegin = job->receiversVector.begin();
            jobPNumsEnd = job->receiversVector.end();
        }
        for (; jobPNumsBegin != jobPNumsEnd; ++jobPNumsBegin) {
            GlobalPort *port;
            if (i == 0) {
                port = getSPPointerByPortNum(*jobPNumsBegin);
            } else if (i == 1) {
                port = getRPPointerByPortNum(*jobPNumsBegin);
            } else {
                Utils::utilError("Unexpected in updateContentionsOnQJump");
            }
            for (auto jobsAtPort = port->activeJobs.begin(); jobsAtPort != port->activeJobs.end(); ++jobsAtPort) {
                if (jobsAtPort->first == job->id) {
                    continue;
                }
                toSetRate = true;
                GlobalJob *tempJob = getActiveJobPointerByJobId(jobsAtPort->first);
                if (tempJob->currentQueue == newQ) {
                    jobContentionItr = jobsContentionCount.find(tempJob->id);
                    jobContentionItr2 = jobContentionItr->second->find(job->id);
                    if (jobContentionItr2 == jobContentionItr->second->end()) {
                        jobContentionItr->second->emplace(std::move(job->id), 1);
                    } else {
                        jobContentionItr2->second += 1;
                    }

                    jobContentionItr = jobsContentionCount.find(job->id);
                    jobContentionItr2 = jobContentionItr->second->find(tempJob->id);
                    if (jobContentionItr2 == jobContentionItr->second->end()) {
                        jobContentionItr->second->emplace(std::move(tempJob->id), 1);
                    } else {
                        jobContentionItr2->second += 1;
                    }

                } else if (tempJob->currentQueue == fromQ) {
                    jobContentionItr = jobsContentionCount.find(tempJob->id);
                    if (jobContentionItr == jobsContentionCount.end()) {
                        cout << "unexpected error in updateContentions upon Q transition 1\n";
                    }
                    jobContentionItr->second->erase(std::move(job->id));
                    jobContentionItr = jobsContentionCount.find(job->id);
                    if (jobContentionItr == jobsContentionCount.end()) {
                        cout << "unexpected error in updateContentions upon Q transition 2\n";
                    }
                    jobContentionItr->second->erase(std::move(tempJob->id));

                } else {
                    continue;
                }
            }
        }
    }
}

void Coordinator::sortAllQsByCoFlowImpact() {
    for (unsigned int i = 0; i < (this->*getNumJobQueuesPtr)(); i++) {
        sortQByCoFlowImpact(i);
    }
    if (verbose == 2) {
	cout <<"Contention_test @@@@@@@@@@@@@@@@ SortedQueues state begin @@@@@@@@@@@@@@@@"<<"\n";
        for (int i = 0; i < (this->*getNumJobQueuesPtr)(); i++) {
            if (activeJobs[i].size() == 0) {
                continue;
            } else {
                cout << "Contention_test Print Contention of jobs in Queue : " << i << endl;
                for (auto &job: activeJobs[i]) {
                    cout << " Contention_test Job ID : " << job->id << " Contention : " << getContentionOfJob(job->id) <<
                         " Impact : " << (this->*getImpactOfJobPtr)(job->id) << " job current queue: "<< job->currentQueue<< " at cycle: "<< cycle <<endl;
                }
            }
        }
	cout <<"Contention_test @@@@@@@@@@@@@@@@ SortedQueues state end @@@@@@@@@@@@@@@@"<<"\n";
    }
}

void Coordinator::sortQByCoFlowImpact(unsigned int queueNum) {
    std::vector<GlobalJob *>::iterator jobsInQBegin = activeJobs[queueNum].begin();
    std::vector<GlobalJob *>::iterator jobsInQEnd = activeJobs[queueNum].end();
    while (jobsInQBegin != jobsInQEnd) {    //	updating new contention values for the jobs in this Queue
        impactFactorT tmpImpact = (this->*getImpactOfJobPtr)((*jobsInQBegin)->id);
        if (tmpImpact < 0) {
            cerr << "Negative impact found for job: "<< (*jobsInQBegin)->id<<" contention is: "<< tmpImpact<< endl;
        }
        (*jobsInQBegin)->setImpactFactor(tmpImpact);
        jobsInQBegin++;
    }

    stable_sort(activeJobs[queueNum].begin(), activeJobs[queueNum].end(), [](GlobalJob *j1, GlobalJob *j2) -> bool {
        return j1->getImpactFactor() < j2->getImpactFactor();
    });
}

void Coordinator::sortQByCoFlowContention(unsigned int queueNum) {
    std::vector<GlobalJob *>::iterator jobsInQBegin = activeJobs[queueNum].begin();
    std::vector<GlobalJob *>::iterator jobsInQEnd = activeJobs[queueNum].end();
    while (jobsInQBegin != jobsInQEnd) {    //	updating new contention values for the jobs in this Queue
        contentionFactorT tmpContention = getContentionOfJob((*jobsInQBegin)->id);
        if (tmpContention < 0) {
            cerr << "Negative contention found for job: "<< (*jobsInQBegin)->id<<" contention is: "<< tmpContention<< endl;
        }
        (*jobsInQBegin)->setContentionFactor(tmpContention);
        jobsInQBegin++;
    }

    stable_sort(activeJobs[queueNum].begin(), activeJobs[queueNum].end(), [](GlobalJob *j1, GlobalJob *j2) -> bool {
        return j1->getContentionFactor() < j2->getContentionFactor();
    });
}

void Coordinator::assignRateOnlyAtSenderPortP(GlobalJob *job,
                                              mapIdT spNum) {    //assigns rate to each flow of job at spNum port whatever possible.
    job->allOrNone = false;
    int jid = job->id;
    int spCap = 0;
    spCap = getFreeCapacity(spNum, Utils::daemonTypeSenderPort);
    if (spCap < Utils::minCapacityToBeAllocated) {
        spCap = 0; // so that 0 rate will be assigned to flows at this port
    } else {
        int numFlows = (getSPPointerByPortNum(spNum))->activeJobs.find(job->id)->second;
        if (numFlows == 0) {
            error("unexpected error in assignRate 1\n");
        } else {
            spCap = spCap / numFlows;
        }
    }
    vector<redIdT>::iterator itr = job->receiversVector.begin();
    vector<redIdT>::iterator itrEnd = job->receiversVector.end();
    while (itr != itrEnd) {
        int rCap = getFreeCapacity(*itr, Utils::daemonTypeReceiverPort);
        if (rCap < Utils::minCapacityToBeAllocated) {
            rCap = 0; // so that 0 rate will be assigned to flows at this port
        }
        int fid = Utils::makeFlowId(jid, spNum, *itr);
        GlobalFlow *gf = job->getFlowPtrByFid(fid);
        if (gf != NULL) {
            int fRate = Utils::minInt(spCap, rCap);
            if (fRate == 0) {
                gf->setRate(0);
            } else if (reserveCapacity(fRate, spNum, Utils::daemonTypeSenderPort) &&
                       reserveCapacity(fRate, *itr, Utils::daemonTypeReceiverPort)) {
                if (executionMode == Utils::simulation) {
                    updateMinTime(fRate, gf->getRemainingSize());
                }
                gf->setRate(fRate);
            } else {
                cout << "unexpected - error in assignRateOnlyToPort 3 fid: " << fid << " fRate: " << fRate << "\n";
            }
        }
        itr++;
    }
}

void Coordinator::assignRate(GlobalJob *job) {    //assigns rate to each flow whatever possible.
    job->allOrNone = false;
    //! Truly assign rates to flows
    for (auto flowItr = job->flowsVec.begin(); flowItr != job->flowsVec.end(); flowItr++) {
        flowIdT fid = (*flowItr)->id;
        mapIdT sid = (*flowItr)->mapId;
        redIdT rid = (*flowItr)->redId;
        int numFlowsAtMapper = (*flowItr)->getMapTaskPtr()->flowsMap.size();
        int numFlowsAtReducer = (*flowItr)->getRedTaskPtr()->flowsMap.size();
        rateT fRate = Utils::minInt(senderPortQuotaPerJob[sid]/numFlowsAtMapper,receiverPortQuotaPerJob[rid]/numFlowsAtReducer);
        if (fRate == 0) {
            (*flowItr)->setRate(0);
        } else if (reserveCapacity(fRate, sid, Utils::daemonTypeSenderPort) &&
                   reserveCapacity(fRate, rid, Utils::daemonTypeReceiverPort)) {
            if (executionMode == Utils::simulation) {
                updateMinTime(fRate, (*flowItr)->getRemainingSize());
            }
            (*flowItr)->setRate(fRate);
        } else {
            cout << "unexpected - error in assignRate 3 fid: " << fid << " fRate: " << fRate << "\n";
        }
    }
}

void Coordinator::assignRateToPilotFlows(GlobalJob *job) {
    job->allOrNone = false;
    std::vector<GlobalFlow *> pilotFlows = *(job->getProbeFlows());
    for (auto pilotFlow : pilotFlows) {
        if (!(pilotFlow->finished())) {
            mapIdT sid = pilotFlow->mapId;
            redIdT rid = pilotFlow->redId;
            rateT sCap = getFreeCapacity(sid, Utils::daemonTypeSenderPort);
            rateT rCap = getFreeCapacity(rid, Utils::daemonTypeReceiverPort);
            rateT fRate = Utils::minInt(sCap, rCap);
            if (fRate == 0) {
                pilotFlow->setRate(0);
            } else {
		assignFullRateToFlow(pilotFlow);
                job->addFlowToSend(pilotFlow);
            }
        }
    }
}

void Coordinator::assignRateForAllFlowsOfJob(rateT rate, GlobalJob *job) {    //assigns rate to each flow whatever possible
    if (executionMode == Utils::simulation) {
        job->allOrNone = true;
        job->allOrNoneRate = rate;
    }
    /*	Above is a code written by iterating over map replacing it with vector	*/
}
//	WARNING: Here we are still iterating over unordered_map for flows. This will be very slow.

void Coordinator::assignRateOrderP(GlobalJob *job) {    // tries to assign rate to one flow of a job at a port.
    job->allOrNone = false;
    for (auto mapTask: job->mapTasksVec){
        //Iterate over job's active mappers
        portIdT mapId = mapTask->hostPort;
        if (isSenderFullyFree(mapId)){
            for (auto redTask: job->redTasksVec){
                //Iterate over job's active reducers
                portIdT redId = redTask->hostPort;
                if (isReceiverFullyFree(redId)) {
                    // Assign the flow with full rate
                    auto flow = job->getFlowPtrByFid(Utils::makeFlowId(job->id, mapId, redId));
                    if (flow) {
                        assignFullRateToFlow(flow);
                        job->addFlowToSend(flow);
                        break;// If one flow is assigned with full rate, we should not continue to search in this mapper.
                    }
                }
            }
        }
    }
}

void Coordinator::assignFullRateToFlow(GlobalFlow *gf) {
    mapIdT mid = gf->mapId;
    redIdT rid = gf->redId;
    if (reserveSenderFully(mid) && reserveReceiverFully(rid)) {
        if (executionMode == Utils::simulation) {
            updateMinTime(Utils::portBytesPerMSec, gf->getRemainingSize());
        }
        gf->setRate(Utils::portBytesPerMSec);
        GlobalSenderPort *spTemp = getSPPointerByPortNum(mid);
        spTemp->setCurrentAllocatedFlow(gf);
    } else {
        Utils::utilError("Error in assignFullRateToFlow: reserver capacity failed. ");
        cerr << "fid: " << gf->id << " map: " << mid << " red: " << rid << "\n";
    }
}

void Coordinator::setRatesPHILAE() {
    (this->*updateQueuesPtr)();
    setRateCycle++;
    commonSetRatesActions();
    lastRateSetCycleNum = setRateCycle;
    
	for (auto job: activeJobs[Utils::thinQPhilae]) {
		assignRateToPilotFlows(job);
	}
    	//! Try to set rates for pilot flows
	for (auto job: activeJobs[Utils::probeQPhilae]) {
		assignRateToPilotFlows(job);
	}
    	//! Try to do jobs in the currentWorkingQueue
	if(__glibc_likely(currentWorkingQueue != Utils::invalidValue)){
	    	for (auto job: activeJobs[currentWorkingQueue]) {
	        	assignRateOrderP(job);
	        	if (!fabricHasFreePorts()) {
	            		return;
	        	}
	    	}
	}
    //! Try to jobs other queues
    for (int i = Utils::firstMainQIndexPHILAE; i < activeJobs.size(); i++) {
        if (i == currentWorkingQueue) {
            continue;
        }
        for (auto job: activeJobs[i]) {
            assignRateOrderP(job);
            if (!fabricHasFreePorts()) {
                return;
            }
        }
    }
}

void Coordinator::updateUnservedJobQuota() {

        //! Iterate over sender ports to determine job's quota
        for (auto senderPortItr = sendersVec.begin(); senderPortItr != sendersVec.end(); senderPortItr++) {
            portIdT mapId = (*senderPortItr)->portNum;
            rateT cap = getFreeCapacity(mapId, Utils::daemonTypeSenderPort);//Get sender Free Capacity
            senderPortQuotaPerJob[mapId] = cap;
            if (cap < Utils::minCapacityToBeAllocated) {
                cap = 0; // so that 0 rate will be assigned to flows at this port
                senderPortQuotaPerJob[mapId] = cap;
            } else {
                int numNotServedJobsAtPort = 0;
                for (auto jobAtPortItr = (*senderPortItr)->activeJobs.begin();//Iterate over jobs at this port
                     jobAtPortItr != (*senderPortItr)->activeJobs.end(); jobAtPortItr++) {
                    if (servedJobMap.count((*jobAtPortItr).first) == 0) {// If this job was not served, we increment numNotServedJobsAtPort by 1
                        numNotServedJobsAtPort++;
                    }
                }
                if (numNotServedJobsAtPort > 0) {
                    senderPortQuotaPerJob[mapId] = cap / numNotServedJobsAtPort;
                }
            }
            //cout << mapId << " servedJobMap.size " << servedJobMap.size()<<" Jobs at Port "<<(*senderPortItr)->activeJobs.size()<<" Free "<<getFreeCapacity(mapId, Utils::daemonTypeSenderPort)<<endl;
        }

    //! Iterate over reciever ports
    for (auto receiverPortItr = receiversVec.begin(); receiverPortItr != receiversVec.end(); receiverPortItr++) {
        portIdT redId = (*receiverPortItr)->portNum;
        rateT cap = getFreeCapacity(redId, Utils::daemonTypeReceiverPort);
        receiverPortQuotaPerJob[redId] = cap;
        if (cap < Utils::minCapacityToBeAllocated) {
            cap = 0; // so that 0 rate will be assigned to flows at this port
            receiverPortQuotaPerJob[redId] = cap;
        } else {
            int numNotServedJobsAtPort = 0;
            for (auto jobAtPortItr = (*receiverPortItr)->activeJobs.begin();jobAtPortItr!=(*receiverPortItr)->activeJobs.end();jobAtPortItr++){
                if (servedJobMap.count((*jobAtPortItr).first)==0){
                    numNotServedJobsAtPort ++ ;
                }
            }
            if (numNotServedJobsAtPort > 0) {
                receiverPortQuotaPerJob[redId] = cap / numNotServedJobsAtPort;
            }
        }
    }
}

void Coordinator::setCurrentSendFlowToNULLForAllPorts() {
    GlobalSenderPort *sp;
    /* This is very inefficient fix the ineffiency*/
    for (int i = 0; i < totalPorts; i++) {
        sp = getSPPointerByPortNum(i);
        if (sp->portNum != i) {
            cout << "Hindala\n";
        }
        sp->setCurrentAllocatedFlow(NULL);
    }
    /* This is very inefficient fix the ineffiency*/
}

void Coordinator::commonSetRatesActions() {    // this will be called from setRates function in beginning
    resetMinTime();
    resetCapacityForAll();
    setCurrentSendFlowToNULLForAllPorts();
//	setRateCycle++;
    toSetRate = false;
}

jobSizeT Coordinator::estimateJobLength(jobIdT jid) {
    GlobalJob *job = getActiveJobPointerByJobId(jid);
    if (job->currentQueue == Utils::probeQPhilae) {
        cout << "unexpected: Coordinator::estimateJobLength called before estimation is over. jid: " << jid << "\n";
    }
    jobSizeT toReturn = job->getEstimatedLength();
    return toReturn;
}

flowSizeT Coordinator::getRemaingFlowSize(jobIdT jid) {
    GlobalJob *job = getActiveJobPointerByJobId(jid);
    if (job->currentQueue == Utils::probeQPhilae) {
        cout << "unexpected: Coordinator::getRemaingFlowSize called before estimation is over. jid: " << jid << "\n";
        exit(-1);
    }
    jobSizeT toReturn = 0;
    for (auto jobInBegin = job->flowsMap.begin(); jobInBegin != job->flowsMap.end(); jobInBegin++) { // why iteration on maps?
        toReturn += job->getEstimatedLength() - (*jobInBegin).second->getSizeInBytes() +
                    (*jobInBegin).second->sizeRemaining;
        if (job->getEstimatedLength() <= 0) {
            cerr << "Coordinator::getRemaingFlowSize called before length estimation" << endl;
            exit(-1);
        }
    }
    toReturn /= job->flowsMap.size();
    return toReturn;
}

bool Coordinator::flowFinishPHILAE(flowIdT fid) {
    GlobalJob *job = getActiveJobPointerByJobId(Utils::getJobIdFromFlowId(fid));
	if(job == NULL){
		cout <<"Unexpected Error: NULL job pointer in flowFinishPHILAE fid: "<< fid<<"\n";
	}
    bool toReturn = flowFinish(fid);
    if (toReturn) {    // implies job finished
        return toReturn;
    }
    if (job->checkIfProbeFlowFinished(fid)) {
        getSPPointerByPortNum(Utils::getMapPortNumFromFlowId(fid))->removeProbeFlow(fid);
        getRPPointerByPortNum(Utils::getRedPortNumFromFlowId(fid))->removeProbeFlow(fid);
    }
    return toReturn;
}

bool Coordinator::flowFinish(flowIdT fid) {
    jobIdT jid = Utils::getJobIdFromFlowId(fid);
    mapIdT sid = Utils::getMapPortNumFromFlowId(fid);
    redIdT rid = Utils::getRedPortNumFromFlowId(fid);
    GlobalJob *job = getActiveJobPointerByJobId(jid);
    GlobalFlow * gf = job->getFlowPtrByFid(fid);


    if (Task * taskPtr = gf->getMapTaskPtr()){
        taskPtr->flowFinish(fid);
    } else {
        cout << "Error in erasing flows from a task." << endl;
    }
    if (Task * taskPtr = gf->getRedTaskPtr()){
        taskPtr->flowFinish(fid);
    } else {
        cout << "Error in erasing flows from a task." << endl;
    }

    GlobalFlow *gfToFin = job->getFlowPtrByFid(fid);
    bool toReturn;
    toReturn = job->removeFlow(fid);
    GlobalReceiverPort *grp = getRPPointerByPortNum(rid);
    if (grp->removeFlow(fid, jid)) {
            if (schedulingPolicy == Utils::schedulingPolicyPHILAE) {
            updateContentions(job, Utils::transitionTypeDeparture, Utils::daemonTypeReceiverPort, /*rPort->first*/
                              rid);    // checking in the function if the job pointer is NULL
        }
        //erasing port from list of ports of job
        job->removePort(rid, Utils::daemonTypeReceiverPort);
    }

    GlobalSenderPort *gsp = getSPPointerByPortNum(sid);
    if (gsp->removeFlow(fid, jid)) {
            if (schedulingPolicy == Utils::schedulingPolicyPHILAE) {    // contention policy check
            updateContentions(job, Utils::transitionTypeDeparture, Utils::daemonTypeSenderPort, /*sPort->first*/
                              sid);    // checking in the function if the job pointer is NULL
        }
        //erasing port from list of ports of job
        job->removePort(sid, Utils::daemonTypeSenderPort);
    }
    std::unordered_map<jobIdT, int>::iterator afHash;
    afHash = activeJobsActiveFlowCount.find(jid);
    afHash->second = afHash->second - 1;
    if (!(gfToFin->isProbe())) {    // we cannot delete probeFlows
        delete gfToFin;    //	deletion being done in flowFinish of Coordinator
    }

        return toReturn;
}

void Coordinator::jobFinish(jobIdT jid) {
    std::vector<GlobalJob *>::iterator itJ, endJ;
    std::vector<vector<GlobalJob *>>::iterator itQ, endQ;
    GlobalJob *job = getActiveJobPointerByJobId(jid);
    timeT time = getTimeMillis();
    job->jobDead(time);
    for (itJ = activeJobs[job->currentQueue].begin(), endJ = activeJobs[job->currentQueue].end(); itJ != endJ; ++itJ) {
        if ((*itJ)->id == jid) {
            activeJobs[job->currentQueue].erase(itJ);
            break;
        }
    }
    activeJobsHash.erase(job->id);
    cout << jid << " " << job->arrivalTime << " " << job->endTime << " " << job->numSenders << " "
                << job->numReceivers << " " << job->bytesSent << " " << job->bytesSent << " " << job->getJCT() << " "
                << job->endTime << " " << job->bytesSent << "\n";
    totalCCT += job->getJCT();
    
    activeJobsActiveFlowCount.erase(jid);
    if (schedulingPolicy == Utils::schedulingPolicyPHILAE) {    // contention policy check
		delete jobsContentionCount.find(jid)->second;
		jobsContentionCount.erase(jid);
    }
    finishedJobs++;

    delete job;
    if (finishedJobs == totalJobs) {
        endEmulation();
    }
    toSetRate = true;
}

bool Coordinator::reserveCapacity(rateT cap, portIdT portNum,
                                  int portType) {    // returns true if cap can be allocated. else false.
    if (portType == Utils::daemonTypeReceiverPort) {
        if (receiversRemCap[portNum] >= cap) {
            receiversRemCap[portNum] -= cap;
            if (receiversRemCap[portNum] <= 0) {
                totalFullyFreeReceivers--;
            }
            return true;
        }
    } else if (portType == Utils::daemonTypeSenderPort) {
        if (sendersRemCap[portNum] >= cap) {
            sendersRemCap[portNum] -= cap;
            if (sendersRemCap[portNum] <= 0) {
                totalFullyFreeSenders--;
            }
            return true;
        }
    }
    return false;
}

bool Coordinator::reserveSenderFully(mapIdT mid) {
    return reserveCapacity(Utils::portBytesPerMSec, mid, Utils::daemonTypeSenderPort);
}

bool Coordinator::reserveReceiverFully(redIdT rid) {
    return reserveCapacity(Utils::portBytesPerMSec, rid, Utils::daemonTypeReceiverPort);
}

rateT Coordinator::getFreeCapacity(portIdT portNum, int portType) {
    if (portNum >= totalPorts) {
        cout << "unexpected - in getFree capacity 1. Invalid PortNum: " << portNum << "\n";
        return 0;
    }
    if (portType == Utils::daemonTypeReceiverPort) {
        return receiversRemCap[portNum];
    } else if (portType == Utils::daemonTypeSenderPort) {
        return sendersRemCap[portNum];
    }
    cout << "unexpected - in getFree capacity 2. Invalid PortType: " << portType << "\n";
    return 0;
}

bool Coordinator::isSenderFullyFree(mapIdT mapid) {
    return sendersRemCap[mapid] == Utils::portBytesPerMSec;
}

bool Coordinator::isReceiverFullyFree(redIdT rid) {
    return receiversRemCap[rid] == Utils::portBytesPerMSec;
}

bool Coordinator::fabricHasFreePorts() {    // returns false if all the senders and/or all the receivers are fully occupied
    if (totalFullyFreeSenders == 0 || totalFullyFreeReceivers == 0) {
        return false;
    }
    return true;
}

void Coordinator::resetCapacityForAll() {
    totalFullyFreeSenders = 0;    // will be incremented from function resetSenderCapacity
    totalFullyFreeReceivers = 0;    // will be incremented from function resetReceiverCapacity
    for (int i = 0; i < totalPorts; i++) {
        resetSenderCapacity(i);
        resetReceiverCapacity(i);
    }
}

void Coordinator::resetSenderCapacity(mapIdT portNum) {
    sendersRemCap[portNum] = Utils::portBytesPerMSec;
    totalFullyFreeSenders += 1;
}

void Coordinator::resetReceiverCapacity(redIdT portNum) {
    receiversRemCap[portNum] = Utils::portBytesPerMSec;
    totalFullyFreeReceivers += 1;
}

void Coordinator::freeSenderCapacityBy(rateT cap, mapIdT portNum) {
    sendersRemCap[portNum] += cap;
    if (sendersRemCap[portNum] == Utils::portBytesPerMSec) {
        totalFullyFreeSenders += 1;
    }
}

void Coordinator::freeReceiverCapacityBy(rateT cap, redIdT portNum) {
    receiversRemCap[portNum] += cap;
    if (receiversRemCap[portNum] == Utils::portBytesPerMSec) {
        totalFullyFreeReceivers += 1;
    }
}

vector<GlobalJob> Coordinator::createAllJobsVector(char *filePath) {
    int numJobs, numPorts;
    vector<GlobalJob> toReturn;
    ifstream infile(filePath);
    std::string line;
    int jobId, numSenders, arrivalTime, numReceivers, senderId, receiverId;
    if (std::getline(infile, line)) {
        std::istringstream iss(line);
        if (!(iss >> numPorts >> numJobs)) {
            error("trace file reading error");
        } else {
            cout << "Creating Trace: numJobs: " << numJobs << " numPorts: " << numPorts << "\n";
        }
    }
    toReturn.reserve(numJobs);
    totalJobs = numJobs;
    totalPorts = numPorts;
    while (std::getline(infile, line)) {
        std::istringstream iss(line);
        if (!(iss >> jobId >> arrivalTime)) {
            error("trace file reading error");
        }
        if (!(iss >> numSenders)) {
            error("trace file reading error");
        }
        vector<int> temps(numSenders);
        for (int i = 0; i < numSenders; i++) {
            if (!(iss >> senderId)) {
                error("trace file reading error");
            } else {
                temps[i] = senderId;
            }
        }
        if (!(iss >> numReceivers)) {
            error("trace file reading error");
        }
        vector<int> tempr(numReceivers);
        for (int i = 0; i < numReceivers; i++) {
            if (!(iss >> receiverId)) {
                error("trace file reading error");
            } else {
                tempr[i] = receiverId;
            }
        }
    }
    return toReturn;
}

bool Coordinator::setTraceReader(string traceFile) {
    infile.open(traceFile);
    traceIsOpen = true;
    unsigned int numJobs, numPorts;
    std::string line;
    if (std::getline(infile, line)) {
        std::istringstream iss(line);
        if (!(iss >> numPorts >> numJobs)) {
            error("trace file reading error");
        } else {
            cout << "Creating Trace: numJobs: " << numJobs << " numPorts: " << numPorts << "\n";
        }
    }else{
		cout << "Error in reading: "<<traceFile<<"\n";
		abort();
	}
    totalJobs = numJobs;
    totalPorts = numPorts;
    outStandingJob = getNextJob();
	if (outStandingJob == NULL){
		cout <<"Here it is\n";
		abort();
	}
    return true;
}

GlobalJob *Coordinator::getNextJob() {
    if (totalJobs == startedJobs) {
        if (traceIsOpen) {
            infile.close();
            traceIsOpen = false;
        }
        return NULL;
    }
    GlobalJob *toReturn;
    std::string line;
    jobIdT jobId;
    int numSenders, arrivalTime, numReceivers;
    mapIdT senderId;
    redIdT receiverId;
    if (std::getline(infile, line)) {
        while (line[0]=='#'){
            std::getline(infile, line);
        }
        std::istringstream iss(line);
        if (!(iss >> jobId >> arrivalTime)) {
            error("trace file reading error arrivalTime");
        }
        if (!(iss >> numSenders)) {
            error("trace file reading error numSender");
        }
        vector<mapIdT> temps(numSenders);
        for (int i = 0; i < numSenders; i++) {
            if (!(iss >> senderId)) {
                error("trace file reading error senderId");
            } else {
                temps[i] = senderId;
            }
        }
        if (!(iss >> numReceivers)) {
            error("trace file reading error numReceiver");
        }
        vector<redIdT> tempr(numReceivers);
        vector<flowSizeT> flowLengths(numSenders * numReceivers);
        for (int i = 0; i < numReceivers; i++) {
            if (!(iss >> receiverId)) {
                error("trace file reading error receiverId");
            } else {
                tempr[i] = receiverId;
                flowSizeT flowLength;
                for (int j = 0; j < numSenders; j++) {
                    if (!(iss >> flowLength)) {
                        error("trace file reading error flowLength");
                    } else {
                        flowLengths[i * numSenders + j] = flowLength;
                    }
                }
            }
        }
        try {
            toReturn = new GlobalJob(jobId, arrivalTime, numSenders, temps, numReceivers, tempr/*, this*/, flowLengths,
                                     executionMode);
        } catch (std::bad_alloc &ba) {
            std::cout << "bad_alloc caught in GlobalJob::makeFlows() : " << ba.what() << " errno: " << errno << "\n";
        }
    } else {
	cout <<"Here 2\n";
        giveMessageToUser("Error in readingJob");
        toReturn = NULL;
    }
    return toReturn;
}

vector<jobIdT> Coordinator::updateActiveJobs() {// Here is an assumption about trace file that all the jobs in trace file are sorted in FIFO order
    vector<jobIdT> toReturn;
    if (!started || startedJobs == totalJobs) {
        if (traceIsOpen) {
            infile.close();
            cout << "+++++++++++++++++Trace File Closed+++++++++++++++++++++\n";
            traceIsOpen = false;
        }
        return toReturn;
    }
    timeT currTime = getTimeMillis();
    if (outStandingJob == NULL) {
        error("LOGICAL ERROR: Outstanding job is NULL when it should not be I.");
        return toReturn;
    }
    while (outStandingJob->arrivalTime <= (currTime + binningPeriod)) {
        if (outStandingJob->started) {
            error("LOGICAL ERROR: Outstanding job is already started.");
            return toReturn;
        }
        startJob(outStandingJob);
	switch(schedulingPolicy){
		case Utils::schedulingPolicyPHILAE:{
			if(impactCalculationSchemeSelection == Utils::simpleContentionImpact){
				activeJobs[Utils::firstMainQIndexPHILAE].push_back(outStandingJob);
			}else{
				if (outStandingJob->initialWidth>thinProbingLimit){
					activeJobs[Utils::probeQPhilae].push_back(outStandingJob);
					if(oracleSimulation == Utils::philaeNonProbingOracle){
						handleWideJobArrivalForPHILAEORACLE(outStandingJob);	// this function should be responsible for ignoring the probing phase and place the job in the write queue. To be used only for nonProbingOracle mode
					}
				} else {
					activeJobs[Utils::thinQPhilae].push_back(outStandingJob);
				}
			}
			break;
		}
		default:{
			activeJobs[0].push_back(outStandingJob);
			break;
		}
	}
        activeJobsHash.insert(
                std::make_pair<jobIdT, GlobalJob *>(std::move(outStandingJob->id), std::move(outStandingJob)));
        activeJobsActiveFlowCount.insert(
                std::make_pair<jobIdT, int>(std::move(outStandingJob->id), std::move(outStandingJob->flowsVec.size())));
        toReturn.push_back(outStandingJob->id);
        outStandingJob = getNextJob();
        if (outStandingJob == NULL) {
            if (startedJobs != totalJobs) {
                error("LOGICAL ERROR: Outstanding job is NULL when it should not be II.");
            }
            break;
        }
        toSetRate = true;
    }
    return toReturn;
}

vector<jobIdT> Coordinator::updateActiveJobsPHILAE() {
    vector<jobIdT> addedJobs;
    addedJobs = updateActiveJobs();    // modify the function to return vector of added job ids
    std::vector<jobIdT>::iterator it, end;
	if(oracleSimulation != Utils::philaeNonProbingOracle){
    		for (it = addedJobs.begin(), end = addedJobs.end(); it != end; it++) {
       			assignProbeFlows(*it);
    		}
	}
    return addedJobs;
}

vector<flowIdT> Coordinator::assignProbeFlows(jobIdT jid) {
    GlobalJob *job = getActiveJobPointerByJobId(jid);
    if (job->currentQueue != Utils::probeQPhilae && job->initialWidth > thinProbingLimit && impactCalculationSchemeSelection != Utils::simpleContentionImpact) {
    	cout <<"jobId: "<< job->id<<" currentQ: "<< job->currentQueue<<" width: "<<job->initialWidth<<"\n";
        error("LOGICAL ERROR: assigningProbeFlows. A wide job is not in probeQueue");
    }
    vector<GlobalSenderPort *> sp;
    vector<GlobalReceiverPort *> rp;
    for (auto itb = job->sendersVector.begin();itb != job->sendersVector.end(); itb++) {
        sp.push_back(getSPPointerByPortNum(*itb));
    }

    for (auto itb = job->receiversVector.begin(); itb != job->receiversVector.end();itb++) {
        rp.push_back(getRPPointerByPortNum(*itb));
    }

    int numProbeFlows = getNumProbeFlows(job);
    job->set_max_num_probe_flow(numProbeFlows);
    assert(numProbeFlows != Utils::invalidValue);
    vector<flowIdT> toReturn;
    for (int i = 0; i < numProbeFlows; i++) {
        mapIdT SenderPort = INT_MAX;
        redIdT ReceiverPort = INT_MAX;
        stable_sort(sp.begin(), sp.end(), [](GlobalSenderPort *p1, GlobalSenderPort *p2) -> bool {
            return p1->getActiveProbeFlowCount() <= p2->getActiveProbeFlowCount();
        });
        stable_sort(rp.begin(), rp.end(), [](GlobalReceiverPort *p1, GlobalReceiverPort *p2) -> bool {
            return p1->getActiveProbeFlowCount() <= p2->getActiveProbeFlowCount();
        });


        if (probeFlowSelection == Utils::leastContentionPilotFlows && restrictOnePilotFlowPerMapper == false) {
            for (int totalItr = 0, probeFlowFoundFlag = 0; probeFlowFoundFlag == 0; totalItr++) {
                for (int senderItr = 0; senderItr < sp.size(); senderItr++) {
                    int receiverItr = totalItr - senderItr;
                    if (receiverItr >= rp.size())
                        continue;
                    if (receiverItr < 0)
                        break;
                    assert(totalItr>=0 && senderItr >= 0 && receiverItr>=0 && senderItr <= sp.size()-1 && receiverItr <= rp.size()-1);
                    SenderPort = sp.at(senderItr)->portNum;
                    ReceiverPort = rp.at(receiverItr)->portNum;
                    flowIdT fid = Utils::makeFlowId(jid, SenderPort, ReceiverPort);
                    if (job->probeFlowsMap.find(fid) == job->probeFlowsMap.end()) {
                        probeFlowFoundFlag = 1;
                        break;
                    }
                }
                if (probeFlowFoundFlag == 1)
                    break;
            }
        } else if (probeFlowSelection == Utils::randomPilotFlows && restrictOnePilotFlowPerMapper == false) {
            vector<int> randList1 = Utils::randomInteger(0, sp.size() - 1, 1);
            SenderPort = sp[randList1[0]]->portNum;
            vector<int> randList2 = Utils::randomInteger(0, rp.size() - 1, 1);
            ReceiverPort = rp[randList2[0]]->portNum;
            flowIdT fid = Utils::makeFlowId(jid, SenderPort, ReceiverPort);
            if (job->probeFlowsMap.find(fid) != job->probeFlowsMap.end()) {
                i--;
                continue;
            }
        } else if (probeFlowSelection == Utils::randomPilotFlows && restrictOnePilotFlowPerMapper == true) {
            if (sp.empty())
                break;
            vector<int> randList1 = Utils::randomInteger(0, sp.size() - 1, 1);
            int lucky = randList1[0];
            SenderPort = sp[lucky]->portNum;
            sp.erase(sp.begin() + lucky);
            vector<int> randList2 = Utils::randomInteger(0, rp.size() - 1, 1);
            ReceiverPort = rp[randList2[0]]->portNum;
        } else if (probeFlowSelection == Utils::leastContentionPilotFlows && restrictOnePilotFlowPerMapper == true) {
            if (sp.empty())
                break;
            SenderPort = sp[0]->portNum;
            sp.erase(sp.begin());
            int Min = INT_MAX;
            int index = INT_MAX;
            for (unsigned int in = 0; in < rp.size(); in++) {
                if (Min > rp[in]->getActiveProbeFlowCount()) {
                    Min = rp[in]->getActiveProbeFlowCount();
                    index = in;
                }
            }
            ReceiverPort = rp[index]->portNum;
        }
        if (SenderPort <= sendersVec.size() && ReceiverPort <= receiversVec.size()){
            flowIdT fid = Utils::makeFlowId(jid, SenderPort, ReceiverPort);
            if (job->addProbeFlow(fid)) {
                getSPPointerByPortNum(SenderPort)->addProbeFlow(fid);// Add the probe flow to sender and receiver port.
                getRPPointerByPortNum(ReceiverPort)->addProbeFlow(fid);
                toReturn.push_back(fid);
            } else {
                cerr << "Logical Error: while adding probe flows. This could be due to REMOVAL of ZERO length flows. fid: "<<fid<<" jid: "<< jid<<" spn: "<<SenderPort<<" rpn: "<<ReceiverPort
                     << endl;
            }
        } else{
            cerr << "Logical Error: Invalid SenderPort choosen" <<endl;
        }

    }
    return toReturn;
}

int Coordinator::getNumProbeFlows(GlobalJob *job) {
        /**
         * Probe flow number selection
         */
        long int toReturn = Utils::invalidValue;
        if((job->initialWidth) <= thinProbingLimit) { // If this is a Thin CoFlow
            toReturn = job->initialWidth;
        } else {
		if (impactCalculationSchemeSelection == Utils::simpleContentionImpact) {
			return 0;	// As if impact is only contention based then there is no need to estimate length, so no probing.
		}
            switch (probeNumSelection) {
                case Utils::minMapRedPilotFlows: {
                    toReturn = Utils::minInt(job->numSenders, job->numReceivers);
                    break;
                }
                case Utils::proportionalMCrossRPilotFlows: {
                    toReturn = int(job->numSenders*job->numReceivers*proportionalPilotFlowsRatio);
                    break;
                }
                case Utils::constantPilotFlows: {
                    toReturn = constantPilotFlowsValue;
                    toReturn = Utils::minInt(toReturn, job->initialWidth);
                    break;
                }
                case Utils::proportionalMapperPilotFlows: {
                    toReturn = int(job->numSenders * proportionalPilotFlowsRatio);
                    break;
                }
                case Utils::proportionalMINMRPilotFlows: {
                    toReturn = int(Utils::minInt(job->numSenders, job->numReceivers) * proportionalPilotFlowsRatio);
                    break;
                }
                default:
                    cerr << "getNumProbeFlows ERROR: invalid value of probeNumSelection";
            }
            if (toReturn < 1) {
                toReturn = 1;
            }
            if (restrictOnePilotFlowPerMapper) {    // this can be a bug in case of constant and one mapper turnoff
                toReturn = Utils::minInt(job->numSenders, toReturn);
            }
        }
        return toReturn;
    }

GlobalJob *Coordinator::getActiveJobPointerByJobId(jobIdT jid) {    // if job is not active it will return null
    std::unordered_map<jobIdT, GlobalJob *>::iterator toReturn;
    toReturn = activeJobsHash.find(jid);
    if (toReturn == activeJobsHash.end()) {
        cout << "returning NULL jid: " << jid << "\n";
        return NULL;
    } else {
        return (GlobalJob *) (toReturn->second);
    }
}

GlobalSenderPort *Coordinator::getSPPointerByPortNum(mapIdT portNum) {    // if portNum is invalid it will return null
    GlobalSenderPort *toReturn;
    toReturn = sendersVec[portNum];
    if (toReturn == NULL) {
        cout << "returning NULL sp portNum: " << portNum << "\n";
        return NULL;
    } else {
        return (GlobalSenderPort *) toReturn;
    }
}

GlobalReceiverPort *Coordinator::getRPPointerByPortNum(redIdT portNum) {    // if portNum is invalid it will return null
    GlobalReceiverPort *toReturn;
    toReturn = receiversVec[portNum];
    if (toReturn == NULL) {
        cout << "returning NULL rp portNum: " << portNum << "\n";
        return NULL;
    } else {
        return (GlobalReceiverPort *) toReturn;
    }
}
	
void Coordinator::startJob(GlobalJob *job) {
    job->startJob(getTimeMillis());
    std::unordered_map<mapIdT, GlobalSenderPort *>::iterator sPort;
    std::unordered_map<redIdT, GlobalReceiverPort *>::iterator rPort;
    std::vector<mapIdT>::iterator its, ends;
    std::vector<redIdT>::iterator itr, endr;
    for (its = job->sendersVector.begin(), ends = job->sendersVector.end(); its != ends; ++its) {
        sPort = sendersMap.find(*its);
        if (sPort == sendersMap.end()) {
            error("Sender port missing");
            cout << "missing for jId: " << job->id << " for Port: " << *its << "\n";
        } else {
            sPort->second->addJob(job);
        }
    }
    for (itr = job->receiversVector.begin(), endr = job->receiversVector.end(); itr != endr; ++itr) {
        rPort = receiversMap.find(*itr);
        if (rPort == receiversMap.end()) {
            error("Receiver port missing");
        } else {
            rPort->second->addJob(job);
        }
    }
	switch(schedulingPolicy){
		case Utils::schedulingPolicyPHILAE:{
			if(impactCalculationSchemeSelection == Utils::simpleContentionImpact){
    				job->currentQueue = Utils::firstMainQIndexPHILAE;
			}else{
				if (outStandingJob->initialWidth>thinProbingLimit){
    					job->currentQueue = Utils::probeQPhilae;
				}else{
	    				job->currentQueue = Utils::thinQPhilae;
				}
			}
			break;
		}
		default:{
    			job->currentQueue = 0;
			break;
		}
	}
    if(schedulingPolicy == Utils::schedulingPolicyPHILAE) {    // contention policy check
        updateContentions(job, Utils::transitionTypeArrival, Utils::invalidValue, Utils::invalidValue);
    }
    startedJobs++;
    job->queueArrivalTime = getTimeMillis();
}

void Coordinator::handleWideJobArrivalForPHILAEORACLE(GlobalJob* jobToBeHandled){// this function should be responsible for ignoring the probing phase and place the job in the write queue. To be used only for nonProbingOracle mode
	probeFinishedPHILAE(jobToBeHandled);
}


void Coordinator::jobCreated(jobIdT *jobId) {
    cout << "COORDINATOR job created: " << *jobId << "\n";
}

int Coordinator::assignSchedulingPolicy(string policyName) {
    if (policyName.compare("philae") == 0){
        cout << "Scheduling policy PHILAE\n";
        return Utils::schedulingPolicyPHILAE;
    } else {
		cout << "Scheduling policy name didn't match. Falling to default policy PHILAE\n";
		return Utils::schedulingPolicyPHILAE;
	}
}

void Coordinator::error(string msg) {
    cerr << msg << "\n";
}

void Coordinator::giveMessageToUser(string msg) {
    cerr << msg << "\n";
}

timeT Coordinator::getTimeMillis() {
    cout << "getTimeMillis, I should not be called\n";
    return 0;
}

void Coordinator::setTime() {
    cout << "setTime, I should not be called\n";
}

void Coordinator::endEmulation() {
    cout << "endEmulation, I should not be called\n";
}

int Coordinator::getNumJobQueuesSingleQueue() {
    return 1;
}

int Coordinator::getNumJobQueuesPHILAE() {
    return Utils::numJobQueues;
}

int Coordinator::getNumJobQueues(){
    return numJobQueues;
}

void Coordinator::updateMinTime(unsigned int rate, long int size) {
    unsigned long int tempTime = (size + rate - 1) / rate;
    if (minTime > tempTime) {
        minTime = tempTime;
    }
}

void Coordinator::resetMinTime() {
    minTime = INT_MAX;
}

void Coordinator::forceSetMinTimeTo(unsigned long int val) {
    minTime = val;
}

unsigned long int Coordinator::getMinTime() {
    return minTime;
}

void Coordinator::fidAssert(flowIdT fid, string loc) {
    jobIdT jid = Utils::getJobIdFromFlowId(fid);
    mapIdT mid = Utils::getMapPortNumFromFlowId(fid);
    redIdT rid = Utils::getRedPortNumFromFlowId(fid);
    if (jid > totalJobs || jid < 0) {
        cout << loc << ": fid jid is culprit fid: " << fid << " jid: " << jid << "\n";
    }
    if (mid >= totalPorts || mid < 0) {
        cout << loc << ": fid mid is culprit fid: " << fid << " mid: " << mid << "\n";
    }
    if (rid >= totalPorts || rid < 0) {
        cout << loc << ": fid rid is culprit fid: " << fid << " rid: " << rid << "\n";
    }
}

void Coordinator::midAssert(mapIdT mid, string loc) {
    if (mid >= totalPorts || mid < 0) {
        cout << loc << ": mid is culprit mid: " << mid << "\n";
    }
}

void Coordinator::ridAssert(redIdT rid, string loc) {
    if (rid >= totalPorts || rid < 0) {
        cout << loc << ": rid is culprit rid: " << rid << "\n";
    }
}

void Coordinator::jidAssert(jobIdT jid, string loc) {
    if (jid > totalJobs || jid < 0) {
        cout << loc << ": jid is culprit " << jid << "\n";
    }
}

unsigned long int Coordinator::getClockTimeMicroSec() {
    struct timeval tp;
    gettimeofday(&tp, NULL);
    return (tp.tv_sec * 1000000 + tp.tv_usec);
}

Coordinator::~Coordinator() {
    for (auto receiver:receiversVec){
        delete(receiver);
    }
    for (auto sender:sendersVec){
        delete(sender);
    }
}

long long int Coordinator::getPortContention(GlobalPort *portPtr) {
    return portPtr->activeJobs.size();
}

void Coordinator::moveJobToQueue(GlobalJob *job, int newQueue) {
	auto oldQueue = job->currentQueue;
	job->currentQueue = newQueue;
	activeJobs[newQueue].push_back(job);
	updateContentionsOnQJump(job,oldQueue);
	auto it = std::find(activeJobs[oldQueue].begin(), activeJobs[oldQueue].end(), job);
	if(it != activeJobs[oldQueue].end()){
		activeJobs[oldQueue].erase(it);
	}
	else{
		cerr << "Exception in moveJobToQueue. Job not found in old queue.";
	}
}

void Coordinator::setNextTimeSlotWorkingQueue() {
//! Define which time slot this cycle is
    workingQueuesToSetVec.clear();
    if (schedulingPolicy == Utils::schedulingPolicyPHILAE && starvationFreedomPolicy == Utils::starvationVulnerable) {// PHILAE : starvation vulnerable Version
            for (int timeSlotAssigned = Utils::totalTimeSlotNum; timeSlotAssigned > 0; timeSlotAssigned--) {
                workingQueuesToSetVec.push_back(Utils::firstMainQIndexPHILAE);
            }
    } else if (schedulingPolicy == Utils::schedulingPolicyPHILAE && starvationFreedomPolicy == Utils::starvationFreedomTimeSlot){ // PHILAE : starvation free version aka multiple queue
        vector<int> workingQueues;
        for (int queueItr = Utils::firstMainQIndexPHILAE; queueItr < (this->*getNumJobQueuesPtr)(); queueItr++) {// Check which queues have active jobs
            if (!activeJobs[queueItr].empty()) {
                workingQueues.push_back(queueItr);
            }
        }

        if (!workingQueues.empty()) {
            for (auto workingQueueItr = workingQueues.begin();
                 workingQueueItr != workingQueues.end(); workingQueueItr++) {// For each non-empty queue
                int queueNum = *workingQueueItr;
                for (int timeSlotAssigned = Utils::totalTimeSlotNum / pow(timeSplitingFactorCLI, queueNum - workingQueues[0]);// Add time slot - change timeSplitingFactor to change the ratio of bandwidth share between different queues. If it is 10 then sharing is like for 3 queues 100, 10, 1.
                     timeSlotAssigned > 0; timeSlotAssigned--) {
                    if (queueNum != 0) {
			workingQueuesToSetVec.push_back(queueNum);
                    }
                }
            }
        }
        if (!workingQueues.empty() && workingQueuesToSetVec.empty()){// Only probe queue has jobs
             workingQueuesToSetVec.push_back(Utils::probeQPhilae);
        }
    }
}

void Coordinator::setWorkingQueue() {
    if (workingQueuesToSetVec.empty()) {
        setNextTimeSlotWorkingQueue();

    }
    if (workingQueuesToSetVec.empty()){// No jobs.
        currentWorkingQueue = Utils::invalidValue ;
        return;
    }
    currentWorkingQueue = workingQueuesToSetVec.front();
    workingQueuesToSetVec.erase(workingQueuesToSetVec.begin());
    assert(currentWorkingQueue>=0 && currentWorkingQueue< (this->*getNumJobQueuesPtr)());
}
