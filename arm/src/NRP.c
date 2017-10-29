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
	strftime(time_buffer,
		sizeof(time_buffer),
		"[%Y-%m-%d %H:%M:%S] ",
		timeinfo);
	return time_buffer;
}
#endif
void uRIP_sortDatabase() {
	int i, s = 1;
	uint8_t key1, key2, key3, key4;
	while (s) {
		s = 0;
		for (i = 1; i < 256; i++) {
		    // Sorting by metrics
			if (routingTable[i][Metrics] < routingTable[i - 1][Metrics]) {
				key1 = routingTable[i][0];
				key2 = routingTable[i][1];
				key3 = routingTable[i][2];
				key4 = routingTable[i][3];
				routingTable[i][0] = routingTable[i - 1][0];
				routingTable[i][1] = routingTable[i - 1][1];
				routingTable[i][2] = routingTable[i - 1][2];
				routingTable[i][3] = routingTable[i - 1][3];
				routingTable[i - 1][0] = key1;
				routingTable[i - 1][1] = key2;
				routingTable[i - 1][2] = key3;
				routingTable[i - 1][3] = key4;
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

	for (int i = 0; i < (uint8_t) packet._length; i++) {
		buf[i + 4] = packet.data[i];
	}
	if (host == 0x00) { // is multicast?
		return radio_send(host, buf, 4 + packet._length, 1);
	}
	else {
		return radio_send(host, buf, 4 + packet._length, 0);
	}
}

void NRP_parsePacket(NRP_packet packet) {
	if (packet.ttl == 254) {
		__DEBUG(printf("%s%s%s-> [RX] [warning] Packet dropped - ttl exceeded %s\n", CYAN, c_printDate(), RED, RESET) ;);
		return;
	}
	if (packet.type == uRIP_update) { // uRIP update
//		if (((packet.source == 0xa7) | (packet.source == 0x17))
//		        & ((rx_addr == 0xa7) | (rx_addr == 0x17))) {
//			return;
//		}
		//__DEBUG(printf("%s%s%s-> [RX] [info] uRIP request for my routes %s\n", CYAN, c_printDate(), WHITE, RESET););
		if (packet._length % 3 == 0)
		{
			for (int i = 0; i < packet._length / 3; i++)
			{
				uRIP_updateRecord(packet.data[i * 3 + 0], packet.data[i * 3 + 1], packet.source);
			}
			return;
		}
		else
		{
			__DEBUG(printf("%s%s%s-> [RX] [err] Incorrect uRIP packet length %s\n", CYAN, c_printDate(), RED, RESET) ;);
		}
	}
	if (packet.destination != rx_addr) { // transit packet
		__DEBUG(printf("%s%s%s-> [RX] (TRANSIT) to: 0x%02X%s\n", CYAN, c_printDate(), BLUE, (unsigned int)packet.destination, RESET) ;);
		packet.ttl++;
		uint8_t host_id = uRIP_lookuphost(packet.destination);
		if (uRIP_lookuphost(packet.destination) != 0xff) {
			NRP_send_packet(routingTable[host_id][NextHop], packet);
		}
		else {
			__DEBUG(printf("%s%s%s-> [RX] [error] No route to host: 0x%02X %s\n", CYAN, c_printDate(), RED, (unsigned int)packet.destination, RESET) ;);
		}
	}
	else {
		__DEBUG(printf("%s%s%s-> [RX] (TO_ME) %s\n", CYAN, c_printDate(), BLUE, RESET) ; NRP_dump_packet(packet) ;);
		CMD_parser(packet);
	}
}

void uRIP_updateRecord(uint8_t route, uint8_t metrics, uint8_t nexthop) {
	metrics++;
	if ((route == 0xff) | (route == 0x00)) {
		__DEBUG(printf("%s%s%s-> [RX] [err] Wrong route: 0x%02X %s\n", CYAN, c_printDate(), RED, (unsigned int)route, RESET) ;);
		return;
	}
	if (metrics == 16) { // request to delete route from routeDB
		if (route == rx_addr)
			return;
		__DEBUG(printf("%s%s%s-> [RX] [info] Deleting route 0x%02X %s\n", CYAN, c_printDate(), YELLOW, (unsigned int)route, RESET) ;);
		uRIP_deleteRoute(route);
		return;
	}
	uint8_t route_id = uRIP_lookuphost(route);
	if ((metrics <= routingTable[route_id][Metrics]) | (route_id == 0xff)) { // New route or more specific
		{
			routingTable[route_id][Timer] = 0;
			if (route_id == 0xff)
			{ // New route?
				__DEBUG(printf("%s%s%s-> [RX] [info] New route: 0x%02X via 0x%02X M=%u%s\n", CYAN, c_printDate(), BLUE, (unsigned int) route, (unsigned int) nexthop, (unsigned int) metrics, RESET) ;);
				routingTable[route_id][Host] = route;
				routingTable[route_id][Metrics] = metrics;
				routingTable[route_id][NextHop] = nexthop;
				uRIP_sortDatabase();
				routingTableCount++;
			}
			else
			{
				routingTable[route_id][Host] = route;
				routingTable[route_id][Metrics] = metrics;
				routingTable[route_id][NextHop] = nexthop;
			}
		}
	}
	else {
		return; // Такая метрика маршрута уже есть - маргрут игнорируем
	}
}
void uRIP_deleteRoute(uint8_t route) {
	uint8_t route_id = uRIP_lookuphost(route);
	if (route_id != 0xff) {
		routingTable[route_id][Host] = 0xff;
		routingTable[route_id][Metrics] = 0xff;
		routingTable[route_id][NextHop] = 0xff;
		routingTable[route_id][Timer] = 0;
		routingTableCount--;
		uRIP_sortDatabase();
	}
}
void uRIP_garbageCollector() {
	uint8_t count = routingTableCount;
	for (uint8_t route_id = 1; route_id < count; route_id++) {
		if (routingTable[route_id][Timer] > ROUTE_GARBAGE_TIMER) {
			__DEBUG(printf("%s%s%s <internal> [info] Delete route (timeout): 0x%02X %s\n", CYAN, c_printDate(), BLUE, (unsigned int) routingTable[route_id][Host], RESET) ;);
			routingTable[route_id][Host] = 0xff;
			routingTable[route_id][Metrics] = 0xff;
			routingTable[route_id][NextHop] = 0xff;
			routingTable[route_id][Timer] = 0;
			routingTableCount--;
			uRIP_sortDatabase();
			uRIP_sendRoutes(0x00);
			return;
		}
		else if (routingTable[route_id][Timer] > ROUTE_TIMEOUT_TIMER) {
			__DEBUG(printf("%s%s%s <internal> [info] Marked route to delete: 0x%02X %s\n", CYAN, c_printDate(), YELLOW, (unsigned int) routingTable[route_id][Host], RESET) ;);
			routingTable[route_id][Metrics] = 16;
			uRIP_sendRoutes(0x00);
		}
		routingTable[route_id][Timer]++;
	}
}

uint8_t uRIP_lookuphost(uint8_t host) {
	for (int i = 0; i < routingTableCount; i++) {
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
		routingTable[i][Timer] = 0xff;
	}
	routingTable[0][Host] = rx_addr;
	routingTable[0][Metrics] = 0;
	routingTable[0][NextHop] = 0;
	routingTable[0][Timer] = 0;
	routingTableCount = 1;
}
/**
 * Отправка таблицы маршрутизации uRIP указанному физическому хосту
 * @param host
 * @return
 */
void uRIP_sendRoutes(uint8_t host) {
	NRP_packet packet;
	packet.version = 1;
	packet.type = uRIP_update;
	packet.source = rx_addr;
	packet.destination = host;
	packet.ttl = 0;
	//packet._length = routingTableCount * 3
	uint8_t j;
	for (unsigned int i = 0; i < routingTableCount * 3; i++) {
		j = i % 27;
		// Converting 2d array to 1d
		packet.data[j] = routingTable[i / 3][i % 3];
		if ((j == 26) || (i == (routingTableCount * 3) - 1))
		{
			packet._length = j + 1;
			NRP_send_packet(host, packet);
		}
	}
}
