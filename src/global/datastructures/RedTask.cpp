/*
 * ReduceTask.cpp
 *
 *      Author: 
 */

#include "RedTask.h"

RedTask::RedTask(jobIdT parentJobId, portIdT portNum) : Task(parentJobId, portNum, Utils::taskTypeRed) {

}

RedTask::~RedTask() {
}

