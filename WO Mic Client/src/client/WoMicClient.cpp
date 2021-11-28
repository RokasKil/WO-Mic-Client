#include "WoMicClient.h"

using namespace std;

WoMicClient::WoMicClient() {

}

WoMicClient::~WoMicClient() {
    stop();

	if (stopThread != NULL && stopThread->joinable() ) {
        stopThread->join();
        stopThread = NULL;
	}
	if (startThread != NULL && startThread->joinable() ) {
        startThread->join();
        startThread = NULL;
	}
    if(wsaInitialized) {
        WSACleanup();
    }
}

int WoMicClient::start() {
    int result;
    if (status == WAITING || status == FAILED) {
        status = CONNECTING;
        if ((result = initOpusRecorder()) == CLIENT_E_OK && (result = openAudioDevice()) == CLIENT_E_OK && (result = connect()) == CLIENT_E_OK ) {
            status = CONNECTED;
        }
        else {
            status = FAILED;
            stop();
        }
        return result;
    }
    return CLIENT_E_INVALIDSTATE;
}

int WoMicClient::startAsync(WoMicClientCallback callback) {
    if (doneStart) {
        startThread->join();
        startThread = NULL;
        doneStart = false;
    }
    if (startThread == NULL) {
        startThread = make_unique<thread>(static_cast<void (WoMicClient::*)(WoMicClientCallback)>(&WoMicClient::start), this, callback);
        return CLIENT_E_OK;
    }
    else {
        return CLIENT_E_INVALIDSTATE;
    }
}

int WoMicClient::stopAsync(WoMicClientCallback callback) {
    if (doneStop) {
        stopThread->join();
        stopThread = NULL;
        doneStop = false;
    }
    if (stopThread == NULL) {
        stopThread = make_unique<thread>(static_cast<void (WoMicClient::*)(WoMicClientCallback)>(&WoMicClient::stop), this, callback);
        return CLIENT_E_OK;
    }
    else {
        return CLIENT_E_INVALIDSTATE;
    }
}

float WoMicClient::getSpeedOff() {
    return speedOff;
}

WoMicClient* WoMicClient::setSpeedOff(float speedOff) {
    this->speedOff = speedOff;
    return this;
}

void WoMicClient::start(WoMicClientCallback callback) {
    int result = start();
    if (callback != NULL) {
        callback(result);
    }
    doneStart = true;
}

void WoMicClient::stop(WoMicClientCallback callback) {
    int result = stop();
    if (callback != NULL) {
        callback(status == FAILED ? failCode : result);
    }
    doneStop = true;
}


int WoMicClient::initOpusRecorder() {
    if (opusDecoder == NULL) {
        int error;
        opusDecoder = opus_decoder_create(sampleRate, channels, &error);
        if (error != OPUS_OK) {
            cout << "opus_decoder_create failed with error: " << error << endl;
            return CLIENT_E_OPUSDECODER;
        }
    }
    return CLIENT_E_OK;
}

int WoMicClient::destroyOpusRecorder() {
    if (opusDecoder != NULL) {
        opus_decoder_destroy(opusDecoder);
        opusDecoder = NULL;
    }
    return CLIENT_E_OK;
}

int WoMicClient::connect() {
    int result;
    struct addrinfo *addrInfoResult = NULL, hints;
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
    pingThread = make_unique<thread>(&WoMicClient::pingLoop, this);
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
            pingFailed(CLIENT_E_SEND_PING_0);
            cout << "CLIENT_E_SEND_PING_0 (" <<  CLIENT_E_SEND_PING_0 << ") error " << result << " " << WSAGetLastError() << endl;
            return CLIENT_E_SEND_PING_0;
        }

        len = 0;
        *((unsigned int*) (buffer + len)) = htonl(0); // packet length network order
        len += 4;
        result = send(clientSocket, buffer, len, 0);
        if (result == SOCKET_ERROR) {
            pingFailed(CLIENT_E_SEND_PING_1);
            cout << "CLIENT_E_SEND_PING_1 (" <<  CLIENT_E_SEND_PING_1 << ") error " << result << " " << WSAGetLastError() << endl;
            return CLIENT_E_SEND_PING_1;
        }
        Sleep(1000);
    }
}

