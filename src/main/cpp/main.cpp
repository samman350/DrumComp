#include <raymob.h>
#include "CircularBuffer.h"
#include "Buttons.h"
#include <vector>
#include <cmath>
#include <android/log.h>

#define BUFFER_SIZE 1024
#define SAMPLING_FREQ 48000

/// global variables ###########################################################################
const int Nx = 4; const int Ny = 4; // how many samples n stuff
const int N_Collections = 6; //
const int totalSamples = Nx*Ny;
const int totalMonstersPerSample = 96000; // 2 seconds of audio allowed per sample
const float pentatone[] = {0.0f, 3.0f, 5.0f ,7.0f, 10.0f,
                           12.0f, 15.0f, 17.0f,19.0f, 22.0f,
                           24.0f, 27.0f, 29.0f, 31.0f, 34.0f};

float *ptrBuffer = new float[totalMonstersPerSample]; // pointer to first element of buffer
int * ptrBufferCounter = new int; // pointer to counter that communicates from buffer to front end

int * ptrPlayBuffCntr = new int; // pointer to Play mode counter that communicates from buffer to front end
int * ptrIntervalTime = new int; // determines speed of play in playmode
int * ptrPlayBuffSize = new int; // pointer to buffer size that communicates from buffer to front end
int * ptrPlayingSample = new int; // pointer to number that says which sample is playing
int * ptrSelectedCollection = new int; // pointer to number that says which sample is playing
int * ptrSelectedSample = new int; // pointer to number that says which sample is playing
float * ptrCheckState = new float; // debug

// improvised numpad functionality

int * ptrNumberCtr = new int;
int NumberS[5] = {0,0,0,0,0}; // array version of number that has been input by user
int * ptrNumber = new int; // number that has been input by the user
bool confirmedNumpad = false;

int contract(const int *arr, int size) {
    int result = 0;
    for (int i = 0; i < size; i++) {
        result = result * 10 + arr[i];
    }
    return result;
} // this function will bring ptrNumberS to ptrNumber, i.e. input Numbers = {0, 1, 2, 3} becomes Number = 123;

float saw(float x){
    return x - floor(x) - 0.5f; // sawtooth function
}

float triang(float x){
    return pow(-1,floor(2*x)) * (2*x - floor(2*x) - 0.5);
}


char * textNumpad = new char[20]; // text to accompany the numpad

int EnvState = 0; // deze state zegt waar we zitten in het envelope preces, attack (0) of tail (1)

/////// index en float buffers ###########################################################

int (*indices)[totalSamples] = new int[N_Collections][totalSamples];
float * synth_buffer = new float[totalMonstersPerSample]; // de buffer die bewerkt wordt in synth modus
CircularBuffer<float> ringbuff = CircularBuffer<float>(50*BUFFER_SIZE); // buffer for play mode, on stack

float*** alloc3D(int a, int b, int c) {
    float*** arr = new float**[a];
    arr[0] = new float*[a * b];
    arr[0][0] = new float[a * b * c];

    for (int i = 0; i < a; ++i) {
        arr[i] = arr[0] + i * b;
        for (int j = 0; j < b; ++j) {
            arr[i][j] = arr[0][0] + (i * b + j) * c;
        }
    }

    return arr;
}

float*** samples = alloc3D(N_Collections, totalSamples, totalMonstersPerSample);

void free3D(float*** arr) {
    delete[] arr[0][0];
    delete[] arr[0];
    delete[] arr;
}

///// INITIALIZE BUFFERS ##################################

