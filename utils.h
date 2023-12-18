#ifndef _UTILS_H_
#define _UTILS_H_

#include <iostream>
#include <string>
using std::string;

// completar digitos de un numero con ceros a la izquierda
string complete_digits(int t, int bytesNum)
{
    string result = std::to_string(t);

    if (result.length() < bytesNum)
    {
        result = string(bytesNum - result.length(), '0') + result;
    }

    return result;
}

// calcular checksum
uint16_t calculateChecksum(const std::string &data)
{
    uint16_t checksum = 0;

    for (char c : data)
    {
        checksum += static_cast<uint8_t>(c);
    }

    return checksum;
}

void retransmition(std::string message, int sockfd, sockaddr_in serverAddr)
{
    int intentos = 0;
    do{
        std::cout << "Reenviando -> " << message << "\n";
        rdtClient.sendRDTmessage(sockfd, message, serverAddr);

        std::string response = rdtClient.receiveACKmessage(sockfd, serverAddr);

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

// parsear mensajes
// CREATE
void parseCreateMessage(const std::string &message, std::string &key, std::string &value)
{
    int keySize = std::stoi(message.substr(1, 4));
    key = message.substr(5, keySize);
    int valueSize = std::stoi(message.substr(5 + keySize, 4));
    value = message.substr(5 + keySize + 4, valueSize);
}
// READ
void parseReadMessage(const std::string &message, std::string &key, std::string &depthSearch)
{
    int keySize = std::stoi(message.substr(1, 4));
    key = message.substr(5, keySize);
    int depthSearchSize = std::stoi(message.substr(5 + keySize, 4));
    depthSearch = message.substr(5 + keySize + 4, depthSearchSize);
}
// UPDATE
void parseUpdateMessage(const std::string &message, std::string &key, std::string &oldValue, std::string &newValue)
{
    int keySize = std::stoi(message.substr(1, 4));
    key = message.substr(5, keySize);
    int oldValueSize = std::stoi(message.substr(5 + keySize, 4));
    oldValue = message.substr(5 + keySize + 4, oldValueSize);
    int newValueSize = std::stoi(message.substr(5 + keySize + 4 + oldValueSize, 4));
    newValue = message.substr(5 + keySize + 4 + oldValueSize + 4, newValueSize);
}

// READ STORAGE -> values
void parseValuesMessage(const std::string &message, std::string& value)
{
    int size = std::stoi(message.substr(0,4));
    value = message.substr(4, size);
}

#endif