void WoMicClient::pingFailed(int reason) {
    closesocket(clientSocket);
    clientSocket = INVALID_SOCKET;
    if (autoReconnect && status == CONNECTED) {
        reconnectAsync();
    }
    else if (status != STOPPING && status != FAILED) {
        failCode = reason;
        status = FAILED;
        stopAsync(failCallback);
    }
}

void WoMicClient::reconnectAsync() {
    status = RECONNECTING;
	if (reconnectThread != NULL && reconnectThread->joinable() ) {
        reconnectThread->join();
        reconnectThread = NULL;
	}
    reconnectThread = make_unique<thread>(&WoMicClient::reconnect, this);

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
    return CLIENT_E_RECONNECTSTATE;
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
    recvThread = make_unique<thread>(&WoMicClient::udpListen, this);
    cout << "UDP server started" << endl;
    return CLIENT_E_OK;
}

int WoMicClient::stopUDPServer() {
    if (serverSocket != INVALID_SOCKET) {
        closesocket(serverSocket);
        serverSocket = INVALID_SOCKET;
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
    unique_ptr<char[]> buffer = make_unique<char[]>(1024*1024);
    int result;
    char protocol;
    int opusLen = 0;
    unsigned char* opusData;
    int frameSize = sampleRate / 1000 * 20; //Not sure how to do this properly
    unique_ptr<short[]> outBuffer = make_unique<short[]>(frameSize * channels);
    cout << "recvFrom" << endl;
    while ((result = recvfrom(serverSocket, buffer.get(), 1024*1024, 0, NULL, NULL)) != SOCKET_ERROR) { //Neziurim kas atsiunte
        // {BYTE protocol should be 4} {BYTE unknown always 0} {WORD opus packet len} {WORD sequence NR} {DWORD timestamp} {BYTE volume (Paid version not gonna do this)} [BYTE * (opus packet len - 7) opus encoded audio data]
        protocol = buffer[0];
        if (protocol != 4) {
            cout << "Protocol not 4 skipping packet " << endl;
            continue;
        }
        opusLen = ntohs(*((unsigned short *) (buffer.get() + 2)));
        opusData = (unsigned char*) buffer.get() + 11;

        result = opus_decode(opusDecoder, opusData, opusLen - 7, outBuffer.get(), sampleRate , 0);
        if (result < 0) {
            cout << "opus_decode error " << result << endl;
        }
        else {
            if (result > 2000) {
                cout << "decoded " << result / channels << " samples" << endl;
            }
            //cout << "decoded " << result / channels << " samples" << endl;
            int buffered = audioQueue.was_size();
            for (int i = 0; i < min(result, CLIENT_QUEUE_SIZE - buffered); i++) {
                audioQueue.push(outBuffer[i]);
            }
            if (audioQueue.was_full()) {
                cout << "audioQueue full "  << endl;
            }
        }
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
        if (!(packetLen == 2 && buffer[len++] == 0 && buffer[len] == 4)) {
            len = 4;
            cout << "CLIENT_E_RECV_CHECKVERSION_2 (" <<  CLIENT_E_RECV_CHECKVERSION_2 << ") error " << packetLen << " " << int(buffer[len++]) << " " << int(buffer[len]) << endl;
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
    if (clientSocket != INVALID_SOCKET) {
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
        if (pingThread != NULL && pingThread->joinable()) {
            pingThread->join();
            pingThread = NULL;
        }
    }
    if (serverSocket != INVALID_SOCKET) {
        stopUDPServer();
    }
    return CLIENT_E_OK;
}
    // REFERENCE_TIME time units per second and per millisecond
#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);
int WoMicClient::openAudioDevice() {
    if (audioThread == NULL) {
        audioResult = CLIENT_E_NONE;
        audioThread = make_unique<thread>(&WoMicClient::startAudio, this);
        while(audioResult == CLIENT_E_NONE) {
            this_thread::sleep_for(chrono::milliseconds(1));
        }
        return audioResult;
    }
    else {
        return CLIENT_E_INVALIDSTATE;
    }
}

int WoMicClient::startAudio() {
    HRESULT hr;
    REFERENCE_TIME hnsRequestedDuration = 0;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pDevice = NULL;
    IMMDeviceCollection *pDevices = NULL;
    WAVEFORMATEX pwfx;
    IPropertyStore *pProps = NULL;
    PROPVARIANT varName;
    UINT cnt;
    IAudioClient *pAudioClient = NULL;
    bool found = false;
    bool comInitialized = false;

    hr = CoInitializeEx(NULL, 0);
    if (!(hr == S_OK || hr == S_FALSE)) {
        audioResult = CLIENT_E_DEVICE_COMINIT;
        return CLIENT_E_DEVICE_COMINIT;
    }
    hr = CoCreateInstance(
           CLSID_MMDeviceEnumerator, NULL,
           CLSCTX_ALL, IID_IMMDeviceEnumerator,
           (void**)&pEnumerator);
    if (FAILED(hr)) {
        cout << "CoCreateInstance failed" << hr << " " << HRESULT_CODE(hr) << endl;
        audioResult = CLIENT_E_DEVICE_CREATEENUMERATOR;
        goto audioStart_Exit;
    }
    comInitialized = true;
    hr = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pDevices);
    if (FAILED(hr)) {
        cout << "EnumAudioEndpoints failed" << hr << " " << HRESULT_CODE(hr) << endl;
        audioResult = CLIENT_E_DEVICE_ENUMAUDIO;
        goto audioStart_Exit;
    }

    hr = pDevices->GetCount(&cnt);
    if (FAILED(hr)) {
        cout << "GetCount failed" << hr << " " << HRESULT_CODE(hr) << endl;
        audioResult = CLIENT_E_DEVICE_COUNTAUDIO;
        goto audioStart_Exit;
    }

    for (UINT i = 0; i < cnt; i++)
    {
        // Get pointer to endpoint number i.
        hr = pDevices->Item(i, &pDevice);
        if (FAILED(hr)) {
            cout << "Item failed" << hr << " " << HRESULT_CODE(hr) << endl;
            audioResult = CLIENT_E_DEVICE_GETAUDIO;
            goto audioStart_Exit;
        }

        hr = pDevice->OpenPropertyStore(STGM_READ, &pProps);
        if (FAILED(hr)) {
            cout << "OpenPropertyStore failed" << hr << " " << HRESULT_CODE(hr) << endl;
            audioResult = CLIENT_E_DEVICE_OPENPROP;
            goto audioStart_Exit;
        }

        // Initialize container for property value.
        PropVariantInit(&varName);

        // Get the endpoint's friendly-name property.
        hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
        pProps->Release();

        if (FAILED(hr)) {
            cout << "GetValue failed" << hr << " " << HRESULT_CODE(hr) << endl;
            audioResult = CLIENT_E_DEVICE_GETPROP;
            goto audioStart_Exit;
        }
        found = wstring(varName.pwszVal) == device;
        PropVariantClear(&varName);
        if (found) {
            break;
        }
        pDevice->Release();
        pDevice = NULL;
    }

    pDevices->Release();
    pDevices = NULL;

    if (pDevice == NULL) {
        wcout << "Failed to get device " << device;
        audioResult = CLIENT_E_DEVICE_NOTFOUND;
        goto audioStart_Exit;
    }
    hr = pDevice->Activate(
                    IID_IAudioClient, CLSCTX_ALL,
                    NULL, (void**)&pAudioClient);
    if (FAILED(hr)) {
        cout << "Activate failed" << hr << " " << HRESULT_CODE(hr) << endl;
        audioResult = CLIENT_E_DEVICE_ACTIVATE;
        goto audioStart_Exit;
    }

    pDevice->Release();
    pDevice = NULL;

    // Call a helper function to negotiate with the audio
    // device for an exclusive-mode stream format.
    pwfx.wFormatTag = WAVE_FORMAT_PCM;
    pwfx.nChannels = channels;
    pwfx.nSamplesPerSec = sampleRate;
    pwfx.wBitsPerSample = 16; //short
    pwfx.nBlockAlign = pwfx.nChannels * pwfx.wBitsPerSample / 8;
    pwfx.nAvgBytesPerSec = pwfx.nSamplesPerSec * pwfx.nBlockAlign;
    pwfx.cbSize = 0;

    hr = pAudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, &pwfx, NULL);
    if (hr != S_OK) {
        cout << "Failed on IsFormatSupported with error " << hr << " " << HRESULT_CODE(hr) << " " << hr << " " << HRESULT_CODE(AUDCLNT_E_UNSUPPORTED_FORMAT) << endl;
        audioResult = CLIENT_E_DEVICE_FORMAT;
        goto audioStart_Exit;
    }
    // Initialize the stream to play at the minimum latency.
    hr = pAudioClient->GetDevicePeriod(NULL, &hnsRequestedDuration);
    if (FAILED(hr)) {
        cout << "GetDevicePeriod failed" << hr << " " << HRESULT_CODE(hr) << endl;
        audioResult = CLIENT_E_DEVICE_PERIOD;
        goto audioStart_Exit;
    }

    hr = pAudioClient->Initialize(
                         AUDCLNT_SHAREMODE_EXCLUSIVE,
                         AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                         hnsRequestedDuration * 2,
                         hnsRequestedDuration * 2,
                         &pwfx,
                         NULL);

    if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
        cout << "Realigning frame buffer" << endl;
        // Align the buffer if needed, see IAudioClient::Initialize() documentation
        UINT32 nFrames = 0;
        hr = pAudioClient->GetBufferSize(&nFrames);
        if (FAILED(hr)) {
            cout << "GetBufferSize  failed" << hr << " " << HRESULT_CODE(hr) << endl;
            audioResult = CLIENT_E_DEVICE_GETBUFFERSIZEREALIGN;
            goto audioStart_Exit;
        }

        hnsRequestedDuration = (REFERENCE_TIME)((double)REFTIMES_PER_SEC / pwfx.nSamplesPerSec * nFrames + 0.5);
        hr = pAudioClient->Initialize(
            AUDCLNT_SHAREMODE_EXCLUSIVE,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
            hnsRequestedDuration,
            hnsRequestedDuration,
            &pwfx,
            NULL);
    }

    if (FAILED(hr)) {
        cout << "Initialize failed" << hr << " " << HRESULT_CODE(hr) << endl;
        goto audioStart_Exit;
    }

    // Create an event handle and register it for
    // buffer-event notifications.
    hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (hEvent == NULL) {
        cout << "hEvent failed" << endl;
        audioResult = CLIENT_E_DEVICE_EVENT;
        goto audioStart_Exit;
    }

    hr = pAudioClient->SetEventHandle(hEvent);
    if (FAILED(hr)) {
        cout << "SetEventHandle failed" << hr << " " << HRESULT_CODE(hr) << endl;
        audioResult = CLIENT_E_DEVICE_SETEVENT;
        goto audioStart_Exit;
    }

    audioResult = CLIENT_E_OK;
    audioResult = audioDeviceLoop(pAudioClient);


    audioStart_Exit:
    if (pAudioClient != NULL) {
        cout << "pAudioClient releasing" << endl;
        pAudioClient->Release();
        pAudioClient = NULL;
    }
    if (pDevice != NULL) {
        cout << "pDevice releasing" << endl;
        pDevice->Release();
        pDevice = NULL;
    }
    if (pDevices != NULL) {
        pDevices->Release();
        pDevices = NULL;
    }
    if (comInitialized)
        CoUninitialize();
    return audioResult;
}

