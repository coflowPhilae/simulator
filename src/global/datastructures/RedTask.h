/*
 * ReduceTask.h
 *
 *      Author: 
 */

#ifndef GLOBAL_DATASTRUCTURES_REDTASK_H_
#define GLOBAL_DATASTRUCTURES_REDTASK_H_

#include "Task.h"

class RedTask : public Task {
public:
    RedTask(jobIdT parentJobId, portIdT portNum);

    virtual ~RedTask();
};

#endif /* GLOBAL_DATASTRUCTURES_REDTASK_H_ */
