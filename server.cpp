#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <string>
#include <vector>
#include <utility>
#include <unordered_set>

#include "RDT.h"
#include "utils.h"

const char *IPSERVER = "127.0.0.1";

const int PORT_MAIN = 12345;
const int PORT_STORAGE_BASE = 12350;

const int BUFFER_SIZE = 1024;
const int STORAGE_INDEX_LIMIT = 3;

// numero de secuencia inicial
RDT rdtClient;        // -> 0
RDT rdtStorage(1000); // -> 1000

// estructura para almacenar sockets de servidores de almacenamiento
struct storageServInfo
{
    int socket;
    sockaddr_in direccion;
};

// vector de servidores de almacenamiento
std::vector<storageServInfo> storageServSockets;

// client address
sockaddr_in clientAddr;
socklen_t clientAddrLen = sizeof(clientAddr);

// hash function
int hashFunction(char c)
{
    return (int)c % 4;
}

// procesar respuesta del servidor de almacenamiento (envia ACK/NAK al cliente) (solo en funciones CREATE, UPDATE, DELETE)
void processStorageResponse(int sockfd, sockaddr_in serverAddr)
{
    // recibe ACK/NAK del servidor de almacenamiento
    std::string rdtMessage = rdtStorage.receiveACKmessage(sockfd, serverAddr);
    // std::cout << "ACK Message from Storage: " << rdtMessage << std::endl;

    // es ACK o NAK?
    if (rdtMessage[0] == 'A')
    {
        // enviar ACK al cliente
        std::cout << "ACK from Storage " << sockfd << std::endl;
        rdtClient.sendACK(sockfd, clientAddr);
    }
    else if (rdtMessage[0] == 'N')
    {
        // TODO: logica de reenvio al servidor de almacenamiento en caso de NAK
    }
    else
    {
        std::cout << "Error in ACK message from Storage" << std::endl;
    }
}

// procesar respuesta del servidor de almacenamiento (sin enviar ACK/NAK al cliente) (solo en funcion READ)
void processStorageResponse2(int sockfd, sockaddr_in serverAddr)
{
    std::string rdtMessage = rdtStorage.receiveACKmessage(sockfd, serverAddr);
    // std::cout << "ACK Message from Storage: " << rdtMessage << std::endl;

    if (rdtMessage[0] == 'A')
    {
        std::cout << "ACK2 from Storage " << sockfd << std::endl;
        // rdtClient.sendACK(sockfd, clientAddr);
    }
    else if (rdtMessage[0] == 'N')
    {
    }
    else
    {
        std::cout << "Error in ACK2 message from Storage" << std::endl;
    }
}

// procesar respuesta del cliente (recibe ACK/NAK del cliente) (solo es funcion READ)
void processClientResponse(int sockfd, sockaddr_in serverAddr)
{
    std::string rdtMessage = rdtClient.receiveACKmessage(sockfd, serverAddr);
    // std::cout << "processClientResponse -> " << rdtMessage << std::endl;
    if (rdtMessage[0] == 'A')
    {
        // std::cout << "ACK from Client" << std::endl;
        // rdtClient.sendACK(sockfd, clientAddr);
    }
    else if (rdtMessage[0] == 'N')
    {
        std::cout << "NAK from Client" << std::endl;
        // TO DO: logica de reenvio al cliente en caso de NAK
    }
    else
    {
        std::cout << "Error in ACK message from Storage" << std::endl;
    }
}

// verificar mensaje recibido del servidor de almacenamiento
bool checkStorageMessage(const std::string &receivedMessage, int sockfd, const sockaddr_in &serverAddr)
{
    uint32_t message_seq_num = rdtStorage.extractSeqNum(receivedMessage);
    uint32_t RDT_seq_num = rdtStorage.getSeqNum();

    if (rdtStorage.checkRDTmessage(receivedMessage) && (message_seq_num == RDT_seq_num))
    {
        rdtStorage.sendACK(sockfd, serverAddr);
        return true;
    }
    else
    {
        // TO DO: analizar y replantear logica de reenvio en caso de NAK
        std::cout << "Incorrect Message" << std::endl;
        // rdtStorage.sendNACK(sockfd, serverAddr);
        return false;
    }
}

