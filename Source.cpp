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

struct userAddressInfo 
{
    long long id{ 0 };
    string pcAddress;
};

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

    addresses.push_back({ generateUniqueId(),"DESKTOP-LT61FS4" });
    addresses.push_back({ generateUniqueId(),"DESKTOP-09499TF" });
    addresses.push_back({ generateUniqueId(),"DESKTOP-H2RFUD" });

    if (fileExists("database.dat")) {
        cout << "File already exists." << endl;
    }
    else {
        createDataBase(&addresses);
    }

    // =========================DATABASE====================================
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

    vector<userAddressInfo> nodeNames{ getNodesData() };
    for (size_t i = 0; i < nodeNames.size(); i++)
    {
        cout << nodeNames[i].pcAddress << endl;
    }

    for (auto nodeName : nodeNames)
    {
        nodes[nodeName.pcAddress] = false;
    }
    string smt;
    for (size_t i{}; i < nodeNames.size(); i++)
    {
        /*cout << "Enter something and press enter to continue: " << endl;
        cin >> smt;*/
        // Тут баг, якщо один з компухтерів включається пізніше, а ти вже конектишся, то виводить
        // ерори в консоль, але потім, коли другий комп(Коляс) все ж таки запускає прогу, то перший
        // отримує повідомлення, але це вже після того як вивелись помилки!... Тобто для нормальної роботи
        // потрібно запускати всі компи одночасно. А так все працює :) PS. Зря наїхав за створення сокета, там все норм.
        SOCKET nodeSocket{ connectToNode(nodeNames[i].pcAddress, DEFAULT_PORT) }; // <<<

        Data sendData{};

        sendData.a = 10;
        sendData.b = 65;

        sendMessage(nodeSocket, (char*)&sendData, sizeof(Data));

        cout << "Sending message: " << sendData.a << " " << sendData.b << " to " << nodeNames[i].pcAddress << endl;

        int receiveData{};

        if (recv(nodeSocket, (char*)&receiveData, sizeof(int), NULL) <= 0)
        {
            cerr << "Can`t receive data from: " << nodeNames[i].pcAddress << endl;

            closesocket(nodeSocket);

            continue;
        }

        if (receiveData != sendData.a + sendData.b)
        {
            cerr << "Invalid response from: " << nodeNames[i].pcAddress << endl;
        }
        else
        {
            nodes[nodeNames[i].pcAddress] = true;
            cout << "Message was received" << endl;
        }
    }

    exitThread = true;

    WaitForSingleObject(connectNewClientThread, INFINITE);

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
        file << address.id << " " << address.pcAddress << '\n';
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
        file >> tempUser.id >> tempUser.pcAddress;
        if (!file)
            break;
        if (tempUser.pcAddress != hostname) {
            users.push_back(tempUser);
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
    while (inputFile >> tempAddress.id >> tempAddress.pcAddress) {
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
        outputFile << address.id << ' ' << address.pcAddress << '\n';
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