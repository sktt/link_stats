#include <sys/types.h>          /* See NOTES */
#include <net/if.h>
#include <poll.h>
#include <linux/nl80211.h>
#include <linux/if_link.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <string.h>
#include <err.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>

typedef unsigned long long u64;

#define MiB (1<<20)

struct {
    struct nlmsghdr hdr;
    struct if_stats_msg rt;
} req;

int main(int argc, char *argv[])
{
    struct rtattr *rta;
    if (argc < 2) {
        fprintf(stderr, "Usage: <ifname> [interval_ms=300]\n");
        exit(1);
    }

    char* ifname = argv[1];
    int interval_ms = 300;
    if (argc >= 3) {
        interval_ms = atoi(argv[2]);
    }
    u64 last_tx = 0;
    u64 last_rx = 0;

    int rtnetlink_sk = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    // why bother bind()?
    while (1) {
        memset(&req, 0, sizeof(req));
        req.hdr.nlmsg_len = sizeof(req);
        req.hdr.nlmsg_type = RTM_GETSTATS;
        req.hdr.nlmsg_flags = NLM_F_REQUEST;
        req.rt.ifindex = if_nametoindex(ifname);
        req.rt.filter_mask = IFLA_STATS_LINK_64;
        if (send(rtnetlink_sk, &req, req.hdr.nlmsg_len, 0) < 0) {
            err(1, "send");
        }

        struct {
            struct nlmsghdr nlh;
            union {
                struct {
                    struct nlmsgerr nlerr;
                    char __end_err[0];
                };
                struct {
                    struct rtmsg rth;
                    struct rtnl_link_stats64 stats;
                };
                char __end_stats[0];
            };
        } nlresp;
        if (recv(rtnetlink_sk, &nlresp, sizeof(nlresp), 0) <= 0) {
            err(2, "recv");
        }
        u64 tx_bytes = nlresp.stats.tx_bytes;
        u64 rx_bytes = nlresp.stats.rx_bytes;

        if (last_tx != 0 || last_rx != 0) {
            u64 dtx = tx_bytes - last_tx;
            u64 drx = rx_bytes - last_rx;
            double mul = 1000.0 / interval_ms;
            printf("ul|float|%0.2f\n", dtx*mul/MiB);
            printf("dl|float|%0.2f\n", drx*mul/MiB);
            printf("\n");
            fflush(stdout); // don't forget to flush :)
        }
        last_rx = nlresp.stats.rx_bytes;
        last_tx = nlresp.stats.tx_bytes;
        usleep(interval_ms * 1000);
    }
    close(rtnetlink_sk);

    return 0;
}
