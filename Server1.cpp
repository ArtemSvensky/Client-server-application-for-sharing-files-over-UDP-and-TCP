#include <iostream>
#include <fstream>
#include <string>
#include <WinSock2.h>
#include <WS2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define BUFFER_SIZE 1024

void handleError(const std::string& errorMessage) {
    std::cerr << errorMessage << " Error code: " << WSAGetLastError() << std::endl;
    WSACleanup();
    exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: Server.exe <IP> <port> <directory>" << std::endl;
        return EXIT_FAILURE;
    }

    const std::string ip = argv[1];
    const int port = std::stoi(argv[2]);
    const std::string directory = argv[3];

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        handleError("WSAStartup failed.");
    }

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket == INVALID_SOCKET) {
        handleError("Failed to create listen socket.");
    }

    SOCKADDR_IN serverAddress{};
    serverAddress.sin_family = AF_INET;
    inet_pton(AF_INET, ip.c_str(), &(serverAddress.sin_addr));
    serverAddress.sin_port = htons(port);

    if (bind(listenSocket, reinterpret_cast<SOCKADDR*>(&serverAddress), sizeof(serverAddress)) == SOCKET_ERROR) {
        handleError("Failed to bind listen socket.");
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        handleError("Failed to listen on socket.");
    }

    std::cout << "Server is listening on " << ip << ":" << port << std::endl;

    while (true) {
        std::cout << "Waiting for incoming connections..." << std::endl;

        SOCKET clientSocket = accept(listenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            handleError("Failed to accept client connection.");
        }

        std::cout << "Client connected." << std::endl;

        // Receive file name and UDP port from the client
        char fileName[BUFFER_SIZE];
        char udpPort[BUFFER_SIZE];
        memset(fileName, 0, sizeof(fileName));
        memset(udpPort, 0, sizeof(udpPort));

        if (recv(clientSocket, fileName, sizeof(fileName), 0) == SOCKET_ERROR) {
            handleError("Failed to receive file name from client.");
        }

        if (recv(clientSocket, udpPort, sizeof(udpPort), 0) == SOCKET_ERROR) {
            handleError("Failed to receive UDP port from client.");
        }

        std::cout << "Received file name: " << fileName << std::endl;
        std::cout << "Received UDP port: " << udpPort << std::endl;

        // Open UDP socket for receiving data
        SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
        if (udpSocket == INVALID_SOCKET) {
            handleError("Failed to create UDP socket.");
        }

        SOCKADDR_IN udpAddress{};
        udpAddress.sin_family = AF_INET;
        udpAddress.sin_addr.s_addr = htonl(INADDR_ANY);
        udpAddress.sin_port = htons(std::stoi(udpPort));

        if (bind(udpSocket, reinterpret_cast<SOCKADDR*>(&udpAddress), sizeof(udpAddress)) == SOCKET_ERROR) {
            handleError("Failed to bind UDP socket.");
        }

        std::cout << "UDP socket is ready to receive data." << std::endl;

        // Receive and save file data
        std::ofstream file(directory + "\\" + fileName, std::ios::binary);
        if (!file) {
            handleError("Failed to create file.");
        }

        char buffer[BUFFER_SIZE];
        int bytesRead;
        int packetId = 0;

        while ((bytesRead = recvfrom(udpSocket, buffer, sizeof(buffer), 0, NULL, NULL)) != SOCKET_ERROR) {
            // Send acknowledgment to the client
            if (send(clientSocket, reinterpret_cast<char*>(&packetId), sizeof(packetId), 0) == SOCKET_ERROR) {
                handleError("Failed to send acknowledgment to client.");
            }

            file.write(buffer, bytesRead);

            ++packetId;
        }

        if (bytesRead == SOCKET_ERROR) {
            handleError("Failed to receive data from client.");
        }

        std::cout << "File transfer complete. Saved file as: " << directory << "\\" << fileName << std::endl;
        file.close();
        closesocket(clientSocket);
        closesocket(udpSocket);
    }

    closesocket(listenSocket);
    WSACleanup();

    return 0;
}