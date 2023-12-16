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

// revisar si respuesta del servidor principal es ACK o NAK
void processServerResponse(int sockfd, sockaddr_in serverAddr)
{
    std::string rdtMessage = rdtClient.receiveACKmessage(sockfd, serverAddr);
    std::cout << "processServerResponse -> " << rdtMessage << std::endl;
    std::string messageType = rdtMessage.substr(0, 1);

    if (messageType == "A")
    {
        std::cout << "Received ACK from server." << std::endl;
    }
    else if (messageType == "N")
    {
        std::cout << "Received NAK from server." << std::endl;
        // TO DO: logica de reenvio en caso de NAK
    }
    else
    {
        std::cerr << "Received unrecognized message type from server." << std::endl;
    }
}

// revisar mensaje recibido del servidor
bool checkServerMessage(const std::string &receivedMessage, int sockfd, const sockaddr_in &mainServAddr)
{
    // extraccion del numero de secuencia del mensaje recibido
    uint32_t message_seq_num = rdtClient.extractSeqNum(receivedMessage);
    // extraccion del numero de secuencia del rdt cliente
    uint32_t RDT_seq_num = rdtClient.getSeqNum();

    // comparar CHECKSUM y numero de secuencia
    if (rdtClient.checkRDTmessage(receivedMessage) && (message_seq_num == RDT_seq_num))
    {
        rdtClient.sendACK(sockfd, mainServAddr);
        return true;
    }
    else
    {
        // TO DO: enviar NAK al servidor principal
        return false;
    }
}

// crear registro (key, value)
void createRegister(int sockfd, sockaddr_in serverAddr)
{
    std::string key;
    std::string value;
    std::cout << "----- Create Register -----" << std::endl;
    std::cout << "Enter key: ";
    std::cin >> key;
    std::cout << "Enter value: ";
    std::cin >> value;

    std::string message = 'C' +
                          complete_digits(key.size(), 4) +
                          key +
                          complete_digits(value.size(), 4) +
                          value;

    std::string rdtMessage = rdtClient.createRDTmessage(message);
    processServerResponse(sockfd, serverAddr);
}

// leer registro (key, depthSearch)
void readRegister(int sockfd, sockaddr_in serverAddr)
{
    std::string key;
    std::string depthSearch;

    // get data
    std::cout << "----- Read Register -----" << std::endl;
    std::cout << "Enter key: ";
    std::cin >> key;
    std::cout << "Enter depth search: ";
    std::cin >> depthSearch;

    // crear mensaje para solicitar datos al servidor principal
    std::string message = 'R' +
                          complete_digits(key.size(), 4) +
                          key +
                          complete_digits(depthSearch.size(), 4) +
                          depthSearch;

    // crear mensaje rdt
    std::string rdtMessage = rdtClient.createRDTmessage(message);
    // enviar mensaje rdt
    rdtClient.sendRDTmessage(sockfd, rdtMessage, serverAddr);
    // recibir respuesta rdt del servidor principal
    // primer mensaje recibido: tamaño de los datos a recibir
    std::string rdtSizeDataMessage = rdtClient.receiveRDTmessage(sockfd, serverAddr);
    int sizeData = 0;
    // verificar si el mensaje recibido es correcto
    if (checkServerMessage(rdtSizeDataMessage, sockfd, serverAddr))
    {
        // extraer el tamaño de los datos a recibir e instanciar la variable sizeData
        std::string sizeDataStr = rdtClient.decodeRDTmessage(rdtSizeDataMessage);
        sizeData = std::stoi(sizeDataStr.substr(1, 4));
    }
    else
    {
        // si el mensaje no es correcto, mostrar error y terminar ejecucion READ
        std::cout << "Error al recibir el tamaño de los datos" << std::endl;
        return;
    }

    std::cout << "registros para leer : " << sizeData << std::endl;

    std::cout << "---------REGISTROS---------" << std::endl;
    // RECIBIR REGISTROS DEL SERVIDOR PRINCIPAL
    for (int i = 0; i < sizeData; i++)
    {
        std::string rdtDataMessage = rdtClient.receiveRDTmessage(sockfd, serverAddr);

        if (checkServerMessage(rdtDataMessage, sockfd, serverAddr))
        {
            std::string data = rdtClient.decodeRDTmessage(rdtDataMessage);
            std::string key;
            std::string value;

            parseCreateMessage(data, key, value);

            std::cout << key << " - " << value << std::endl;
        }
        else
        {
            std::cout << "Error al recibir los datos" << std::endl;
        }
    }
}

// actualizar registro (key, oldValue, newValue)
void updateRegister(int sockfd, sockaddr_in serverAddr)
{
    std::string key;
    std::string oldValue;
    std::string newValue;

    // get data
    std::cout << "----- Update Register -----" << std::endl;
    std::cout << "Enter key: ";
    std::cin >> key;
    std::cout << "Enter old value: ";
    std::cin >> oldValue;
    std::cout << "Enter new value: ";
    std::cin >> newValue;

    // crear mensaje para modificar datos al servidor principal
    std::string message = 'U' +
                          complete_digits(key.size(), 4) +
                          key +
                          complete_digits(oldValue.size(), 4) +
                          oldValue +
                          complete_digits(newValue.size(), 4) +
                          newValue;
    // crear mensaje rdt
    std::string rdtMessage = rdtClient.createRDTmessage(message);
    // enviar mensaje rdt
    rdtClient.sendRDTmessage(sockfd, rdtMessage, serverAddr);
    // recibir respuesta del servidor principal
    processServerResponse(sockfd, serverAddr);
}

// eliminar registro (key, value)
void deleteRegister(int sockfd, sockaddr_in serverAddr)
{
    std::string key;
    std::string value;
    // get data
    std::cout << "----- Delete Register -----" << std::endl;
    std::cout << "Enter key: ";
    std::cin >> key;
    std::cout << "Enter value: ";
    std::cin >> value;

    // crear mensaje para eliminar datos al servidor principal
    std::string message = 'D' +
                          complete_digits(key.size(), 4) +
                          key +
                          complete_digits(value.size(), 4) +
                          value;

    // crear mensaje rdt
    std::string rdtMessage = rdtClient.createRDTmessage(message);
    // enviar mensaje rdt
    rdtClient.sendRDTmessage(sockfd, rdtMessage, serverAddr);
    // recibir respuesta del servidor principal
    processServerResponse(sockfd, serverAddr);
}

int main()
{
    system("clear");
    // socket cliente
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1)
    {
        std::cerr << "Error al crear el socket" << std::endl;
        return EXIT_FAILURE;
    }

    // configuracion del servidor principal
    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    while (true)
    {

        std::cout << "----- Menú CRUD -----" << std::endl;
        std::cout << "1. Crear registro" << std::endl;
        std::cout << "2. Leer registro" << std::endl;
        std::cout << "3. Actualizar registro" << std::endl;
        std::cout << "4. Eliminar registro" << std::endl;
        std::cout << "5. Salir" << std::endl;
        std::cout << "Ingrese una opción: ";

        int option;
        std::cin >> option;

        std::cin.ignore();

        switch (option)
        {
        case 1:
            createRegister(sockfd, serverAddr);
            break;
        case 2:
            readRegister(sockfd, serverAddr);
            break;
        case 3:
            updateRegister(sockfd, serverAddr);
            break;
        case 4:
            deleteRegister(sockfd, serverAddr);
            break;
        case 5:
            std::cout << "Saliendo del programa." << std::endl;
            close(sockfd);
            return 0;
        default:
            std::cerr << "Opción no válida. Intente nuevamente." << std::endl;
        }
    }

    close(sockfd);
    return 0;
}
