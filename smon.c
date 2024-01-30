#include <ifaddrs.h>
#include <linux/if.h>
#include <linux/if_link.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define INITIAL_STAT_CAPACITY 4
#define REFRESH_INTERVAL 1

typedef struct {
    char if_name[IFNAMSIZ];
    uint32_t rx_bytes;
    uint32_t tx_bytes;
} stat_t;

static struct ifaddrs *ifaddr;
static stat_t *prev_stats = NULL;
static size_t prev_stats_len = 0;

static inline void init_stats(void)
{
    if (getifaddrs(&ifaddr) == -1) {
        puts("[FATAL] Cannot get network interface information!");
        exit(1);
    }

    size_t capacity = INITIAL_STAT_CAPACITY;
    prev_stats = (stat_t *)calloc(capacity, sizeof(stat_t));

    for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (!(ifa->ifa_flags & IFF_UP) || ifa->ifa_addr->sa_family != AF_PACKET || ifa->ifa_data == NULL)
            continue;

        /* Check if capacity is enough */
        if (prev_stats_len == capacity) {
            capacity += INITIAL_STAT_CAPACITY;
            prev_stats = (stat_t *)realloc(prev_stats, sizeof(stat_t) * capacity);
        }

        struct rtnl_link_stats *rtnl_stat = ifa->ifa_data;
        strncpy(prev_stats[prev_stats_len].if_name, ifa->ifa_name, IFNAMSIZ - 1);
        prev_stats[prev_stats_len].rx_bytes = rtnl_stat->rx_bytes;
        prev_stats[prev_stats_len++].tx_bytes = rtnl_stat->tx_bytes;
    }

    if (prev_stats_len == 0) {
        puts("[WARN] No network interface to monitor!");
        free(prev_stats);
        freeifaddrs(ifaddr);
        exit(EXIT_SUCCESS);
    } else {
        prev_stats = (stat_t *)realloc(prev_stats, sizeof(stat_t) * prev_stats_len);
    }

    freeifaddrs(ifaddr);
}

static inline stat_t *find_prev_stat(char *if_name)
{
    size_t i = 0;
    while (i < prev_stats_len && strcmp(if_name, prev_stats[i].if_name) != 0)
        ++i;
    return i == prev_stats_len + 1 ? NULL : &prev_stats[i];
}

static inline char normalize(uint32_t value, double *normalized)
{
    if (value < 1048576) {
        *normalized = value / 1024.0;
        return 'K';
    }
    *normalized = value / 1048576.0;
    return 'M';
}

static inline void print_line_stat(char *name, uint32_t rx, uint32_t tx, uint32_t total)
{
    double normalized;
    char unit = normalize(rx, &normalized);
    printf("%15s %8.2f %cB/s ", name, normalized, unit);

    unit = normalize(tx, &normalized);
    printf("%8.2f %cB/s ", normalized, unit);

    unit = normalize(total, &normalized);
    printf("%8.2f %cB/s\n", normalized, unit);
}

static inline void show_stats(void)
{
    if (getifaddrs(&ifaddr) == -1) {
        puts("[FATAL] Cannot get network interface information!");
        exit(EXIT_FAILURE);
    }

    puts("smon v1.0.0, press CTRL+C to exit");
    printf("%15s %13s %13s %13s\n", "Name", "RX", "TX", "Total");
    puts("=========================================================");

    uint32_t total_rx_new_bytes = 0, total_tx_new_bytes = 0, grand_total_new_bytes = 0;
    for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (!(ifa->ifa_flags & IFF_UP) || ifa->ifa_addr->sa_family != AF_PACKET || ifa->ifa_data == NULL)
            continue;

        stat_t *prev_stat = find_prev_stat(ifa->ifa_name);
        if (prev_stat == NULL)
            continue;

        struct rtnl_link_stats *rtnl_stat = ifa->ifa_data;
        uint32_t rx_new_bytes = rtnl_stat->rx_bytes - prev_stat->rx_bytes;
        uint32_t tx_new_bytes = rtnl_stat->tx_bytes - prev_stat->tx_bytes;
        uint32_t total_new_bytes = rx_new_bytes + tx_new_bytes;

        total_rx_new_bytes += rx_new_bytes;
        total_tx_new_bytes += tx_new_bytes;
        grand_total_new_bytes += total_new_bytes;

        prev_stat->rx_bytes = rtnl_stat->rx_bytes;
        prev_stat->tx_bytes = rtnl_stat->tx_bytes;

        print_line_stat(ifa->ifa_name, rx_new_bytes, tx_new_bytes, total_new_bytes);
    }

    puts("---------------------------------------------------------");
    print_line_stat("Total", total_rx_new_bytes, total_tx_new_bytes, grand_total_new_bytes);

    freeifaddrs(ifaddr);
}

int main(void) {
    init_stats();
    while (1) {
        printf("\e[1;1H\e[2J");
        show_stats();
        sleep(REFRESH_INTERVAL);
    }
    free(prev_stats);
    return EXIT_SUCCESS;
}
