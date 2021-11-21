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
        if ((result = initOpusRecorder()) == CLIENT_E_OK && (result = openAudioDevice()) == CLIENT_E_OK && (result = connect()) == CLIENT_E_OK ) {
            status = CONNECTED;
        }
        else {
            status = FAILED;
        }
        return result;
    }
    return CLIENT_E_INVALIDSTATE;
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
    char protocol;
    int opusLen = 0;
    unsigned char* opusData;
    int frameSize = sampleRate / 1000 * 20; //Not sure how to do this properly
    unique_ptr<short[]> outBuffer = std::make_unique<short[]>(frameSize * channels);
    cout << "recvFrom" << endl;
    while ((result = recvfrom(serverSocket, buffer.get(), 1024*1024, 0, NULL, NULL)) != SOCKET_ERROR) { //Neziurim kas atsiunte
        // {BYTE protocol should be 4} {BYTE unknown always 0} {WORD opus packet len} {WORD sequence NR} {DWORD timestamp} {BYTE volume (Paid version not gonna do this)} [BYTE * (opus packet len) opus encoded audio data]
        protocol = buffer[0];
        if (protocol != 4) {
            cout << "Protocol not 4 skipping packet " << endl;
        }
        opusLen = ntohs(*((unsigned short *) (buffer.get() + 2)));
        opusData = (unsigned char*) buffer.get() + 11;
        result = opus_decode(opusDecoder, opusData, opusLen, outBuffer.get(), sampleRate / 1000 * 20, 0);
        if (result < 0) {
            cout << "opus_decode error " << result << endl;
        }
        else {
            //cout << "decoded " << result / channels << " samples" << endl;
            for (int i = 0; i < result && !audioQueue.was_full(); i++) {
                if (outBuffer[i] != short(int(outBuffer[i]))) {
                    cout << "REEE" << endl;
                }
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
    return -1;
}
    // REFERENCE_TIME time units per second and per millisecond
#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

#define EXIT_ON_ERROR(hres)  \
              if (FAILED(hres)) { goto Exit; }

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);
int WoMicClient::openAudioDevice() {
    HRESULT hr;
    REFERENCE_TIME hnsRequestedDuration = 0;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pDevice = NULL;
    IMMDeviceCollection *pDevices = NULL;
    WAVEFORMATEX pwfx;
    CoInitializeEx(NULL, NULL);


    LPWSTR pwszID = NULL;
    IPropertyStore *pProps = NULL;
    PROPVARIANT varName;
    UINT  cnt;
    bool found = false;

    hr = CoCreateInstance(
           CLSID_MMDeviceEnumerator, NULL,
           CLSCTX_ALL, IID_IMMDeviceEnumerator,
           (void**)&pEnumerator);
    cout << "CoCreateInstance " << hr << " " << HRESULT_CODE(hr) << endl;
    EXIT_ON_ERROR(hr)
    hr = pEnumerator->EnumAudioEndpoints(
                        eRender, DEVICE_STATE_ACTIVE, &pDevices);
    cout << "EnumAudioEndpoints " << hr << " " << HRESULT_CODE(hr) << endl;
    EXIT_ON_ERROR(hr)

    hr = pDevices->GetCount(&cnt);
    cout << "GetCount " << hr << " " << HRESULT_CODE(hr) << endl;
    EXIT_ON_ERROR(hr)
    for (ULONG i = 0; i < cnt; i++)
    {
        // Get pointer to endpoint number i.
        hr = pDevices->Item(i, &pDevice);
        EXIT_ON_ERROR(hr)

        // Get the endpoint ID string.
        hr = pDevice->GetId(&pwszID);
        EXIT_ON_ERROR(hr)

        hr = pDevice->OpenPropertyStore(
                          STGM_READ, &pProps);
        EXIT_ON_ERROR(hr)
        // Initialize container for property value.
        PropVariantInit(&varName);

        // Get the endpoint's friendly-name property.
        hr = pProps->GetValue(
                       PKEY_Device_FriendlyName, &varName);
        EXIT_ON_ERROR(hr)

        // Print endpoint friendly name and endpoint ID.
        printf("Endpoint %d: \"%S\" (%S)\n",
               i, varName.pwszVal, pwszID);

        CoTaskMemFree(pwszID);
        pwszID = NULL;
        pProps->Release();
        found = wstring(varName.pwszVal) == L"Line 1 (Virtual Audio Cable)";
        PropVariantClear(&varName);
        if (found) {
            break;
        }
        pDevice->Release();
    }

    hr = pDevice->Activate(
                    IID_IAudioClient, CLSCTX_ALL,
                    NULL, (void**)&pAudioClient);
    cout << "Activate" << hr << " " << HRESULT_CODE(hr) << endl;
    pDevice->Release();

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
        return CLIENT_E_DEVICE_FORMAT;
    }
    // Initialize the stream to play at the minimum latency.
    hr = pAudioClient->GetDevicePeriod(NULL, &hnsRequestedDuration);
    cout << "GetDevicePeriod" << hr << " " << HRESULT_CODE(hr) << endl;
    EXIT_ON_ERROR(hr)

    hr = pAudioClient->Initialize(
                         AUDCLNT_SHAREMODE_EXCLUSIVE,
                         AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                         hnsRequestedDuration,
                         hnsRequestedDuration,
                         &pwfx,
                         NULL);

    cout << "Initialize" << hr << " " << HRESULT_CODE(hr) << endl;
    if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
        // Align the buffer if needed, see IAudioClient::Initialize() documentation
        UINT32 nFrames = 0;
        hr = pAudioClient->GetBufferSize(&nFrames);
        EXIT_ON_ERROR(hr)
        hnsRequestedDuration = (REFERENCE_TIME)((double)REFTIMES_PER_SEC / pwfx.nSamplesPerSec * nFrames + 0.5);
        hr = pAudioClient->Initialize(
            AUDCLNT_SHAREMODE_EXCLUSIVE,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
            hnsRequestedDuration,
            hnsRequestedDuration,
            &pwfx,
            NULL);
        cout << "Initialize2" << hr << " " << HRESULT_CODE(hr) << endl;
    }
    EXIT_ON_ERROR(hr)
    // Create an event handle and register it for
    // buffer-event notifications.
    hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    cout << "CreateEvent" << hEvent << endl;
    if (hEvent == NULL)
    {
        hr = E_FAIL;
        goto Exit;
    }

    hr = pAudioClient->SetEventHandle(hEvent);
    cout << "SetEventHandle" << hr << " " << HRESULT_CODE(hr) << endl;
    EXIT_ON_ERROR(hr);
    audioThread = std::make_unique<thread>(&WoMicClient::audioDeviceLoop, this);
    return CLIENT_E_OK;
