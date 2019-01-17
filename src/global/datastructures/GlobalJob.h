/*
 * GlobalJob.h
 *
 *      Author: 
 */

#ifndef GLOBALJOB_H_
#define GLOBALJOB_H_

#include <iostream>
#include <string.h>   //strlen
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>    //close
#include <stdlib.h>
#include <time.h>
#include <unistd.h>   //close
#include <netdb.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include <stdint.h>
#include <errno.h>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include "../../utils/utility.hpp"
#include "GlobalFlow.h"
#include "MapTask.h"
#include "RedTask.h"
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/moment.hpp>
#include <numeric>
using namespace boost::accumulators;

class GlobalFlow;

class GlobalJob {
public:
    GlobalJob(jobIdT id, timeT arrivalTime, int numSenders, vector<mapIdT> sendersVector, int numReceivers,
              vector<redIdT> receiversVector/*, Coordinator* coordinator*/, vector<flowSizeT> flowSizes,
              unsigned int executionMode);

    void startJob(timeT time);

    vector<flowIdT> simulate(timeT simTime, int orderP);    // simulates each flow in the job and return ids of flows finished in the vector
    void updateDataSentByFlowElseWhere(flowIdT fid, flowSizeT delta);    // updates data in job and task data structures
    void makeTasks();
    void makeFlows();

    void incrementSentData(jobSizeT dataSize);
	jobSizeT getBytesSent(); 
    jobSizeT getRemainingSize();
	void updateSizeAndFlowBytesSentOnFlowCompletion(flowIdT fid);	// this function should only be called when size is not being updated by any other means.

    void updateLongestAndShortestFlowLength(flowSizeT flowLen);
    void jobDead(timeT time);
    timeT getJCT();
    bool removeFlow(flowIdT fid);    // returns true if all flows are over;
    void setImpactFactor(impactFactorT newFactor);
    impactFactorT getImpactFactor();
    contentionFactorT getContentionFactor();
	void setContentionFactor(contentionFactorT newFactor);
    bool addProbeFlow(flowIdT fid);
    std::vector<GlobalFlow *> *getProbeFlows();
    bool checkIfProbeFlowFinished(flowIdT fid);    // to be called on event of a flow finish. Returns true if the finished flow was probe. This function assumes that flow with fid has finished and doesn't checks for it.
    int getActiveProbeFlowCount();
    bool probeFinished();    // can be called any time. Returns true iff probe flows are allocated and all are finished. In case if no probe flows are assigned or all are not finished returns true.
    flowSizeT getEstimatedLength();    // to be used with estimation algorithms
    void setEstimatedLength(flowSizeT length);    // to be used with estimation algorithms
    void estimateLength();    // to be used with estimation algorithms
	void set_max_num_probe_flow(unsigned int numProbeFlows);
	unsigned int get_max_num_probe_flow();
    bool removePort(portIdT portNum, int portType);

    /*	WARNING: Till flow pointers are stored in a vector it will be very inefficient to use this function where we want to loop over flows that will make complexity f^2*/
    GlobalFlow *getFlowPtrByFid(flowIdT fid);    //	Returns pointer to GlobalFlow if found else returns Null
    /*	WARNING: Till probeFlow pointers are stored in a vector it will be very inefficient to use this function where we want to loop over flows that will make complexity pf^2*/
    GlobalFlow *getProbeFlowPtrByFid(flowIdT fid);
    unsigned int getMinActiveNumberOfTasks();    // return minimum of activeSenders and activeReceivers
    unsigned int getActiveNumberOfMappers();
    unsigned int getActiveNumberOfReducers();
    flowSizeT getLongestFlowLength();
    flowSizeT getShortestFlowLength();
    void sortTasksBySize();
    void sortMapTasksBySize();
    void sortRedTasksBySize();
    mapIdT getMapPortNumAtIndex(unsigned int index);
    redIdT getRedPortNumAtIndex(unsigned int index);
    void addFlowToSend(GlobalFlow *gf);
    virtual ~GlobalJob();

    jobIdT id;
    timeT arrivalTime;
    int numSenders;    // initial number of senders
    vector<mapIdT> sendersVector;    // has active ports of jobs - DEPRECATED - use MapTasks now
    int numReceivers;    // initial number of senders and receivers
    vector<redIdT> receiversVector;    // has active ports of jobs	- DEPRECATED - use RedTasks now
    long int initialWidth;
    vector<flowSizeT> flowSizes;
    bool started;
    jobSizeT bytesSent;    // Might be inconsistent in case of testbed. It will be correctly updated only for certain scheduling policies.
    timeT startTime, endTime, probingEndTime;
    flowSizeT averageFlowLength;
    flowSizeT getAverageFlowLength() const;
    void setProbingEndTime(timeT probingEndTime);
    long getProbingEndTime() const;
    std::vector<MapTask *> mapTasksVec;    // should only have activeMapTasks
    std::unordered_map<mapTaskIdT, MapTask *> mapTasksMap;
    std::vector<RedTask *> redTasksVec;    // should only have activeRedTasks
    std::unordered_map<redTaskIdT, RedTask *> redTasksMap;
    std::unordered_map<flowIdT, GlobalFlow *> flowsMap;        //	contains pointer to all active flows.
    std::vector<GlobalFlow *> flowsToSend;    // this is coming handy while simulating in case of orderPRate assignments. If this doesn't exists we will have to iterate over all flows to simulate making it orderP2.
    std::vector<GlobalFlow *> flowsVec;
    std::vector<GlobalFlow *> probeFlowsVec;
    std::unordered_map<flowIdT, GlobalFlow *> probeFlowsMap;
    std::unordered_map<flowIdT, GlobalFlow *> startedFlowsMap;
    bool dead;
    int currentQueue;
    void reportTransmissionStatus(int cycle);
    
    unsigned int max_num_probe_flows;
    int finalQueueNum;
    bool firstTransmissionStarted, finalTransmissionStarted;
	contentionFactorT contentionFactor;
    impactFactorT currentImpactFactor;    // this will be used for sorting purpose. Updating this is responsibility of Coordinator and job doesn't takes care of that. Job exposes getter and setter for it.
    unsigned int executionMode;
//	std::unordered_map<int, GlobalFlow*> probeFlows;
    int activeProbeCount;
    long int queueArrivalTime;
    long int queueTransmissionStartTime;
    flowSizeT estimatedLength;    // Might be inconsistent in case of testbed. It will be correctly updated only for certain scheduling policies.
    bool allOrNone;    //	this is for assignRateScheme^2 all-or-none
    rateT allOrNoneRate;    //	this is for assignRateScheme^2 all-or-none
    flowSizeT longestFlowLength;    // Might be inconsistent in case of testbed. It will be correctly updated only for certain scheduling policies.
    flowSizeT shortestFlowLength;
    void updateTaskShorestFlowLength(GlobalFlow *flowPtr);
    MapTask *getMapTaskPtrFromMapTaskId(mapTaskIdT id);
    RedTask *getRedTaskPtrFromRedTaskId(redTaskIdT id);
    void updateAverageFlowLength();
    bool toSimulate;
    void setFinalQueueNum();
    jobSizeT totalSize;
    jobSizeT getTotalSize() const;
    void setTotalSize(jobSizeT size);
	void setDeadline(timeT deadlineToSet);
	timeT getDeadline();
	timeT getEstimatedCompletionTime(rateT maxRate);	// this function should be used only with PHILAE, or similar estimation based scheduling techniques
};

#endif /* GLOBALJOB_H_ */