void InitializeSamples(){
    float frequency = 440.0f;
    for (int i = 0; i < N_Collections; ++i) {
        for (int j = 0; j < totalSamples; ++j) {
            for (int k = 0; k < totalMonstersPerSample; ++k) {

                // ERASER op 16e button
                if (j == 15){
                    samples[i][j][k] = 0.0f; // de eraser

                // nette tonen op alles behalve 16, en lagen 0 tm 2:
                }else if (j < 15 && i < N_Collections - 2){
                    float amplitude = 1.0f;
                    float attack = 0.005f;
                    float tail = 20000.0f;

                    if (k <= (int)(amplitude/attack)){
                        float f = attack * k;

                        samples[i][j][k] = 0.3f*f*(sin(pow(2.0f,pentatone[j]/12) * k * 2 * M_PI * (frequency / 48000.0f)) +
                                0.2*sin(pow(2.0,pentatone[j]/12) * k * 2 * M_PI * (2.0f*frequency / 48000.0f)));

                    } else if (k > (int)(amplitude/attack) && k < (int)(amplitude/attack) + tail){
                        float g = (-(amplitude/tail)*k + pow(amplitude,2.0f)/(tail*attack) + amplitude);

                        samples[i][j][k] = 0.3f*g*(sin(pow(2.0f,pentatone[j]/12) * k * 2 * M_PI * (frequency / 48000.0f)) +
                                0.2*sin(pow(2.0,pentatone[j]/12) * k * 2 * M_PI * (2.0f*frequency / 48000.0f)));

                    } else {samples[i][j][k] = 0.0f;}

                }else if (j < 15 && i > N_Collections - 3){

                    if (j == 0){ // Kick drum
                        float amplitude = 1.0f;
                        float attack = 0.002f;
                        float tail = 5000.0f;
                        float drumfreq = 80.0f;

                        if (k <= (int)(amplitude/attack)){

                            float f = attack * k;

                            samples[i][j][k] = 2.f*f*sin(pow(2.0f,pentatone[j]/12) * k * 2 * M_PI * (drumfreq / 48000.0f));

                        } else if (k > (int)(amplitude/attack) && k < (int)(amplitude/attack) + tail){

                            float g = (-(amplitude/tail)*k + pow(amplitude,2.0f)/(tail*attack) + amplitude);

                            samples[i][j][k] = 2.f*g*sin(pow(2.0f,pentatone[j]/12) * k * 2 * M_PI * (drumfreq / 48000.0f));

                        } else {samples[i][j][k] = 0.0f;}

                    } else if (j==1) { // Low Snare
                        samples[i][j][k] = 1.0f * sin(k * 2.0f * M_PI * (100.0f / 48000.0f)) *
                                           exp(-0.001f * k) +
                                           0.7f * sin(k * 2.0f * M_PI * (165.0f / 48000.0f)) *
                                           exp(-0.05f * k) +
                                           0.5f * sin(k * 2.0f * M_PI * (260.0f / 48000.0f)) *
                                           exp(-0.02f * k) +
                                           0.4f * sin(k * 2.0f * M_PI * (415.0f / 48000.0f)) *
                                           exp(-0.01f * k) +
                                           0.4f * sin(k * 2.0f * M_PI * (650.0f / 48000.0f)) *
                                           exp(-0.005f * k);
                    } else if (j==2) { // Snare
                        samples[i][j][k] = 1.0f * sin(k * 2.0f * M_PI * (200.0f / 48000.0f)) *
                                           exp(-0.001f * k) +
                                           0.7f * sin(k * 2.0f * M_PI * (330.0f / 48000.0f)) *
                                           exp(-0.05f * k) +
                                           0.5f * sin(k * 2.0f * M_PI * (520.0f / 48000.0f)) *
                                           exp(-0.02f * k) +
                                           0.4f * sin(k * 2.0f * M_PI * (830.0f / 48000.0f)) *
                                           exp(-0.01f * k) +
                                           0.4f * sin(k * 2.0f * M_PI * (1300.0f / 48000.0f)) *
                                           exp(-0.005f * k);
                    } else if (j==3) { // High Snare
                        samples[i][j][k] = 1.0f * sin(k * 2.0f * M_PI * (400.0f / 48000.0f)) *
                                           exp(-0.001f * k) +
                                           0.7f * sin(k * 2.0f * M_PI * (660.0f / 48000.0f)) *
                                           exp(-0.05f * k) +
                                           0.5f * sin(k * 2.0f * M_PI * (1040.0f / 48000.0f)) *
                                           exp(-0.02f * k) +
                                           0.4f * sin(k * 2.0f * M_PI * (1660.0f / 48000.0f)) *
                                           exp(-0.01f * k) +
                                           0.4f * sin(k * 2.0f * M_PI * (2600.0f / 48000.0f)) *
                                           exp(-0.005f * k);
                    } else if (j==4) { // High hat
                        samples[i][j][k] = 0.5f*(1.0f * sin(k * 2.0f * M_PI * (3200.0f / 48000.0f))+
                                           0.8f * sin(k * 2.0f * M_PI * (3655.0f / 48000.0f)) +
                                           0.6f * sin(k * 2.0f * M_PI * (4080.0f / 48000.0f)) +
                                           0.5f * sin(k * 2.0f * M_PI * (4725.0f / 48000.0f)) +
                                           0.4f * sin(k * 2.0f * M_PI * (5265.0f / 48000.0f)) +
                                           0.4f * sin(k * 2.0f * M_PI * (6110.0f / 48000.0f))) * exp(-0.001f * k);
                    }
                    //} else if (j==3){ // High hat

                    } else  { samples[i][j][k] = 0.0f; // de eraser }
                }
            }
        }
    }
}

// initialize the samples with sine waves of different frequencies, that fall off with 1/t^0.3 :
void InitializeIndices(){
    for (int i = 0; i < N_Collections; ++i){
        for (int j = 0; j < totalSamples; ++j) {
            //if (i == 0)
            //    indices[i][j] = j;
            //else
            //    indices[i][j] = (j+2) % totalSamples; // the 0.0f reference
            indices[i][j] = 15; // initialize stilte
        }
    }
}

void InitializeSynthBuffer(){
    for (int i = 0; i < totalMonstersPerSample; ++i){
            synth_buffer[i] = 0.0f;
    }
}

/////// SETUP AUDIO BACKEND COMM #############################################

typedef enum{
    PLAY_SAMPLE,
    PLAY_SEQUENCE,
    PLAY_SYNTH
}StreamMode;

StreamMode mode = PLAY_SAMPLE;

const int bufferExcessCoef = 50; // how much margin in the ring buffer

static void AudioCB(void* buffer, unsigned int frames)
{
    short *d = (short *)buffer; // pointer to first element of buffer
    switch(mode){
        case PLAY_SAMPLE:
            for (unsigned int i = 0; i < frames; ++i) {
                if ((*ptrBufferCounter) < totalMonstersPerSample) {
                    d[i] = (short) (15000.0f * ptrBuffer[(*ptrBufferCounter)]);
                }
                else {
                    d[i] = (short)0.0f;
                }
                (*ptrBufferCounter)++;
            }
            break;
        case PLAY_SEQUENCE:
            for (unsigned int i = 0; i < frames; ++i) {
                d[i] = (short)(15000.0f*ringbuff.get());
            }
            break;
        case PLAY_SYNTH:
            for (unsigned int i = 0; i < frames; ++i) {
                if ((*ptrBufferCounter) < totalMonstersPerSample) {
                    d[i] = (short) (15000.0f * synth_buffer[(*ptrBufferCounter)]);
                }
                else {
                    d[i] = (short)0.0f;
                }
                (*ptrBufferCounter)++;
            }
            break;
    }
}

