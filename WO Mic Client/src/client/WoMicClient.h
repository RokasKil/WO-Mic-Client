#ifndef WOMICCLIENT_H
#define WOMICCLIENT_H
#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <string>
#include <atomic_queue/atomic_queue.h>
#include <iostream>
#include <thread>
#include "ClientDefines.h"
#include <opus.h>
#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <setupapi.h>
#include <initguid.h>
#include <devpkey.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <chrono>

using namespace std;


typedef atomic_queue::AtomicQueue<int32_t, CLIENT_QUEUE_SIZE, 256*256*100> AudioQueue;

typedef void (*WoMicClientCallback)(int) ;

enum ClientStatus {
    WAITING,
    CONNECTING,
    CONNECTED,
    RECONNECTING,
    STOPPING,
    FAILED
};

class WoMicClient
{
public:
    WoMicClient();
    ~WoMicClient();
    WoMicClient(string ip, unsigned short serverPort, unsigned short clientPort, string device, float cutOff, bool autoReconnect, float speedOff);
    int start();
    int stop();
    int startAsync(WoMicClientCallback callback);
    int stopAsync(WoMicClientCallback callback);
    ClientStatus getStatus();


    string getIp();
    WoMicClient* setIp(string ip);

    unsigned short getServerPort();
    WoMicClient* setServerPort(unsigned short serverPort);

    unsigned short getClientPort();
    WoMicClient* setClientPort(unsigned short clientPort);

    string getDevice();
    WoMicClient* setDevice(string device);

    bool getAutoReconnect();
    WoMicClient* setAutoReconnect(bool autoReconnect);

    float getCutOff();
    WoMicClient* setCutOff(float cutOff);

    float getSpeedOff();
    WoMicClient* getSpeedOff(float speedOff);

    float bufferLeft();
private:
    void start(WoMicClientCallback callback);
    void stop(WoMicClientCallback callback);

    int connect();
    int handshake();
    int disconnect();
    int startUDPServer();
    int stopUDPServer();
    void reconnectAsync();
    int reconnect();

    int initOpusRecorder();
    int destroyOpusRecorder();

    int udpListen();
    int pingLoop();
    void pingFailed();

    int openAudioDevice();

    int closeAudioDevice();

    int startAudio();
    int audioDeviceLoop(IAudioClient *pAudioClient);

    ClientStatus status = WAITING;
    bool autoReconnect;
    string ip;
    unsigned short serverPort;
    unsigned short usedServerPort;
    unsigned short clientPort; // for our UDP reciever
    string device;
    atomic<float> cutOff; //when to delete voice data
    atomic<float> speedOff;
    AudioQueue audioQueue;
    SOCKET clientSocket = INVALID_SOCKET; //TCP client socket
    SOCKET serverSocket = INVALID_SOCKET; //UDP server socket for incoming audio data
    unique_ptr<thread> recvThread = NULL;
    unique_ptr<thread> pingThread = NULL;
    unique_ptr<thread> audioThread = NULL;
    unique_ptr<thread> reconnectThread = NULL;
    unique_ptr<thread> stopThread = NULL;
    unique_ptr<thread> startThread = NULL;
    bool wsaInitialized = false;
    OpusDecoder* opusDecoder = NULL;
    HANDLE hEvent = NULL;

    atomic<int> audioResult;

    atomic<bool> doneStart = {false};

    atomic<bool> doneStop = {false};

    const int sampleRate = 48000;
    const int channels = 1;
};

#endif // WOMICCLIENT_H
