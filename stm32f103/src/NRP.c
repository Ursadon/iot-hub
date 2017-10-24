/*
 * NRP.c
 *
 *  Created on: 12 окт. 2017 г.
 *      Author: NikolayL
 */
#include "stm32f10x.h"
#include "NRP.h"
#include "nrf24.h"
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"

/* Lexicographically compare two arrays of size NUM_COLS. */
int sortByMetrics(const void* a, const void* b) {
	return ((const int *) a)[1] - ((const int *) b)[1];
}

uint64_t convertPipeAddress(uint8_t address) {
	uint64_t tx_address64;
	tx_address64 = 0xA0A1F0F100LL + (uint64_t) address;
	return tx_address64;
	//return address + address * 256 + address * 65536 + address * 16777216 + address * 4294967296;
}

uint8_t uRIP_lookuphost(uint8_t host) {
	for (int i = 0; i < 255; i++) {
		if (routingTable[i][Route] == host) {
			return i;
		}
	}
	return 0xff;
}
void bubble_sort() {
	int i, s = 1;
	uint8_t key1, key2, key3;
	while (s) {
		s = 0;
		for (i = 1; i < 256; i++) {
			if (routingTable[i][1] < routingTable[i - 1][1]) {
				key1 = routingTable[i][0];
				key2 = routingTable[i][1];
				key3 = routingTable[i][2];

				routingTable[i][0] = routingTable[i - 1][0];
				routingTable[i][1] = routingTable[i - 1][1];
				routingTable[i][2] = routingTable[i - 1][2];

				routingTable[i - 1][0] = key1;
				routingTable[i - 1][1] = key2;
				routingTable[i - 1][2] = key3;
				s = 1;
			}
		}
	}
}

void uRIP_updateRecord(uint8_t route, uint8_t metrics, uint8_t nexthop) {
	uint8_t route_id = uRIP_lookuphost(route);
	if ((route_id == 0xff) | (metrics < routingTable[route_id][Metrics])) { // Новый маршрут
		routingTable[route_id][Route] = route;
		routingTable[route_id][Metrics] = metrics + 1;
		routingTable[route_id][NextHop] = nexthop;
		taskENTER_CRITICAL();
		bubble_sort();
		taskEXIT_CRITICAL();
		if (route_id == 0xff) {
			routingTableCount++;
		}
	} else {
		return; // Такая метрика маршрута уже есть - маргрут игнорируем
	}
}

void uRIP_deleteRecord(uint8_t route) {
	uint8_t route_id = uRIP_lookuphost(route);
	if (route_id == 0xff)
		return;
	routingTable[route_id][Route] = 0xff;
	routingTable[route_id][Metrics] = 0xff;
	routingTable[route_id][NextHop] = 0xff;
	routingTableCount++;
}

void uRIP_flush() {
	for (int i = 0; i < 256; i++) {
		routingTable[i][Route] = 0xff;
		routingTable[i][Metrics] = 0xff;
		routingTable[i][NextHop] = 0xff;
	}
	routingTable[0][Route] = rx_addr;
	routingTable[0][Metrics] = 0;
	routingTable[0][NextHop] = 0;
	routingTableCount = 1;
}

/**
 * Отправка пакета по протоколу NRP к указанному физическому девайсу
 * @param host
 * @param packet
 * @return
 */
uint8_t NRP_send_packet(uint8_t host, NRP_packet packet) {
	uint8_t rval;
	uint8_t header = (packet.version << 4) + packet.type;
	uint8_t buf[32] = { header, packet.destination, packet.source, 0 };

	for (int i = 0; i < packet._length; i++) {
		buf[i + 4] = packet.data[i];
	}
	radio_openWritingPipe(0xA0A1F0F100LL + host);
	radio_stopListening();

	if (host == 0x00) { // is multicast?
		rval = radio_write_multicast(buf, 4 + packet._length, 1);
	} else {
		rval = radio_write_multicast(buf, 4 + packet._length, 0);
	}
	radio_startListening();
	return rval;
}
uint8_t uRIP_sendUpdate(uint8_t host) {
	NRP_packet packet;
	packet.version = 1;
	packet.type = uRIP_update;
	packet.source = rx_addr;
	packet.destination = host;
	packet.ttl = 0;
	packet._length = routingTableCount * 3;
	// Convertind 2d array to 1d
	for (unsigned int i = 0; i < routingTableCount * 3; i++) {
		packet.data[i] = routingTable[i / 3][i % 3];
	}

	return NRP_send_packet(host, packet);
}
void NRP_parsePacket(NRP_packet packet) {
	if (packet.ttl == 254) {
		//cout << "[warning] Packet dropped - ttl exceeded" << endl;
		return;
	}
	if (packet.type == uRIP_update) { // uRIP update
		//cout << CYAN << printDate() << RESET << "-> [RX] [info] uRIP update response" << endl;
		// TODO: проверять корректность данных 0 остаток от деления на 3 должен быть равен 0
		// Конвертируем 1d массив в 2d и сверяем с маршрутами
		volatile NRP_packet packet2 = packet;
		for (int i = 0; i < packet._length / 3; i++) {
			uRIP_updateRecord(packet.data[i * 3 + 0], packet.data[i * 3 + 1],
					packet.source);
		}
		return;
	};
	if (packet.destination == 0x00) { // Discovery packet
		return;
	};
	if (packet.destination != rx_addr) { // transit packet
		packet._transit = 1;
		//cout << "(TRANSIT)" << endl
	} else {
		packet._transit = 0;
		//cout << "(TO ME)" << endl;
	};
}
