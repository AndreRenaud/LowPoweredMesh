#include <sys/select.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>

#include "mesh.h"
#include "parser.h"

#define BASE_PORT 4000

#define mseconds() (int)({ struct timespec _ts; \
		clock_gettime(CLOCK_MONOTONIC, &_ts); \
		_ts.tv_sec * 1000 + _ts.tv_nsec / (1000 * 1000); \
		})
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))


static int npeers = 0;
static int peers[100] = {0};
static struct addrinfo *peers_info[100];
static int listener = -1;
static int node_index = -1;

static void usage(const char *prog)
{
	fprintf(stderr, "%s [-i index] [-p peer] [-v]\n", prog);
	exit(EXIT_FAILURE);
}

static int create_listener(int port)
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char port_str[20];

	sprintf(port_str, "%d", port);


	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, port_str, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
						p->ai_protocol)) == -1) {
			perror("listener: socket");
			continue;
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("listener: bind");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "listener: failed to bind socket\n");
		return 2;
	}

	freeaddrinfo(servinfo);

	return sockfd;
}

static int create_talker(const char *hostname, int port, struct addrinfo **peer)
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char port_str[20];

	sprintf(port_str, "%d", port);

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(hostname, port_str, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and make a socket
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
						p->ai_protocol)) == -1) {
			perror("talker: socket");
			continue;
		}

		break;
	}

	*peer = p;

	if (p == NULL) {
		fprintf(stderr, "talker: failed to bind socket\n");
		return -1;
	}

	return sockfd;
}

static int mesh_stub_init(void *data, int len)
{
	return 1;
}

static int mesh_radio_send(void *data, int len)
{
	int i;
	for (i = 0; i < npeers; i++) {
		//printf("Sending %d to %d\n", len, i);
		sendto(peers[i], data, len, 0, peers_info[i]->ai_addr,
				peers_info[i]->ai_addrlen);
	}
	return 0;
}

static int mesh_radio_recv(void *data, int len)
{
	struct sockaddr src_addr;
	socklen_t addrlen = sizeof(src_addr);
	int numbytes = recvfrom(listener, data, len, MSG_DONTWAIT,
			&src_addr, &addrlen);
	//if (numbytes > 0)
		//printf("recv %d\n", len);
	return numbytes > 0;
}

static int mesh_stub_app_recv(void *data, int len)
{
	mesh_packet_t packet;
	memcpy(&packet, data, len);
	printf("Got pkt type %d\n", packet.info.pkt_type);
	switch (packet.info.pkt_type) {
	case mesh_pkt_ack_app:
	case mesh_pkt_nack:
		printf("App recv: me=%d len=%d hops=%d/%d src=%d dst=%d\n",
				node_index, len, packet.info.hop_count, packet.info.hop_count_max, packet.nwk.src, packet.nwk.dst);
		printf("Data: %s\n", packet.data);
		printf("ack reqd=%d\n", mesh_is_ack_required(&packet));
		if (mesh_is_ack_required(&packet))
			mesh_send_ack("ACK", 4, &packet);
		break;
	case mesh_pkt_ack:
		printf("Got ack\n");
		break;

	}
	return 1;
}

static int mesh_stub_get_timer(void *data, int len)
{
	uint32_t *timer = data;
	if (len == sizeof(uint32_t)) {
		*timer = mseconds();
		return 1;
	}
	return 0;
}

static int cmd_send(int argc, char *argv[])
{
	int node;
	int max_hops = 10;
	int len;
	char *data = argv[2];

	if (argc < 3) {
		fprintf(stderr, "Usage: send node_id string\n");
		return -1;
	}

	node = atoi(argv[1]);
	len = strlen(data) + 1;
	len = min(len, MESH_DATA_PAYLOAD_SIZE);
	data[len - 1] = '\0';

	mesh_send(node, mesh_pkt_ack_app, data, len, max_hops);
	printf("Sent '%s' to %d\n", data, node);

	return 0;
}

static int cmd_stats(int argc, char *argv[])
{
	mesh_stats_t stats = mesh_get_stats();
	printf("Stats\n");
	printf("Sent: %d\n", stats.pkts_sent);
	printf("Intercepted: %d\n", stats.pkts_intercepted);
	printf("Repeated: %d\n", stats.pkts_repeated);
	printf("Retried: %d\n", stats.pkts_retried);
	printf("Others: %d\n", stats.pkts_retried_others);
	printf("Entries: %d\n", stats.rte_entries);
	printf("Overwritten: %d\n", stats.rte_overwritten);
	printf("Error: 0x%x\n", mesh_get_error_mask());
	printf("Get pnd pkt count: %d\n", mesh_get_pnd_pkt_count());
	return 0;
}

static int cmd_routes(int argc, char *argv[])
{
	const mesh_rte_table_t *route;
	int i = 0;

	printf("Routes: %d\n", mesh_get_num_routing_entries());
	do {
		route = mesh_get_routing_entry(i);
		if (!route)
			break;
		printf("%d: dst=%d next=%d num=%d score=%d\n", i,
				route->dst, route->next_hop,
				route->num_hops, route->score);

		i++;
	} while (1);

	return 0;
}

static struct parser_function parser_functions[] = {
	{"send", cmd_send},
	{"stats", cmd_stats},
	{"routes", cmd_routes},
};

int main(int argc, char *argv[])
{
	int opt;
	mesh_driver_t driver;

	printf("mesh prog\n");

	while ((opt = getopt(argc, argv, "i:p:v")) != -1) {
		switch (opt) {
		case 'i': node_index = atoi(optarg); break;
		case 'p':
			peers[npeers] = create_talker("localhost",
					BASE_PORT + atoi(optarg), &peers_info[npeers]);
			npeers++;
			break;
			;
		default: usage(argv[0]); break;
		}
	}
	if (node_index == -1)
		usage(argv[0]);

	listener = create_listener(BASE_PORT + node_index);
	if (listener < 0)
		return EXIT_FAILURE;

	driver.radio_init = mesh_stub_init;
	driver.radio_recv = mesh_radio_recv;
	driver.radio_send = mesh_radio_send;
	driver.app_recv = mesh_stub_app_recv;
	driver.get_timer = mesh_stub_get_timer;

	if (!mesh_init(node_index, 1, "name", driver, false)) {
		fprintf(stderr, "mesh_init\n");
		return -1;
	}

	while (1) {
		fd_set fds;
		struct timeval timeout;

		timeout.tv_sec = 0;
		timeout.tv_usec = 1000;

		mesh_service();

		/* Check stdin for in-coming commands */
		FD_ZERO(&fds);
		FD_SET(fileno(stdin), &fds);
		if (select(fileno(stdin) + 1, &fds, NULL, NULL, &timeout) > 0) {
			char line[1024];
			if (fgets(line, sizeof(line), stdin))
				parse_line(line, parser_functions,
						ARRAY_SIZE(parser_functions));
		}
	}

	extern void mesh_test(void);
	mesh_test();
	return 0;
}