void FillPlayBuffer() {
    float sum = 0;
    for (int k = 0; k < N_Collections; ++k) {
        for (int j = 0; j < totalSamples; ++j) { // scan door alle samples en collecties voor de juiste monsters
            int BufferIndex = (*ptrPlayBuffCntr) - (*ptrIntervalTime) * j;
            if ((BufferIndex >= 0) && (BufferIndex < totalMonstersPerSample) ||
                    (((*ptrPlayBuffCntr) - (*ptrIntervalTime) * totalSamples) >= 0 &&
                     ((*ptrPlayBuffCntr) - (*ptrIntervalTime) * totalSamples) < totalMonstersPerSample)) {
                sum += samples[k][indices[k][j]][BufferIndex];
            }
        }
    }
    ringbuff.put(sum); // this is the part where you put the slice in the buffer
    (*ptrPlayBuffCntr)++;
    (*ptrPlayBuffCntr) = (*ptrPlayBuffCntr) % (*ptrPlayBuffSize); // reset play index if end of buffer is reached
}

//////////// Definieer the user interface modii ###########################

typedef enum{
    NORMAL,
    WRITE_MODE,
    SYNTH_MODE,
    KEYBOARD_MODE
}UI_Mode;

UI_Mode uimode = NORMAL;

//////// BUTTON FUNCTIONS ################################################################################

// Normal

void DrumFunction(int label, AudioStream& stream){
    mode = PLAY_SAMPLE;
    (*ptrBufferCounter) = 0;
    ptrBuffer = samples[*ptrSelectedCollection][label];
    PlayAudioStream(stream);
    DrawText(TextFormat("I will Drum"), 100, 100, 100, RED);
}

void PlayFunction(int label, AudioStream& stream){
    mode = PLAY_SEQUENCE;
    *ptrPlayBuffCntr = 0;
    PlayAudioStream(stream);
    DrawText(TextFormat("I will Play"), 100, 100, 100, RED);
}

void StopFunction(int label, AudioStream& stream){
    DrawText(TextFormat("I will Stop"), 100, 100, 100, RED);
    StopAudioStream(stream);
    mode = PLAY_SAMPLE;
}

void WriteFunction(int label, AudioStream& stream){
    // go to write mode

    mode = PLAY_SAMPLE;

    switch(uimode){
        case NORMAL:
            uimode = WRITE_MODE;
            break;
        case SYNTH_MODE:
            uimode = WRITE_MODE;
            break;
        case WRITE_MODE:
            uimode = NORMAL;
            break;
    }
    DrawText(TextFormat("I toggle write mode on/off"), 100, 100, 100, RED);
}

void SynthFunction(int label, AudioStream& stream){
    // go to synth mode

    mode = PLAY_SYNTH;

    switch(uimode){
        case NORMAL:
            uimode = SYNTH_MODE;
            break;
        case SYNTH_MODE:
            uimode = NORMAL;
            break;
        case WRITE_MODE:
            uimode = SYNTH_MODE;
            break;
    }
    DrawText(TextFormat("I toggle write mode on/off"), 100, 100, 100, RED);
}

void CollectionPlusFunction(int label, AudioStream& stream){
    *ptrSelectedCollection = (*ptrSelectedCollection + 1) % N_Collections;
    DrawText(TextFormat("increase collection"), 100, 100, 100, RED);
}

void CollectionMinFunction(int label, AudioStream& stream){
    *ptrSelectedCollection = (*ptrSelectedCollection - 1) % N_Collections;
    DrawText(TextFormat("decrease collection"), 100, 100, 100, RED);
}

void SpeedPlusFunction(int label, AudioStream& stream){
    *ptrIntervalTime -= 500;
    DrawText(TextFormat("increase speed"), 100, 100, 100, RED);
}

void SpeedMinFunction(int label, AudioStream& stream){
    *ptrIntervalTime += 500;
    DrawText(TextFormat("decrease speed"), 100, 100, 100, RED);
}

// Only in WRITE MODE

void SelectSampleFunction(int label, AudioStream& stream){
    *ptrSelectedSample = label; // pointer to number that says which sample is playing
    mode = PLAY_SAMPLE;
    (*ptrBufferCounter) = 0;
    ptrBuffer = samples[*ptrSelectedCollection][label];
    PlayAudioStream(stream);
    DrawText(TextFormat("This sample is selected and played"), 100, 100, 100, RED);
}

void WriteToSequenceFunction(int label, AudioStream& stream){
    indices[*ptrSelectedCollection][label] = *ptrSelectedSample; // pointer to number that says which sample is playing
    DrawText(TextFormat("The sample has been written to the sequence"), 100, 100, 100, RED);
}

// Only in KEYBOARD_MODE

typedef enum{
    ADD_SINE,
    ADD_SAW,
    ENVELOPE,
    Add_SAMPLE
}SynthOptions;

SynthOptions synthop = ADD_SINE;