int WoMicClient::getAvailableDevices(vector<wstring>& devices) {
    int result = CLIENT_E_OK;
    HRESULT hr;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pDevice = NULL;
    IMMDeviceCollection *pDevices = NULL;
    IPropertyStore *pProps = NULL;
    PROPVARIANT varName;
    UINT cnt;
    bool comInitialized = false;
    vector<wstring> localDevices;

    hr = CoInitializeEx(NULL, 0);
    if (!(hr == S_OK || hr == S_FALSE)) {
        result = CLIENT_E_DEVICE_COMINIT;
        goto getAvailableDevices_Exit;
    }
    hr = CoCreateInstance(
           CLSID_MMDeviceEnumerator, NULL,
           CLSCTX_ALL, IID_IMMDeviceEnumerator,
           (void**)&pEnumerator);
    if (FAILED(hr)) {
        cout << "CoCreateInstance failed" << hr << " " << HRESULT_CODE(hr) << endl;
        result = CLIENT_E_DEVICE_CREATEENUMERATOR;
        goto getAvailableDevices_Exit;
    }
    comInitialized = true;
    hr = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pDevices);
    if (FAILED(hr)) {
        cout << "EnumAudioEndpoints failed" << hr << " " << HRESULT_CODE(hr) << endl;
        result = CLIENT_E_DEVICE_ENUMAUDIO;
        goto getAvailableDevices_Exit;
    }

    hr = pDevices->GetCount(&cnt);
    if (FAILED(hr)) {
        cout << "GetCount failed" << hr << " " << HRESULT_CODE(hr) << endl;
        result = CLIENT_E_DEVICE_COUNTAUDIO;
        goto getAvailableDevices_Exit;
    }
    for (UINT i = 0; i < cnt; i++)
    {
        // Get pointer to endpoint number i.
        hr = pDevices->Item(i, &pDevice);
        if (FAILED(hr)) {
            cout << "Item failed" << hr << " " << HRESULT_CODE(hr) << endl;
            result = CLIENT_E_DEVICE_GETAUDIO;
            goto getAvailableDevices_Exit;
        }

        hr = pDevice->OpenPropertyStore(STGM_READ, &pProps);
        if (FAILED(hr)) {
            cout << "OpenPropertyStore failed" << hr << " " << HRESULT_CODE(hr) << endl;
            result = CLIENT_E_DEVICE_OPENPROP;
            goto getAvailableDevices_Exit;
        }

        // Initialize container for property value.
        PropVariantInit(&varName);

        // Get the endpoint's friendly-name property.
        hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
        pProps->Release();

        if (FAILED(hr)) {
            cout << "GetValue failed" << hr << " " << HRESULT_CODE(hr) << endl;
            result = CLIENT_E_DEVICE_GETPROP;
            goto getAvailableDevices_Exit;
        }
        localDevices.push_back(wstring(varName.pwszVal));
        PropVariantClear(&varName);
        pDevice->Release();
        pDevice = NULL;
    }
    getAvailableDevices_Exit:
    if (pDevice != NULL) {
        pDevice->Release();
        pDevice = NULL;
    }
    if (pDevices != NULL) {
        pDevices->Release();
        pDevices = NULL;
    }
    if (comInitialized)
        CoUninitialize();

    if (result == CLIENT_E_OK) {
        devices.clear();
        devices.insert(devices.end(), localDevices.begin(), localDevices.end());
    }
    return result;
}


