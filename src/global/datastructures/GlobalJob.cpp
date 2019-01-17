/*
 * GlobalJob.cpp
 *
 *      Author: 
 */

#include "GlobalJob.h"

GlobalJob::GlobalJob(jobIdT id, timeT arrivalTime, int numSenders, vector<mapIdT>sendersVector, int numReceivers, vector<redIdT> receiversVector, vector<flowSizeT> flowSizes, unsigned int executionMode):id(id), arrivalTime(arrivalTime), numSenders(numSenders), sendersVector(sendersVector), numReceivers(numReceivers), receiversVector(receiversVector), flowSizes(flowSizes), executionMode(executionMode){
	started = false;
	bytesSent = 0;
	endTime = Utils::invalidValue;
	startTime = Utils::invalidValue;
    	probingEndTime = Utils::invalidValue;
	dead = false;
	currentQueue = Utils::invalidValue;
	setContentionFactor(Utils::invalidValue);
	setImpactFactor(Utils::invalidValue);
	activeProbeCount = 0;
	allOrNone = false;
	allOrNoneRate = 0;
	setEstimatedLength(0);
	initialWidth = numSenders*numReceivers;
	longestFlowLength = 0;
	shortestFlowLength = ULONG_MAX;
    	firstTransmissionStarted = false;
    	finalTransmissionStarted = false;
    	finalQueueNum = Utils::invalidValue;
	set_max_num_probe_flow(Utils::invalidValue);
    queueArrivalTime = Utils::invalidValue;
    queueTransmissionStartTime = Utils::invalidValue;
}

void GlobalJob::startJob(timeT time){
	started = true;
	startTime = time;
	makeTasks();
	makeFlows();
    	updateAverageFlowLength();
    	jobSizeT totalSizeToSet = 0;
    	for (auto flowItr = flowsVec.begin();flowItr!=flowsVec.end();flowItr++){
        	totalSizeToSet+=(*flowItr)->sizeRemaining;
	}
   	setTotalSize(totalSizeToSet);
}

vector<flowIdT> GlobalJob::simulate(timeT simTime,int orderP){
	vector<flowIdT> toReturn;
	toSimulate = false;

	for(auto flowToSend : flowsToSend){
		toSimulate = false;
		if(flowToSend->rate > 0){
			toSimulate = true;
		}
		if(toSimulate){
			flowSizeT dataSentByF = flowToSend->simulate(simTime);
			updateDataSentByFlowElseWhere(flowToSend->id, dataSentByF);
            		updateLongestAndShortestFlowLength(flowToSend->getBytesSentSoFar());
            		updateTaskShorestFlowLength(flowToSend);
			if(flowToSend->sizeRemaining <= 0){
				toReturn.push_back(flowToSend->id);
			}
		}
	}
	flowsToSend.clear();
	return toReturn;
}

void GlobalJob::updateDataSentByFlowElseWhere(flowIdT fid, flowSizeT delta){
	incrementSentData(delta);
	std::unordered_map<mapTaskIdT, MapTask*>::iterator mit = mapTasksMap.find(Utils::getMapTaskIdFromFlowIdAndJobId(fid,id));
	if(mit != mapTasksMap.end()){
		mit->second->incrementBytesShuffled(delta);
	}else{
		cout <<"unexpected: GlobalJob::updateDataSentByFlowElseWhere flow exists but corresponding mapTask not found mapTaskId: "<<mit->second->id<<"\n";
	}

	std::unordered_map<redTaskIdT, RedTask*>::iterator rit = redTasksMap.find(Utils::getRedTaskIdFromFlowIdAndJobId(fid,id));
	if(rit != redTasksMap.end()){
		rit->second->incrementBytesShuffled(delta);
	}else{
		cout <<"unexpected: GlobalJob::updateDataSentByFlowElseWhere flow exists but corresponding redTask not found redTaskId: "<<rit->second->id<<"\n";
	}
}

void GlobalJob::makeTasks(){
	for(int is = 0; is < numSenders; is++){
		MapTask* mt = new MapTask(id, sendersVector[is]);
		mapTaskIdT mid = mt->id;
		mapTasksVec.push_back(mt);
		mapTasksMap.insert(std::make_pair<mapTaskIdT,MapTask*>(std::move(mid), std::move(mt)));
	}
	for(int ir = 0; ir < numReceivers; ir++){
		RedTask* rt = new RedTask(id, receiversVector[ir]);
		redTaskIdT rid = rt->id;
		redTasksVec.push_back(rt);
		redTasksMap.insert(std::make_pair<redTaskIdT,RedTask*>(std::move(rid), std::move(rt)));
	}
}