void CallKeyboard(const char* text) { // Use TextFormat
    for (int i = 0; i < 5; ++i) {
        NumberS[i] = 0;
    }
    *textNumpad = *text;
    confirmedNumpad = false;
    uimode = KEYBOARD_MODE;
}

// Synth functions

void AddSineFunction(int label, AudioStream& stream) {
    synthop = ADD_SINE;
    if (!confirmedNumpad){
        CallKeyboard(TextFormat("Frequency? [Hz]"));
    } else{
        for (int i = 0; i < totalMonstersPerSample; ++i) {
            synth_buffer[i] += 0.3f * sin(2.0f * M_PI * (*ptrNumber)*i/48000);
        }
        confirmedNumpad = false;
        uimode = SYNTH_MODE;
    }
}

void AddSawFunction(int label, AudioStream& stream) {
    synthop = ADD_SAW;
    if (!confirmedNumpad){
        CallKeyboard(TextFormat("Freq? [Hz]"));
    } else{
        for (int i = 0; i < totalMonstersPerSample; ++i) {
            synth_buffer[i] += 0.6f*triang(((float)(*ptrNumber)*(float)i/48000.0f));
        }
        confirmedNumpad = false;
        uimode = SYNTH_MODE;
    }
}

float attack = 0.0f;
void AddEnvelopeFunction(int label, AudioStream& stream) {
    synthop = ENVELOPE;

    float tail = 0.0f;
    float amplitude = 1.0f;

    if (!confirmedNumpad && EnvState == 0){

        CallKeyboard(TextFormat("Attack? [ms]"));

    } else if (confirmedNumpad && EnvState == 0) {

        attack = amplitude/(*ptrNumber * 48.0f);

        for (int i = 0; i < totalMonstersPerSample; ++i) {
            if (i <= (int) (amplitude / attack)) {
                float f = attack * i;
                synth_buffer[i] *= f;
            }
        }

        confirmedNumpad = false;
        EnvState++;
        CallKeyboard(TextFormat("Tail? [ms]"));
    } else if (confirmedNumpad && EnvState == 1){

        tail = *ptrNumber * 48.0f;

        for (int i = 0; i < totalMonstersPerSample; ++i) {

            //if (i > (int)(amplitude/attack) && i < (int)(amplitude/attack) + tail) {
            if (i > (int)(amplitude/attack) && i < (int)(amplitude/attack) + tail) {
                float g = -(amplitude / tail) * i + pow(amplitude, 2.0f) / (tail * attack) +
                           amplitude;
                synth_buffer[i] *= g;
            } else if (i > (int)(amplitude/attack) + tail){
                synth_buffer[i] = 0.0f;
            }
        }
        confirmedNumpad = false;
        EnvState = 0;
        uimode = SYNTH_MODE;
    }
}

void NormalizeFunction(int label, AudioStream& stream) {

}

void ResetSynthFunction(int label, AudioStream& stream) {
    for (int i = 0; i < totalMonstersPerSample; ++i) {
        synth_buffer[i] *= 0.0f ;
    }
}

void LowerVolumeSynthFunction(int label, AudioStream& stream) {
    for (int i = 0; i < totalMonstersPerSample; ++i) {
        synth_buffer[i] *= 0.9f ;
    }
}

void PlaySynthFunction(int label, AudioStream& stream) {
    mode = PLAY_SYNTH;
    *ptrBufferCounter = 0;
    PlayAudioStream(stream);
    DrawText(TextFormat("I will Play"), 100, 100, 100, RED);
}

void AddSynthToSampleFunction(int label, AudioStream& stream) {
    synthop = Add_SAMPLE;
    if (!confirmedNumpad){
        CallKeyboard(TextFormat("Which sample (0-14)?"));
    } else{
        for (int i = 0; i < totalMonstersPerSample; ++i) {
            samples[*ptrSelectedCollection][*ptrNumber][i] = synth_buffer[i];
        }
        confirmedNumpad = false;
        uimode = SYNTH_MODE;
    }
}

// voeg synth toe aan hele collectie, en maak automatisch pentatoon
void AddSynthToAllFunction(int label, AudioStream& stream) {
    for (int j = 0; j < totalSamples - 1; ++j) {
        for (int i = 0; i < totalMonstersPerSample; ++i) {
            samples[*ptrSelectedCollection][j][(int)(i/pow(2.0f, pentatone[j]/12))] = synth_buffer[i];
        }
    }
}

// Keyboard functions

void NumberKeyFunction(int label, AudioStream& stream) {
    // shift 1 naar links in array
    for (int i = 1; i < 5; ++i) {
        NumberS[i-1] = NumberS[i];
    }
    // input nummer
    NumberS[4] = label;
}

void EraseKeyFunction(int label, AudioStream& stream) {
    for (int i = 0; i < 4; ++i) {
        NumberS[i+1] = NumberS[i]; // schuif simpelweg alles 1 naar rechts
    }
}

void ConfirmKeyFunction(int label, AudioStream& stream) {
    *ptrNumber = contract(NumberS, 5); // maak van array een getal
    confirmedNumpad = true;

    switch(synthop){
        case ADD_SINE:
            AddSineFunction(label, stream); // ga terug naar die functie, dit keer met confirmed true op zak
            break;
        case ADD_SAW:
            AddSawFunction(label, stream);
            break;
        case ENVELOPE:
            AddEnvelopeFunction(label, stream);
            break;
        case Add_SAMPLE:
            AddSynthToSampleFunction(label, stream);
            break;
    }
    //uimode = SYNTH_MODE;
}