int WoMicClient::audioDeviceLoop(IAudioClient *pAudioClient) {
    HRESULT hr;
    int result;
    IAudioRenderClient *pRenderClient = NULL;
    UINT32 bufferFrameCount;
    BYTE *pData;
    unique_ptr<BYTE[]> preloaded;
    unsigned int preloadedLen = 0;
    bool playing = false;
    int speed = 0;
    // Get the actual size of the two allocated buffers.
    hr = pAudioClient->GetBufferSize(&bufferFrameCount);
    if (FAILED(hr)) {
        cout << "GetBufferSize in loop failed" << hr << " " << HRESULT_CODE(hr) << endl;
        result = CLIENT_E_DEVICE_GETBUFFERSIZE;
        goto audioDeviceLoop_Exit;
    }
    preloaded = make_unique<BYTE[]>(bufferFrameCount * channels * 2);
    hr = pAudioClient->GetService(
                         IID_IAudioRenderClient,
                         (void**)&pRenderClient);

    if (FAILED(hr)) {
        result = CLIENT_E_DEVICE_RENDER;
        goto audioDeviceLoop_Exit;
    }

    hr = pAudioClient->Start();  // Start playing.
    if (FAILED(hr)) {
        result = CLIENT_E_DEVICE_START;
        goto audioDeviceLoop_Exit;
    }
    playing = true;

    purgeAudioQueue(bufferFrameCount * channels);

    // Each loop fills one of the two buffers.

    while (true)
    {
        // Wait for next buffer event to be signaled.
        DWORD retval = WaitForSingleObject(hEvent, 2000);
        if (retval != WAIT_OBJECT_0)
        {
            // Event handle timed out after a 2-second wait.
            result = CLIENT_E_DEVICE_TIMEOUT;
            goto audioDeviceLoop_Exit;
            //goto Exit;
        }

        // Grab the next empty buffer from the audio device.
        hr = pRenderClient->GetBuffer(bufferFrameCount, &pData);
        if (FAILED(hr)) {
            result = CLIENT_E_DEVICE_GETBUFFER;
            goto audioDeviceLoop_Exit;
        }

        //EXIT_ON_ERROR(hr)
        unsigned int buffered = audioQueue.was_size();
        memcpy(pData, preloaded.get(), preloadedLen * 2);
        for (unsigned int i = preloadedLen; i < min(buffered, bufferFrameCount * channels) ; i++) {
            *(short*)(pData + i * 2) = (audioQueue.pop());
            if (speedOff != 0 && buffered - min(buffered, bufferFrameCount * channels) > sampleRate * speedOff && ++speed % 100 == 0) {
                audioQueue.pop();
                speed = 0;
            }
        }
        for (unsigned int i = preloadedLen + buffered; i < bufferFrameCount * channels; i++) { // if we ran out of data fill the rest with zeroes
            *(short*)(pData + i * 2) = 0; // should probably use an algorithm call instead of a for loop here
        }
        // 2 is bytes per sample

        hr = pRenderClient->ReleaseBuffer(bufferFrameCount, 0);
        if (FAILED(hr)) {
            result = CLIENT_E_DEVICE_RELEASEBUFFER;
            goto audioDeviceLoop_Exit;
        }
        //EXIT_ON_ERROR(hr)

        buffered = audioQueue.was_size();
        preloadedLen = min(buffered, bufferFrameCount * channels);
        for (unsigned int i = 0; i < preloadedLen; i++) { // Preload the next packet
            *(short*)(preloaded.get() + i * 2) = (audioQueue.pop());
            if (speedOff != 0 && buffered - preloadedLen > sampleRate * speedOff && ++speed % 100 == 0) {
                audioQueue.pop();
                speed = 0;
            }
        }
        if ( (cutOff != 0 && ((long long)(audioQueue.was_size()) - bufferFrameCount * channels) > sampleRate * cutOff) || clearBufferFlag) {
            clearBufferFlag = false;
            purgeAudioQueue(bufferFrameCount * channels);
        }
    }
    audioDeviceLoop_Exit:
    cout << "audioDeviceLoop failed " << result << endl;
    if (playing) {
        cout << "(ping playing" << endl;
        pAudioClient->Stop();  // Stop playing.
    }
    if (pRenderClient != NULL) {
        cout << "pRenderClient releasing" << endl;
        pRenderClient->Release();
        pRenderClient = NULL;
    }
    if (status != STOPPING && status != FAILED) {
        failCode = result;
        status = FAILED;
        stopAsync(failCallback);
    }
    return result;
}
void WoMicClient::purgeAudioQueue(int leave) {
    int signedBuffered = audioQueue.was_size();
    for(int i = 0; i < signedBuffered - leave; i++) {
        audioQueue.pop();
    }
    cout << "Purged audio queue " << audioQueue.was_size() << " was " << signedBuffered << " left " << leave << endl;
}

