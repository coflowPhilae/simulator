#include "GlobalReceiverPort.h"
GlobalReceiverPort::GlobalReceiverPort(portIdT portNum) : GlobalPort(portNum, Utils::daemonTypeReceiverPort) {

}

GlobalReceiverPort::~GlobalReceiverPort() {
}