///////////////// BUTTON DRAWING AND FUNCTIONALITY ####################


void ImplementButtonFunctionality(std::vector<appButton>& buttons,Vector2 fingerpos) {

    for (auto& button: buttons) {

        if (button.onClick == StopFunction) {
            if (!button.Touched(fingerpos)) {
                DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                              button.normColor);
            } else {
                DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                              button.selectColor);
                button.OnClick();
            }

        } else if (button.onClick == WriteFunction) {
            if (!button.Touched(fingerpos)) {
                DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                              button.normColor);
            } else {
                DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                              button.selectColor);
                button.OnClick();
            }

        } else if (button.onClick == PlayFunction) { // play button is a triangle
            if (!button.Touched(fingerpos)) {
                DrawTriangle({(float) (button.positionx + button.Size / 2),
                              (float) (button.positiony + button.Size / 2)},
                             {(float) button.positionx, (float) button.positiony},
                             {(float) button.positionx,
                              (float) (button.positiony + button.Size)},
                             button.normColor);
            } else {
                DrawTriangle({(float) (button.positionx + button.Size / 2),
                              (float) (button.positiony + button.Size / 2)},
                             {(float) button.positionx, (float) button.positiony},
                             {(float) button.positionx,
                              (float) (button.positiony + button.Size)},
                             button.selectColor);
                button.OnClick();
            }

        }else if (button.onClick == SynthFunction) {
            if (!button.Touched(fingerpos)) {
                DrawRectangleGradientV(button.positionx, button.positiony, button.Size, button.Size,
                              BLACK, WHITE);
            } else {
                DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                              button.selectColor);
                button.OnClick();
            }

        } else if (button.onClick == SpeedPlusFunction) {
            if (!button.Touched(fingerpos)) {
                DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                              button.normColor);
            } else {
                DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                              button.selectColor);
                button.OnClick();
            }

        }

        else if (button.onClick == SpeedMinFunction) {
            if (!button.Touched(fingerpos)) {
                DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                              button.normColor);
            } else {
                DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                              button.selectColor);
                button.OnClick();
            }
        }

        else if (button.onClick == CollectionPlusFunction) {
            if (!button.Touched(fingerpos)) {
                DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                              button.normColor);
            } else {
                DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                              button.selectColor);
                button.OnClick();
            }

        }

        switch (uimode) {

            case NORMAL:

                if (button.onClick == DrumFunction) {
                    if (!button.Touched(fingerpos)) {
                        if (mode == PLAY_SEQUENCE && *ptrPlayingSample == button.sampleIndex) {
                            DrawRectangle(button.positionx, button.positiony, button.Size,
                                          button.Size,
                                          button.playColor);
                        } else {
                            DrawRectangle(button.positionx, button.positiony, button.Size,
                                          button.Size,
                                          button.normColor);
                        }
                    } else {
                        DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                                      button.selectColor);
                        button.OnClick();
                    }
                }

                break;

            case WRITE_MODE:

                if (button.onClick == SelectSampleFunction) {
                    if (!button.Touched(fingerpos)) {
                        if (*ptrSelectedSample == button.sampleIndex) {
                            DrawRectangle(button.positionx, button.positiony, button.Size,
                                          button.Size,
                                          button.selectColor);
                        } else {
                            DrawRectangle(button.positionx, button.positiony, button.Size,
                                          button.Size,
                                          button.normColor);
                        }
                    } else {
                        DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                                      button.selectColor);
                        button.OnClick();
                    }

                } else if (button.onClick == WriteToSequenceFunction) {
                    if (!button.Touched(fingerpos)) {
                        if (mode == PLAY_SEQUENCE && *ptrPlayingSample == button.sampleIndex) {
                            DrawRectangle(button.positionx, button.positiony, button.Size,
                                          button.Size,
                                          button.playColor);
                        } else if (indices[*ptrSelectedCollection][button.sampleIndex] ==
                                   *ptrSelectedSample) {
                            DrawRectangle(button.positionx, button.positiony, button.Size,
                                          button.Size,
                                          button.selectColor);
                        } else {
                            DrawRectangle(button.positionx, button.positiony, button.Size,
                                          button.Size,
                                          button.normColor);
                        }
                    } else {
                        DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                                      button.selectColor);
                        button.OnClick();
                    }

                }
                break;

            case SYNTH_MODE:

                if (button.onClick == AddSineFunction) {
                    if (!button.Touched(fingerpos)) {
                        DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                                      button.normColor);
                        DrawText(TextFormat("sin"), button.positionx, button.positiony, 70, BLACK);
                    } else {
                        DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                                      button.selectColor);
                        DrawText(TextFormat("sin"), button.positionx, button.positiony, 70, BLACK);
                        button.OnClick();
                    }

                } else if (button.onClick == AddSawFunction) {
                    if (!button.Touched(fingerpos)) {
                        DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                                      button.normColor);
                        DrawText(TextFormat("tri"), button.positionx, button.positiony, 70, BLACK);
                    } else {
                        DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                                      button.selectColor);
                        DrawText(TextFormat("tri"), button.positionx, button.positiony, 70, BLACK);
                        button.OnClick();
                    }

                } else if (button.onClick == AddEnvelopeFunction) {
                    if (!button.Touched(fingerpos)) {
                        DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                                      button.normColor);
                        DrawText(TextFormat("env"), button.positionx, button.positiony, 70, BLACK);
                    } else {
                        DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                                      button.selectColor);
                        DrawText(TextFormat("env"), button.positionx, button.positiony, 70, BLACK);
                        button.OnClick();
                    }

                } else if (button.onClick == PlaySynthFunction) {
                    if (!button.Touched(fingerpos)) {
                        DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                                      button.normColor);
                        DrawText(TextFormat("play"), button.positionx, button.positiony, 70, BLACK);
                    } else {
                        DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                                      button.selectColor);
                        DrawText(TextFormat("play"), button.positionx, button.positiony, 70, BLACK);
                        button.OnClick();
                    }

                } else if (button.onClick == AddSynthToSampleFunction) {
                    if (!button.Touched(fingerpos)) {
                        DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                                      button.normColor);
                        DrawText(TextFormat("add"), button.positionx, button.positiony, 70, BLACK);
                    } else {
                        DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                                      button.selectColor);
                        DrawText(TextFormat("add"), button.positionx, button.positiony, 70, BLACK);
                        button.OnClick();
                    }

                } else if (button.onClick == AddSynthToAllFunction) {
                    if (!button.Touched(fingerpos)) {
                        DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                                      button.normColor);
                        DrawText(TextFormat("add al"), button.positionx, button.positiony, 70, BLACK);
                    } else {
                        DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                                      button.selectColor);
                        DrawText(TextFormat("add al"), button.positionx, button.positiony, 70, BLACK);
                        button.OnClick();
                    }

                } else if (button.onClick == LowerVolumeSynthFunction) {
                    if (!button.Touched(fingerpos)) {
                        DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                                      button.normColor);
                        DrawText(TextFormat("0.9x"), button.positionx, button.positiony, 70, BLACK);
                    } else {
                        DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                                      button.selectColor);
                        DrawText(TextFormat("0.9x"), button.positionx, button.positiony, 70, BLACK);
                        button.OnClick();
                    }

                } else if (button.onClick == ResetSynthFunction) {
                    if (!button.Touched(fingerpos)) {
                        DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                                      button.normColor);
                        DrawText(TextFormat("reset"), button.positionx, button.positiony, 70, BLACK);
                    } else {
                        DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                                      button.selectColor);
                        DrawText(TextFormat("reset"), button.positionx, button.positiony, 70, BLACK);
                        button.OnClick();
                    }

                }
                break;

            case KEYBOARD_MODE:
                if (button.onClick == NumberKeyFunction) {
                    if (!button.Touched(fingerpos)) {
                        DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                                      button.normColor);
                        DrawText(TextFormat("%i", button.sampleIndex), button.positionx, button.positiony, 70, BLACK);
                    } else {
                        DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                                      button.selectColor);
                        DrawText(TextFormat("%i", button.sampleIndex), button.positionx, button.positiony, 70, BLACK);
                        button.OnClick();
                    }

                } else if (button.onClick == EraseKeyFunction) {
                    if (!button.Touched(fingerpos)) {
                        DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                                      button.normColor);
                        DrawText(TextFormat("<"), button.positionx, button.positiony, 70, BLACK);
                    } else {
                        DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                                      button.selectColor);
                        DrawText(TextFormat("<"), button.positionx, button.positiony, 70, BLACK);
                        button.OnClick();
                    }

                } else if (button.onClick == ConfirmKeyFunction) {
                    if (!button.Touched(fingerpos)) {
                        DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                                      button.normColor);
                        DrawText(TextFormat("yes"), button.positionx, button.positiony, 70, BLACK);
                    } else {
                        DrawRectangle(button.positionx, button.positiony, button.Size, button.Size,
                                      button.selectColor);
                        DrawText(TextFormat("yes"), button.positionx, button.positiony, 70, BLACK);
                        button.OnClick();
                    }

                }
                break;
        }
    }
}

