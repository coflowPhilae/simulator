/*
 * CoordinatorSim.h
 *
 *      Author: 
 */

#ifndef GLOBAL_COORDINATOR_COORDINATORSIM_H_
#define GLOBAL_COORDINATOR_COORDINATORSIM_H_

#include "Coordinator.h"
#include "../../libs/json.hpp"

class CoordinatorSim : public Coordinator {
public:
    CoordinatorSim(char *traceFilePath, string schedulingPolicyName, string outputFileDiscriminator,
                       int probeNumSelection, int probeFlowSelection, int oneProbePerMapper,
                       int isStarvationFreedom);
    CoordinatorSim(nlohmann::json config);

    void run();

    void simulatePorts(unsigned long int simTime);

    bool makePorts();

    void setTime();

    unsigned long int getTimeMillis();

    void incrementTimeBySimQuanta();

    void incrementTimeBy(unsigned long int incr);

    void endEmulation();

    void printPortJobs(portIdT portNum, int portType);

    virtual ~CoordinatorSim();

    unsigned long int getSimTime();

    void setSimTime(unsigned long int simTimeNew);

    void calculateSimTime();

    unsigned long int currentTimeMillis;
    jobSizeT dataSentLast;
    unsigned long int simTime;    // this is for one simulation

};

#endif /* GLOBAL_COORDINATOR_COORDINATORSIM_H_ */
