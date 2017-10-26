#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#include "NRP.h"
#include "main.h"
#include "additionals.h"

#ifdef _PRINTF_DEBUG_
char* c_printDate() {
	static char time_buffer[80];
	time_t rawtime;
	struct tm * timeinfo;
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(time_buffer, sizeof(time_buffer), "[%Y-%m-%d %H:%M:%S] ",
			timeinfo);
	return time_buffer;
}
#endif
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
uint64_t convertPipeAddress(uint8_t address) {
	return BASEADDR + (uint64_t) address;
}

bool NRP_send_packet(uint8_t host, NRP_packet packet) {
	uint8_t header = (packet.version << 4) + packet.type;
	uint8_t buf[32] = { header, packet.destination, packet.source, packet.ttl };

	for (int i = 0; i < (uint8_t)packet._length; i++) {
		buf[i + 4] = packet.data[i];
	}
	if (host == 0x00) { // is multicast?
		return radio_send(host, buf, 4 + packet._length, 1);
	} else {
		return radio_send(host, buf, 4 + packet._length, 0);
	}
}

void NRP_parsePacket(NRP_packet packet) {
	if (packet.ttl == 254) {
		__DEBUG(printf("%s%s%s-> [RX] [warning] Packet dropped - ttl exceeded %s\n", CYAN, c_printDate(), RED, RESET););
		return;
	}
	if (packet.type == uRIP_update) { // uRIP update
		if (((packet.source == 0xa7) | (packet.source == 0x17)) & ((rx_addr == 0xa7) | (rx_addr == 0x17))) {
			return;
		}
		//__DEBUG(printf("%s%s%s-> [RX] [info] uRIP request for my routes %s\n", CYAN, c_printDate(), WHITE, RESET););

		// TODO: проверять корректность данных 0 остаток от деления на 3 должен быть равен 0
		// Конвертируем 1d массив в 2d и сверяем с маршрутами
		for (int i = 0; i < packet._length / 3; i++) {
			uRIP_updateRecord(packet.data[i * 3 + 0], packet.data[i * 3 + 1],
					packet.source);
		}
		return;
	};

	if (packet.destination != rx_addr) { // transit packet
		__DEBUG(printf("%s%s%s-> [RX] (TRANSIT) to: 0x%02X%s\n", CYAN, c_printDate(), BLUE, (unsigned int)packet.destination ,RESET););
		packet.ttl++;
		uint8_t host_id = uRIP_lookuphost(packet.destination);
		if(uRIP_lookuphost(packet.destination) != 0xff){
			NRP_send_packet(routingTable[host_id][NextHop], packet);
		} else {
			__DEBUG(printf("%s%s%s-> [RX] [warning] No route to host: 0x%02X %s\n", CYAN, c_printDate(), RED, (unsigned int)packet.destination, RESET););
		}
	} else {
		__DEBUG(printf("%s%s%s-> [RX] (TO_ME) %s\n", CYAN, c_printDate(), BLUE, RESET); NRP_dump_packet(packet););
	};
}

/* Lexicographically compare two arrays of size NUM_COLS. */
int sortByMetrics(const void* a, const void* b) {
	return ((const int *) a)[1] - ((const int *) b)[1];
}
void uRIP_updateRecord(uint8_t route, uint8_t metrics, uint8_t nexthop) {
	uint8_t route_id = uRIP_lookuphost(route);
	if ((route_id == 0xff) | (metrics < routingTable[route_id][Metrics])) { // Новый маршрут
		routingTable[route_id][Host] = route;
		routingTable[route_id][Metrics] = metrics + 1;
		routingTable[route_id][NextHop] = nexthop;
		bubble_sort();
		if (route_id == 0xff) {
			__DEBUG(printf("%s%s%s-> [RX] [info] New route: 0x%02X via 0x%02X M=%u%s\n", CYAN, c_printDate(), BLUE, (unsigned int) route, (unsigned int) nexthop, (unsigned int) metrics+1,RESET););
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
	routingTable[route_id][Host] = 0xff;
	routingTable[route_id][Metrics] = 0xff;
	routingTable[route_id][NextHop] = 0xff;
	routingTableCount++;
	__DEBUG(printf("%s%s%s <internal> [info] Delete route: 0x%02X %s\n", CYAN, c_printDate(), BLUE, (unsigned int) route, RESET););

}
uint8_t uRIP_lookuphost(uint8_t host) {
	for (int i = 0; i < 255; i++) {
		if (routingTable[i][Host] == host) {
			return i;
		}
	}
	return 0xff;
}

void uRIP_flush() {
	for (int i = 0; i < 256; i++) {
		routingTable[i][Host] = 0xff;
		routingTable[i][Metrics] = 0xff;
		routingTable[i][NextHop] = 0xff;
	}
	routingTable[0][Host] = rx_addr;
	routingTable[0][Metrics] = 0;
	routingTable[0][NextHop] = 0;
	routingTableCount = 1;
}
/**
 * Отправка таблицы маршрутизации uRIP указанному физическому хосту
 * @param host
 * @return
 */
bool uRIP_sendRoutes(uint8_t host) {
	NRP_packet packet;
	packet.version = 1;
	packet.type = uRIP_update;
	packet.source = rx_addr;
	packet.destination = host;
	packet.ttl = 0;
	packet._length = routingTableCount * 3;
	// Converting 2d array to 1d
	for (unsigned int i = 0; i < routingTableCount * 3; i++) {
		packet.data[i] = routingTable[i / 3][i % 3];
	}

	return NRP_send_packet(host, packet);
}
