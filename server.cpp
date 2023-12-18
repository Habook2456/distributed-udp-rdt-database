#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <string>
#include <vector>
#include <utility>
#include <unordered_set>
#include <thread>

#include "RDT.h"
#include "utils.h"

const char *IPSERVER = "127.0.0.1";

const int PORT_MAIN = 12345;
const int PORT_STORAGE_BASE = 12350;

const int BUFFER_SIZE = 1024;
const int STORAGE_INDEX_LIMIT = 3;

const int MAX_ENTRIES = 3;

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

void printStorageServers()
{
    for (int i = 0; i < storageServSockets.size(); i++)
    {
        std::cout << "Storage " << i << " - " << storageServSockets[i].direccion.sin_addr.s_addr << std::endl;
    }
}

void retransmition(std::string message, int sockfd, sockaddr_in serverAddr, RDT& rdt)
{
    int intentos = 0;
    do{
        std::cout << "Reenviando -> " << message << "\n";
        rdt.sendRDTmessage(sockfd, message, serverAddr);

        std::string response = rdt.receiveACKmessage(sockfd, serverAddr);

        if(response.substr(0,3) == "ACK"){
            std::cout << "ACK recibido. Datos entregados correctamente al servidor." << std::endl;
            break;
        } else if (response.substr(0,3) == "NAK"){
            std::cout << "NAK recibido. Reintentando..." << std::endl;
            intentos++;
        } else{
            std::cout << "Error en el mensaje recibido" << std::endl;
        }

    } while (intentos < MAX_ENTRIES);

    if(intentos == MAX_ENTRIES){
        std::cout << "Número máximo de reintentos alcanzado. No se pudo entregar el mensaje al servidor." << std::endl;
    }
}

// procesar respuesta del servidor de almacenamiento (solo en funciones CREATE, UPDATE, DELETE)
void processStorageResponse(int sockfd, sockaddr_in serverAddr, std::string message)
{
    // recibe ACK/NAK del servidor de almacenamiento
    std::string rdtMessage = rdtStorage.receiveACKmessage(sockfd, serverAddr);

    // es ACK o NAK?
    if (rdtMessage[0] == 'A')
    {
        std::cout << "ACK from Storage " << sockfd << std::endl;
    }
    else if (rdtMessage[0] == 'N')
    {
        std::cout << "NAK from Storage " << sockfd << std::endl;
        retransmition(message, sockfd, serverAddr, rdtStorage);
    }
    else
    {
        std::cout << "Error in ACK message from Storage" << std::endl;
    }
}

// procesar respuesta del cliente (recibe ACK/NAK del cliente) (solo es funcion READ)
void processClientResponse(int sockfd, sockaddr_in serverAddr, std::string message)
{
    std::string rdtMessage = rdtClient.receiveACKmessage(sockfd, serverAddr);
    // std::cout << "processClientResponse -> " << rdtMessage << std::endl;
    if (rdtMessage[0] == 'A')
    {
        std::cout << "ACK from Client" << std::endl;
    }
    else if (rdtMessage[0] == 'N')
    {
        std::cout << "NAK from Client" << std::endl;
        retransmition(message, sockfd, serverAddr, rdtClient);
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

        std::cout << "Message Seq Num: " << message_seq_num << std::endl;
        std::cout << "RDT Seq Num: " << RDT_seq_num << std::endl;

    if (rdtStorage.checkRDTmessage(receivedMessage) && (message_seq_num > RDT_seq_num))
    {
        rdtStorage.sendACK(sockfd, serverAddr);
        return true;
    }
    else
    {
        // TO DO: analizar y replantear logica de reenvio en caso de NAK
        std::cout << "Incorrect Message" << std::endl;
        rdtStorage.sendNAK(sockfd, serverAddr);
        return false;
    }
}

// enviar mensaje al servidor de almacenamiento y procesar respuesta
void sendAndProcessRDTMessage(int index, const std::string &message)
{
    rdtStorage.sendRDTmessage(storageServSockets[index].socket, message, storageServSockets[index].direccion);
    processStorageResponse(storageServSockets[index].socket, storageServSockets[index].direccion, message);
}

bool isServerAvailable(int index)
{

    // std::cout << "isServerAvailable? -> " << index << std::endl;

    // enviar K
    std::string message = rdtStorage.createRDTmessage("K");
    rdtStorage.sendRDTmessage(storageServSockets[index].socket, message, storageServSockets[index].direccion);

    struct timeval tv;
    tv.tv_sec = 2; // 2 segundos de timeout 
    tv.tv_usec = 0; 
    setsockopt(storageServSockets[index].socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));

    // recibir ACK
    std::string rdtMessage = rdtStorage.receiveACKmessage(storageServSockets[index].socket, storageServSockets[index].direccion);
    std::string messageType = rdtMessage.substr(0, 3);
    return messageType == "ACK";
}

// keep alive function
void keepAlive(int &index, int &nextIndex)
{
    while (!isServerAvailable(index)) // busco index
    {
        index = nextIndex;
        nextIndex = (index == STORAGE_INDEX_LIMIT) ? 0 : index + 1;
    }

    while(!isServerAvailable(nextIndex)) // busco el index + 1
    {
        nextIndex = (nextIndex == STORAGE_INDEX_LIMIT) ? 0 : nextIndex + 1;
    }
}

