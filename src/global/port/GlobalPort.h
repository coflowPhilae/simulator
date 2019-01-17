#ifndef GLOBALPORT_H_
#define GLOBALPORT_H_

#include <iostream>
#include <string.h>   //strlen
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>   //close
#include <stdint.h>
#include <errno.h>
#include <vector>
#include <unordered_map>
#include "../../utils/utility.hpp"
#include "../datastructures/GlobalFlow.h"
#include "../datastructures/GlobalJob.h"

using namespace std;

class GlobalPort {
public:
    GlobalPort(portIdT portNum, int portType);

    virtual ~GlobalPort();

    void setCapacity(rateT capacity);

    rateT getCapacity();

    void addJob(GlobalJob *job);

    bool removeFlow(flowIdT fid, jobIdT jid);    // returns true if job has been deleted.
    
    void removeJob(jobIdT jid);

    void commonInitialization();

    bool addProbeFlow(flowIdT fid);

    bool removeProbeFlow(flowIdT fid);

    int getActiveProbeFlowCount();


    portIdT portNum;
    int portType;
    rateT capacity;    // capacity in bytesPerMsec
    unordered_map<flowIdT, GlobalFlow *> activeFlowsMap;    // map of flow id and its pointer
    std::vector<GlobalFlow *> activeFlowsVec;
    unordered_map<jobIdT, int> activeJobs;    //there will be a numberOfFlows entry corresponding to each job if its active, there maybe entry for non active jobs but they will be 0
    int activeProbeFlowCount;
    unsigned int executionMode;
};

#endif /* GLOBALPORT_H_ */
