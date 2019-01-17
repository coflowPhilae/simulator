#ifndef GLOBALRECEIVERPORT_H_
#define GLOBALRECEIVERPORT_H_

#include "GlobalPort.h"

class GlobalReceiverPort : public GlobalPort {
public:
    GlobalReceiverPort(portIdT portNum);

    virtual ~GlobalReceiverPort();
};

#endif /* GLOBALRECEIVERPORT_H_ */