// enviar mensaje al servidor de almacenamiento y procesar respuesta
void sendAndProcessRDTMessage(int index, const std::string &message)
{
    rdtStorage.sendRDTmessage(storageServSockets[index].socket, message, storageServSockets[index].direccion);
    processStorageResponse(storageServSockets[index].socket, storageServSockets[index].direccion);
}

// FUNCION DE LECTURA RECURSIVA
std::vector<std::pair<std::string, std::string>> readData(std::string key, int depthSearch, std::unordered_set<std::string> &visitedKeys)
{
    // RESULTADO
    std::vector<std::pair<std::string, std::string>> result;

    // CASO BASE
    if (depthSearch <= 0 || visitedKeys.find(key) != visitedKeys.end())
    {
        return result;
    }

    // GUARDAR CLAVE VISITADA
    visitedKeys.insert(key);

    // CALCULAR INDICE DE SERVIDOR DE ALMACENAMIENTO (hash function)
    // se almacena en dos servidores de almacenamiento (index y nextIndex) (nextIndex = index + 1 || 0 if index = 3)
    int index = hashFunction(key[0]);
    int nextIndex = (index == STORAGE_INDEX_LIMIT) ? 0 : index + 1;

    // CREAR MENSAJE PARA PEDIR DATOS AL SERVIDOR DE ALMACENAMIENTO
    std::string readMessage = rdtStorage.createRDTmessage("R" + complete_digits(key.size(), 4) + key);
    // sendAndProcessRDTMessage(index, readMessage);

    // TO DO: keep alive logic

    /*
    servidor almacenamiento index encendido? -> pedir datos
    servidor almacenamiento index apagado? -> servidor almacenamiento nextIndex encendido? -> pedir datos
    servidor almacenamiento index y nextIndex apagados? -> error
    */

    // ENVIAR MENSAJE PARA PEDIR DATOS AL SERVIDOR DE ALMACENAMIENTO -> INDEX
    rdtStorage.sendRDTmessage(storageServSockets[index].socket, readMessage, storageServSockets[index].direccion);
    processStorageResponse2(storageServSockets[index].socket, storageServSockets[index].direccion);

    // RECIBIR NUMERO DE DATOS A RECIBIR - FORMATO RDT MESSAGE
    std::string rdtMessage = rdtStorage.receiveRDTmessage(storageServSockets[index].socket, storageServSockets[index].direccion);
    int numData = 0;

    // MENSAJE CORRECTO?
    if (checkStorageMessage(rdtMessage, storageServSockets[index].socket, storageServSockets[index].direccion))
    {
        // DECODIFICAR MENSAJE
        std::string message = rdtStorage.decodeRDTmessage(rdtMessage);
        // OBTENER NUMERO DE DATOS
        numData = std::stoi(message.substr(1, 4));
    }
    else
    {
        std::cout << "Incorrect Message" << std::endl;
    }

    // RECIBIR DATOS DEL SERVIDOR DE ALMACENAMIENTO - FORMATO RDT MESSAGE
    for (int i = 0; i < numData; i++)
    {
        std::string rdtMessage = rdtStorage.receiveRDTmessage(storageServSockets[index].socket, storageServSockets[index].direccion);
        // MENSAJE CORRECTO?
        if (checkStorageMessage(rdtMessage, storageServSockets[index].socket, storageServSockets[index].direccion))
        {
            // DECODIFICAR MENSAJE
            std::string message = rdtStorage.decodeRDTmessage(rdtMessage);
            std::string value;

            // OBTENER VALOR DEL MENSAJE
            parseValuesMessage(message, value);
            // INSERTAR EN EL VECTOR DE RESULTADO
            result.push_back(std::make_pair(key, value));
            // OBTENER VECTOR RESULTADO DE LA LLAMADA RECURSIVA
            auto recursiveResult = readData(value, depthSearch - 1, visitedKeys);
            // CONCATENAR VECTORES
            result.insert(result.end(), recursiveResult.begin(), recursiveResult.end());
        }
        else
        {
            std::cout << "Incorrect Message" << std::endl;
        }
    }

    return result;
}