void GlobalJob::makeFlows(){
	for(int is = 0; is < numSenders; is++){
		for(int ir = 0; ir < numReceivers; ir++){
			flowIdT fid = Utils::makeFlowId(id, sendersVector[is], receiversVector[ir]);
			flowSizeT size = flowSizes[ir*numSenders + is];	// flowSizes vector is all length for r1 then all for r2 ....so on
			if(size < Utils::minAllowedFlowLength){	// skipping flows of length less than a threshold.
				continue;
			}
			try{
				GlobalFlow* gf = new GlobalFlow(size, executionMode, fid);
                		gf->jobPtr = this;
				flowsVec.push_back(gf);
				flowsMap.insert(std::make_pair<flowIdT,GlobalFlow*>(std::move(fid), std::move(gf)));

                		if (Task * taskPtr = gf->getMapTaskPtr()){
                		    taskPtr->addFlow(fid,gf);
                		};
                		if (Task * taskPtr = gf->getRedTaskPtr()){
                		    taskPtr->addFlow(fid,gf);
                		};
			}catch(std::bad_alloc& ba){
				std::cout << "bad_alloc caught in GlobalJob::makeFlows() : "<<ba.what()<<" errno: "<<errno << "\n";
			}
		}
	}
	flowSizes.clear();
}

bool GlobalJob::removeFlow(flowIdT fid){
	std::vector<GlobalFlow*>::iterator begin, end;
	for(begin = flowsVec.begin(), end = flowsVec.end(); begin != end; begin++){
		if((*begin)->id == fid){
			GlobalFlow* gf = (*begin);
			flowsVec.erase(begin);
			flowsMap.erase(fid);
			break;
		}
	}
	return flowsVec.empty();
}

void GlobalJob::incrementSentData(jobSizeT dataSize) {
	bytesSent += dataSize;
}

jobSizeT GlobalJob::getBytesSent() {
	return bytesSent;
}

jobSizeT GlobalJob::getRemainingSize() {
	return (getTotalSize() - getBytesSent());
}

void GlobalJob::updateSizeAndFlowBytesSentOnFlowCompletion(flowIdT fid){	// this function should only be called when size is not being updated by any other means.
	GlobalFlow* gf = getFlowPtrByFid(fid);
	if(gf != NULL){
		incrementSentData(gf->getSizeInBytes());
		gf->updateFlowBytesSentOnFlowCompletion();
	}else{
		cout << "Unexpected error: NULL GlobalFlow pointer in GlobalJob::updateSizeAndFlowBytesSentOnFlowCompletion fid: "<< fid<<"\n";
	}
}

void GlobalJob::updateLongestAndShortestFlowLength(flowSizeT flowLen){
	if(flowLen>longestFlowLength){
		longestFlowLength = flowLen;
	}
    	if (flowLen<shortestFlowLength){
        	shortestFlowLength = flowLen;
    	}
}

void GlobalJob::jobDead(timeT time){
	endTime = time;
	dead = true;
}

timeT GlobalJob::getJCT(){
	return endTime - startTime;
}

void GlobalJob::setImpactFactor(impactFactorT factor){
	currentImpactFactor = factor;
}

impactFactorT GlobalJob::getImpactFactor(){
	return currentImpactFactor;
}

void GlobalJob::setContentionFactor(contentionFactorT factor){
	contentionFactor = factor;
}

contentionFactorT GlobalJob::getContentionFactor(){
	return contentionFactor;
}

bool GlobalJob::addProbeFlow(flowIdT fid){
//		cout <<"probe fid for job : "<<id<<" fid: "<<fid<<"\n";
	bool toReturn = true;
	GlobalFlow* fPtr = getFlowPtrByFid(fid);
    	//assert(fPtr);
	if(fPtr == NULL){
		toReturn = false;
	}
	else{
		probeFlowsVec.push_back(fPtr);
		probeFlowsMap.insert(std::make_pair<flowIdT,GlobalFlow*>(std::move(fid), std::move(fPtr)));
		fPtr->markProbe();
		activeProbeCount++;
	}
	return toReturn;
}

