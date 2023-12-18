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
const int MAX_ENTRIES = 3;
const int TIMEOUT = 5;
RDT rdtClient;

void retransmition(std::string message, int sockfd, sockaddr_in serverAddr)
{
    int intentos = 0;
    do
    {
        std::cout << "Reenviando -> " << message << "\n";
        rdtClient.sendRDTmessage(sockfd, message, serverAddr);

        struct timeval tv;
        tv.tv_sec = TIMEOUT; // 2 segundos de timeout
        tv.tv_usec = 0;

        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));

        std::string response = rdtClient.receiveACKmessage(sockfd, serverAddr);

        if (response.substr(0, 3) == "ACK")
        {
            std::cout << "ACK recibido. Datos entregados correctamente al servidor." << std::endl;
            break;
        }
        else if (response.substr(0, 3) == "NAK")
        {
            std::cout << "NAK recibido. Reintentando..." << std::endl;
            intentos++;
        } else if(response.substr(0,3) == ""){
            std::cout << "Timeout. No se recibió respuesta del servidor principal." << std::endl;
            intentos++;
        }
        else
        {
            std::cout << "Error en el mensaje recibido" << std::endl;
        }

    } while (intentos < MAX_ENTRIES);

    if (intentos == MAX_ENTRIES)
    {
        std::cout << "Número máximo de reintentos alcanzado. No se pudo entregar el mensaje al servidor." << std::endl;
    }
}

// revisar si respuesta del servidor principal es ACK o NAK
void processServerResponse(int sockfd, sockaddr_in serverAddr, std::string message)
{

    struct timeval tv;
    tv.tv_sec = TIMEOUT; // 2 segundos de timeout
    tv.tv_usec = 0;

    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));

    std::string rdtMessage = rdtClient.receiveACKmessage(sockfd, serverAddr);

    std::string messageType = rdtMessage.substr(0, 3);

    if (messageType == "ACK")
    {
        std::cout << "Received ACK from server." << std::endl;
    }
    else if (messageType == "NAK")
    {
        std::cout << "Received NAK from server." << std::endl;
        retransmition(message, sockfd, serverAddr);
    }
    else if (messageType == "")
    {
        std::cout << "Timeout. No se recibió respuesta del servidor principal." << std::endl;
        retransmition(message, sockfd, serverAddr);
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

    // std::cout << "Message Seq Num: " << message_seq_num << std::endl;
    // std::cout << "RDT Seq Num: " << RDT_seq_num << std::endl;

    // comparar CHECKSUM y numero de secuencia
    if (rdtClient.checkRDTmessage(receivedMessage) && (message_seq_num > RDT_seq_num))
    {
        rdtClient.sendACK(sockfd, mainServAddr);
        return true;
    }
    else
    {
        // TO DO: enviar NAK al servidor principal
        rdtClient.sendNAK(sockfd, mainServAddr);
        return false;
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

    processServerResponse(sockfd, serverAddr, rdtMessage);
}

void readRegister(int sockfd, sockaddr_in serverAddr, std::string key, std::string depthSearch)
{

    /*
    READ MESSAGE FORMAT
    R              -> type message          (1 byte)
    0000           -> size key              (4 bytes)
    key            -> string key            (size key bytes)
    0000           -> size depthSearch      (4 bytes)
    depthSearch    -> string depthSearch    (size depthSearch bytes)
    1 -> no recursivo
    mayor a 1 -> recursivo
    */

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

    // recibir respuesta rdt del servidor principal (ACK o NAK)
    processServerResponse(sockfd, serverAddr, rdtMessage);

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

    std::cout << "Numero de Registros: " << sizeData << std::endl;
}

void updateRegister(int sockfd, sockaddr_in serverAddr, std::string key, std::string oldValue, std::string newValue)
{

    std::string message = 'U' +
                          complete_digits(key.size(), 4) +
                          key +
                          complete_digits(oldValue.size(), 4) +
                          oldValue +
                          complete_digits(newValue.size(), 4) +
                          newValue;

    std::string rdtMessage = rdtClient.createRDTmessage(message);

    rdtClient.sendRDTmessage(sockfd, rdtMessage, serverAddr);

    processServerResponse(sockfd, serverAddr, rdtMessage);
}

void deleteRegister(int sockfd, sockaddr_in serverAddr, std::string key, std::string value)
{
    /*
    DELETE MESSAGE FORMAT
    D              -> type message    (1 byte)
    0000           -> size key        (4 bytes)
    key            -> string key      (size key bytes)
    0000           -> size value      (4 bytes)
    value          -> string value    (size value bytes)
    */

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

    // recibir respuesta del servidor principal (ACK o NAK)
    processServerResponse(sockfd, serverAddr, rdtMessage);
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
        readRegister(sockfd, serverAddr, argv[2], argv[3]);
        break;
    case 'U':
        updateRegister(sockfd, serverAddr, argv[2], argv[3], argv[4]);
        break;
    case 'D':
        deleteRegister(sockfd, serverAddr, argv[2], argv[3]);
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