void ImplementGraphs(std::vector<appGraph>& graphics){

    switch (uimode) {

        case SYNTH_MODE:
            graphics[0].onShow();
            break;
        case NORMAL:
            break;
        case WRITE_MODE:
            break;
        case KEYBOARD_MODE:
            DrawText(textNumpad, 100,
                     300, 100, RED);
            for (int i = 0; i < 5; ++i) {
                DrawText(TextFormat("%i", NumberS[i]), 300 + 100 * i,
                         500, 100, RED);
            }
            break;
    }
}

////########## MAKE BUTTONS AND PUT IN A VECTOR

std::vector<appButton> Buttons = std::vector<appButton>();
std::vector<appGraph> Graphics = std::vector<appGraph>();

void SetupInterface(AudioStream& stream){
    // this just consists of putting the buttons in the vector
    int height = GetScreenHeight();
    int width = GetScreenWidth();

    // play button etc
    appButton playButton(width/25,height/32,
                         200,
                         BLACK, GOLD, RED,
                         PlayFunction, 0, stream);
    Buttons.emplace_back(playButton);

    appButton stopButton(width/5,height/32,
                         200,
                         BLACK, GOLD, RED,
                         StopFunction, 0, stream);
    Buttons.emplace_back(stopButton);

    appButton writeButton(2*width/5,height/32,
                         200,
                         RED, GOLD, RED,
                         WriteFunction, 0, stream);
    Buttons.emplace_back(writeButton);

    appButton synthButton(3*width/5,height/32,
                          200,
                          RED, GOLD, RED,
                          SynthFunction, 0, stream);
    Buttons.emplace_back(synthButton);

    appButton collectPlusButton(9*width/10,5*height/16,
                          100,
                          BLACK, GOLD, RED,
                          CollectionPlusFunction, 0, stream);
    Buttons.emplace_back(collectPlusButton);

    appButton collectMinButton(9*width/10,6*height/16,
                                100,
                                BLACK, GOLD, RED,
                                CollectionMinFunction, 0, stream);
    Buttons.emplace_back(collectMinButton);

    appButton speedPlusButton(9*width/10,2*height/16,
                                100,
                                BLACK, GOLD, RED,
                                SpeedPlusFunction, 0, stream);
    Buttons.emplace_back(speedPlusButton);

    appButton speedMinButton(9*width/10,3*height/16,
                               100,
                               BLACK, GOLD, RED,
                               SpeedMinFunction, 0, stream);
    Buttons.emplace_back(speedMinButton);

    // 4 x 4 drum buttons
    int N_x = 4; int N_y = 4;
    for (int i = 1; i < Nx+1; ++i) {
        for (int j = 1; j < Ny+1; ++j) {
            appButton drumButton(i*width/Nx - width/Nx + width/40,height / 2 + j*height/(2*Ny) - height/(2*Ny),
                                 width/5,
                                 GRAY, GOLD, RED,
                                 DrumFunction, (j-1)*Nx + (i-1), stream); // to determine which sample to play
            Buttons.emplace_back(drumButton);
        }
    }

    // write to seq buttons
    for (int i = 1; i < Nx+1; ++i) {
        for (int j = 1; j < Ny+1; ++j) {
            appButton drumButton(i*width/Nx - width/Nx + width/40,height / 2 + j*height/(2*Ny) - height/(2*Ny),
                                 width/5,
                                 GRAY, GOLD, RED,
                                 WriteToSequenceFunction, (j-1)*Nx + (i-1), stream); // to determine which sample to play
            Buttons.emplace_back(drumButton);
        }
    }

    // select sample button
    for (int i = 1; i < Nx+1; ++i) {
        for (int j = 1; j < Ny+1; ++j) {
            appButton drumButton(i*width/(1.5f*Nx) - width/Nx + width/5,height / 8 + j*height/(3*Ny) - height/(3*Ny),
                                 width/7,
                                 GRAY, GOLD, RED,
                                 SelectSampleFunction, (j-1)*Nx + (i-1), stream); // to determine which sample to play
            Buttons.emplace_back(drumButton);
        }
    }

    // keyboard buttons:

    //numbers
    for (int i = 1; i < 4; ++i) {
        for (int j = 1; j < 4; ++j) {
            appButton numButton(i*width/Nx - width/Nx + width/10,height / 2 + j*height/(2*Ny) - height/(2*Ny),
                                 width/5,
                                 GRAY, GOLD, RED,
                                 NumberKeyFunction, (j-1)*3 + i, stream); // number keys 1 - 9
            Buttons.emplace_back(numButton);
        }
    }

    // de andere nummers passen mooi in 3x3 grid, dus de nul moet apart
    appButton num0Button(2*width/Nx - width/Nx + width/10,height / 2 + 4*height/(2*Ny) - height/(2*Ny),
                        width/5,
                        GRAY, GOLD, RED,
                        NumberKeyFunction, 0, stream); // number key 0
    Buttons.emplace_back(num0Button);


    appButton eraseButton(1*width/Nx - width/Nx + width/10,height / 2 + 4*height/(2*Ny) - height/(2*Ny),
                         width/5,
                         GRAY, GOLD, RED,
                         EraseKeyFunction, 0, stream); // backspace button bottom left
    Buttons.emplace_back(eraseButton);


    appButton confirmButton(4*width/Nx - width/Nx + width/20,height / 2 + 4*height/(2*Ny) - height/(2*Ny),
                          width/5,
                          GRAY, GOLD, RED,
                          ConfirmKeyFunction, 0, stream); // backspace button bottom left
    Buttons.emplace_back(confirmButton);

    // synth buttons

    appButton addSineButton(1*width/Nx - width/Nx + width/40, height / 2 + 1*height/(2*Ny) - height/(2*Ny),
                            width/5,
                            GRAY, GOLD, RED,
                             AddSineFunction, 0, stream);
    Buttons.emplace_back(addSineButton);

    appButton addSawButton(1*width/Nx - width/Nx + width/40, height / 2 + 2*height/(2*Ny) - height/(2*Ny),
                            width/5,
                            GRAY, GOLD, RED,
                            AddSawFunction, 0, stream);
    Buttons.emplace_back(addSawButton);

    appButton envelopeButton(1*width/Nx - width/Nx + width/40, height / 2 + 3*height/(2*Ny) - height/(2*Ny),
                             width/5,
                             GRAY, GOLD, RED,
                            AddEnvelopeFunction, 0, stream);
    Buttons.emplace_back(envelopeButton);

    appButton LowerVolumeSynthButton(1*width/Nx - width/Nx + width/40, height / 2 + 4*height/(2*Ny) - height/(2*Ny),
                                     width/5,
                                     GRAY, GOLD, RED,
                            LowerVolumeSynthFunction, 0, stream);
    Buttons.emplace_back(LowerVolumeSynthButton);

    appButton PlaySynthButton(3*width/Nx - width/Nx + width/40, height / 2 + 1*height/(2*Ny) - height/(2*Ny),
                              width/5,
                              GRAY, GOLD, RED,
                            PlaySynthFunction, 0, stream);
    Buttons.emplace_back(PlaySynthButton);

    appButton resetSynthButton(3*width/Nx - width/Nx + width/40, height / 2 + 2*height/(2*Ny) - height/(2*Ny),
                               width/5,
                               GRAY, GOLD, RED,
                            ResetSynthFunction, 0, stream);
    Buttons.emplace_back(resetSynthButton);

    appButton addSampleSynthButton(3*width/Nx - width/Nx + width/40, height / 2 + 3*height/(2*Ny) - height/(2*Ny),
                                   width/5,
                                   GRAY, GOLD, RED,
                            AddSynthToSampleFunction, 0, stream);
    Buttons.emplace_back(addSampleSynthButton);

    appButton addSampleAllButton(3*width/Nx - width/Nx + width/40, height / 2 + 4*height/(2*Ny) - height/(2*Ny),
                                   width/5,
                                   GRAY, GOLD, RED,
                                   AddSynthToAllFunction, 0, stream);
    Buttons.emplace_back(addSampleAllButton);

    // GRAAFJES:

    appGraph synthGraph(width/25, height/6, 3*width/4,width/2,
                        synth_buffer, totalMonstersPerSample);
    Graphics.emplace_back(synthGraph);

}