std::vector<GlobalFlow*>* GlobalJob::getProbeFlows(){
	return &probeFlowsVec;
}

bool GlobalJob::checkIfProbeFlowFinished(flowIdT fid){	// to be called on event of a flow finish. Returns true if the finished flow was probe.  This function assumes that flow with fid has finished and doesn't checks for it.
	bool toReturn = false;
	if(currentQueue == Utils::firstMainQIndexPHILAE){
		return toReturn;
	}
	GlobalFlow* gf = getProbeFlowPtrByFid(fid);
	if(gf != NULL){
		activeProbeCount --;
		toReturn = true;
	}
	return toReturn;
}

int GlobalJob::getActiveProbeFlowCount(){
	return activeProbeCount;
}
	void GlobalJob::set_max_num_probe_flow(unsigned int numProbeFlows){
		max_num_probe_flows = numProbeFlows;	
	}
	unsigned int GlobalJob::get_max_num_probe_flow(){
		return max_num_probe_flows;	
	}

bool GlobalJob::probeFinished(){
    if(activeProbeCount == 0 && probeFlowsVec.size()>0){
    //if(activeProbeCount == 0){
		return true;
	}else{
		return false;
	}
}	// can be called any time. Returns true iff probe flows are allocated and all are finished. In case if no probe flows are assigned or all are not finished returns true.

flowSizeT GlobalJob::getEstimatedLength(){
	if(probeFinished()){
		return estimatedLength;
	}else{
		//cout <<"unexpected: GlobalJob:getEstimatedLength called before estimation is over. jid: "<<id<<" activeProbeCount: "<<activeProbeCount<<" probeFlowsVec.size(): "<<probeFlowsVec.size()<<"\n";
		return Utils::invalidValue;
	}
}

void GlobalJob::setEstimatedLength(flowSizeT length){
	estimatedLength = length;
    //if (length!=0)
       // cerr << id << " " << estimatedLength<<endl;
}

void GlobalJob::estimateLength(){
	/*	Below is a code written by iterating over map replacing it with vector	*/

	std::vector<GlobalFlow*>::iterator probeIterator;
	jobSizeT estimate = 0;
	//jobSizeT alternateEstimate = 0;
	int numProbeFlows = probeFlowsVec.size();
	if(numProbeFlows == 0){
		Utils::utilError("LOGICAL ERROR: GlobalJob::estimateLength. Zero Probe Flows");
		return;
	}
	for(probeIterator = probeFlowsVec.begin(); probeIterator != probeFlowsVec.end(); ++probeIterator){
		estimate += ((*probeIterator)->getSizeInBytes()/numProbeFlows);
	}
	//alternateEstimate = getBytesSent()/((numSenders*numReceivers)-flowsVec.size());
	//cout <<id <<","<<probeFlowsVec.size() <<","<< ((numSenders*numReceivers)-flowsVec.size())<<","<<estimate<<","<<alternateEstimate<<","<<totalSize/(numSenders*numReceivers)<<","<<totalSize<<","<<numSenders<<","<<numReceivers<<","<<numSenders*numReceivers<<"\n";
	//setEstimatedLength(alternateEstimate);
	setEstimatedLength(estimate);
	return;
	/*	Above is a code written by iterating over map replacing it with vector	*/
}

bool GlobalJob::removePort(portIdT portNum, int portType){
	bool toReturn = false;
	bool toReturn1 = false;
	if(portType == Utils::daemonTypeReceiverPort){
		vector<redIdT>::iterator itP = receiversVector.begin();
		while(itP != receiversVector.end()){
			if(*itP == portNum){
				receiversVector.erase(itP);
				toReturn = true;
				break;
			}
			itP++;
		}

		vector<RedTask*>::iterator itT = redTasksVec.begin();
		redTaskIdT rid = Utils::getRedTaskIdFromPortIdAndJobId(portNum, id);
		while(itT != redTasksVec.end()){
			if((*itT)->id == rid){
				redTasksVec.erase(itT);
				toReturn1 = true;
				break;
			}
			itT++;
		}
		redTasksMap.erase(rid);
	}else if(portType == Utils::daemonTypeSenderPort){
		vector<mapIdT>::iterator itP = sendersVector.begin();
		while(itP != sendersVector.end()){
			if(*itP == portNum){
				sendersVector.erase(itP);
				toReturn = true;
				break;
			}
			itP++;
		}

		vector<MapTask*>::iterator itT = mapTasksVec.begin();
		mapTaskIdT mid = Utils::getMapTaskIdFromPortIdAndJobId(portNum, id);
		while(itT != mapTasksVec.end()){
			if((*itT)->id == mid){
				mapTasksVec.erase(itT);
				toReturn1 = true;
				break;
			}
			itT++;
		}
		mapTasksMap.erase(mid);
	}
	return toReturn && toReturn1;
}

