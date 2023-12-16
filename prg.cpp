#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <string>
#include <thread>

#include "RDT.h"
#include "utils.h"

const int PORT = 12345;
const int BUFFER_SIZE = 1024;
RDT rdtClient;

void readMessage(int sockfd)
{
    sockaddr_in senderAddr;
    socklen_t senderAddrLen = sizeof(senderAddr);
    char buffer[BUFFER_SIZE];

    while (true)
    {
        // Recibir mensajes en un bucle infinito
        ssize_t bytesRead = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&senderAddr, &senderAddrLen);

        if (bytesRead == -1)
        {
            std::cerr << "Error al recibir el mensaje" << std::endl;
            // Puedes agregar lógica adicional aquí según tus necesidades
        }
        else
        {
            buffer[bytesRead] = '\0';
            std::string receivedMessage(buffer);

            std::cout << "Mensaje recibido: " << receivedMessage << std::endl;
        }
    }
}

void processServerResponse(int sockfd, sockaddr_in serverAddr)
{
    // Esperar y procesar la respuesta del servidor
    std::string rdtMessage = rdtClient.receiveACKmessage(sockfd, serverAddr);

    std::string messageType = rdtMessage.substr(0, 1);

    if (messageType == "A")
    {
        // El servidor envió un ACK
        std::cout << "Received ACK from server." << std::endl;

        // Continuar con la siguiente operación si es necesario
    }
    else if (messageType == "N")
    {
        // El servidor envió un NACK
        std::cout << "Received NACK from server." << std::endl;

        // Tomar medidas correctivas según sea necesario, como retransmitir el mensaje
        // (Puedes implementar la lógica de retransmisión aquí)
    }
    else
    {
        // Tipo de mensaje no reconocido
        std::cerr << "Received unrecognized message type from server." << std::endl;
        // Puedes manejar esto según tus requisitos, como notificar al usuario o abortar la operación.
    }
}

void createRegister(int sockfd, sockaddr_in serverAddr, std::string key, std::string value)
{

    std::string message = 'C' +
                          complete_digits(key.size(), 4) +
                          key +
                          complete_digits(value.size(), 4) +
                          value;

    std::string rdtMessage = rdtClient.createRDTmessage(message);

    // std::cout << "RDT message: " << rdtMessage << std::endl;

    rdtClient.sendRDTmessage(sockfd, rdtMessage, serverAddr);

    processServerResponse(sockfd, serverAddr);
}

void readRegister(int sockfd, sockaddr_in serverAddr)
{
    //TO DO
}

void updateRegister(int sockfd, sockaddr_in serverAddr)
{
    
    std::string key;
    std::string oldValue;
    std::string newValue;

    std::cout << "----- Update Register -----" << std::endl;
    std::cout << "Enter key: ";
    std::cin >> key;
    std::cout << "Enter old value: ";
    std::cin >> oldValue;
    std::cout << "Enter new value: ";
    std::cin >> newValue;

    std::string message = 'U' +
                          complete_digits(key.size(), 4) +
                          key +
                          complete_digits(oldValue.size(), 4) +
                          oldValue +
                          complete_digits(newValue.size(), 4) +
                          newValue;

    std::string rdtMessage = rdtClient.createRDTmessage(message);

    rdtClient.sendRDTmessage(sockfd, rdtMessage, serverAddr);
}

void deleteRegister(int sockfd, sockaddr_in serverAddr)
{
    // TO DO
}

int main(int argc, char *argv[])
{

    // Crear el socket UDP
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1)
    {
        std::cerr << "Error al crear el socket" << std::endl;
        return EXIT_FAILURE;
    }

    // Configurar la dirección del servidor
    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Cambia esto a la dirección IP del servidor

    // std::thread(readMessage, sockfd).detach();

    const char* option = argv[1];

    switch (option[0])
    {
    case 'C':
        createRegister(sockfd, serverAddr, argv[2], argv[3]);
        break;
    case 'R':
        readRegister(sockfd, serverAddr);
        break;
    case 'U':
        updateRegister(sockfd, serverAddr);
        break;
    case 'D':
        deleteRegister(sockfd, serverAddr);
        break;
    case 'E':
        std::cout << "Saliendo del programa." << std::endl;
        close(sockfd);
        return 0;
    default:
        std::cerr << "Opción no válida. Intente nuevamente." << std::endl;
    }



    close(sockfd);
    return 0;
}
