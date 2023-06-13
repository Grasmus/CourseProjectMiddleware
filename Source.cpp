#include <iostream>
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <string>
#include <map>

#pragma comment (lib, "Ws2_32.lib")

#define DEFAULT_PORT "27015"
#define DEFAULT_BUFLEN 512

using namespace std;

static bool exitThread{ false };
static map<string, bool> nodes{};

struct Data
{
    char nodeName[100]{};
    int a;
    int b;
};

SOCKET generateSocket(PCSTR port)
{
    struct addrinfo* result = nullptr, * ptr = nullptr, hints{};

    hints.ai_family = AF_UNSPEC;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_addr = NULL;
    hints.ai_canonname = NULL;
    hints.ai_next = NULL;

    int iResult = getaddrinfo(NULL, port, &hints, &result);

    if (iResult != 0)
    {
        cerr << "getaddrinfo failed: " << iResult << endl;

        return -1;
    }

    SOCKET listenSocket{ INVALID_SOCKET };

    int optval{ 1 };

    for (auto resultIterator = result; resultIterator != NULL; resultIterator = resultIterator->ai_next)
    {
        listenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

        if (listenSocket == INVALID_SOCKET)
        {
            continue;
        }

        if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(int)) == -1)
        {
            cerr << "setsockopt() error" << endl;

            closesocket(listenSocket);

            return -1;
        }

        iResult = bind(listenSocket, result->ai_addr, (int)result->ai_addrlen);

        if (iResult == SOCKET_ERROR)
        {
            cerr << "bind failed with error: " << WSAGetLastError() << endl;

            freeaddrinfo(result);
            closesocket(listenSocket);

            return -1;
        }
    }

    freeaddrinfo(result);

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        cerr << "Listen failed with error: " << WSAGetLastError() << endl;

        closesocket(listenSocket);

        return -1;
    }

    return listenSocket;
}

SOCKET establishConnection(SOCKET socket)
{
    SOCKET ClientSocket{ INVALID_SOCKET };

    ClientSocket = accept(socket, NULL, NULL);

    if (ClientSocket == INVALID_SOCKET)
    {
        cerr << "Accept failed: " << WSAGetLastError() << endl;

        closesocket(socket);

        return -1;
    }

    return ClientSocket;
}

void sendMessage(SOCKET socket, const char* sendBuf, int size)
{
    int iResult{ send(socket, sendBuf, size, 0) };

    if (iResult == SOCKET_ERROR)
    {
        cerr << "send failed: " << WSAGetLastError() << endl;

        closesocket(socket);

        return;
    }
}

DWORD WINAPI readNewNode(LPVOID socketParam)
{
    SOCKET listenSocket{ *(SOCKET*)socketParam };
    int  activity{};
    Data data{};

    while (!exitThread)
    {
        SOCKET clientSocket{ establishConnection(listenSocket) };

        if (clientSocket == -1)
        {
            cerr << "Can't connect to client" << endl;

            continue;
        }

        char messageChunk[DEFAULT_BUFLEN]{};

        int size = recv(clientSocket, messageChunk, DEFAULT_BUFLEN - 1, 0);

        if (size == sizeof(Data))
        {
            memcpy(&data, messageChunk, sizeof(Data));

            int resonse{ data.a + data.b };

            sendMessage(clientSocket, (char*)&resonse, sizeof(int));

            nodes[data.nodeName] = true;
        }

        closesocket(clientSocket);
    }

    return 0;
}

vector<string> getNodesData()
{
    vector<string> nodeNames{};

    //Get node names from db

    return nodeNames;
}

SOCKET connectToNode(string nodeName, PCSTR port)
{
    struct addrinfo* result = nullptr, * ptr = nullptr, hints{};

    hints.ai_family = AF_UNSPEC;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_addr = NULL;
    hints.ai_canonname = NULL;
    hints.ai_next = NULL;

    int iResult = getaddrinfo(nodeName.c_str(), port, &hints, &result);

    if (iResult != 0)
    {
        cerr << "getaddrinfo failed: " << iResult << endl;

        return 0;
    }

    int optval{ 1 };

    SOCKET nodeSocket{};

    addrinfo* resultIterator{};

    for (resultIterator = result; resultIterator != NULL; resultIterator = resultIterator->ai_next)
    {
        nodeSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

        if (nodeSocket == INVALID_SOCKET)
        {
            continue;
        }

        if (connect(nodeSocket, resultIterator->ai_addr, resultIterator->ai_addrlen) == -1)
        {
            closesocket(nodeSocket);

            continue;
        }

        break;
    }

    if (resultIterator == NULL)
    {
        cerr << "Can`t connect to node: " << nodeName << endl;

        return 0;
    }

    freeaddrinfo(resultIterator);

    return nodeSocket;
}

int main()
{
    WSADATA wsaData{};
    int iResult{ WSAStartup(MAKEWORD(2, 2), &wsaData) };

    if (iResult != 0)
    {
        cerr << "WSAStartup failed with error: " << iResult << endl;
        return 1;
    }

    SOCKET listenSocket{ generateSocket(DEFAULT_PORT) };

    if (listenSocket == -1)
    {
        WSACleanup();

        return 1;
    }

    HANDLE connectNewClientThread
    {
        CreateThread(
            NULL,
            0,
            readNewNode,
            (LPVOID)&listenSocket,
            0,
            0
        )
    };

    if (connectNewClientThread == NULL)
    {
        cerr << "Can't create thread connectNewClientThread" << endl;

        closesocket(listenSocket);

        WSACleanup();

        return 0;
    }

    vector<string> nodeNames{ getNodesData() };

    for (string nodeName : nodeNames)
    {
        nodes[nodeName] = false;
    }

    for (size_t i{}; i < nodeNames.size(); i++)
    {
        SOCKET nodeSocket{ connectToNode(nodeNames[i], DEFAULT_PORT) };

        Data sendData{};

        sendData.a = 10;
        sendData.b = 65;

        sendMessage(nodeSocket, (char*)&sendData, sizeof(Data));

        int receiveData{};

        if (recv(nodeSocket, (char*)&receiveData, sizeof(int), NULL) <= 0)
        {
            cerr << "Can`t receive data from: " << nodeNames[i] << endl;

            closesocket(nodeSocket);

            continue;
        }

        if (receiveData != sendData.a + sendData.b)
        {
            cerr << "Invalid response from: " << nodeNames[i] << endl;
        }
        else
        {
            nodes[nodeNames[i]] = true;
        }
    }

    exitThread = true;

    WaitForSingleObject(connectNewClientThread, INFINITE);

    closesocket(listenSocket);

    WSACleanup();

    return 0;
}