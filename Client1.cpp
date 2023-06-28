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
    if (argc < 6) {
        std::cerr << "Usage: Client.exe <server IP> <server port> <UDP port> <file path> <timeout>" << std::endl;
        return EXIT_FAILURE;
    }

    const std::string serverIP = argv[1];
    const int serverPort = std::stoi(argv[2]);
    const int udpPort = std::stoi(argv[3]);
    const std::string filePath = argv[4];
    const int timeout = std::stoi(argv[5]);

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        handleError("WSAStartup failed.");
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        handleError("Failed to create client socket.");
    }

    SOCKADDR_IN serverAddress{};
    serverAddress.sin_family = AF_INET;
    inet_pton(AF_INET, serverIP.c_str(), &(serverAddress.sin_addr));
    serverAddress.sin_port = htons(serverPort);

    if (connect(clientSocket, reinterpret_cast<SOCKADDR*>(&serverAddress), sizeof(serverAddress)) == SOCKET_ERROR) {
        handleError("Failed to connect to the server.");
    }

    std::cout << "Connected to the server." << std::endl;

    // Send file name and UDP port to the server
    const std::string fileName = filePath.substr(filePath.find_last_of("\\/") + 1);
    const std::string udpPortString = std::to_string(udpPort);

    if (send(clientSocket, fileName.c_str(), static_cast<int>(fileName.size()) + 1, 0) == SOCKET_ERROR) {
        handleError("Failed to send file name to server.");
    }

    if (send(clientSocket, udpPortString.c_str(), static_cast<int>(udpPortString.size()) + 1, 0) == SOCKET_ERROR) {
        handleError("Failed to send UDP port to server.");
    }

    std::cout << "Sent file name: " << fileName << std::endl;
    std::cout << "Sent UDP port: " << udpPort << std::endl;

    // Open the file for reading
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        handleError("Failed to open file.");
    }

    // Open UDP socket for sending data
    SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocket == INVALID_SOCKET) {
        handleError("Failed to create UDP socket.");
    }

    SOCKADDR_IN udpAddress{};
    udpAddress.sin_family = AF_INET;
    inet_pton(AF_INET, serverIP.c_str(), &(udpAddress.sin_addr));
    udpAddress.sin_port = htons(udpPort);

    char buffer[BUFFER_SIZE];
    std::streamsize bytesRead;
    int packetId = 0;

    while (true) {
        file.read(buffer, sizeof(buffer));
        bytesRead = file.gcount();

        if (bytesRead > 0) {
            // Send data packet via UDP
            if (sendto(udpSocket, buffer, static_cast<int>(bytesRead), 0, reinterpret_cast<SOCKADDR*>(&udpAddress), sizeof(udpAddress)) == SOCKET_ERROR) {
                handleError("Failed to send data packet via UDP.");
            }

            std::cout << "Sent data packet ID: " << packetId << ", Size: " << bytesRead << " bytes" << std::endl;

            // Wait for acknowledgment packet
            char ackBuffer[BUFFER_SIZE];
            int receivedPacketId = -1;
            int bytesReceived = 0;

            timeval tv;
            tv.tv_sec = timeout;
            tv.tv_usec = 0;

            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(clientSocket, &readSet);
            if (select(0, &readSet, nullptr, nullptr, &tv) > 0) {
                if (FD_ISSET(clientSocket, &readSet)) {
                    bytesReceived = recv(clientSocket, ackBuffer, sizeof(ackBuffer), 0);
                    if (bytesReceived == SOCKET_ERROR) {
                        handleError("Failed to receive acknowledgment packet from server.");
                    }
                    else if (bytesReceived == 0) {
                        handleError("Connection closed by the server.");
                    }
                    else {
                        receivedPacketId = *reinterpret_cast<int*>(ackBuffer);

                        if (receivedPacketId != packetId) {
                            std::cout << "Received unexpected acknowledgment packet ID: " << receivedPacketId << std::endl;
                        }
                        else {
                            std::cout << "Received acknowledgment for packet ID: " << receivedPacketId << std::endl;
                        }
                    }
                }
            }
            else {
                // Timeout occurred, resend the packet
                std::cout << "Timeout occurred. Resending packet ID: " << packetId << std::endl;
                file.seekg(-bytesRead, std::ios::cur);
                continue;
            }

            ++packetId;
        }
        else {
            break;  // End of file
        }
    }

    // Notify the server about the completion of file transfer
    int endTransmission = -1;
    if (send(clientSocket, reinterpret_cast<char*>(&endTransmission), sizeof(endTransmission), 0) == SOCKET_ERROR) {
        handleError("Failed to send end transmission signal to server.");
    }

    std::cout << "File transfer complete." << std::endl;

    file.close();
    closesocket(clientSocket);
    closesocket(udpSocket);
    WSACleanup();

    return 0;
}