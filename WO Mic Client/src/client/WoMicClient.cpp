#include "WoMicClient.h"

WoMicClient::WoMicClient()
{

}

WoMicClient::WoMicClient(string ip, short serverPort, short clientPort, string device, float cutOff, bool autoReconnect) {
    this->ip = ip;
    this->serverPort = serverPort;
    this->clientPort = clientPort;
    this->device = device;
    this->cutOff = cutOff;
    this->autoReconnect = autoReconnect;
}

int WoMicClient::start() {
    return 0;
}

int WoMicClient::stop() {
    return 0;
}

ClientStatus WoMicClient::getStatus() {
    return status;
}

string WoMicClient::getIp() {
    return ip;
}

void WoMicClient::setIp(string ip) {
    this->ip = ip;
}

short WoMicClient::getServerPort() {
    return serverPort;
}

void WoMicClient::setServerPort(short serverPort) {
    this->serverPort = serverPort;
}

short WoMicClient::getClientPort() {
    return clientPort;
}

void WoMicClient::setClientPort(short clientPort) {
    this->clientPort = clientPort;
}

string WoMicClient::getDevice() {
    return device;
}

void WoMicClient::setDevice(short device) {
    this->device = device;
}

float WoMicClient::getCutOff() {
    return cutOff;
}

void WoMicClient::setCutOff(short cutOff) {
    this->cutOff = cutOff;
}

float WoMicClient::bufferLeft() {
    return float(audioQueue.was_size())/sampleRate/channels;
}
