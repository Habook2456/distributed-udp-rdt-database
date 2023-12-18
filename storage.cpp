#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>

#include "RDT.h"
#include "utils.h"
#include "BD.h"

RDT rdtStorage(1000);
UDPdatabase database; // base de datos - abstraccion de grafo

// procesar mensaje del servidor principal
void processServerResponse(int sockfd, sockaddr_in serverAddr)
{
    std::string rdtMessage = rdtStorage.receiveACKmessage(sockfd, serverAddr);

    if (rdtMessage[0] == 'A')
    {
        std::cout << "ACK from Server" << std::endl;
        // rdtStorage.sendACK(sockfd, serverAddr);
    }
    else if (rdtMessage[0] == 'N')
    {
        // TO DO: logica de reenvio en caso de NAK
        std::cout << "NAK from Server" << std::endl;
    }
    else
    {
        std::cout << "Error in ACK message from Storage" << std::endl;
    }
}

// PROCESAR MENSAJE
void processMessage(std::string message, int sockfd, const sockaddr_in &mainServAddr)
{
    // std::cout << "Mensaje recibido: " << message << std::endl;
    char operation = message[0];

    switch (operation)
    {
    case 'C': // create command
    {
        std::cout << "INSERT DATA" << std::endl;
        std::string key;
        std::string value;

        // obtener (key, value)
        parseCreateMessage(message, key, value);

        // insertar (key, value) en la base de datos
        database.createData(key, value);
        std::cout << key << " - " << value << std::endl;
        database.countData();
        break;
    }
    case 'R': // read command
    {
        std::cout << "READ DATA" << std::endl;

        // obtener Key
        int keySize = std::stoi(message.substr(1, 4));
        std::string key = message.substr(5, keySize);

        // obtener valores conectados al Key
        std::vector<std::string> data = database.readData(key);

        // enviar numero de valores al servidor principal
        /*
        message format:
        R               -> type message (1 by   te)
        0000            -> size of vector data (4 bytes)
        */
        std::string message = "R" + complete_digits(data.size(), 4);
        std::string rdtMessage = rdtStorage.createRDTmessage(message);

        rdtStorage.sendRDTmessage(sockfd, rdtMessage, mainServAddr);
        processServerResponse(sockfd, mainServAddr);

        // enviar valores al servidor principal
        for (int i = 0; i < data.size(); i++)
        {
            std::cout << key << " - " << data[i] << std::endl;
            /*
            message format:
            0000            -> size of data (4 bytes)
            data            -> value (size bytes)
            */
            string message = complete_digits(data[i].size(), 4) + data[i];
            std::string rdtMessage = rdtStorage.createRDTmessage(message);

            // enviar al servidor principal
            rdtStorage.sendRDTmessage(sockfd, rdtMessage, mainServAddr);
            processServerResponse(sockfd, mainServAddr);
        }

        database.countData();

        break;
    }
    case 'U': // update command
    {
        std::cout << "UPDATE DATA" << std::endl;
        std::string key;
        std::string oldValue;
        std::string newValue;

        // obtener (key, oldValue, newValue)
        parseUpdateMessage(message, key, oldValue, newValue);

        // actualizar (key, oldValue, newValue) en la base de datos
        database.updateData(key, oldValue, newValue);
        database.countData();
        break;
    }
    case 'D': // delete command
    {
        std::cout << "DELETE DATA" << std::endl;
        std::string key;
        std::string value;

        // obtener (key, value)
        parseCreateMessage(message, key, value);

        // eliminar (key, value) en la base de datos
        database.deleteData(key, value);
        database.countData();
        break;
    }
    case 'K': // keep alive command
    {
        std::cout << "KEEP ALIVE" << std::endl;
        break;
    }
    default:
        break;
    }
}

// verificar mensaje del servidor principal
bool checkServerMessage(const std::string &receivedMessage, int sockfd, const sockaddr_in &mainServAddr)
{
    uint32_t message_seq_num = rdtStorage.extractSeqNum(receivedMessage);
    uint32_t RDT_seq_num = rdtStorage.getSeqNum();

    if (rdtStorage.checkRDTmessage(receivedMessage) && (message_seq_num == RDT_seq_num))
    {
        // std::cout << "Correct Message in storage (check)" << std::endl;
        //  std::string ackMessage = "ACK" + complete_digits(message_seq_num, 10);
        rdtStorage.sendACK(sockfd, mainServAddr);
        // rdtServer.sendACK(sockfd, clientAddr);
        return true;
    }
    else
    {
        // std::cout << "Incorrect Message in storage (check)" << std::endl;
        //  std::string nackMessage = "NAK" + complete_digits(RDT_seq_num, 10);
        //  rdtServer.sendNACK(sockfd, clientAddr);
        return false;
    }
}

void processServerMessage(int sockfd, const sockaddr_in &mainServAddr)
{
    while (true)
    {
        // recibir mensaje
        std::string RDTmessage = rdtStorage.receiveRDTmessage(sockfd, mainServAddr);
        // verificar mensaje
        if (checkServerMessage(RDTmessage, sockfd, mainServAddr))
        {
            std::string message = rdtStorage.decodeRDTmessage(RDTmessage);
            // std::cout << "Message from Main Server: " << message << std::endl;
            //  procesar mensaje
            processMessage(message, sockfd, mainServAddr);
        }
        else
        {
            std::cout << "Incorrect Message in storage" << std::endl;
        }
    }
}

int main(int argc, char *argv[])
{
    system("clear");
    if (argc != 2)
    {
        std::cerr << "Uso: " << argv[0] << " <numero de puerto>" << std::endl;
        return -1;
    }

    // obtener el numero de puerto desde los argumentos de la linea de comandos
    int puerto = std::stoi(argv[1]);

    int servidor2Socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (servidor2Socket == -1)
    {
        std::cerr << "Error al crear el socket del servidor 2 (puerto " << puerto << ")." << std::endl;
        return -1;
    }

    sockaddr_in servidor2Direccion;
    servidor2Direccion.sin_family = AF_INET;
    servidor2Direccion.sin_addr.s_addr = INADDR_ANY;
    servidor2Direccion.sin_port = htons(puerto);

    if (bind(servidor2Socket, reinterpret_cast<struct sockaddr *>(&servidor2Direccion), sizeof(servidor2Direccion)) == -1)
    {
        std::cerr << "Error al vincular el socket a la dirección." << std::endl;
        close(servidor2Socket);
        return -1;
    }

    sockaddr_in mainServAddr;
    socklen_t mainServAddrLen = sizeof(mainServAddr);

    processServerMessage(servidor2Socket, mainServAddr);

    close(servidor2Socket);

    return 0;
}
