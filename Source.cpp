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
#include "Logger.h"

#pragma comment (lib, "Ws2_32.lib")

constexpr auto DEFAULT_PORT = "27015";
constexpr auto DEFAULT_BUFLEN = 512;
constexpr auto DECISIONMAKER_WAIT_MILIS = 10000;

using namespace std;

struct Data
{
    char nodeName[100]{};
    int a;
    int b;
};

struct Activity
{
    bool isActive{};
    bool isDecisionMakerActive{}; //makes sense only if isDecisionMakerCenter = true
};

struct userAddressInfo 
{
    long long id{ 0 };
    string pcAddress;
    bool isDecisionMakerCenter{};
    Activity activity{}; //makes sense only if isDecisionMakerCenter = true
};

static unique_ptr<loggernamespace::Logger> logger{};
static int decisionMakersAmount{};
static bool exitThread{ false };
static map<string, userAddressInfo> nodes{};
static bool isDecisionMakerCenter{};
static bool isDecisionMakerCenterActive{};
// We need to use protorypes
SOCKET connectToNode(string nodeName, PCSTR port);

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
            logger->addLog("setsockopt() error");

            closesocket(listenSocket);

            return -1;
        }

        iResult = bind(listenSocket, result->ai_addr, (int)result->ai_addrlen);

        if (iResult == SOCKET_ERROR)
        {
            logger->addLog("bind failed with error: " + to_string(WSAGetLastError()));

            freeaddrinfo(result);
            closesocket(listenSocket);

            return -1;
        }
    }

    void* addr{};

    char ipstr[INET6_ADDRSTRLEN]{};

    if (result->ai_family == AF_INET) 
    {
        struct sockaddr_in* ipv4 = (struct sockaddr_in*)result->ai_addr;
        addr = &(ipv4->sin_addr);
    }
    else 
    {
        struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)result->ai_addr;
        addr = &(ipv6->sin6_addr);
    }

    inet_ntop(result->ai_family, addr, ipstr, sizeof ipstr);

    logger->setAddress(ipstr);

    freeaddrinfo(result);

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        logger->addLog("Listen failed with error: " + to_string(WSAGetLastError()));

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
        logger->addLog("Accept failed: " + to_string(WSAGetLastError()));

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
        logger->addLog("send failed: " + to_string(WSAGetLastError()));

        closesocket(socket);

        return;
    }
}

long long generateNum(int min, int max) {
    std::random_device rd;
    std::mt19937_64 generator(rd());
    std::uniform_int_distribution<long long> distribution(min, max);
    return distribution(generator);
}

DWORD WINAPI connectNewNode(LPVOID socketParam)
{
    SOCKET listenSocket{ *(SOCKET*)socketParam };
    int  activity{};
    Data data{};

    while (!exitThread)
    {
        SOCKET clientSocket{ establishConnection(listenSocket) };

        logger->addLog("Conecting to listenSocket");

        if (clientSocket == -1)
        {
            logger->addLog("Can't connect to client");

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

            nodes[data.nodeName].activity.isActive = true;

            if (isDecisionMakerCenter && isDecisionMakerCenterActive)
            {
                nodes[data.nodeName].activity.isDecisionMakerActive = generateNum(0, decisionMakersAmount) >= decisionMakersAmount / 2;

                sendMessage(clientSocket, (char*)&nodes[data.nodeName].activity.isDecisionMakerActive, sizeof(bool));

                logger->addLog("ConnectNewNode make center active:  " + to_string(nodes[data.nodeName].activity.isDecisionMakerActive));
            }
        }
        else if (size == sizeof(bool))
        {
            memcpy(&isDecisionMakerCenterActive, messageChunk, sizeof(bool));

            logger->addLog("Is decision maker active: " + to_string(isDecisionMakerCenterActive));
        }

        closesocket(clientSocket);
    }

    return 0;
}

DWORD WINAPI decisionMaker(LPVOID socketParam)
{
    while (!exitThread) {
        Sleep(DECISIONMAKER_WAIT_MILIS);

        if (isDecisionMakerCenterActive)
        {
            for (auto iter{ nodes.begin() }; iter != nodes.end(); iter++)
            {
                if (iter->second.activity.isActive && iter->second.isDecisionMakerCenter)
                {
                    SOCKET nodeSocket{ connectToNode(iter->second.pcAddress, DEFAULT_PORT) };

                    bool sendData = generateNum(0, decisionMakersAmount) >= decisionMakersAmount / 2;

                    sendMessage(nodeSocket, (char*)&sendData, sizeof(bool));

                    logger->addLog("decisionMaker of " + iter->second.pcAddress + " is " + to_string(sendData));
                }
            }
        }
    }
    
    return 0;
}

vector<userAddressInfo> getNodesData()
{
    vector<userAddressInfo> nodeNames{};

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
        logger->addLog("getaddrinfo failed: " + to_string(iResult) + " Name: " + nodeName);

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
        return 0;
    }

    freeaddrinfo(resultIterator);

    return nodeSocket;
}

