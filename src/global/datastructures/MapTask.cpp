/*
 * MapTask.cpp
 *
 *      Author: 
 */

#include "MapTask.h"

MapTask::MapTask(jobIdT parentJobId, portIdT portNum) : Task(parentJobId, portNum, Utils::taskTypeMap) {

}

MapTask::~MapTask() {
}

