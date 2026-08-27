/* network.h - network stack detection
 *
 * Two independent facts, deliberately not conflated: whether a packet
 * driver is loaded right now (hardware/driver presence), and whatever
 * IP configuration mTCP's own config file claims (which could be
 * stale, or present even with no driver currently loaded).
 */

#ifndef DOSFETCH_NETWORK_H
#define DOSFETCH_NETWORK_H

#include <stddef.h>

/* Formats "vector 0xNN" if a packet driver's signature is found
 * anywhere in the reserved interrupt range (0x60-0x80), or
 * "not detected" if none is found. This only ever reads memory at
 * each candidate vector's target address to check for the standard
 * "PKT DRVR" signature -- it never calls/executes any vector, so
 * there's no risk from probing one that isn't actually a packet
 * driver.
 */
void get_packet_driver_info(char *buf, size_t buflen);

/* Formats "IP <addr>, GW <addr>" from mTCP's CONFIG.MTCP-style config
 * file (path from the MTCPCFG env var) into buf, or an explanatory
 * "not configured"/"UNKNOWN (...)" message if MTCPCFG isn't set, the
 * file can't be opened, or it has neither an IPADDR nor GATEWAY line
 * (e.g. a DHCP-only config).
 */
void get_network_ip_info(char *buf, size_t buflen);

#endif