int WoMicClient::closeAudioDevice() {

    if (hEvent != NULL){
        CloseHandle(hEvent);
        hEvent = NULL;
    }
    if (audioThread != NULL) {
        audioThread->join();
        audioThread = NULL;
    }
    return CLIENT_E_OK;
}

int WoMicClient::stop() {
    if (status != FAILED) {
        status = STOPPING;
    }
    cout << "disconnect" << endl;
    disconnect();
    cout << "closeAudioDevice" << endl;
    closeAudioDevice();
    cout << "destroyOpusRecorder" << endl;
    destroyOpusRecorder();

    cout << "join reconnectThread" << endl;
	if (reconnectThread != NULL && reconnectThread->joinable() ) {
        reconnectThread->join();
        reconnectThread = NULL;
	}
    cout << "stopped" << endl;
    if (status == STOPPING) {
        status = WAITING;
    }
    return CLIENT_E_OK;
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

wstring WoMicClient::getDevice() {
    return device;
}

WoMicClient* WoMicClient::setDevice(wstring device) {
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

WoMicClientCallback WoMicClient::getFailCallback() {
    return failCallback;
}

WoMicClient* WoMicClient::setFailCallback(WoMicClientCallback callback) {
    failCallback = callback;
    return this;
}

int WoMicClient::getFailCode() {
    return failCode;
}

int WoMicClient::getSamplesInBuffer() {
    return audioQueue.was_size();
}

int WoMicClient::getSampleRate() {
    return sampleRate;
}

int WoMicClient::getChannels() {
    return channels;
}

void WoMicClient::setClearBufferFlag() {
    clearBufferFlag = true;
}



