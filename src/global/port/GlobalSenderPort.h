#ifndef GLOBALSENDERPORT_H_
#define GLOBALSENDERPORT_H_

#include "GlobalPort.h"
#include "../../utils/utility.hpp"
#include <map>

class GlobalSenderPort : public GlobalPort {
public:
    GlobalSenderPort(portIdT portNum);

    void resetAllocations();

    virtual ~GlobalSenderPort();

    void setCurrentAllocatedFlow(GlobalFlow *gf);

    GlobalFlow *getCurrentAllocatedFlow();

    bool removeFlow(flowIdT fid, jobIdT jid);    // returns true if job has been deleted.

    //vector<msgT> flowRates; // pushed in sequence fid and its corresponding rate.
    long int lastRateSentTime;
    GlobalFlow *currentAllocatedFlow;
};

#endif /* GLOBALSENDERPORT_H_ */
