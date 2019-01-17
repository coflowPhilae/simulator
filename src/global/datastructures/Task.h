/*
 * Task.h
 *
 *      Author: 
 */

#ifndef GLOBAL_DATASTRUCTURES_TASK_H_
#define GLOBAL_DATASTRUCTURES_TASK_H_

#include <unordered_map>
#include "../../utils/utility.hpp"
class GlobalFlow;
class Task {
public:
    Task(jobIdT parentJobId, portIdT portNum, int taskType);

    taskSizeT getBytesShuffled();

    void incrementBytesShuffled(taskSizeT delta);

    virtual ~Task();

    taskIdT id;
    int taskType;
    jobIdT parentJobId;
    portIdT hostPort;
    taskSizeT bytesShuffled;
    flowSizeT shortestFlowLength; //< this is the minimum of already sent length of all flows in the task
    std::unordered_map<flowIdT, GlobalFlow *> flowsMap;
    /**
     * Simple setter
     * @param shortestFlowLength
     */
    void setShortestFlowLength(flowSizeT shortestFlowLength);

    /**
     * Simple getter
     * @return ShortestFlowLength
     */
    flowSizeT getShortestFlowLength() const;

    /**
     * Update \ref shortestFlowLength of \ref Task when a flow finishes simulation every cycle.
     * @param byteSentSoFar simulated flow's already sent size
     */
    void updateTaskShorestFlowLength(flowSizeT byteSentSoFar);

    void flowFinish(flowIdT i);

    void addFlow(flowIdT fid, GlobalFlow *gf);
};

#endif /* GLOBAL_DATASTRUCTURES_TASK_H_ */
