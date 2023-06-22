#include <iostream>
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <string>
#include <map>
#include <Windows.h>
#include <fstream>
#include <limits>
#include <random>
#include <stdio.h>

#pragma comment (lib, "Ws2_32.lib")

#define DEFAULT_PORT "27015"
#define DEFAULT_BUFLEN 512

using namespace std;

struct Data
{
    char nodeName[100]{};
    int a;
    int b;
};

struct userAddressInfo 
{
    long long id{ 0 };
    string pcAddress;
    bool isDecisionMakerCenter{};
};

struct Activity
{
    bool isActive{};
    bool isDecisionMakerActive{}; //makes sense only if isDecisionMakerCenter = true
};


static bool exitThread{ false };
static map<userAddressInfo, Activity> nodes{};
static bool isDecisionMakerCenter{};
static bool isDecisionMakerCenterActive{};

//================DATABASE PROTOTYPES=====================
bool fileExists(const string& filename);
long long generateUniqueId();
void createDataBase(vector<userAddressInfo>* addresses);
vector<userAddressInfo> readAddresses();
void deleteAddressById(string filename, int idToDelete);
void updateDataBase(string filename, vector<userAddressInfo>* users);
void addAddressToDataBase(string filename, string newAddress);
//================DATABASE PROTOTYPES=====================

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

DWORD WINAPI connectNewNode(LPVOID socketParam)
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

            int response{ data.a + data.b };

            sendMessage(clientSocket, (char*)&response, sizeof(int));
            sendMessage(clientSocket, (char*)&isDecisionMakerCenterActive, sizeof(bool));

            for (auto iter{ nodes.begin() }; iter != nodes.end(); iter++)
            {
                if (iter->first.pcAddress == data.nodeName)
                {
                    iter->second.isActive = true;

                    if (isDecisionMakerCenter && isDecisionMakerCenterActive)
                    {
                        iter->second.isDecisionMakerActive = generateUniqueId() <= 7500000;

                        sendMessage(clientSocket, (char*)&iter->second.isDecisionMakerActive, sizeof(bool));

                        break;
                    }
                }
            }
        }
        else if (size == sizeof(bool))
        {
            memcpy(&isDecisionMakerCenterActive, messageChunk, sizeof(bool));
        }

        closesocket(clientSocket);
    }

    return 0;
}

DWORD WINAPI decisionMaker(LPVOID socketParam)
{
    Sleep(60000);

    if (isDecisionMakerCenterActive)
    {
        for (auto iter{ nodes.begin() }; iter != nodes.end(); iter++)
        {
            if (iter->second.isActive && iter->first.isDecisionMakerCenter)
            {
                SOCKET nodeSocket{ connectToNode(iter->first.pcAddress, DEFAULT_PORT) };

                bool sendData = generateUniqueId() <= 7500000;

                sendMessage(nodeSocket, (char*)&sendData, sizeof(Data));

                cout << "decisionMaker of " << iter->first.pcAddress << " is " << sendData << endl;
            }
        }
    }
}

vector<userAddressInfo> getNodesData()
{
    vector<userAddressInfo> nodeNames{};

    //Get node names from db
    nodeNames = readAddresses();

    return nodeNames;
}

SOCKET connectToNode(string nodeName, PCSTR port)
{
    struct addrinfo* result = nullptr, * ptr = nullptr, hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
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
    // =========================DATABASE====================================
    int idToDelete;
    string addressToAdd;
    vector<userAddressInfo> addresses;

    addresses.push_back({ generateUniqueId(),"DESKTOP-LT61FS4", true });
    addresses.push_back({ generateUniqueId(),"DESKTOP-09499TF", true });
    addresses.push_back({ generateUniqueId(),"DESKTOP-H2RFUD", true });

    if (fileExists("database.dat")) {
        cout << "File already exists." << endl;
    }
    else {
        createDataBase(&addresses);
    }

    // =========================DATABASE====================================

    vector<userAddressInfo> nodesData{ getNodesData() };
    for (size_t i = 0; i < nodesData.size(); i++)
    {
        cout << nodesData[i].pcAddress << endl;
    }

    for (auto nodeData : nodesData)
    {
        nodes[nodeData].isActive = false;
    }

    SOCKET listenSocket{ generateSocket(DEFAULT_PORT) };

    if (listenSocket == -1)
    {
        WSACleanup();

        return 1;
    }

    char hostname[100]{};

    if (gethostname(hostname, sizeof(hostname)) == -1) 
    {
        cerr << "Cannot get hostname." << endl;

        WSACleanup();

        closesocket(listenSocket);

        exit(1);
    };

    string smt;

    for (size_t i{}; i < nodesData.size(); i++)
    {
        /*cout << "Enter something and press enter to continue: " << endl;
        cin >> smt;*/
        // There's a bug here, if one of the computers turns on later, and you're already connected, it outputs
        // errors to the console, but then, when the second computer (KOLYAS) still starts the program, the first one
        // receives a message, but this is after the errors are displayed! That is, for normal operation
        // you need to start all computers at the same time. And so everything works :) PS. I shouldn't have bothered to create a socket, everything is fine there.

        SOCKET nodeSocket{ connectToNode(nodesData[i].pcAddress, DEFAULT_PORT) }; // <<<

        Data sendData{};

        memcpy(sendData.nodeName, hostname, sizeof(sendData.nodeName));

        sendData.a = 10;
        sendData.b = 65;

        sendMessage(nodeSocket, (char*)&sendData, sizeof(Data));

        cout << "Sending message: " << sendData.a << " " << sendData.b << " to " << nodesData[i].pcAddress << endl;

        int receiveData{};
        bool receiveIsDecisionMakerNodeActive{};
        bool receiveIsDecisionMakerActive{};

        if (recv(nodeSocket, (char*)&receiveData, sizeof(int), NULL) <= 0)
        {
            cerr << "Can`t receive data from: " << nodesData[i].pcAddress << endl;

            closesocket(nodeSocket);

            continue;
        }

        if (receiveData != sendData.a + sendData.b)
        {
            cerr << "Invalid response from: " << nodesData[i].pcAddress << endl;
        }
        else
        {
            nodes[nodesData[i]].isActive = true;
            cout << "Message was received" << endl;

            if (nodesData[i].isDecisionMakerCenter && isDecisionMakerCenter)
            {
                if (recv(nodeSocket, (char*)&receiveIsDecisionMakerNodeActive, sizeof(bool), NULL) <= 0)
                {
                    cerr << "Can`t receive data from: " << nodesData[i].pcAddress << endl;

                    closesocket(nodeSocket);

                    continue;
                }

                if (receiveIsDecisionMakerNodeActive)
                {
                    if (recv(nodeSocket, (char*)&receiveIsDecisionMakerActive, sizeof(bool), NULL) <= 0)
                    {
                        cerr << "Can`t receive data from: " << nodesData[i].pcAddress << endl;

                        closesocket(nodeSocket);

                        continue;
                    }

                    isDecisionMakerCenterActive = receiveIsDecisionMakerActive;
                }
            }
        }

        closesocket(nodeSocket);
    }

    HANDLE connectNewNodeThread
    {
        CreateThread(
            NULL,
            0,
            connectNewNode,
            (LPVOID)&listenSocket,
            0,
            0
        )
    };

    if (connectNewNodeThread == NULL)
    {
        cerr << "Can't create thread connectNewNodeThread" << endl;

        closesocket(listenSocket);

        WSACleanup();

        return 1;
    }

    HANDLE decisionMakerCenterThread{};

    if (isDecisionMakerCenter)
    {
        decisionMakerCenterThread =
            CreateThread(
                NULL,
                0,
                decisionMaker,
                0,
                0,
                0
            );

        if (decisionMakerCenterThread == NULL)
        {
            cerr << "Can't create thread decisionMakerCenterThread" << endl;

            exitThread = true;

            WaitForSingleObject(connectNewNodeThread, INFINITE);

            closesocket(listenSocket);

            WSACleanup();

            return 1;
        }
    }

    exitThread = true;

    WaitForSingleObject(connectNewNodeThread, INFINITE);
    WaitForSingleObject(decisionMakerCenterThread, INFINITE);

    closesocket(listenSocket);

    WSACleanup();

    return 0;
}

