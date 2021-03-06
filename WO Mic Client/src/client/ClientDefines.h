#ifndef CLIENTDEFINES_H_INCLUDED
#define CLIENTDEFINES_H_INCLUDED

#define CLIENT_E_NONE -2
#define CLIENT_E_INVALIDSTATE -1
#define CLIENT_E_OK 0
#define CLIENT_E_WSASTARTUP 1
#define CLIENT_E_GETADDRINFO 2
#define CLIENT_E_CONNECTSOCKET 3
#define CLIENT_E_CONNECT 4

#define CLIENT_E_SEND_CHECKVERSION_0 5
#define CLIENT_E_SEND_CHECKVERSION_1 6
#define CLIENT_E_RECV_CHECKVERSION_0 7
#define CLIENT_E_RECV_CHECKVERSION_1 8
#define CLIENT_E_RECV_CHECKVERSION_2 9


#define CLIENT_E_SEND_SETCODEC_0 10
#define CLIENT_E_SEND_SETCODEC_1 11
#define CLIENT_E_RECV_SETCODEC_0 12
#define CLIENT_E_RECV_SETCODEC_1 13
#define CLIENT_E_RECV_SETCODEC_2 14

#define CLIENT_E_SEND_START_0 15
#define CLIENT_E_SEND_START_1 16
#define CLIENT_E_RECV_START_0 17
#define CLIENT_E_RECV_START_1 18
#define CLIENT_E_RECV_START_2 19


#define CLIENT_E_UDP_SOCKET 20
#define CLIENT_E_UDP_BIND 21
#define CLIENT_E_UDP_RECV 22

#define CLIENT_E_SEND_PING_0 23
#define CLIENT_E_SEND_PING_1 24

#define CLIENT_E_RECONNECTSTATE 25

#define CLIENT_E_OPUSDECODER 26

#define CLIENT_E_DEVICE_FORMAT 27
#define CLIENT_E_DEVICE_TIMEOUT 28
#define CLIENT_E_DEVICE_COMINIT 29
#define CLIENT_E_DEVICE_CREATEENUMERATOR 30
#define CLIENT_E_DEVICE_ENUMAUDIO 31
#define CLIENT_E_DEVICE_COUNTAUDIO 32
#define CLIENT_E_DEVICE_GETAUDIO 33
#define CLIENT_E_DEVICE_OPENPROP 35
#define CLIENT_E_DEVICE_NOTFOUND 36
#define CLIENT_E_DEVICE_INITIALIZE 37
#define CLIENT_E_DEVICE_SETEVENT 38
#define CLIENT_E_DEVICE_PERIOD 39
#define CLIENT_E_DEVICE_GETPROP 40
#define CLIENT_E_DEVICE_ACTIVATE 41
#define CLIENT_E_DEVICE_GETBUFFERSIZEREALIGN 42
#define CLIENT_E_DEVICE_EVENT 43
#define CLIENT_E_DEVICE_GETBUFFERSIZE 44
#define CLIENT_E_DEVICE_RENDER 45
#define CLIENT_E_DEVICE_START 46
#define CLIENT_E_DEVICE_GETBUFFER 47
#define CLIENT_E_DEVICE_RELEASEBUFFER 48

#define CLIENT_WOMIC_CHECKVERSION 101
#define CLIENT_WOMIC_SETCODEC 102
#define CLIENT_WOMIC_START 103
#define CLIENT_WOMIC_STOP 104
#define CLIENT_WOMIC_PING 105

#define CLIENT_RECONNECT 106

#define CLIENT_QUEUE_SIZE 48000*2
#endif // CLIENTDEFINES_H_INCLUDED
