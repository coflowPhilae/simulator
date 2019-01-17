/*
* Coordinator.h
*
*      Author: 
*/

#ifndef GLOBAL_COORDINATOR_COORDINATOR_H_
#define GLOBAL_COORDINATOR_COORDINATOR_H_

#include <iostream>
#include <string.h>   //strlen
#include <sys/types.h>
#include <stdint.h>
#include <vector>
#include <unordered_map>
#include <unistd.h>
#include <algorithm>
#include "../../utils/utility.hpp"
#include "../port/GlobalSenderPort.h"
#include "../port/GlobalReceiverPort.h"
#include "../datastructures/GlobalJob.h"
#include <fstream>
#include "../../libs/json.hpp"

class Coordinator {
public:
    Coordinator(string traceFilePath, string schedulingPolicyName, unsigned int executionMode,
                    string outputFileDiscriminator, int ProbeNumSelection, int probeFlowSelection, int oneProbePerMapper,
                    int starvationFreedomPolicy, nlohmann::json config);
    Coordinator(nlohmann::json config);
    virtual ~Coordinator();
    unsigned int oracleSimulation;
    unsigned int impactCalculationSchemeSelection;
    //	Below are only in Coordinator
    nlohmann::json config;
    unsigned int simQuantaInMSec;
    unsigned int numJobQueues;
    impactFactorT (Coordinator::*getImpactOfJobPtr)(jobIdT jid);
    contentionFactorT getContentionOfJob(jobIdT jid); // returns number of jobs the job with id Jid will block
    void updateContentions(GlobalJob *job, int transitionType, int portType, portIdT portNum);    // 3rd and 4th arguments are not valid in case of arrival
    void updateContentionsOnQJump(GlobalJob *job, int fromQ);    //this function assumes that job object has already been updated to reflect newQ; currently being called from updateQueues; during call of this function job pointer is in both activeJobs[fromQ] activeJobs[newQ]
    //! Update jobs to different queues
    void (Coordinator::* updateQueuesPtr)();
    void updateQueuesPHILAE();
    jobSizeT (Coordinator::* getJobQueueJumpLimPtr)(GlobalJob *job);
    void sortAllQsByCoFlowImpact();	//sorts all the queues by impact

    void sortQByCoFlowImpact(unsigned int qNum);    //sorts the queues by impact
	void sortQByCoFlowContention(unsigned int queueNum);	// sorts the queues by contention
    bool assignAllOrNoneRate(GlobalJob *job);//returns true if allocated all-or-none rate else returns false
    void assignRateOnlyAtSenderPortP(GlobalJob *job, mapIdT spNum);

    void assignRate(GlobalJob *job);    //assigns rate to each flow whatever possible
    void assignRateToPilotFlows(GlobalJob *job);

    void assignRateForAllFlowsOfJob(rateT rate, GlobalJob *job);

    void assignRateOrderP(GlobalJob *job);    // tries to assign rate to one flow of a job at a port.
    bool assignAllOrNoneRateOrderP(GlobalJob *job);    // return true if allocated rate to one flow at all active ports of each job.
    void assignFullRateToFlow(GlobalFlow *gf);
    void (Coordinator::*setRatesPtr)();
    void commonSetRatesActions();
    jobSizeT estimateJobLength(jobIdT jid);
    bool (Coordinator::* flowFinishPtr)(flowIdT);
    bool flowFinish(flowIdT fid);    // returns true if the corresponding job too Finished.
    bool flowFinishPHILAE(flowIdT fid);    // returns true if the corresponding job too Finished.
    void jobFinish(jobIdT jid);
    GlobalJob *getActiveJobPointerByJobId(jobIdT jid);    // if job is not active it will return null
    GlobalSenderPort *getSPPointerByPortNum(portIdT portNum);    // if portNum is invalid it will return null
    GlobalReceiverPort *getRPPointerByPortNum(portIdT portNum);    // if portNum is invalid it will return null
    bool reserveCapacity(rateT cap, portIdT portNum, int portType);

