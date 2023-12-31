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
    /*
    struct timeval tv;
    tv.tv_sec = TIMEOUT; // 2 segundos de timeout
    tv.tv_usec = 0;

    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv)); */

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

    /*
    CREATE MESSAGE FORMAT

    C       -> type message    (1 byte)
    0000    -> size key        (4 bytes)
    key     -> string key      (size key bytes)
    0000    -> size value      (4 bytes)
    value   -> string value    (size value bytes)
    */
    // capa de aplicacion
    std::string message = 'C' +
                          complete_digits(key.size(), 4) +
                          key +
                          complete_digits(value.size(), 4) +
                          value;

    // crear mensaje RDT - capa de transporte
    std::string rdtMessage = rdtClient.createRDTmessage(message);

    // enviar mensaje RDT
    rdtClient.sendRDTmessage(sockfd, rdtMessage, serverAddr);

    // recibir respuesta del servidor principal (ACK o NAK)
    processServerResponse(sockfd, serverAddr, rdtMessage);
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

    /*
    UPDATE MESSAGE FORMAT
    U              -> type message    (1 byte)
    0000           -> size key        (4 bytes)
    key            -> string key      (size key bytes)
    0000           -> size oldValue   (4 bytes)
    oldValue       -> string oldValue (size oldValue bytes)
    0000           -> size newValue   (4 bytes)
    newValue       -> string newValue (size newValue bytes)
    */

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

    // recibir respuesta del servidor principal (ACK o NAK)
    processServerResponse(sockfd, serverAddr, rdtMessage);
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
