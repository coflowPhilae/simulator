/*
 * GlobalFlow.h
 *
 *      Author: 
 */

#ifndef GLOBALFLOW_H_
#define GLOBALFLOW_H_

#include <iostream>
#include <string.h>   //strlen
#include <stdlib.h>
#include <vector>
#include <unordered_map>
#include "../../utils/utility.hpp"
using namespace std;
class Task;
class GlobalJob;
class GlobalFlow {
public:
    GlobalFlow(flowSizeT sizeInBytes, unsigned int executionMode, flowIdT flowId);

    virtual ~GlobalFlow();

    rateT getRate();

    void setRate(rateT rate);

    void setSizeInBytes(flowSizeT size);

    flowSizeT getSizeInBytes();

    flowSizeT getRemainingSize();

    flowSizeT getBytesSentSoFar();
    flowSizeT simulate(timeT simTime); //	simulates send of flow and returns bytes send.
    flowSizeT decrementSizeRemaining(
            flowSizeT upto);    // decrements size remaining till upto does not let it go below 0; Returns number of bytes decreased
    bool finished();    //	return true if flows is finished
	void updateFlowBytesSentOnFlowCompletion();	// this function should only be called when size is not being updated by any other means.
    void setOtherIds();

    bool markProbe();

    void markNonProbe();

    bool isProbe();

    void commonInitialization();
    Task * getRedTaskPtr();
    Task * getMapTaskPtr();

    rateT rate;
    flowIdT id;
    jobIdT jobId;
    mapIdT mapId;
    redIdT redId;
    flowSizeT totalSizeInBytes;
    flowSizeT sizeRemaining;
    flowSizeT bytesSentSoFar;
    unsigned int executionMode;
    bool probe;
    GlobalJob * jobPtr;
};

#endif /* GLOBALFLOW_H_ */