    bool reserveSenderFully(mapIdT mid);

    bool reserveReceiverFully(redIdT rid);

    rateT getFreeCapacity(portIdT portNum, int portType);

    bool isSenderFullyFree(mapIdT mid);

    bool isReceiverFullyFree(redIdT rid);

    bool fabricHasFreePorts();    // returns false if all the senders and/or all the receivers are fully occupied
    void resetCapacityForAll();

    void resetSenderCapacity(mapIdT portNum);

    void resetReceiverCapacity(redIdT portNum);

    void freeSenderCapacityBy(rateT cap, mapIdT portNum);

    void freeReceiverCapacityBy(rateT cap, redIdT portNum);

    vector<GlobalJob> createAllJobsVector(char *filePath);

    bool setTraceReader(string traceFile);    // this should go to Coordinator
    GlobalJob *getNextJob();    // this should go to Coordinator
    vector<jobIdT> (Coordinator::* updateActiveJobsPtr)();

    vector<jobIdT>
    updateActiveJobs();    //	Adds jobs which should be started now to Queues and ports. Returns vector of ids of newly added jobs in this cycle
    vector<jobIdT>
    updateActiveJobsPHILAE();    //	Adds jobs which should be started now to Queues and ports. Returns vector of ids of newly added jobs in this cycle
    vector<flowIdT> assignProbeFlows(jobIdT jid);    // returns vector of flow ids which are assigned as probe flow for the job with jid.
    void jobCreated(jobIdT *jobId);

    void startJob(GlobalJob *job);
	void handleWideJobArrivalForPHILAEORACLE(GlobalJob* jobToBeHandled);// this function should be responsible for ignoring the probing phase and place the job in the write queue. To be used only for nonProbingOracle mode

    int assignSchedulingPolicy(string policyName);

    void error(string msg);

    void giveMessageToUser(string msg);
    int (Coordinator::* getNumJobQueuesPtr)();	// this should only return number of queues in main system and not the queues for deadline if any. And if there is any deadline queue it will be the last queue in the activejobs.
    int getNumJobQueuesSingleQueue();
    int getNumJobQueues();
    int getNumJobQueuesPHILAE();

    int getNumProbeFlows(GlobalJob *job);

    flowSizeT getRemaingFlowSize(jobIdT jid);

    void updateMinTime(rateT rate, long int size);

    void resetMinTime();

    void forceSetMinTimeTo(unsigned long int val);

    unsigned long int getMinTime();

    void fidAssert(flowIdT fid, string loc);

    void jidAssert(jobIdT jid, string loc);

    void midAssert(mapIdT mid, string loc);

    void ridAssert(redIdT rid, string loc);

    unsigned long int getClockTimeMicroSec();

    void resetFabricStatus();

    void setCurrentSendFlowToNULLForAllPorts();
    //	Above are only in Coordinator

    //	Below are in both Coordinator and derived classes
    virtual unsigned long int getTimeMillis();

    virtual void setTime();

    virtual void endEmulation();
    //	Above are in both Coordinator and derived classes
    long long int getPortContention(GlobalPort *portPtr);
    void updateUnservedJobQuota();
	unsigned int nextWorkingQueue;
    void reportFabricStatus();
    void setRatesPHILAE();
    //! estimates length and moves job to the right queue
    //! \param job the pilot finished job
    void probeFinishedPHILAE(GlobalJob *job);
    void moveJobToQueue(GlobalJob *job, int newQueue);
    void setNextTimeSlotWorkingQueue();
    void setWorkingQueue();

