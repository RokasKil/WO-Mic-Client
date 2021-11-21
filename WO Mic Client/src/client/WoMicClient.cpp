#include "WoMicClient.h"

WoMicClient::WoMicClient() {

}

WoMicClient::~WoMicClient() {
    if(wsaInitialized) {
        WSACleanup();
    }
}

WoMicClient::WoMicClient(string ip, unsigned short serverPort, unsigned short clientPort, string device, float cutOff, bool autoReconnect) {
    this->ip = ip;
    this->serverPort = serverPort;
    this->clientPort = clientPort;
    this->device = device;
    this->cutOff = cutOff;
    this->autoReconnect = autoReconnect;
}

int WoMicClient::start() {
    int result;
    if (status == WAITING || status == FAILED) {
        status = CONNECTING;
        if ((result = openAudioDevice()) == CLIENT_E_OK && (result = connect()) == CLIENT_E_OK ) {
            status = CONNECTED;
        }
        else {
            status = FAILED;
        }
        return result;
    }
    return CLIENT_E_INVALIDSTATE;
}

int WoMicClient::connect() {
    int result;
    struct addrinfo *addrInfoResult = NULL,
            *addrInfoPtr = NULL,
            hints;
    if (!wsaInitialized ) {
        WSADATA wsaData;
        result = WSAStartup(MAKEWORD(2,2), &wsaData);
        if (result != 0) {
            cout << "WSAStartup failed with error: " << result << " " << WSAGetLastError() << endl;
            return CLIENT_E_WSASTARTUP;
        }
        else {
            wsaInitialized = true;
        }
    }

    result = startUDPServer();

    if (result != CLIENT_E_OK) {
        return result;
    }

    ZeroMemory( &hints, sizeof(hints) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    result = getaddrinfo(ip.c_str(), to_string(serverPort).c_str(), &hints, &addrInfoResult);
    if (result != 0) {
        cout << "getaddrinfo failed with error: " << result << " " << WSAGetLastError() << endl;
        return CLIENT_E_GETADDRINFO;
    }

    for (auto ptr = addrInfoResult; ptr != NULL; ptr = ptr->ai_next) {
        clientSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (clientSocket == INVALID_SOCKET) {
            cout << "socket failed with error: " << result << " " << WSAGetLastError() << endl;
            return CLIENT_E_CONNECTSOCKET;
        }

        // Connect to server.
        result = ::connect(clientSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (result == SOCKET_ERROR) {
            closesocket(clientSocket);
            clientSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }
    freeaddrinfo(addrInfoResult);

    if (clientSocket == INVALID_SOCKET) {
        cout << "Unable to connect to server: " << ip << ":" << serverPort << endl;
        return CLIENT_E_CONNECT;
    }
    cout << "Connected" << endl;
    result = handshake();
    if (result != CLIENT_E_OK) {
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
        return result;
    }
	if (pingThread != NULL && pingThread->joinable() ) {
        pingThread->join();
        pingThread = NULL;
	}
    pingThread = std::make_unique<thread>(&WoMicClient::pingLoop, this);
    return result;
}

int WoMicClient::pingLoop() {

    char buffer[128];
    int result;
    int len = 0;
    while (true) {
        len = 0;
        buffer[len++] = CLIENT_WOMIC_PING;
        result = send(clientSocket, buffer, len, 0);
        if (result == SOCKET_ERROR) {
            pingFailed();
            cout << "CLIENT_E_SEND_PING_0 (" <<  CLIENT_E_SEND_PING_0 << ") error " << result << " " << WSAGetLastError() << endl;
            return CLIENT_E_SEND_PING_0;
        }

        len = 0;
        *((unsigned int*) (buffer + len)) = htonl(0); // packet length network order
        len += 4;
        result = send(clientSocket, buffer, len, 0);
        if (result == SOCKET_ERROR) {
            pingFailed();
            cout << "CLIENT_E_SEND_PING_1 (" <<  CLIENT_E_SEND_PING_1 << ") error " << result << " " << WSAGetLastError() << endl;
            return CLIENT_E_SEND_PING_1;
        }
        Sleep(1000);
    }
}

void WoMicClient::pingFailed() {
    closesocket(clientSocket);
    clientSocket = INVALID_SOCKET;
    if (autoReconnect && status == CONNECTED) {
        reconnectAsync();
    }
    else {
        status = FAILED;
    }
}

void WoMicClient::reconnectAsync() {
    status = RECONNECTING;
	if (reconnectThread != NULL && reconnectThread->joinable() ) {
        reconnectThread->join();
        reconnectThread = NULL;
	}
    reconnectThread = std::make_unique<thread>(&WoMicClient::reconnect, this);

}

int WoMicClient::reconnect() {
    int result;
    int sleepMiliseconds = 1000;
    status = RECONNECTING;
    while (status == RECONNECTING) {
        if (clientSocket != INVALID_SOCKET) {
            result = disconnect();
            if (result != CLIENT_E_OK) {
                return result;
            }
        }
        cout << "Reconnecting" << endl;
        result = connect();
        if (result == CLIENT_E_OK) {
            if (status == RECONNECTING) {// just in case
                status = CONNECTED;
            }
        }
        else {
            cout << "Reconnect waiting for " << sleepMiliseconds / 1000 << " sec" << endl;
            Sleep(sleepMiliseconds);
            sleepMiliseconds = min(sleepMiliseconds * 5, 10000); // sleep for 1, 5, 10 seconds
        }
    }
}

int WoMicClient::startUDPServer() {
    int result;
    struct sockaddr_in serverAddrInfo;
    if (serverSocket != INVALID_SOCKET) { // Jei server jau yra palikt
        if (serverPort != usedServerPort ){ // Nebent keitësi port
            if ((result = stopUDPServer()) != CLIENT_E_OK) {
                return result;
            }
        }
        else {
            return CLIENT_E_OK;
        }
    }

    if ((serverSocket = socket(AF_INET, SOCK_DGRAM , 0)) == INVALID_SOCKET)
	{
		cout << "Could not create udp server socket:" << WSAGetLastError() << endl;
		return CLIENT_E_UDP_SOCKET;
	}
	serverAddrInfo.sin_family = AF_INET;
	serverAddrInfo.sin_addr.s_addr = INADDR_ANY;
	serverAddrInfo.sin_port = htons(serverPort);

	//Bind
	if (bind(serverSocket, (struct sockaddr *) &serverAddrInfo, sizeof(serverAddrInfo)) == SOCKET_ERROR) {
		cout << "Failed to bind udp server: " << WSAGetLastError() << endl;;
		return CLIENT_E_UDP_BIND;
	}
	else {
        usedServerPort = serverPort;
	}
	if (recvThread != NULL && recvThread->joinable() ) {
        recvThread->join();
        recvThread = NULL;
	}
    recvThread = std::make_unique<thread>(&WoMicClient::udpListen, this);
    cout << "UDP server started" << endl;
    return CLIENT_E_OK;
}

int WoMicClient::stopUDPServer() {
    if (serverSocket != INVALID_SOCKET) {
        closesocket(serverSocket);
    }
    if (recvThread != NULL) {
        if (recvThread->joinable()) {
            recvThread->join();
        }
        recvThread = NULL;
    }
    return CLIENT_E_OK;
}

int WoMicClient::udpListen() {
    unique_ptr<char[]> buffer = std::make_unique<char[]>(1024*1024);
    int result;
    cout << "recvFrom" << endl;
    while ((result = recvfrom(serverSocket, buffer.get(), 1024*1024, 0, NULL, NULL)) != SOCKET_ERROR) { //Neziurim kas atsiunte
        cout << "got " << result << "data" << endl;
    }
    cout << "recvfrom failed: " << result << WSAGetLastError() << endl;
    return CLIENT_E_UDP_RECV;
}

int WoMicClient::handshake() {
    char buffer[128];
    int result;
    int len = 0;
    unsigned int packetLen = 0;

    //Checking version might be able to skip this
    buffer[len++] = CLIENT_WOMIC_CHECKVERSION;
    result = send(clientSocket, buffer, len, 0);
    if (result == SOCKET_ERROR) {
        cout << "CLIENT_E_SEND_CHECKVERSION_0 (" <<  CLIENT_E_SEND_CHECKVERSION_0 << ") error " << result << " " << WSAGetLastError() << endl;
        return CLIENT_E_SEND_CHECKVERSION_0;
    }

    len = 0;
    *((unsigned int*) (buffer + len)) = htonl(6); // packet length network order
    len += 4;
    // Didn't bother to find these out
    buffer[len++] = 4;
    buffer[len++] = 5;
    buffer[len++] = 0;
    buffer[len++] = 0;
    buffer[len++] = 0;
    buffer[len++] = 0;
    result = send(clientSocket, buffer, len, 0);
    if (result == SOCKET_ERROR) {
        cout << "CLIENT_E_SEND_CHECKVERSION_1 (" <<  CLIENT_E_SEND_CHECKVERSION_1 << ") error " << result << " " << WSAGetLastError() << endl;
        return CLIENT_E_SEND_CHECKVERSION_1;
    }

    result = recv(clientSocket, buffer, 1, 0);
    if (result == SOCKET_ERROR || result != 1 || buffer[0] != CLIENT_WOMIC_CHECKVERSION) {
        cout << "CLIENT_E_RECV_CHECKVERSION_0 (" <<  CLIENT_E_RECV_CHECKVERSION_0 << ") error " << result << " " << WSAGetLastError() << endl;
        return CLIENT_E_RECV_CHECKVERSION_0;
    }

    result = recv(clientSocket, buffer, 6, 0);
    if (result == SOCKET_ERROR || result != 6) {
        cout << "CLIENT_E_RECV_CHECKVERSION_1 (" <<  CLIENT_E_RECV_CHECKVERSION_1 << ") error " << result << " " << WSAGetLastError() << endl;
        return CLIENT_E_RECV_CHECKVERSION_1;
    }
    else {
        len = 0;
        packetLen = ntohl(*((unsigned int*) (buffer + len)));// packet length host order
        len += 4;
        if (!(packetLen == 2 && buffer[len++] == 0 && buffer[len++] == 4)) {
            len = 4;
            cout << "CLIENT_E_RECV_CHECKVERSION_2 (" <<  CLIENT_E_RECV_CHECKVERSION_2 << ") error " << packetLen << " " << int(buffer[len++]) << " " << int(buffer[len++]) << endl;
            return CLIENT_E_RECV_CHECKVERSION_2;
        }
    }

    cout << "Handshake: got version" << endl;
    //Setting codec
    len = 0;
    buffer[len++] = CLIENT_WOMIC_SETCODEC;
    result = send(clientSocket, buffer, len, 0);
    if (result == SOCKET_ERROR) {
        cout << "CLIENT_E_SEND_SETCODEC_0 (" <<  CLIENT_E_SEND_SETCODEC_0 << ") error " << result << " " << WSAGetLastError() << endl;
        return CLIENT_E_SEND_SETCODEC_0;
    }

    len = 0;
    *((unsigned int*) (buffer + len)) = htonl(6); // packet length network order
    len += 4;
    buffer[len++] = 2; // Algortimas OPUS, vienintelis pasirinkimas
    buffer[len++] = 2; // SampleRate 2 => 48000
    *((unsigned int*) (buffer + len)) = htonl(serverPort); // port 4 byte formatu
    len += 4;
    result = send(clientSocket, buffer, len, 0);
    if (result == SOCKET_ERROR) {
        cout << "CLIENT_E_SEND_SETCODEC_1 (" <<  CLIENT_E_SEND_SETCODEC_1 << ") error " << result << " " << WSAGetLastError() << endl;
        return CLIENT_E_SEND_SETCODEC_1;
    }

    result = recv(clientSocket, buffer, 1, 0);
    if (result == SOCKET_ERROR || result != 1 || buffer[0] != CLIENT_WOMIC_SETCODEC) {
        cout << "CLIENT_E_RECV_SETCODEC_0 (" <<  CLIENT_E_RECV_SETCODEC_0 << ") error " << result << " " << WSAGetLastError() << endl;
        return CLIENT_E_RECV_SETCODEC_0;
    }

    result = recv(clientSocket, buffer, 5, 0);
    if (result == SOCKET_ERROR || result != 5) {
        cout << "CLIENT_E_RECV_SETCODEC_1 (" <<  CLIENT_E_RECV_SETCODEC_1 << ") error " << result << " " << WSAGetLastError() << endl;
        return CLIENT_E_RECV_SETCODEC_1;
    }
    else {
        len = 0;
        packetLen = ntohl(*((unsigned int*) (buffer + len)));// packet length host order
        len += 4;
        if (!(packetLen == 1 && buffer[len] == 0)) { // 0 = ok
            cout << "CLIENT_E_RECV_SETCODEC_2 (" <<  CLIENT_E_RECV_SETCODEC_2 << ") error " << packetLen << " " << int(buffer[len]) << endl;
            return CLIENT_E_RECV_SETCODEC_2;
        }
    }
    cout << "Handshake: set codec" << endl;

    //Telling womic to start

    len = 0;
    buffer[len++] = CLIENT_WOMIC_START;
    result = send(clientSocket, buffer, len, 0);
    if (result == SOCKET_ERROR) {
        cout << "CLIENT_E_SEND_START_0 (" <<  CLIENT_E_SEND_START_0 << ") error " << result << " " << WSAGetLastError() << endl;
        return CLIENT_E_SEND_START_0;
    }

    len = 0;
    *((unsigned int*) (buffer + len)) = htonl(0); // packet length network order
    len += 4;
    result = send(clientSocket, buffer, len, 0);
    if (result == SOCKET_ERROR) {
        cout << "CLIENT_E_SEND_START_1 (" <<  CLIENT_E_SEND_START_1 << ") error " << result << " " << WSAGetLastError() << endl;
        return CLIENT_E_SEND_START_1;
    }

    result = recv(clientSocket, buffer, 1, 0);
    if (result == SOCKET_ERROR || result != 1 || buffer[0] != CLIENT_WOMIC_START) {
        cout << "CLIENT_E_RECV_START_0 (" <<  CLIENT_E_RECV_START_0 << ") error " << result << " " << WSAGetLastError() << endl;
        return CLIENT_E_RECV_START_0;
    }

    result = recv(clientSocket, buffer, 5, 0);
    if (result == SOCKET_ERROR || result != 5) {
        cout << "CLIENT_E_RECV_START_1 (" <<  CLIENT_E_RECV_START_1 << ") error " << result << " " << WSAGetLastError() << endl;
        return CLIENT_E_RECV_START_1;
    }
    else {
        len = 0;
        packetLen = ntohl(*((int*) (buffer + len)));// packet length host order
        len += 4;
        if (!(packetLen == 1 && buffer[len] == 0)) { // 0 = ok
            cout << "CLIENT_E_RECV_START_2 (" <<  CLIENT_E_RECV_START_2 << ") error " << packetLen << " " << int(buffer[len]) << endl;
            return CLIENT_E_RECV_START_2;
        }
    }
    cout << "Handshake: started" << endl;
    //Hand shake complete womic should be running
    return CLIENT_E_OK;
}

int WoMicClient::disconnect() {
    return -1;
}

int WoMicClient::openAudioDevice() {
    return CLIENT_E_OK;
}

int WoMicClient::closeAudioDevice() {

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

WoMicClient* WoMicClient::setIp(string ip) {
    this->ip = ip;
    return this;
}

unsigned short WoMicClient::getServerPort() {
    return serverPort;
}

WoMicClient* WoMicClient::setServerPort(unsigned short serverPort) {
    this->serverPort = serverPort;
    return this;
}

unsigned short WoMicClient::getClientPort() {
    return clientPort;
}

WoMicClient* WoMicClient::setClientPort(unsigned short clientPort) {
    this->clientPort = clientPort;
    return this;
}

string WoMicClient::getDevice() {
    return device;
}

WoMicClient* WoMicClient::setDevice(string device) {
    this->device = device;
    return this;
}

float WoMicClient::getCutOff() {
    return cutOff;
}

WoMicClient* WoMicClient::setCutOff(float cutOff) {
    this->cutOff = cutOff;
    return this;
}

bool WoMicClient::getAutoReconnect() {
    return autoReconnect;
}

WoMicClient* WoMicClient::setAutoReconnect(bool autoReconnect) {
    this->autoReconnect = autoReconnect;
    return this;
}

float WoMicClient::bufferLeft() {
    return float(audioQueue.was_size())/sampleRate/channels;
}
