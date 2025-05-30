#include <iostream>
#include <portaudio.h>
#include <thread>
#include <chrono>
#include <vector>
#include <cmath>

#include "text_segment.h"
#define SAMPLE_RATE 44100
std::vector<float> currentSpeechBuffer;

static int recordCallback(const void *inputBuffer, void *outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo *timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void *userData)
{
    constexpr float SILENCE_THRESHOLD = 0.02f;
    constexpr int SILENCE_FRAMES_BEFORE_STOP = 10;

    static bool isSpeaking = false;
    static int silenceCounter = 0;

    if (!inputBuffer) {
        std::cout << "[Silence]" << std::endl;
        silenceCounter++;
        return paContinue;
    }

    const float *in = static_cast<const float *>(inputBuffer);
    bool voiceDetected = false;

    for (unsigned long i = 0; i < framesPerBuffer; ++i) {
        if (std::fabs(in[i]) > SILENCE_THRESHOLD) {
            voiceDetected = true;
            break;
        }
    }

    if (voiceDetected) {
        if (!isSpeaking) {
            std::cout << "Voice started!" << std::endl;
            isSpeaking = true;
        }
        currentSpeechBuffer.insert(currentSpeechBuffer.end(), in, in + framesPerBuffer);
        silenceCounter = 0;
    } else {
        std::cout << "[Silence]" << std::endl;
        silenceCounter++;
        if (isSpeaking && silenceCounter >= SILENCE_FRAMES_BEFORE_STOP) {
            std::cout << "Voice stopped. Segment complete." << std::endl;
            isSpeaking = false;
            std::cout << "Segment collected: " << currentSpeechBuffer.size() << " samples" << std::endl;
            // TODO: Send currentSpeechBuffer to Whisper.cpp.
            currentSpeechBuffer.clear();
            silenceCounter = 0;
        }
    }

    return paContinue;
}

int main()
{
    PaStream *stream;
    PaError err;

    err = Pa_Initialize();
    if( err != paNoError )
        printf(  "PortAudio error: %s\n", Pa_GetErrorText( err ) );

    auto inputDevice = Pa_GetDefaultInputDevice();
    if (inputDevice == paNoDevice)
    {
        std::cout << "No input device found" << std::endl;
        return 1;
    }
    Pa_OpenDefaultStream(&stream, 1, 0, paFloat32, SAMPLE_RATE, 256, recordCallback, nullptr);
    Pa_StartStream(stream);
    std::this_thread::sleep_for(std::chrono::seconds(5));
    Pa_StopStream(stream);
    Pa_CloseStream(stream);

    err = Pa_Terminate();
    if( err != paNoError )
        printf(  "PortAudio error: %s\n", Pa_GetErrorText( err ) );

    return 0;
}