/*	WARNING: Till flow pointers are stored in a vector it will be very inefficient to use this function where we want to loop over flows that will make complexity f^2*/
GlobalFlow* GlobalJob::getFlowPtrByFid(flowIdT fid){	//	Returns pointer to GlobalFlow if found else returns Null
	/*	Below is a code written by iterating over map replacing it with vector	*/
	GlobalFlow* toReturn = NULL;
	std::unordered_map<flowIdT,GlobalFlow*>::iterator flowIterator;	//	this step is important as we want to keep pointer to object in the map
	flowIterator = flowsMap.find(fid);
	if(flowIterator != flowsMap.end()){
		toReturn = flowIterator->second;
	}
	return toReturn;
	/*	Above is a code written by iterating over map replacing it with vector	*/
}

/*	WARNING: Till probeFlow pointers are stored in a vector it will be very inefficient to use this function where we want to loop over flows that will make complexity pf^2*/
GlobalFlow* GlobalJob::getProbeFlowPtrByFid(flowIdT fid){
	GlobalFlow* toReturn = NULL;
	std::unordered_map<flowIdT,GlobalFlow*>::iterator flowIterator;	//	this step is important as we want to keep pointer to object in the map
	flowIterator = probeFlowsMap.find(fid);
	if(flowIterator != probeFlowsMap.end()){
		toReturn = flowIterator->second;
	}
	if(toReturn != NULL){
		if(!(toReturn ->isProbe())){
			toReturn = NULL;
		}
	}
	return toReturn;
}

unsigned int GlobalJob::getMinActiveNumberOfTasks(){
	return Utils::minInt(getActiveNumberOfMappers(), getActiveNumberOfReducers());
}

unsigned int GlobalJob::getActiveNumberOfMappers(){
	return mapTasksVec.size();
}

unsigned int GlobalJob::getActiveNumberOfReducers(){
	return redTasksVec.size();
}

void GlobalJob::sortTasksBySize(){
	sortMapTasksBySize();
	sortRedTasksBySize();
}

void GlobalJob::sortMapTasksBySize(){
	stable_sort(mapTasksVec.begin(), mapTasksVec.end(),[](MapTask* m1, MapTask* m2)->bool{
		return m1->getBytesShuffled() < m2->getBytesShuffled();
	});
}

void GlobalJob::sortRedTasksBySize(){
	stable_sort(redTasksVec.begin(), redTasksVec.end(),[](RedTask* r1, RedTask* r2)->bool{
		return r1->getBytesShuffled() < r2->getBytesShuffled();
	});
}

mapIdT GlobalJob::getMapPortNumAtIndex(unsigned int index){
	if(mapTasksVec.size() > index){
//		cout <<"mapPortNumAtIndex: "<<index<<" portNum: "<<mapTasksVec[index]->hostPort<<"\n";
		return mapTasksVec[index]->hostPort;
	}else{
		Utils::utilError("Error in GlobalJob::getMapNumAtIndex");
		return 0;
	}
}

