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
#include <functional>
#include <vector>

typedef atomic_queue::AtomicQueue<int32_t, CLIENT_QUEUE_SIZE, 256*256*100> AudioQueue;

typedef std::function<void(int)> WoMicClientCallback;

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
    int start();
    int start(bool reconnect);
    int stop();
    int startAsync(WoMicClientCallback callback);
    int startAsync(WoMicClientCallback callback, bool reconnect);
    int stopAsync(WoMicClientCallback callback);
    ClientStatus getStatus();


    std::string getIp();
    WoMicClient* setIp(std::string ip);

    unsigned short getServerPort();
    WoMicClient* setServerPort(unsigned short serverPort);

    unsigned short getClientPort();
    WoMicClient* setClientPort(unsigned short clientPort);

    std::wstring getDevice();
    WoMicClient* setDevice(std::wstring device);

    bool getAutoReconnect();
    WoMicClient* setAutoReconnect(bool autoReconnect);

    float getCutOff();
    WoMicClient* setCutOff(float cutOff);

    float getSpeedOff();
    WoMicClient* setSpeedOff(float speedOff);

    WoMicClientCallback getFailCallback();
    WoMicClient* setFailCallback(WoMicClientCallback callback);

    int getFailCode();

    float bufferLeft();

    int getSamplesInBuffer();

    int getSampleRate();

    int getChannels();

    int getAvailableDevices(std::vector<std::wstring> &devices);

    void setClearBufferFlag();
private:
    void start(WoMicClientCallback callback, bool reconnect);
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
    void pingFailed(int reason);

    int openAudioDevice();

    int closeAudioDevice();

    int startAudio();
    int audioDeviceLoop(IAudioClient *pAudioClient);
    void purgeAudioQueue(int leave);

    ClientStatus status = WAITING;
    bool autoReconnect;
    std::string ip;
    unsigned short serverPort;
    unsigned short usedServerPort;
    unsigned short clientPort; // for our UDP reciever
    std::wstring device;
    std::atomic<float> cutOff = {1}; //when to delete voice data (seconds)
    std::atomic<float> speedOff = {1}; // when to speed up voice data (seconds)
    AudioQueue audioQueue;
    SOCKET clientSocket = INVALID_SOCKET; //TCP client socket
    SOCKET serverSocket = INVALID_SOCKET; //UDP server socket for incoming audio data
    std::unique_ptr<std::thread> recvThread = NULL;
    std::unique_ptr<std::thread> pingThread = NULL;
    std::unique_ptr<std::thread> audioThread = NULL;
    std::unique_ptr<std::thread> reconnectThread = NULL;
    std::unique_ptr<std::thread> stopThread = NULL;
    std::unique_ptr<std::thread> startThread = NULL;
    bool wsaInitialized = false;
    OpusDecoder* opusDecoder = NULL;
    HANDLE hEvent = NULL;

    std::atomic<int> audioResult;
    std::atomic<bool> doneStart = {false};
    std::atomic<bool> doneStop = {false};
    std::atomic<bool> clearBufferFlag = {false};

    int failCode;

    WoMicClientCallback failCallback;

    const int sampleRate = 48000;
    const int channels = 1;
};

#endif // WOMICCLIENT_H
