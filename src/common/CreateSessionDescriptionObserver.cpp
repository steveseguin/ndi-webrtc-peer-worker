//
// Created by anba8005 on 12/20/18.
//

#include "CreateSessionDescriptionObserver.h"
#include <iostream>

CreateSessionDescriptionObserver::CreateSessionDescriptionObserver(shared_ptr<Signaling> signaling, string command,
                                                                   int64_t correlation) : signaling(signaling),
                                                                                          command(command),
                                                                                          correlation(correlation) {
}

void CreateSessionDescriptionObserver::OnSuccess(webrtc::SessionDescriptionInterface *desc) {
    // Serialize sdp
    std::string sdp;
    if (!desc->ToString(&sdp)) {
        signaling->replyError(command, "Failed to serialize sdp", correlation);
        return;
    }
    //
    json payload;
    payload["type"] = desc->type();
    payload["sdp"] = sdp;
    signaling->replyWithPayload(command, payload, correlation);
}

void CreateSessionDescriptionObserver::OnFailure(const string &error) {
    signaling->replyError(command, error, correlation);
}

CreateSessionDescriptionObserver *
CreateSessionDescriptionObserver::Create(shared_ptr<Signaling> signaling, string command, int64_t correlation) {
    return new rtc::RefCountedObject<CreateSessionDescriptionObserver>(signaling, command, correlation);
}