Exit:
    cout << "hr " << hr << " " << HRESULT_CODE(hr) << endl;
    return -1000;
}
int WoMicClient::audioDeviceLoop() {
    HRESULT result;
    IAudioRenderClient *pRenderClient = NULL;
    UINT32 bufferFrameCount;
    BYTE *pData;

    // Get the actual size of the two allocated buffers.
    result = pAudioClient->GetBufferSize(&bufferFrameCount);
    //EXIT_ON_ERROR(hr)

    result = pAudioClient->GetService(
                         IID_IAudioRenderClient,
                         (void**)&pRenderClient);
    if (FAILED(result)) {
        return -10001;
    }
    // Ask MMCSS to temporarily boost the thread priority
    // to reduce glitches while the low-latency stream plays.


    result = pAudioClient->Start();  // Start playing.

    int asd = 0;
    // Each loop fills one of the two buffers.
    while (true)
    {
        // Wait for next buffer event to be signaled.
        DWORD retval = WaitForSingleObject(hEvent, 2000);
        if (retval != WAIT_OBJECT_0)
        {
            // Event handle timed out after a 2-second wait.
            pAudioClient->Stop();
            result = ERROR_TIMEOUT;
            return CLIENT_E_DEVICE_TIMEOUT;
            //goto Exit;
        }

        // Grab the next empty buffer from the audio device.
        result = pRenderClient->GetBuffer(bufferFrameCount, &pData);
        //EXIT_ON_ERROR(hr)
        unsigned int buffered = audioQueue.was_size();
        if (asd++ % 1000 == 0) {
            cout << "Need " << bufferFrameCount * channels << "buffered" << audioQueue.was_size() << endl;
        }
        for (int i = 0; i < min(buffered, bufferFrameCount * channels); i++) {
            *(short*)(pData + i * 2) = short(audioQueue.pop());
        }

        for (int i = buffered; i < bufferFrameCount * channels; i++) { // if we ran out of data fill the rest with zeroes
            *(short*)(pData + i * 2) = 0;   // should probably use an algorithm call instead of a for loop here
        }
        // 2 is bytes per sample
        // Load the buffer with data from the audio source.
        //hr = pMySource->LoadData(bufferFrameCount, pData, &flags);
        //EXIT_ON_ERROR(hr)

        result = pRenderClient->ReleaseBuffer(bufferFrameCount, 0);
        //EXIT_ON_ERROR(hr)
    }
    result = pAudioClient->Stop();  // Stop playing.
    pRenderClient->Release();
}

int WoMicClient::closeAudioDevice() {

    if (hEvent != NULL){
        CloseHandle(hEvent);
    }
    return -1;
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