redIdT GlobalJob::getRedPortNumAtIndex(unsigned int index){
	if(redTasksVec.size() > index){
		return redTasksVec[index]->hostPort;
	}else{
		Utils::utilError("Error in GlobalJob::getRedNumAtIndex");
		return 0;
	}
}
void GlobalJob::reportTransmissionStatus(int cycle){
	vector<flowIdT> toReturn;
	std::vector<GlobalFlow*>::iterator begin, end;
	bool toSimulate = false;
	for(begin = flowsToSend.begin(), end = flowsToSend.end(); begin != end; begin++){
		//for(begin = flowsVec.begin(), end = flowsVec.end(); begin != end; begin++){
		toSimulate = false;
		if((*begin)->rate > 0){
			toSimulate = true;
		}
		if(1||toSimulate){
			if((*begin)->isProbe()){
				//cerr <<"Cycle: "<< cycle<<" sending Probe data: jobId " << id << " fid: "<<(*begin)->id<<" fRate: "<< (*begin)->rate <<" size remaining " <<(*begin)->sizeRemaining<<endl;
			}
			if((*begin)->sizeRemaining <= 0){
				//cout << "sizeRemaining <= 0"<<endl;
			}
		}
	}
}
void GlobalJob::addFlowToSend(GlobalFlow* gf){
	flowsToSend.push_back(gf);
}


flowSizeT GlobalJob::getLongestFlowLength() {
    return longestFlowLength;
}
flowSizeT GlobalJob::getShortestFlowLength() {
    return shortestFlowLength;
}

long GlobalJob::getProbingEndTime() const {
    return probingEndTime;
}

void GlobalJob::setProbingEndTime(timeT probingEndTime) {
    GlobalJob::probingEndTime = probingEndTime;
}

void GlobalJob::updateTaskShorestFlowLength(GlobalFlow * flowPtr) {
    flowSizeT sentBytes = flowPtr->getBytesSentSoFar();
    flowIdT fid = flowPtr->id;
    mapTaskIdT mapTaskId = Utils::getMapTaskIdFromFlowId(fid);
    redTaskIdT redTaskId = Utils::getRedTaskIdFromFlowId(fid);
    MapTask * mapTask = getMapTaskPtrFromMapTaskId(mapTaskId);
    RedTask * redTask = getRedTaskPtrFromRedTaskId(redTaskId);
    if (mapTask == NULL || redTask ==NULL){
        cerr << "Cannot find task for flow : " << flowPtr->id << endl;
    }else{
        mapTask->updateTaskShorestFlowLength(sentBytes);
        redTask->updateTaskShorestFlowLength(sentBytes);
    }
}

MapTask *GlobalJob::getMapTaskPtrFromMapTaskId(mapTaskIdT id) {
    if (mapTasksMap.find(id)==mapTasksMap.end()){
        return NULL;
    }else{
        return mapTasksMap.find(id)->second;
    }
}

RedTask *GlobalJob::getRedTaskPtrFromRedTaskId(redTaskIdT id) {
    if (redTasksMap.find(id)==redTasksMap.end()) {
        return NULL;
    }else {
        return redTasksMap.find(id)->second;
    }
}

void GlobalJob::updateAverageFlowLength() {
    flowSizeT toReturn = 0;
    for (auto flowItr = flowsVec.begin();flowItr != flowsVec.end();flowItr++){
        toReturn += (*flowItr)->sizeRemaining;
    }
    toReturn /= flowsVec.size();
    averageFlowLength = toReturn;
}

flowSizeT GlobalJob::getAverageFlowLength() const {
    return averageFlowLength;
}

jobSizeT GlobalJob::getTotalSize() const {
    return totalSize;
}

void GlobalJob::setTotalSize(jobSizeT size) {
    totalSize = size;
}

timeT GlobalJob::getEstimatedCompletionTime(rateT maxRate) {	// this function should be used only with PHILAE, or similar estimation based scheduling techniques
	if(probeFinished()){
		return (timeT)getEstimatedLength()/(timeT)maxRate;
	}else{
		cout <<"Unexpected error: GlobalJob::getEstimatedCompletionTime called before estimation is over. jid: "<<id<<" activeProbeCount: "<<activeProbeCount<<" probeFlowsVec.size(): "<<probeFlowsVec.size()<<"\n";
		return Utils::invalidValue;
	}
}

GlobalJob::~GlobalJob() {
	flowsVec.clear();
	receiversVector.clear();
	sendersVector.clear();

	std::vector< GlobalFlow*>::iterator begin, end;
	for(begin = probeFlowsVec.begin(), end = probeFlowsVec.end(); begin != end; begin++){
		if(!((GlobalFlow*)(*begin)->isProbe())){
			continue;//	deletion being done in flowFinish of Coordinator
		}		
		delete (GlobalFlow*)(*begin);
	}
	probeFlowsVec.clear();
	flowsMap.clear();
}