/// MAIN #############################################################################################

int main(){
    const Color darkGreen = {20, 160, 133, 255};
    *ptrBufferCounter = 0; // counter for playing samples
    *ptrPlayBuffCntr = 0;  // counter for play mode
    *ptrIntervalTime = 48000/2; // time per sample in play mode
    *ptrPlayBuffSize = 16*(*ptrIntervalTime);
    *ptrCheckState = 0.0f;
    *ptrPlayingSample = ((*ptrPlayBuffCntr)/(*ptrIntervalTime));

    *ptrSelectedCollection = 0; // the first collection is selected by default
    *ptrSelectedSample = 16; // silence is selected by default
    int MonsterCount = 0;
    float WaitingTime = 0.0f;     // initial delay to synch sound and simulation
    float WaitTimeScaler = 0.00001f;

    //double previousTime = 0.0f;    // Previous time measure
    //double currentTime = 0.0;           // Current time measure
    //float deltaTime = 0.0f;

    InitWindow(0, 0, "boop!");

    // Audio
    const int sampleRate = SAMPLING_FREQ;
    const int channels   = 1;

    InitAudioDevice();
    InitializeSamples();
    InitializeIndices();
    InitializeSynthBuffer();

    SetAudioStreamBufferSizeDefault(BUFFER_SIZE);
    AudioStream stream = LoadAudioStream(sampleRate, 16, channels);
    SetAudioStreamCallback(stream, AudioCB); // raylib will pull audio

    SetupInterface(stream); // make the buttons and assign functionality wrt audio

    Vector2 TouchPos = {0};

    while (!WindowShouldClose()) {
        //previousTime = currentTime;

        *ptrPlayingSample = ((*ptrPlayBuffCntr)/(*ptrIntervalTime) + ringbuff.size()/(*ptrIntervalTime) ) % (Nx*Ny);
        *ptrPlayBuffSize = 16*(*ptrIntervalTime); // update in case speed is changed

        TouchPos = {0}; // reset
        TouchPos = GetTouchPosGood(0);


        switch(mode){
            case PLAY_SAMPLE:
                break;
            case PLAY_SEQUENCE:
                FillPlayBuffer();
                break;
        }

        MonsterCount++;

        if (MonsterCount == BUFFER_SIZE) {

            MonsterCount = 0;

            BeginDrawing();
            ClearBackground(GREEN);

            ImplementButtonFunctionality(Buttons, TouchPos);
            ImplementGraphs(Graphics);

            //DrawText(TextFormat("which sample: %i", *ptrPlayingSample), 100, 350, 50, RED);
            //DrawText(TextFormat("env state: %i", EnvState), 100, 410, 50, RED);
            //DrawText(TextFormat("ptrB: %p", ptrBuffer), 100, 470, 50, RED);
            //DrawText(TextFormat("ptrS: %p", samples), 100, 530, 50, RED);
            DrawText(TextFormat("Collection: %i", (*ptrSelectedCollection)), GetScreenWidth()/20,
                     6*GetScreenHeight()/13, 100, RED);

            //DrawText(TextFormat("selected sample: %i", (*ptrSelectedSample)), 100,
            //         650, 50, RED);

            EndDrawing();
            WaitingTime = WaitTimeScaler*((float)ringbuff.size() - (float)(bufferExcessCoef/5)*BUFFER_SIZE);
            WaitTime((double)WaitingTime); // adaptive time delay to let audio catch up with sim
            //currentTime = GetTime();
            //deltaTime = (float)(currentTime - previousTime);
        }



    }

    free3D(samples);
    delete[] indices;
    delete[] ptrBuffer;
    delete ptrBufferCounter;
    delete ptrPlayBuffCntr;
    delete ptrIntervalTime;
    delete ptrPlayBuffSize;
    delete ptrPlayingSample;
    delete ptrCheckState;
    delete ptrSelectedCollection;
    delete ptrSelectedSample;

    StopAudioStream(stream);
    CloseAudioDevice();
    CloseWindow();
}