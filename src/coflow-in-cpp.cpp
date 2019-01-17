//============================================================================
// Name        : coflow-in-c.cpp
//============================================================================

#include <iostream>
#include <cstdlib>
#include <string.h>

#include "global/coordinator/CoordinatorSim.h"
#include "utils/utility.hpp"

using namespace std;

int main(int argc, char* argv[]) {
	srand(22071992);
	int daemonType = atoi(argv[1]);
	if(daemonType == Utils::daemonTypeCoordinatorSim){
		//	Depricated. Might not work for some policies and may not parse some arguments correctly. Instead use config file.	
		std::string schedulingPolicy(argv[3]);
		std::string outputFileDiscriminator(argv[4]);
		int starvationFreedomPolicy = atoi(argv[5]);
		int probeNumSelection = atoi(argv[6]);
		int probeFlowSelection = atoi(argv[7]);
		int oneProbePerMapper = atoi(argv[8]);
		cout << "Creating coordinator for simulation for policy: " << schedulingPolicy << "\n";
		cout << "probeNumSelection:" << probeNumSelection << endl << "probeFlowSelection : "
		     << probeFlowSelection << endl << "restrictOnePilotFlowPerMapper : "
		     << oneProbePerMapper << endl << "starvationFreedomPolicy : " << starvationFreedomPolicy << endl;
		CoordinatorSim CoordinatorSim(argv[2], schedulingPolicy, outputFileDiscriminator, probeNumSelection,
		                              probeFlowSelection, oneProbePerMapper, starvationFreedomPolicy);
	} else if(daemonType == Utils::readConfig){
		// Read config from file
		std::string jsonConfigPath = argv[2];
		ifstream f(jsonConfigPath);
		nlohmann::json config;
		f >> config; // Parse config file
		CoordinatorSim CoordinatorSim(config);
	} else {
		cerr << "Unidentified Daemon" <<endl;
	}
	return 0;
}
