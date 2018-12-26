//
// Created by anba8005 on 12/15/18.
//

#ifndef GYVAITV_WEBRTC_PEERFACTORYCONTEXT_H
#define GYVAITV_WEBRTC_PEERFACTORYCONTEXT_H

#define USE_BUILTIN_SW_CODECS

#include "pc/peerconnectionfactory.h"

#include "BaseAudioDeviceModule.h"
#include "VideoDeviceModule.h"

#include "VideoCapturer.h"

class PeerFactoryContext {
public:
    PeerFactoryContext();

    BaseAudioDeviceModule *getADM();
    VideoDeviceModule *getVDM();

    rtc::scoped_refptr<webrtc::PeerConnectionInterface>
    createPeerConnection(webrtc::PeerConnectionObserver *observer);

    rtc::scoped_refptr<webrtc::AudioTrackInterface>
    createAudioTrack(cricket::AudioOptions options, const char *label = "audio");

    rtc::scoped_refptr<webrtc::VideoTrackInterface>
    createVideoTrack(std::unique_ptr<cricket::VideoCapturer> capturer, const char *label = "video");

    rtc::scoped_refptr<webrtc::VideoTrackInterface>
    createVideoTrack(const char *label = "video");


private:
    webrtc::PeerConnectionInterface::RTCConfiguration config;
    //
    rtc::scoped_refptr<BaseAudioDeviceModule> adm;
    rtc::scoped_refptr<VideoDeviceModule> vdm;
    //
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory;
    //
    std::unique_ptr<rtc::Thread> networkThread;
    std::unique_ptr<rtc::Thread> workerThread;
    //
    std::unique_ptr<rtc::NetworkManager> networkManager;
    std::unique_ptr<rtc::PacketSocketFactory> socketFactory;
    std::unique_ptr<cricket::PortAllocator> portAllocator;
};


#endif //GYVAITV_WEBRTC_PEERFACTORYCONTEXT_H
