#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/ThereminEngine.h"
#include "model/CurveModel.h"
#include "input/Gamepad.h"

namespace at
{
enum class Mode { Auto = 0, Live, Midi };
enum PitchSource { SrcStick = 0, SrcStickGlide, SrcTouchpad, SrcTilt };
enum VolumeSource { VolR2 = 0, VolL2, VolLeftStick, VolTouchY, VolFull };

struct RecPoint { double beat; float pitch; float vol; };

class Processor : public juce::AudioProcessor
{
public:
    Processor();
    ~Processor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Auto Theremin"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // ---- pattern (edited on the message thread)
    Pattern& getPattern() { return uiPattern; }
    void patternChanged();                     // call after every edit
    std::atomic<int> patternReplacedCount { 0 };   // editor refreshes when this changes

    // ---- shared with the editor
    std::atomic<double> displayBeat { 0 };
    std::atomic<bool> displayPlaying { false };
    std::atomic<float> displayPitch { 60 }, displayVol { 0 };
    std::atomic<float> liveP01 { 0.5f }, liveV01 { 0 };
    std::atomic<int> hostQuarterPerBar { 4 };
    std::atomic<double> currentBpm { 120 };
    std::atomic<bool> internalPlay { false };
    std::atomic<bool> recordArmed { false };
    std::atomic<bool> auditionOn { false };
    std::atomic<float> auditionPitch { 60 };
    std::atomic<bool> fieldActive { false };
    std::atomic<float> fieldX { 0.5f }, fieldY { 0.5f };
    std::atomic<int> liveOctaveShift { 0 };

    int readRecorded (std::vector<RecPoint>& out);
    int readScope (float* dest, int maxSamples);

    GamepadService& gamepad() { return *pad; }
    Mode getMode() const { return (Mode) (int) modeParam->load(); }
    double patternLengthBeats() const;

    static juce::StringArray echoDivisionNames();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void loadFactory (int index);
    EngineParams readParams (double bpm) const;
    void handleMidi (const juce::MidiMessage&);
    void handleLiveButtons (const GamepadSnapshot&);

    ThereminEngine engine;
    juce::SharedResourcePointer<GamepadService> pad;

    Pattern uiPattern, pendingPattern, audioPattern;
    juce::SpinLock patternLock;
    std::atomic<bool> patternDirty { true };
    int hintP = 0, hintV = 0;

    std::vector<float> pitchBuf, volBuf, outL, outR;
    double sr = 48000, internalPpq = 0;
    bool wasInternal = false;

    // live state
    float liveRaw = 0.5f, liveV = 0, prevBlockP = 60, prevBlockV = 0;
    uint32_t lastPress[BtnCount] {};
    bool pressInit = false;

    // midi state
    std::vector<int> heldNotes;
    float midiVel = 0.8f, midiBend = 0, modWheel = 0, expression = 1;
    int midiNote = 60;

    // recording + scope fifos
    juce::AbstractFifo recFifo { 16384 };
    std::vector<RecPoint> recStore = std::vector<RecPoint> (16384);
    int recCounter = 0;
    juce::AbstractFifo scopeFifo { 16384 };
    std::vector<float> scopeStore = std::vector<float> (16384);
    std::vector<float> scopeTmp;

    int currentProgram = 0;

    std::atomic<float>* modeParam = nullptr;
    std::atomic<float>* p[40] {};
    enum Pid { pGlide, pCorr, pScale, pKey, pVibD, pVibR, pSmart, pTimbre, pDetect, pWarm, pForm, pBright,
               pResp, pEchoSync, pEchoDiv, pEchoTime, pEchoFb, pEchoMix, pVerbSize, pVerbDamp, pVerbMix,
               pOut, pPitchSrc, pVolSrc, pLow, pOct, pInvert, pBendRange, pCount };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};
} // namespace at
