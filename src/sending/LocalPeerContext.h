//
// Created by anba8005 on 12/20/18.
//

#ifndef GYVAITV_WEBRTC_LOCALPEERCONTEXT_H
#define GYVAITV_WEBRTC_LOCALPEERCONTEXT_H

#include "../common/PeerContext.h"

class LocalPeerContext : public PeerContext {
public:
    LocalPeerContext(shared_ptr<Signaling> signaling);

    virtual ~LocalPeerContext();

    void start() override;

    void end() override;

};


#endif //GYVAITV_WEBRTC_LOCALPEERCONTEXT_H