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

using namespace std;

typedef atomic_queue::AtomicQueue<int32_t, 48000 * 2, 256*256*100> AudioQueue;

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
    WoMicClient(string ip, unsigned short serverPort, unsigned short clientPort, string device, float cutOff, bool autoReconnect);
    int start();
    int stop();
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

    float bufferLeft();
private:
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

    int audioDeviceLoop();

    ClientStatus status = WAITING;
    bool autoReconnect;
    string ip;
    unsigned short serverPort;
    unsigned short usedServerPort;
    unsigned short clientPort; // for our UDP reciever
    string device;
    float cutOff; //when to delete voice data
    AudioQueue audioQueue;
    SOCKET clientSocket = INVALID_SOCKET; //TCP client socket
    SOCKET serverSocket = INVALID_SOCKET; //UDP server socket for incoming audio data
    unique_ptr<thread> recvThread = NULL;
    unique_ptr<thread> pingThread = NULL;
    unique_ptr<thread> audioThread = NULL;
    unique_ptr<thread> reconnectThread = NULL;
    bool wsaInitialized = false;

    OpusDecoder* opusDecoder = NULL;
    HANDLE hEvent = NULL;
    IAudioClient *pAudioClient = NULL;

    const int sampleRate = 48000;
    const int channels = 1;
};

#endif // WOMICCLIENT_H