// procesar mensaje recibido del cliente - accion a realizar
void processMessage(int sockfd, std::string &message)
{
    char option = message[0];

    switch (option)
    {
    case 'C': // create command
    {
        std::cout << "Create Command" << std::endl;

        std::string key;
        std::string value;

        // obtener (key, value)
        parseCreateMessage(message, key, value);

        // CALCULAR INDICE DE SERVIDOR DE ALMACENAMIENTO (hash function)
        // se almacena en dos servidores de almacenamiento (index y nextIndex) (nextIndex = index + 1 || 0 if index = 3)}

        // TO DO: keep alive logic
        /*
        caso 1: ambos servidores de almacenamiento encendidos
        servidor almacenamiento index encendido? -> almacenar datos
        servidor almacenamiento nextIndex encendido? -> almacenar datos

        caso 2: index apagado, nextIndex encendido
        servidor almacenamiento index apagado? -> index = nextIndex, nextIndex = nextIndex + 1 || 0 if nextIndex = 3
        verificar ambos servidores hasta que ambos esten encendidos

        caso 3: index encendido, nextIndex apagado
        servidor almacenamiento index encendido? -> almacenar datos
        servidor almacenamiento nextIndex apagado? -> nextIndex = nextIndex + 1 || 0 if nextIndex = 3
        verificar servidor nextIndex hasta que este encendido

        caso 4: ambos servidores de almacenamiento apagados
        index = nextIndex, nextIndex = nextIndex + 1 || 0 if nextIndex = 3
        hasta que ambos servidores de almacenamiento esten encendidos
        */

        int index = hashFunction(key[0]);
        int nextIndex = (index == STORAGE_INDEX_LIMIT) ? 0 : index + 1;

        std::string storageMessage = rdtStorage.createRDTmessage(message);

        // almacenamiento en index y nextIndex
        sendAndProcessRDTMessage(index, storageMessage);
        sendAndProcessRDTMessage(nextIndex, storageMessage);

        break;
    }
    case 'R': // read command
    {
        std::cout << "Read Command" << std::endl;

        std::string key;
        std::string depthSearch;

        // obtener clave y profundidad de busqueda
        parseReadMessage(message, key, depthSearch);

        // INTERACCION CON LOS SERVIDORES DE ALMACENAMIENTO
        // claves visitadas
        std::unordered_set<std::string> visitedKeys;

        // obtener datos de funcion recursiva
        // PEDIR DATOS A LOS SERVIDORES DE ALMACENAMIENTO
        auto result = readData(key, std::stoi(depthSearch), visitedKeys);
        // TERMINA LA INTERACCION CON LOS SERVIDORES DE ALMACENAMIENTO

        // print result
        for (auto &pair : result)
        {
            std::cout << pair.first << " - " << pair.second << std::endl;
        }

        // EMPIEZA INTERACCION CON EL CLIENTE
        // Enviar numero de datos al cliente
        std::string message = "R" + complete_digits(result.size(), 4);
        std::string rdtMessage = rdtStorage.createRDTmessage(message);

        rdtStorage.sendRDTmessage(sockfd, rdtMessage, clientAddr);
        processClientResponse(sockfd, clientAddr);

        // Enviar registros al cliente
        for (auto &pair : result)
        {
            std::string dataMessage = "R" + complete_digits(pair.first.size(), 4) +
                                  pair.first +
                                  complete_digits(pair.second.size(), 4) +
                                  pair.second;
            std::string rdtDataMessage = rdtStorage.createRDTmessage(dataMessage);

            rdtClient.sendRDTmessage(sockfd, rdtDataMessage, clientAddr);
            processClientResponse(sockfd, clientAddr);
        }

        break;
    }
    case 'U': // update command
    {
        std::cout << "Update Command" << std::endl;

        std::string key;
        std::string oldValue;
        std::string newValue;

        // obtener clave, valor antiguo y valor nuevo
        parseUpdateMessage(message, key, oldValue, newValue);

        // CALCULAR INDICE DE SERVIDOR DE ALMACENAMIENTO (hash function)
        // se actualiza los datos en dos servidores de almacenamiento (index y nextIndex) (nextIndex = index + 1 || 0 if index = 3)}

        // TO DO: keep alive logic - similar a CREATE

        int index = hashFunction(key[0]);
        int nextIndex = (index == STORAGE_INDEX_LIMIT) ? 0 : index + 1;

        std::string updateMessage = rdtStorage.createRDTmessage(message);

        sendAndProcessRDTMessage(index, updateMessage);
        sendAndProcessRDTMessage(nextIndex, updateMessage);

        break;
    }
    case 'D': // delete command
    {
        std::cout << "Delete Command" << std::endl;

        // get key and value
        std::string key;
        std::string value;

        parseCreateMessage(message, key, value);

        // CALCULAR INDICE DE SERVIDOR DE ALMACENAMIENTO (hash function)
        // se eliminan los datos en dos servidores de almacenamiento (index y nextIndex) (nextIndex = index + 1 || 0 if index = 3)}

        // TO DO: keep alive logic - similar a CREATE
        int index = hashFunction(key[0]);
        int nextIndex = (index == STORAGE_INDEX_LIMIT) ? 0 : index + 1;

        std::string storageMessage = rdtStorage.createRDTmessage(message);

        sendAndProcessRDTMessage(index, storageMessage);
        sendAndProcessRDTMessage(nextIndex, storageMessage);

        break;
    }
    default:
    {
        std::cout << "Unknown option" << std::endl;
        break;
    }
    }
}

