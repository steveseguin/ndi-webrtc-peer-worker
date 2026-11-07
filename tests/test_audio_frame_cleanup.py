#!/usr/bin/env python3
"""Compile the production audio case and extractor against small NDI API stubs.

Run: python3 tests/test_audio_frame_cleanup.py
Requires Python 3 and a C++14 compiler with AddressSanitizer (CXX, default c++).
This checks ownership, not NDI/WebRTC integration. No SDK or CMake is needed.
If ptrace prevents LeakSanitizer from running, set ASAN_OPTIONS=detect_leaks=0
(the allocation/deallocation mismatch check remains enabled).
"""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / "src/NDIReader.cpp").read_text()
# Extract verbatim production code, rather than maintaining a copy of the fix.
audio_case = source.split("case NDIlib_frame_type_audio:", 1)[1].split("break;", 1)[0]
start = source.index("NDIlib_audio_frame_v2_t NDIReader::extractAudioFrame(")
end = source.index("NDIReader::Configuration::Configuration", start)
extractor = source[start:end]

harness = r'''
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

struct NDIlib_audio_frame_v2_t {
    int sample_rate, no_channels, no_samples;
    int64_t timecode;
    float *p_data;
    int channel_stride_in_bytes;
};
struct NDIlib_audio_frame_interleaved_16s_t {
    int sample_rate, no_channels, no_samples, reference_level;
    int16_t *p_data;
};
struct AudioSink {
    int calls = 0;
    void feedRecorderData(int16_t *data, int samples) {
        assert(samples == 3);
        assert(data[0] == 123);
        ++calls;
    }
};
int frees = 0;
int conversions = 0;
float *original;
int expectedOffset;
bool expectCopy;
std::vector<uint8_t> expectedBytes;

void NDIlib_recv_free_audio_v2(void *, NDIlib_audio_frame_v2_t *frame) {
    assert(frame->p_data == original);
    delete[] frame->p_data;
    ++frees;
}
void NDIlib_util_audio_to_interleaved_16s_v2(
        const NDIlib_audio_frame_v2_t *frame,
        NDIlib_audio_frame_interleaved_16s_t *out) {
    assert(frame->no_channels == 2);
    assert(frame->no_samples == 3);
    assert(frame->sample_rate == 48000);
    assert(frame->timecode == 987654);
    assert((frame->p_data != original) == expectCopy);
    const auto *bytes = reinterpret_cast<const uint8_t *>(frame->p_data);
    assert(std::memcmp(bytes, expectedBytes.data(), expectedBytes.size()) == 0);
    assert(frame->p_data[0] == expectedOffset * 10.0f);
    out->no_samples = frame->no_samples;
    out->no_channels = frame->no_channels;
    out->p_data[0] = 123;
    ++conversions;
}
class NDIReader {
public:
    NDIlib_audio_frame_v2_t extractAudioFrame(NDIlib_audio_frame_v2_t, int, int);
    void process(NDIlib_audio_frame_v2_t audio_frame, int channelOffset, AudioSink *adm) {
        void *_pNDI_recv = nullptr;
        int numChannels = 2;
        NDIlib_audio_frame_interleaved_16s_t audio_frame_16bpp_interleaved;
        NDIlib_audio_frame_v2_t audio_frame_extracted;
        // BEGIN verbatim production audio case
@AUDIO_CASE@
        // END verbatim production audio case
    }
};
@EXTRACTOR@

void check(int channels, int offset, bool sinkPresent) {
    // Four floats per channel: three samples and one padding element.
    // Verify that extraction respects channel_stride_in_bytes as well.
    original = new float[channels * 4];
    for (int c = 0; c < channels; ++c)
        for (int s = 0; s < 4; ++s)
            original[c * 4 + s] = float(c * 10 + s);
    expectedOffset = offset;
    expectCopy = channels != 2 || offset != 0;
    auto *begin = reinterpret_cast<uint8_t *>(original + offset * 4);
    expectedBytes.assign(begin, begin + 2 * 4 * sizeof(float));
    NDIlib_audio_frame_v2_t frame{48000, channels, 3, 987654, original,
                                int(4 * sizeof(float))};
    AudioSink sink;
    NDIReader reader;
    reader.process(frame, offset, sinkPresent ? &sink : nullptr);
    assert(sink.calls == (sinkPresent ? 1 : 0));
}
int main() {
    for (int i = 0; i < 100; ++i) {
        check(2, 0, true);  // Borrow SDK buffer; never delete it a second time.
        check(8, 0, true);  // Default stereo subset from multichannel source.
        check(8, 4, true);  // Nonzero channel offset.
        check(8, 6, false); // Last pair, with no audio sink attached.
    }
    assert(frees == 400);
    assert(conversions == 400);
    std::cout << "PASS: 400 audio frames (borrowed and owned buffers)\n";
}
'''.replace("@AUDIO_CASE@", audio_case).replace("@EXTRACTOR@", extractor)

with tempfile.TemporaryDirectory(prefix="ndi-audio-cleanup-") as directory:
    cpp = Path(directory) / "audio_cleanup.cpp"
    executable = Path(directory) / "audio_cleanup"
    cpp.write_text(harness)
    subprocess.run(shlex.split(os.environ.get("CXX", "c++")) + [
        "-std=c++14", "-g", "-O1", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
        str(cpp), "-o", str(executable),
    ], check=True)
    env = os.environ.copy()
    env["ASAN_OPTIONS"] = env.get("ASAN_OPTIONS", "detect_leaks=1") + ":alloc_dealloc_mismatch=1"
    env["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
    subprocess.run([str(executable)], env=env, check=True)
