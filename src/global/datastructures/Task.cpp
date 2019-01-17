/*
 * Task.cpp
 *
 *      Author: 
 */

#include "Task.h"

Task::Task(jobIdT parentJob, portIdT portNum, int taskT) {
    assert(taskT == Utils::taskTypeMap || taskT == Utils::taskTypeRed);
    id = Utils::makeTaskId(parentJob, portNum, taskT);
    parentJobId = parentJob;
    hostPort = portNum;
    taskType = taskT;
    shortestFlowLength = ULONG_MAX;
    bytesShuffled = 0;
//	cout <<"HostPort "<<hostPort<<" ID: "<<id<<" taskType: "<<taskType<<"\n";
}

taskSizeT Task::getBytesShuffled() {
    return bytesShuffled;
}

void Task::incrementBytesShuffled(taskSizeT delta) {
    bytesShuffled += delta;
}

Task::~Task() {
}

flowSizeT Task::getShortestFlowLength() const {
    return shortestFlowLength;
}

void Task::setShortestFlowLength(flowSizeT shortestFlowLength) {
    Task::shortestFlowLength = shortestFlowLength;
}

void Task::updateTaskShorestFlowLength(flowSizeT length) {
    if (length < getShortestFlowLength()) {
        setShortestFlowLength(length);
    }
}

void Task::flowFinish(flowIdT fid) {
    flowsMap.erase(fid);
}

void Task::addFlow(flowIdT fid, GlobalFlow *gf) {
    flowsMap.insert({fid,gf});
}