// verificar mensaje recibido del cliente
bool checkClientMessage(const std::string &receivedMessage, int sockfd, const sockaddr_in &clientAddr)
{
    uint32_t message_seq_num = rdtClient.extractSeqNum(receivedMessage);
    uint32_t RDT_seq_num = rdtClient.getSeqNum();

    if (rdtClient.checkRDTmessage(receivedMessage) && (message_seq_num == RDT_seq_num))
    {
        std::cout << "Correct Message" << std::endl;

        // rdtClient.sendACK(sockfd, clientAddr);
        return true;
    }
    else
    {
        std::cout << "Incorrect Message" << std::endl;
        // rdtClient.sendNACK(sockfd, clientAddr);
        return false;
    }
}

// procesar mensaje del cliente
void processClientMessage(int sockfd, const sockaddr_in &clientAddr)
{
    while (true)
    {
        // recibir mensaje
        std::string RDTmessage = rdtClient.receiveRDTmessage(sockfd, clientAddr);
        // verificar mensaje
        if (checkClientMessage(RDTmessage, sockfd, clientAddr))
        {
            // mensaje correcto -> procesar mensaje
            std::string message = rdtClient.decodeRDTmessage(RDTmessage);
            std::cout << "Message from client: " << message << std::endl;
            processMessage(sockfd, message);
        }
        else
        {
            // TODO: logica de reenvio en caso de NAK
            // incorrect message -> SEND NAK TO CLIENT
        }
    }
}

void connectToStorageServers()
{
    // configuracion servidor de almacenamiento
    sockaddr_in servidor2Direccion;
    servidor2Direccion.sin_family = AF_INET;
    servidor2Direccion.sin_addr.s_addr = INADDR_ANY;

    // informacion de cada servidor de almacenamiento (socket, direccion) -> 4 servidores
    for (int i = 0; i < 4; ++i)
    {
        int puertoServidor2 = PORT_STORAGE_BASE + i; // Puertos 12350, 12351, 12352, 12353
        servidor2Direccion.sin_port = htons(puertoServidor2);

        int servidor2Socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (servidor2Socket == -1)
        {
            std::cerr << "Error al crear el socket del servidor 2." << std::endl;
        }
        else
        {
            storageServInfo info;
            info.socket = servidor2Socket;
            info.direccion = servidor2Direccion;
            storageServSockets.push_back(info);
        }
    }
}

int main()
{
    system("clear");

    // socket UDP servidor principal
    int mainSocketfd = socket(AF_INET, SOCK_DGRAM, 0);

    // configuracion servidor principal
    sockaddr_in mainServerAddr;
    memset(&mainServerAddr, 0, sizeof(mainServerAddr));
    mainServerAddr.sin_family = AF_INET;
    mainServerAddr.sin_port = htons(PORT_MAIN);
    mainServerAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(mainSocketfd, (struct sockaddr *)&mainServerAddr, sizeof(mainServerAddr)) == -1)
    {
        std::cerr << "Error al vincular el socket" << std::endl;
        close(mainSocketfd);
        return EXIT_FAILURE;
    }

    std::cout << "Servidor principal escuchando en " << IPSERVER << ":" << PORT_MAIN << std::endl;

    connectToStorageServers();

    std::cout << "Se crearon " << storageServSockets.size() << " servidores 2." << std::endl;

    while (true)
    {
        processClientMessage(mainSocketfd, clientAddr);
    }

    close(mainSocketfd);
    return 0;
}
