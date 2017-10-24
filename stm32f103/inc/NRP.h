/*
 * NRP.h
 *
 *  Created on: 12 окт. 2017 г.
 *      Author: NikolayL
 */

#ifndef NRP_H_
#define NRP_H_

uint8_t rx_addr;

enum Route {Route=0, Metrics, NextHop};

enum PacketType {Data=0, uRIP_update};

typedef struct NRP_packet     //Создаем структуру!
{
    uint8_t version:4;
    uint8_t type:4;
    uint8_t destination;
    uint8_t source;
    uint8_t ttl;
    uint8_t data[32];
    uint8_t _transit:1;
    uint8_t _length;

} NRP_packet;

uint8_t routingTable[256][3];
unsigned int routingTableCount;

uint64_t convertPipeAddress(uint8_t address);
uint8_t lookuphost(uint8_t host);
void NRP_parsePacket(NRP_packet packet);

uint8_t uRIP_sendUpdate(uint8_t host);
void uRIP_flush();

#endif /* NRP_H_ */
