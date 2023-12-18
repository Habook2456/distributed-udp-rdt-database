#ifndef _RDT_H_
#define _RDT_H_

#include <iostream>
#include <string>
#include "utils.h"

const int MAX_BUFFER_SIZE = 1024;

class RDT
{
public:
    // numero de secuencia acumulado
    uint32_t accSeq_num = 0;

    // caracter de relleno - padding
    char fillCharacter = '*';

    // constructor
    RDT(uint32_t seqNum) : accSeq_num(seqNum) {}
    RDT(){};

    // acumular numero de secuencia
    uint32_t accumulate(uint32_t messageSize)
    {
        accSeq_num += messageSize;
        return accSeq_num;
    }

    // rellenar mensaje con caracter de relleno - padding
    std::string fillString(std::string message)
    {
        return message + std::string(975 - message.size(), fillCharacter);
    }

    // extraer numero de secuencia del mensaje
    uint32_t extractSeqNum(const std::string &rdtMessage)
    {
        return std::stoul(rdtMessage.substr(1, 10));
    }

    // extraer tamaño del mensaje
    uint16_t extractMessageSize(const std::string &rdtMessage)
    {
        return std::stoul(rdtMessage.substr(11, 5));
    }

    // extraer mensaje
    std::string extractMessage(const std::string &rdtMessage, uint16_t messageSize)
    {
        return rdtMessage.substr(16, messageSize);
    }

    // extraer CHECKSUM del mensaje
    uint16_t extractChecksum(const std::string &rdtMessage)
    {
        return std::stoul(rdtMessage.substr(991, 9));
    }

    // get numero de secuencia acumulado
    uint32_t getSeqNum()
    {
        return accSeq_num;
    }

    // crear mensaje RDT
    std::string createRDTmessage(const std::string &message)
    {
        /* RDT MESSAGE FORMAT
        D               -> type of message (data)               (1b)
        0000000000      -> sequence number                      (10b)
        00000           -> message size without padding         (5b)
        message         -> message with padding                 (975b)
        000000000       -> checksum - message without padding   (9b)
        total size: 1000b
        */

        std::string rdtMessage = 'D' +
                                 complete_digits(accumulate(message.size()), 10) +
                                 complete_digits(message.size(), 5) +
                                 fillString(message) +
                                 complete_digits(calculateChecksum(message), 9);
        return rdtMessage;
    }

    // decodificar mensaje RDT - extraer mensaje sin padding
    std::string decodeRDTmessage(const std::string &rdtMessage)
    {
        uint32_t seq_num = extractSeqNum(rdtMessage);
        uint16_t messageSize = extractMessageSize(rdtMessage);
        std::string message = extractMessage(rdtMessage, messageSize);
        uint16_t checksum = extractChecksum(rdtMessage);

        accSeq_num = seq_num;

        return message;
    }

    // calcular CHECKSUM del mensaje == CHECKSUM del mensaje recibido
    bool checkRDTmessage(const std::string &rdtMessage)
    {
        uint32_t seq_num = extractSeqNum(rdtMessage);
        uint16_t messageSize = extractMessageSize(rdtMessage);
        std::string message = extractMessage(rdtMessage, messageSize);
        uint16_t checksum = extractChecksum(rdtMessage);

        return calculateChecksum(message) == checksum;
    }

    // enviar mensaje RDT
    void sendRDTmessage(int sockfd, const std::string &message, const sockaddr_in &destAddr)
    {
        sendto(sockfd, message.c_str(), message.size(), 0, (struct sockaddr *)&destAddr, sizeof(destAddr));
    }

    // recibir mensaje RDT - actualizar numero de secuencia acumulado
    std::string receiveRDTmessage(int sockfd, const sockaddr_in &senderAddr)
    {
        char buffer[MAX_BUFFER_SIZE];
        socklen_t senderAddrLen = sizeof(senderAddr);

        ssize_t bytesReceived = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&senderAddr, &senderAddrLen);

        if (bytesReceived == -1)
        {
            std::cerr << "Error al recibir el mensaje" << std::endl;
            return "";
        }

        buffer[bytesReceived] = '\0';
        std::string receivedMessage(buffer);

        // actualizar numero de secuencia acumulado
        // accSeq_num = extractSeqNum(receivedMessage);

        return receivedMessage;
    }

    // recibir mensaje ACK - no actualizar numero de secuencia acumulado
    std::string receiveACKmessage(int sockfd, sockaddr_in &senderAddr)
    {
        char buffer[MAX_BUFFER_SIZE];
        socklen_t senderAddrLen = sizeof(senderAddr);

        ssize_t bytesReceived = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&senderAddr, &senderAddrLen);

        if (bytesReceived == -1)
        {
            std::cerr << "Error al recibir el mensaje" << std::endl;
            return "";
        }

        buffer[bytesReceived] = '\0';
        std::string receivedMessage(buffer);

        return receivedMessage;
    }

    // enviar ACK
    void sendACK(int sockfd, const sockaddr_in &destAddr)
    {
        /*
        ACK             -> acknowledgement message              (3b)
        0000000000      -> sequence number                      (10b)
        */
        std::string ackMessage = "ACK" + complete_digits(accSeq_num, 10);
        sendRDTmessage(sockfd, ackMessage, destAddr);
    }

    // enviar NAK
    void sendNAK(int sockfd, const sockaddr_in &destAddr)
    {
        /*
        NAK             -> acknowledgement message              (3b)
        0000000000      -> sequence number                      (10b)
        */
        std::string nackMessage = "NAK" + complete_digits(accSeq_num, 10);
        sendRDTmessage(sockfd, nackMessage, destAddr);
    }
};

#endif