	float proportionalPilotFlowsRatio;
	unsigned int constantPilotFlowsValue;
    unordered_map<mapIdT, GlobalSenderPort *> sendersMap;
    unordered_map<redIdT, GlobalReceiverPort *> receiversMap;
    std::vector<GlobalSenderPort *> sendersVec;
    std::vector<GlobalReceiverPort *> receiversVec;
    vector<GlobalJob> allJobs;    /*now this may contain only objects of currently active jobs or maybe nothing.*/
    vector<vector<GlobalJob *>> activeJobs;
    unordered_map<jobIdT, GlobalJob *> activeJobsHash;    // jid, job* hash;
    unordered_map<jobIdT, int> activeJobsActiveFlowCount;
    unordered_map<jobIdT, unordered_map<jobIdT, int> *> jobsContentionCount;    //<jid, map<conflicting jid, num conflicting ports>>, will contain an entry for self too
    std::string traceFilePath;
    unsigned int totalPorts;
    unsigned int finishedJobs;
    unsigned int startedJobs;
    unsigned int totalJobs;
    unsigned int q0Limit;
    double qMultiplier;
    unsigned long totalCCT;
    unsigned int thinProbingLimit;
    bool started;
    bool canExit;
    int schedulingPolicy;
    string schedulingPolicyName;
    vector<jobSizeT> queueLimitsVector;
    vector<rateT> sendersRemCap;    // port Bytes per MSec
    vector<rateT> receiversRemCap;    // port Bytes per MSec
    unsigned int totalFullyFreeSenders;
    unsigned int totalFullyFreeReceivers;
    ofstream microscopeFile;
    long int startTimeMillis;
    unsigned int executionMode;
    ifstream infile;
    GlobalJob *outStandingJob;
    bool traceIsOpen;
    string outputFileDiscriminator;
    unsigned int binningPeriod = 10240;    // when you close binning look at calculateSimTime too.
    unsigned long int minTime;
    unsigned int rateAssignmentOrder;    // Not in use currently	scheduling policy specific parameter to be set during policy configuration.
    unsigned int jobSizeMetric;    //	scheduling policy specific parameter to be set during policy configuration.
    unsigned int assignRateScheme;
    // Debug and logging variables
    long long debugTimePort;
    long long debugTimeCoordinator;
    long long debugTimeDiffPort;
    long long debugTimeDiffCoordinator;
    unsigned int verbose;
    struct timeval debugTimeStruct;
    unsigned long setRateCycle;    //	reflects number of cycles, in multiple of Utils::simQuanta, passed.
    unordered_map<mapIdT, rateT> freedSenderCap;
    unordered_map<redIdT, rateT> freedReceiverCap;
    unsigned int flowsFinishedThisCycle;
    unsigned int flowsAddedThisCycle;
    unsigned int currentWorkingQueue;
    bool toSetRate;    // being set true whenever there is a re-ordering in q. In job finish, contention update and job arrival.
    long lastRateSetCycleNum;
    int cycle;
    unsigned int starvationFreedomPolicy;
	unsigned int timeSplitingFactorCLI; 
    unordered_map<jobIdT ,GlobalJob *> servedJobMap;
    unordered_map<jobIdT ,GlobalJob *> unServedJobMap;
    vector<int> workingQueuesToSetVec;
    vector<rateT> receiverPortQuotaPerJob;
    /**
     * The receiver port quota for each unserved job in stage 2
     */
    vector<rateT> senderPortQuotaPerJob;
    /**
     * Option for how to choose the probe flow
     * 0 : random
     * 1 : least contention
     */
    unsigned int probeFlowSelection;
    /**
     * Option for how many probe flow we will select
     * 0 : min ( mapper,reducer)
     * 1 : 1
     * 2 : 1% of total flows
     */
    unsigned int probeNumSelection;
    /**
     * Option for whether allow multiple probe flows per mapper
     * 0 : allow multiple probe flows per mapper
     * 1 : only one probe flow per mapper
     */
    unsigned int restrictOnePilotFlowPerMapper;
	unsigned int numDeadlineQueues;
	unsigned int impactCalculationMethod;

    impactFactorT getImpactOfJobPhilae(jobIdT jid);
	void printAllJobsQPortAndContention(string userNote);
};

#endif /* GLOBAL_COORDINATOR_COORDINATOR_H_ */
