/*
 * MapTask.h
 *
 *      Author: 
 */

#ifndef GLOBAL_DATASTRUCTURES_MAPTASK_H_
#define GLOBAL_DATASTRUCTURES_MAPTASK_H_

#include "Task.h"

class MapTask : public Task {
public:
    MapTask(jobIdT parentJobId, portIdT portNum);

    virtual ~MapTask();

};

#endif /* GLOBAL_DATASTRUCTURES_MAPTASK_H_ */
