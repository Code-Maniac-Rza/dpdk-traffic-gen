#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_lcore.h>
#include <rte_cycles.h>
#include <signal.h>
#include <stdio.h>

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32
#define MAX_PORTS 4
#define REPORT_INTERVAL 1000000 // 1 second in microseconds

static volatile int keep_running = 1;

struct perf_stats {
    uint64_t packets_sent;
    uint64_t bytes_sent;
    uint64_t latency_sum;
    uint64_t latency_count;
} stats[MAX_PORTS];

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

    if (!rte_eth_dev_is_valid_port(port)) {
        printf("Invalid Ethernet port: %u\n", port);
        return -1;
    }

    retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
    if (retval < 0) {
        printf("Failed to configure Ethernet port %u\n", port);
        return retval;
    }

    retval = rte_eth_rx_queue_setup(port, 0, 128, rte_eth_dev_socket_id(port), NULL, mbuf_pool);
    if (retval < 0) {
        printf("Failed to set up RX queue for port %u\n", port);
        return retval;
    }

    retval = rte_eth_tx_queue_setup(port, 0, 128, rte_eth_dev_socket_id(port), NULL);
    if (retval < 0) {
        printf("Failed to set up TX queue for port %u\n", port);
        return retval;
    }

    retval = rte_eth_dev_start(port);
    if (retval < 0) {
        printf("Failed to start Ethernet port %u\n", port);
        return retval;
    }

    rte_eth_promiscuous_enable(port);
    return 0;
}

static int traffic_gen(void *arg) {
    struct rte_mempool *mbuf_pool = (struct rte_mempool *)arg;
    uint16_t port_id = rte_lcore_id() % rte_eth_dev_count_avail(); 
    struct rte_mbuf *packets[BURST_SIZE];
    uint16_t nb_tx;
    uint64_t start_time, end_time;

    printf("Core %u generating traffic on port %u\n", rte_lcore_id(), port_id);

    while (keep_running) {
        start_time = rte_rdtsc();

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

            uint64_t *timestamp = (uint64_t *)data;
            *timestamp = start_time;
        }

        nb_tx = rte_eth_tx_burst(port_id, 0, packets, BURST_SIZE);

        for (int i = 0; i < nb_tx; i++) {
            stats[port_id].packets_sent++;
            stats[port_id].bytes_sent += packets[i]->data_len;

            end_time = rte_rdtsc();
            uint64_t *timestamp = (uint64_t *)rte_pktmbuf_mtod(packets[i], void *);
            uint64_t latency = end_time - *timestamp;

            stats[port_id].latency_sum += latency;
            stats[port_id].latency_count++;
        }

        for (int i = nb_tx; i < BURST_SIZE; i++) {
            rte_pktmbuf_free(packets[i]);
        }
    }

    return 0;
}

static int report_stats(void *arg) {
    while (keep_running) {
        rte_delay_us_block(REPORT_INTERVAL);

        printf("\nPerformance Report:\n");
        for (uint16_t port_id = 0; port_id < rte_eth_dev_count_avail(); port_id++) {
            double avg_latency = stats[port_id].latency_count
                                     ? ((double)stats[port_id].latency_sum / stats[port_id].latency_count) * 1e6 / rte_get_tsc_hz()
                                     : 0.0;

            printf("Port %u:\n", port_id);
            printf("  Packets Sent: %" PRIu64 "\n", stats[port_id].packets_sent);
            printf("  Bytes Sent: %" PRIu64 " B\n", stats[port_id].bytes_sent);
            printf("  Throughput: %.2f Mbps\n",
                   (stats[port_id].bytes_sent * 8.0) / (REPORT_INTERVAL / 1e6));
            printf("  Average Latency: %.2f us\n", avg_latency);
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    struct rte_mempool *mbuf_pool;
    uint16_t port_id;

    signal(SIGINT, handle_signal);

    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "EAL initialization failed\n");

    unsigned nb_ports = rte_eth_dev_count_avail();
    if (nb_ports == 0)
        rte_exit(EXIT_FAILURE, "No Ethernet ports available\n");

    if (nb_ports > MAX_PORTS)
        nb_ports = MAX_PORTS;

    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports,
                                        MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (!mbuf_pool)
        rte_exit(EXIT_FAILURE, "Failed to create mbuf pool\n");

    RTE_ETH_FOREACH_DEV(port_id) {
        if (port_id >= nb_ports)
            break;
        if (port_init(port_id, mbuf_pool) != 0)
            rte_exit(EXIT_FAILURE, "Failed to initialize port %" PRIu16 "\n", port_id);
    }

    unsigned lcore_id;
    RTE_LCORE_FOREACH_WORKER(lcore_id) {
        rte_eal_remote_launch(traffic_gen, mbuf_pool, lcore_id);
    }

    report_stats(NULL);

    rte_eal_mp_wait_lcore();

    printf("\nTraffic generation stopped. Final Report:\n");
    for (port_id = 0; port_id < nb_ports; port_id++) {
        printf("Port %u:\n", port_id);
        printf("  Total Packets Sent: %" PRIu64 "\n", stats[port_id].packets_sent);
        printf("  Total Bytes Sent: %" PRIu64 " B\n", stats[port_id].bytes_sent);
    }

    return 0;
}