int main()
{
    logger = unique_ptr<loggernamespace::Logger>(new loggernamespace::Logger());

    logger->initialize();

    if (logger->isInitialized())
    {
        cerr << "Can't create logger" << endl;

        return 1;
    }

    WSADATA wsaData{};
    int iResult{ WSAStartup(MAKEWORD(2, 2), &wsaData) };

    if (iResult != 0)
    {
        logger->addLog("WSAStartup failed with error: " + to_string(iResult));

        return 1;
    }

#pragma region database

    int idToDelete;
    string addressToAdd;
    vector<userAddressInfo> addresses;

    addresses.push_back({ generateUniqueId(),"DESKTOP-LT61FS4", true });
    addresses.push_back({ generateUniqueId(),"DESKTOP-09499TF", true });
    addresses.push_back({ generateUniqueId(),"DESKTOP-H2RFUD7", true });

    if (fileExists("database.dat")) 
    {
        logger->addLog("Database file already exists.");
    }
    else 
    {
        createDataBase(&addresses);
    }

    vector<userAddressInfo> nodesData{ getNodesData() };

    for (size_t i = 0; i < nodesData.size(); i++)
    {
        nodes[nodesData[i].pcAddress] = nodesData[i];
    }

#pragma endregion

    SOCKET listenSocket{ generateSocket(DEFAULT_PORT) };

    if (listenSocket == -1)
    {
        WSACleanup();

        return 1;
    }

    char hostname[100]{};

    if (gethostname(hostname, sizeof(hostname)) == -1) 
    {
        logger->addLog("Cannot get hostname.");

        WSACleanup();

        closesocket(listenSocket);

        exit(1);
    };

    string smt;
    
    bool ifTheOnlyDecisionMaker{ true };

    for (size_t i{}; i < nodesData.size(); i++)
    {
        //cout << "Connecting to:  " << nodesData[i].pcAddress << endl;

        logger->addLog("Connecting to:  " + nodesData[i].pcAddress);

        SOCKET nodeSocket{ connectToNode(nodesData[i].pcAddress, DEFAULT_PORT) }; // <<<
        if (!nodeSocket)
            continue;
        else {
            if (nodesData[i].isDecisionMakerCenter && isDecisionMakerCenter) {
                ifTheOnlyDecisionMaker = false;
            }
        }

        logger->addLog("Connected to " + nodesData[i].pcAddress + " succsessfully");

        Data sendData{};

        memcpy(sendData.nodeName, hostname, sizeof(sendData.nodeName));

        sendData.a = 10;
        sendData.b = 65;

        sendMessage(nodeSocket, (char*)&sendData, sizeof(Data));

        logger->addLog("Sending message: " + 
            to_string(sendData.a) + " " + 
            to_string(sendData.b) + " to " + 
            nodesData[i].pcAddress);

        int receiveData{};
        bool receiveIsDecisionMakerNodeActive{};
        bool receiveIsDecisionMakerActive{};

        if (recv(nodeSocket, (char*)&receiveData, sizeof(int), NULL) <= 0)
        {
            logger->addLog("Can`t receive data from: " + nodesData[i].pcAddress);

            closesocket(nodeSocket);

            continue;
        }

        if (receiveData != sendData.a + sendData.b)
        {
            logger->addLog("Invalid response from: " + nodesData[i].pcAddress);
        }
        else
        {
            nodes[nodesData[i].pcAddress].activity.isActive = true;

            logger->addLog("Message was received");

            if (nodesData[i].isDecisionMakerCenter && isDecisionMakerCenter)
            {
                decisionMakersAmount++;

                if (recv(nodeSocket, (char*)&receiveIsDecisionMakerNodeActive, sizeof(bool), NULL) <= 0)
                {
                    logger->addLog("Can`t receive data from: " + nodesData[i].pcAddress);

                    closesocket(nodeSocket);

                    continue;
                }

                if (receiveIsDecisionMakerNodeActive)
                {
                    if (recv(nodeSocket, (char*)&receiveIsDecisionMakerActive, sizeof(bool), NULL) <= 0)
                    {
                        logger->addLog("Can`t receive data from: " + nodesData[i].pcAddress);

                        closesocket(nodeSocket);

                        continue;
                    }

                    isDecisionMakerCenterActive = receiveIsDecisionMakerActive;
                }
            }
        }

        closesocket(nodeSocket);
    }

    if (ifTheOnlyDecisionMaker && isDecisionMakerCenter)
    {
        isDecisionMakerCenterActive = true;
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
        logger->addLog("Can't create thread connectNewNodeThread");

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
            logger->addLog("Can't create thread decisionMakerCenterThread");

            exitThread = true;

            WaitForSingleObject(connectNewNodeThread, INFINITE);

            closesocket(listenSocket);

            WSACleanup();

            return 1;
        }

        logger->addLog("Decision maker thread started");
    }

    getchar();

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

    if (!file) 
    {
        logger->addLog("Failed to open the database file.");
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
        logger->addLog("Cannot get hostname in db.");
    };
    
    vector<userAddressInfo> users;
    userAddressInfo tempUser;
    fstream file("database.dat", ios::in | ios::out | ios::app);
    if (!file) {
        logger->addLog("Failed to open the database file.");
        exit(EXIT_FAILURE);
    }
    while (file) {
        file >> tempUser.id >> tempUser.pcAddress >> tempUser.isDecisionMakerCenter;
        if (!file)
            break;
        if (tempUser.pcAddress != hostname) 
        {
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
        logger->addLog("Failed to open the database file.");
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
        logger->addLog("Failed to open the database file for writing.");
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
            logger->addLog("This address is already exists in database.");
            return;
        }
    }
    ofstream file("database.dat", ios::app);
    if (!file.is_open()) {
        logger->addLog("Failed to open the database file in addAddressToDataBase.");
        return;
    }
    file << generateUniqueId() << " " << newAddress << '\n';
    file.close();
    logger->addLog("Address was successfully added to database.");
}
long long generateUniqueId() {
    std::random_device rd;
    std::mt19937_64 generator(rd());
    std::uniform_int_distribution<long long> distribution;
    return distribution(generator) % (10000000 + 1);
}
//================DATABASE REALIZATION=====================
