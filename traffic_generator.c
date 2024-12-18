#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_lcore.h>
#include <signal.h>
#include <rte_cycles.h>
#include <stdio.h>

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32
#define MAX_PORTS 4

static volatile int keep_running = 1;
static uint64_t total_packets_sent[MAX_PORTS] = {0};

#define CHECK_ERROR(condition, message) \
    if (condition) { \
        rte_exit(EXIT_FAILURE, message "\n"); \
    }

void handle_signal(int signal) {
    if (signal == SIGINT) {
        printf("\nReceived SIGINT, stopping traffic generation...\n");
        keep_running = 0;
    }
}

static int port_init(uint16_t port, struct rte_mempool *mbuf_pool) {
    struct rte_eth_conf port_conf = {0};
    const uint16_t rx_rings = 1, tx_rings = 1;
    int retval;

    CHECK_ERROR(!rte_eth_dev_is_valid_port(port), "Invalid Ethernet port");

    retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
    CHECK_ERROR(retval != 0, "Failed to configure the Ethernet port");

    retval = rte_eth_rx_queue_setup(port, 0, 128, rte_eth_dev_socket_id(port), NULL, mbuf_pool);
    CHECK_ERROR(retval < 0, "Failed to set up RX queue");

    retval = rte_eth_tx_queue_setup(port, 0, 128, rte_eth_dev_socket_id(port), NULL);
    CHECK_ERROR(retval < 0, "Failed to set up TX queue");

    retval = rte_eth_dev_start(port);
    CHECK_ERROR(retval < 0, "Failed to start the Ethernet port");

    rte_eth_promiscuous_enable(port);

    return 0;
}

static int traffic_gen(void *arg) {
    struct rte_mempool *mbuf_pool = (struct rte_mempool *)arg;
    uint16_t port_id = rte_lcore_id() % rte_eth_dev_count_avail(); 
    struct rte_mbuf *packets[BURST_SIZE];
    uint16_t nb_tx;

    printf("Core %u is generating traffic on port %u\n", rte_lcore_id(), port_id);

    while (keep_running) {

        for (int i = 0; i < BURST_SIZE; i++) {
            packets[i] = rte_pktmbuf_alloc(mbuf_pool);
            if (!packets[i]) {
                printf("Failed to allocate mbuf\n");
                break;
            }
            char *data = rte_pktmbuf_append(packets[i], 64);
            if (!data) {
                rte_pktmbuf_free(packets[i]);
                printf("Failed to append data to mbuf\n");
                break;
            }
            for (int j = 0; j < 64; j++) {
                data[j] = (j % 2 == 0) ? 0xAA : 0x55; 
            }
        }
        nb_tx = rte_eth_tx_burst(port_id, 0, packets, BURST_SIZE);

        total_packets_sent[port_id] += nb_tx;
        for (int i = nb_tx; i < BURST_SIZE; i++) {
            rte_pktmbuf_free(packets[i]);
        }
    }

    return 0;
}

int main(int argc, char *argv[]) {
    struct rte_mempool *mbuf_pool;
    uint16_t port_id;

    signal(SIGINT, handle_signal);
    int ret = rte_eal_init(argc, argv);
    CHECK_ERROR(ret < 0, "EAL initialization failed");


    unsigned nb_ports = rte_eth_dev_count_avail();
    CHECK_ERROR(nb_ports == 0, "No Ethernet ports available");

    if (nb_ports > MAX_PORTS) {
        nb_ports = MAX_PORTS;
        printf("Limiting to %d ports\n", MAX_PORTS);
    }

    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports,
                                        MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    CHECK_ERROR(!mbuf_pool, "Failed to create mbuf pool");

    RTE_ETH_FOREACH_DEV(port_id) {
        if (port_id >= nb_ports) break; 
        if (port_init(port_id, mbuf_pool) != 0) {
            rte_exit(EXIT_FAILURE, "Failed to initialize port %" PRIu16 "\n", port_id);
        }
    }

    unsigned lcore_id;
    RTE_LCORE_FOREACH_WORKER(lcore_id) {
        rte_eal_remote_launch(traffic_gen, mbuf_pool, lcore_id);
    }

    printf("Traffic generation started. Press Ctrl+C to stop.\n");
    rte_eal_mp_wait_lcore();

    printf("\nTraffic generation stopped. Summary:\n");
    for (port_id = 0; port_id < nb_ports; port_id++) {
        printf("Port %u: Sent %" PRIu64 " packets\n", port_id, total_packets_sent[port_id]);
    }

    return 0;
}
