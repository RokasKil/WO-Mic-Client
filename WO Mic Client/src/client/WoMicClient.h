#ifndef WOMICCLIENT_H
#define WOMICCLIENT_H
#include <string>
#include <atomic_queue/atomic_queue.h>
using namespace std;

typedef atomic_queue::AtomicQueue<int32_t, 2048, 256*256*100> AudioQueue;

enum ClientStatus {
    WAITING,
    CONNECTING,
    CONNECTED,
    RECONNECTING,
    FAILED
};

class WoMicClient
{
public:
    WoMicClient();
    WoMicClient(string ip, short serverPort, short clientPort, string device, float cutOff, bool autoReconnect);
    int start();
    int stop();
    ClientStatus getStatus();


    string getIp();
    void setIp(string ip);

    short getServerPort();
    void setServerPort(short serverPort);

    short getClientPort();
    void setClientPort(short clientPort);

    string getDevice();
    void setDevice(short device);

    bool getAutoReconnect();
    void setAutoReconnect(bool autoReconnect);

    float getCutOff();
    void setCutOff(short cutOff);
    float bufferLeft();

protected:

private:
    ClientStatus status;
    bool autoReconnect;
    string ip;
    short serverPort;
    short clientPort; // for our UDP reciever
    string device;
    float cutOff; //when to delete voice data
    AudioQueue audioQueue;
    const int sampleRate = 48000;
    const int channels = 1;
};

#endif // WOMICCLIENT_H