// FUNCION DE LECTURA RECURSIVA
std::vector<std::pair<std::string, std::string>> readData(std::string key, int depthSearch, std::unordered_set<std::string> &visitedKeys)
{
    // RESULTADO
    std::vector<std::pair<std::string, std::string>> result;

    // Verificar casos base
    if (depthSearch <= 0 || visitedKeys.find(key) != visitedKeys.end())
    {
        return result; // Devolver un vector vacío si la profundidad es 0 o la clave ya fue visitada
    }

    // Agregar la clave actual a las claves visitadas
    visitedKeys.insert(key);

    // CALCULAR INDICE DE SERVIDOR DE ALMACENAMIENTO (hash function)
    // se almacena en dos servidores de almacenamiento (index y nextIndex) (nextIndex = index + 1 || 0 if index = 3)
    int index = hashFunction(key[0]);
    int nextIndex = (index == STORAGE_INDEX_LIMIT) ? 0 : index + 1;
    keepAlive(index, nextIndex);

    // CREAR MENSAJE PARA PEDIR DATOS AL SERVIDOR DE ALMACENAMIENTO
    std::string readMessage = rdtStorage.createRDTmessage("R" + complete_digits(key.size(), 4) + key);

    // ENVIAR MENSAJE PARA PEDIR DATOS AL SERVIDOR DE ALMACENAMIENTO -> INDEX
    sendAndProcessRDTMessage(index, readMessage);

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

    std::vector<std::string> tempValues;
    // RECIBIR DATOS DEL SERVIDOR DE ALMACENAMIENTO - FORMATO RDT MESSAGE
    for (int i = 0; i < numData; i++)
    {
        std::string rdtMessage = rdtStorage.receiveRDTmessage(storageServSockets[index].socket, storageServSockets[index].direccion);
        // MENSAJE CORRECTO?
        if (checkStorageMessage(rdtMessage, storageServSockets[index].socket, storageServSockets[index].direccion))
        {
            // std::cout << "message de storage: " << rdtMessage << std::endl;
            //  DECODIFICAR MENSAJE
            std::string message = rdtStorage.decodeRDTmessage(rdtMessage);
            std::string value;
            // OBTENER VALOR DEL MENSAJE
            parseValuesMessage(message, value);

            // INSERTAR EN EL VECTOR DE RESULTADO
            result.push_back(std::make_pair(key, value));
            tempValues.push_back(value);
        }
    }

    // llamar recursivamente a la funcion para cada valor obtenido
    for (int i = 0; i < tempValues.size(); i++)
    {
        auto recursiveResult = readData(tempValues[i], depthSearch - 1, visitedKeys);
        result.insert(result.end(), recursiveResult.begin(), recursiveResult.end());
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

        //printStorageServers();

        std::string key;
        std::string value;

        // obtener (key, value)
        parseCreateMessage(message, key, value);

        int index = hashFunction(key[0]);
        int nextIndex = (index == STORAGE_INDEX_LIMIT) ? 0 : index + 1;

        // consultar estado de los servidores de almacenamiento
        keepAlive(index, nextIndex);

        std::string storageMessage = rdtStorage.createRDTmessage(message);
        //std::cout << "mensaje a enviar: " << storageMessage << std::endl;

        // almacenamiento en index y nextIndex
        sendAndProcessRDTMessage(index, storageMessage);
        sendAndProcessRDTMessage(nextIndex, storageMessage);

        break;
    }
    case 'R': // read command
    {
        std::cout << "Read Command ------------------------------------------" << std::endl;

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

        std::cout << "REGISTROS TOTALES: " << result.size() << std::endl;

        // EMPIEZA INTERACCION CON EL CLIENTE
        // Enviar numero de datos al cliente
        std::string message = "R" + complete_digits(result.size(), 4);
        std::string rdtMessage = rdtStorage.createRDTmessage(message);

        rdtStorage.sendRDTmessage(sockfd, rdtMessage, clientAddr);
        processClientResponse(sockfd, clientAddr, rdtMessage);

        // Enviar registros al cliente
        for (auto &pair : result)
        {
            std::string dataMessage = "R" + complete_digits(pair.first.size(), 4) +
                                      pair.first +
                                      complete_digits(pair.second.size(), 4) +
                                      pair.second;

            /* server -> client
            R
            0000
            key
            0000
            value
            */
            std::string rdtDataMessage = rdtStorage.createRDTmessage(dataMessage);

            rdtClient.sendRDTmessage(sockfd, rdtDataMessage, clientAddr);
            processClientResponse(sockfd, clientAddr, rdtDataMessage);
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

        keepAlive(index, nextIndex);

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
        keepAlive(index, nextIndex);

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

    std::cout << "Message Seq Num: " << message_seq_num << std::endl;
    std::cout << "RDT Seq Num: " << RDT_seq_num << std::endl;

    if (rdtClient.checkRDTmessage(receivedMessage) && (message_seq_num > RDT_seq_num))
    {
        std::cout << "Correct Message" << std::endl;
        rdtClient.accSeq_num = message_seq_num;
        rdtClient.sendACK(sockfd, clientAddr);
        return true;
    }
    else
    {
        std::cout << "Incorrect Message" << std::endl;
        rdtClient.sendNAK(sockfd, clientAddr);
        return false;
    }
}

// procesar mensaje del cliente
void processClientMessage(int sockfd, const sockaddr_in &clientAddr)
{
    while (true)
    {
        // recibir mensaje del cliente - capa de transporte
        std::string RDTmessage = rdtClient.receiveRDTmessage(sockfd, clientAddr);

        // verificar mensaje
        if (checkClientMessage(RDTmessage, sockfd, clientAddr))
        {
            // mensaje correcto -> procesar mensaje - extraer mensaje para capa de aplicacion
            std::string message = rdtClient.decodeRDTmessage(RDTmessage);
            std::cout << "Message from client: " << message << std::endl;
            processMessage(sockfd, message);
        }
        else
        {
            // get retransmition
            std::cout << "Incorrect Message" << std::endl;
        }

        rdtClient.reset();
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



    processClientMessage(mainSocketfd, clientAddr);
        

    close(mainSocketfd);
    return 0;
}