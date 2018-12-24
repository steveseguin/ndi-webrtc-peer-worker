//
// Created by anba8005 on 12/23/18.
//

#include "NDIReader.h"

#include <iostream>

const size_t NDI_FIND_TIMEOUT_SECONDS = 5;

NDIReader::NDIReader(std::string name, std::string ips) : running(false), name(name), ips(ips) {}

NDIReader::~NDIReader() {
    if (_pNDI_recv) {
        // destroy ndi receiver
        NDIlib_recv_destroy(_pNDI_recv);
    }
    NDIlib_destroy();
}

void NDIReader::open() {
    assert(!running.load());

    // Init
    if (!NDIlib_initialize())
        throw std::runtime_error("Cannot run NDI.");

    // Create the descriptor of the object to create
    NDIlib_find_create_t find_create;
    find_create.show_local_sources = true;
    find_create.p_groups = nullptr;
    find_create.p_extra_ips = ips.empty() ? nullptr : ips.c_str();

    // Create a finder
    NDIlib_find_instance_t pNDI_find = NDIlib_find_create_v2(&find_create);
    if (!pNDI_find)
        throw std::runtime_error("Cannot create NDI finder");

    // Wait until there is one source
    uint32_t no_sources = 0;
    const NDIlib_source_t *p_sources = nullptr;
    size_t retries = 0;
    while (!no_sources && retries < NDI_FIND_TIMEOUT_SECONDS) {
        std::cerr << "Looking for sources ..." << std::endl;
        NDIlib_find_wait_for_sources(pNDI_find, 1000/* One second */);
        p_sources = NDIlib_find_get_current_sources(pNDI_find, &no_sources);
        retries++;
    }

    std::cerr << "Network sources (" << no_sources << " found)" << std::endl;
    bool found = false;
    NDIlib_source_t _ndi_source;
    for (uint32_t i = 0; i < no_sources; i++) {
        std::cerr << i + 1 << " " << p_sources[i].p_ndi_name << std::endl;
        if (name == std::string(p_sources[i].p_ndi_name)) {
            found = true;
            _ndi_source = p_sources[i];
        }
    }

    // check if desired source available
    if (!found) {
        throw std::runtime_error("Source " + name + " not found");
    }

    // create NDI receiver descriptor
    NDIlib_recv_create_v3_t recv_create_desc;
    recv_create_desc.source_to_connect_to = _ndi_source;
    recv_create_desc.bandwidth = NDIlib_recv_bandwidth_highest;
    recv_create_desc.color_format = NDIlib_recv_color_format_UYVY_RGBA;
    recv_create_desc.p_ndi_recv_name = "NDIReader";
    recv_create_desc.allow_video_fields = false;

    // create NDI receiver
    _pNDI_recv = NDIlib_recv_create_v3(&recv_create_desc);
    if (!_pNDI_recv)
        throw std::runtime_error("Error creating NDI receiver");

    // cleanup
    NDIlib_find_destroy(pNDI_find);
}

void NDIReader::start(VideoDeviceModule *vdm, BaseAudioDeviceModule *adm) {
    assert(!running.load());

    //
    this->vdm = vdm;
    this->adm = adm;

    // start worker thread
    running = true;
    runner = std::thread(&NDIReader::run, this);
}

void NDIReader::stop() {
    // stop worker thread
    running = false;
    runner.join();
}

void NDIReader::run() {
    while (running.load()) {
        NDIlib_video_frame_v2_t video_frame;
        NDIlib_audio_frame_v2_t audio_frame;
        NDIlib_metadata_frame_t metadata_frame;

        switch (NDIlib_recv_capture_v2(_pNDI_recv, &video_frame, &audio_frame, &metadata_frame, 1000)) {
            // No data
            case NDIlib_frame_type_none:
                std::cerr << "No data received" << std::endl;
                break;

                // Video data
            case NDIlib_frame_type_video:
                // send
                if (vdm)
                    vdm->feedFrame(video_frame.xres, video_frame.yres, video_frame.p_data,
                                   video_frame.line_stride_in_bytes);
                // free
                NDIlib_recv_free_video_v2(_pNDI_recv, &video_frame);
                break;

                // Audio data
            case NDIlib_frame_type_audio: {
                // Allocate enough space for 16bpp interleaved buffer
                NDIlib_audio_frame_interleaved_16s_t audio_frame_16bpp_interleaved;
                audio_frame_16bpp_interleaved.reference_level = 16;     // We are going to have 16dB of headroom
                audio_frame_16bpp_interleaved.p_data = new int16_t[audio_frame.no_samples * audio_frame.no_channels];
                audio_frame_16bpp_interleaved.sample_rate = audio_frame.sample_rate;
                audio_frame_16bpp_interleaved.no_channels = audio_frame.no_samples;

                // Convert it
                NDIlib_util_audio_to_interleaved_16s_v2(&audio_frame, &audio_frame_16bpp_interleaved);
                // Free the original buffer
                NDIlib_recv_free_audio_v2(_pNDI_recv, &audio_frame);

                // send
                if (adm)
                    adm->feedRecorderData(audio_frame_16bpp_interleaved.p_data,
                                          audio_frame_16bpp_interleaved.no_samples);
                // Free the interleaved audio data
                delete[] audio_frame_16bpp_interleaved.p_data;
            }
                break;

                // Meta data
            case NDIlib_frame_type_metadata:
                NDIlib_recv_free_metadata(_pNDI_recv, &metadata_frame);
                break;

                // There is a status change on the receiver (e.g. new web interface)
            case NDIlib_frame_type_status_change:
                // std::cerr << "Receiver connection status changed." << std::endl;
                break;

                // Everything else
            default:
                break;
        }
    }
}