//================DATABASE REALIZATION=====================
bool fileExists(const string& filename)
{
    ifstream file(filename);
    return file.good();
}
void createDataBase(vector<userAddressInfo>* addresses) {
    fstream file("database.dat", ios::in | ios::out | ios::app);

    if (!file) {
        cerr << "Failed to open the database file." << endl;
        exit(EXIT_FAILURE);
    }
    for (const auto& address : *addresses) {
        file << address.id << " " << address.pcAddress << " " << address.isDecisionMakerCenter << '\n';
    }
    file.close();
}
vector<userAddressInfo> readAddresses() {
    char hostname[64];
    if (gethostname(hostname, sizeof(hostname)) == -1) {
        cout << "Cannot get hostname." << endl;
    };
    vector<userAddressInfo> users;
    userAddressInfo tempUser;
    fstream file("database.dat", ios::in | ios::out | ios::app);
    if (!file) {
        cerr << "Failed to open the database file." << endl;
        exit(EXIT_FAILURE);
    }
    while (file) {
        file >> tempUser.id >> tempUser.pcAddress >> tempUser.isDecisionMakerCenter;
        if (!file)
            break;
        if (tempUser.pcAddress != hostname) {
            users.push_back(tempUser);
        }
        else
        {
            isDecisionMakerCenter = tempUser.isDecisionMakerCenter;
        }
    }
    file.close();
    return users;
}
void deleteAddressById(string filename, int idToDelete) {
    ifstream inputFile(filename);
    if (!inputFile) {
        std::cerr << "Failed to open the database file." << std::endl;
        return;
    }
    vector<userAddressInfo> addresses;
    userAddressInfo tempAddress;
    while (inputFile >> tempAddress.id >> tempAddress.pcAddress >> tempAddress.isDecisionMakerCenter) {
        if (tempAddress.id != idToDelete) {
            addresses.push_back(tempAddress);
        }
    }
    inputFile.close();
    updateDataBase(filename, &addresses);
}
void updateDataBase(string filename, vector<userAddressInfo>* addresses) {
    ofstream outputFile(filename, std::ios::trunc);
    if (!outputFile) {
        std::cerr << "Failed to open the database file for writing." << std::endl;
        return;
    }

    for (const auto& address : *addresses) {
        outputFile << address.id << ' ' << address.pcAddress << ' ' << address.isDecisionMakerCenter << '\n';
    }
    outputFile.close();
}
void addAddressToDataBase(string filename, string newAddress) {
    vector<userAddressInfo> addresses = readAddresses();
    for (const auto& address : addresses) {
        if (address.pcAddress == newAddress) {
            cerr << "This address is already exists in database." << endl;
            return;
        }
    }
    ofstream file("database.dat", ios::app);
    if (!file.is_open()) {
        cerr << "Failed to open the database file in addAddressToDataBase." << endl;
        return;
    }
    file << generateUniqueId() << " " << newAddress << '\n';
    file.close();
    cout << "Address was successfully added to database." << endl;
}
long long generateUniqueId() {
    std::random_device rd;
    std::mt19937_64 generator(rd());
    std::uniform_int_distribution<long long> distribution;
    return distribution(generator) % (10000000 + 1);
}
//================DATABASE REALIZATION=====================