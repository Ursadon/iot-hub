/*
 * Copyright (c) 2017, NikolayL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * File:   NRP.h
 * Author: NikolayL
 *
 * Created on October 9, 2017, 12:58 AM
 */
#ifndef NRP_H
#define NRP_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _RPI_
	const uint8_t rx_addr = 0x17;
#elif _OPI_
	const uint8_t rx_addr = 0xA7;
#else
	static const uint8_t rx_addr = 0x07;
#endif

#define BASEADDR 0xA0A1F0F100LL
#define ROUTE_GARBAGE_TIMER 6
#define ROUTE_TIMEOUT_TIMER 4
	enum Route { Host = 0, Metrics, NextHop, Timer };
	enum PacketType { ptData = 0, uRIP_update, MCUcontrol } PacketType;

	unsigned int routingTableCount;
	unsigned int routingTable[256][4];

	typedef struct NRP_packet     //Создаем структуру!
	{
		uint8_t version : 4;
		uint8_t type : 4;
		uint8_t destination;
		uint8_t source;
		uint8_t ttl;
		uint8_t data[28];
		uint8_t _length;
	} NRP_packet;

	bool NRP_send_packet(uint8_t host, NRP_packet packet);
	void NRP_parsePacket(NRP_packet packet);

	uint8_t uRIP_lookuphost(uint8_t host);
	void uRIP_flush();
	void uRIP_updateRecord(uint8_t route, uint8_t metrics, uint8_t nexthop);
	void uRIP_sendRoutes(uint8_t host);
	void uRIP_deleteRoute(uint8_t route);
	void uRIP_garbageCollector();
	uint64_t convertPipeAddress(uint8_t address);
	void uRIP_sortDatabase();
	char* c_printDate();

#ifdef __cplusplus
}
#endif

#endif /* NRP_H */
