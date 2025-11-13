// Tick Network Checksum Functions
// Implementation of network protocol checksums

#include "tick_net.h"

// Internet checksum algorithm (RFC 1071)
// Used for IP, ICMP, ICMPv6, UDP, TCP checksums
// Returns checksum in network byte order
u16 tick_inet_checksum(const void *data, usz len) {
  const u8 *buf = (const u8 *)data;
  u32 sum = 0;

  // Sum all 16-bit words
  while (len > 1) {
    sum += (u16)((buf[0] << 8) | buf[1]);
    buf += 2;
    len -= 2;
  }

  // Add odd byte if present
  if (len > 0) {
    sum += (u16)(buf[0] << 8);
  }

  // Fold 32-bit sum to 16 bits
  while (sum >> 16) {
    sum = (sum & 0xFFFF) + (sum >> 16);
  }

  // Return one's complement in network byte order
  return (u16)~sum;
}

// IPv4 header checksum
// Calculates checksum for IPv4 header (assumes checksum field is zero)
u16 tick_ipv4_checksum(const tick_ipv4_hdr_t *hdr) {
  return tick_inet_checksum(hdr, (usz)(hdr->ihl * 4));
}

// UDP checksum for IPv4
// Parameters:
//   udp_hdr: UDP header (checksum field should be zero)
//   payload: UDP payload data
//   payload_len: Length of payload in bytes
//   src_ip: Source IPv4 address
//   dst_ip: Destination IPv4 address
u16 tick_udp_checksum_ipv4(const tick_udp_hdr_t *udp_hdr, const void *payload,
                            usz payload_len, tick_ipv4_addr_t src_ip,
                            tick_ipv4_addr_t dst_ip) {
  // Create pseudo-header
  tick_ipv4_pseudo_hdr_t pseudo = {.src = src_ip,
                                    .dst = dst_ip,
                                    .zero = 0,
                                    .protocol = TICK_IPPROTO_UDP,
                                    .length = udp_hdr->len};

  // Calculate checksum over pseudo-header + UDP header + payload
  u32 sum = 0;
  const u8 *buf;
  usz len;

  // Sum pseudo-header
  buf = (const u8 *)&pseudo;
  len = sizeof(pseudo);
  while (len > 1) {
    sum += (u16)((buf[0] << 8) | buf[1]);
    buf += 2;
    len -= 2;
  }

  // Sum UDP header
  buf = (const u8 *)udp_hdr;
  len = sizeof(tick_udp_hdr_t);
  while (len > 1) {
    sum += (u16)((buf[0] << 8) | buf[1]);
    buf += 2;
    len -= 2;
  }

  // Sum payload
  buf = (const u8 *)payload;
  len = payload_len;
  while (len > 1) {
    sum += (u16)((buf[0] << 8) | buf[1]);
    buf += 2;
    len -= 2;
  }
  if (len > 0) {
    sum += (u16)(buf[0] << 8);
  }

  // Fold and return one's complement
  while (sum >> 16) {
    sum = (sum & 0xFFFF) + (sum >> 16);
  }
  return (u16)~sum;
}

// UDP checksum for IPv6
// Parameters:
//   udp_hdr: UDP header (checksum field should be zero)
//   payload: UDP payload data
//   payload_len: Length of payload in bytes
//   src_ip: Source IPv6 address
//   dst_ip: Destination IPv6 address
u16 tick_udp_checksum_ipv6(const tick_udp_hdr_t *udp_hdr, const void *payload,
                            usz payload_len, tick_ipv6_addr_t src_ip,
                            tick_ipv6_addr_t dst_ip) {
  // Create pseudo-header
  tick_ipv6_pseudo_hdr_t pseudo = {.src = src_ip,
                                    .dst = dst_ip,
                                    .length = udp_hdr->len,
                                    .zero = {0, 0, 0},
                                    .next_hdr = TICK_IPPROTO_UDP};

  // Calculate checksum over pseudo-header + UDP header + payload
  u32 sum = 0;
  const u8 *buf;
  usz len;

  // Sum pseudo-header
  buf = (const u8 *)&pseudo;
  len = sizeof(pseudo);
  while (len > 1) {
    sum += (u16)((buf[0] << 8) | buf[1]);
    buf += 2;
    len -= 2;
  }

  // Sum UDP header
  buf = (const u8 *)udp_hdr;
  len = sizeof(tick_udp_hdr_t);
  while (len > 1) {
    sum += (u16)((buf[0] << 8) | buf[1]);
    buf += 2;
    len -= 2;
  }

  // Sum payload
  buf = (const u8 *)payload;
  len = payload_len;
  while (len > 1) {
    sum += (u16)((buf[0] << 8) | buf[1]);
    buf += 2;
    len -= 2;
  }
  if (len > 0) {
    sum += (u16)(buf[0] << 8);
  }

  // Fold and return one's complement
  while (sum >> 16) {
    sum = (sum & 0xFFFF) + (sum >> 16);
  }
  return (u16)~sum;
}

// ICMP checksum
// Calculates checksum for ICMP message (header + payload)
// icmp_hdr: ICMP header (checksum field should be zero)
// payload: ICMP payload data
// payload_len: Length of payload in bytes
u16 tick_icmp_checksum(const tick_icmp_hdr_t *icmp_hdr, const void *payload,
                       usz payload_len) {
  u32 sum = 0;
  const u8 *buf;
  usz len;

  // Sum ICMP header
  buf = (const u8 *)icmp_hdr;
  len = sizeof(tick_icmp_hdr_t);
  while (len > 1) {
    sum += (u16)((buf[0] << 8) | buf[1]);
    buf += 2;
    len -= 2;
  }

  // Sum payload
  buf = (const u8 *)payload;
  len = payload_len;
  while (len > 1) {
    sum += (u16)((buf[0] << 8) | buf[1]);
    buf += 2;
    len -= 2;
  }
  if (len > 0) {
    sum += (u16)(buf[0] << 8);
  }

  // Fold and return one's complement
  while (sum >> 16) {
    sum = (sum & 0xFFFF) + (sum >> 16);
  }
  return (u16)~sum;
}

// ICMPv6 checksum
// Calculates checksum for ICMPv6 message with IPv6 pseudo-header
// icmpv6_hdr: ICMPv6 header (checksum field should be zero)
// payload: ICMPv6 payload data
// payload_len: Length of payload in bytes
// src_ip: Source IPv6 address
// dst_ip: Destination IPv6 address
u16 tick_icmpv6_checksum(const tick_icmpv6_hdr_t *icmpv6_hdr,
                         const void *payload, usz payload_len,
                         tick_ipv6_addr_t src_ip, tick_ipv6_addr_t dst_ip) {
  // Calculate total ICMPv6 message length
  u32 icmpv6_len = sizeof(tick_icmpv6_hdr_t) + payload_len;

  // Create pseudo-header
  tick_ipv6_pseudo_hdr_t pseudo = {.src = src_ip,
                                    .dst = dst_ip,
                                    .length = tick_htonl(icmpv6_len),
                                    .zero = {0, 0, 0},
                                    .next_hdr = TICK_IPPROTO_ICMPV6};

  // Calculate checksum over pseudo-header + ICMPv6 header + payload
  u32 sum = 0;
  const u8 *buf;
  usz len;

  // Sum pseudo-header
  buf = (const u8 *)&pseudo;
  len = sizeof(pseudo);
  while (len > 1) {
    sum += (u16)((buf[0] << 8) | buf[1]);
    buf += 2;
    len -= 2;
  }

  // Sum ICMPv6 header
  buf = (const u8 *)icmpv6_hdr;
  len = sizeof(tick_icmpv6_hdr_t);
  while (len > 1) {
    sum += (u16)((buf[0] << 8) | buf[1]);
    buf += 2;
    len -= 2;
  }

  // Sum payload
  buf = (const u8 *)payload;
  len = payload_len;
  while (len > 1) {
    sum += (u16)((buf[0] << 8) | buf[1]);
    buf += 2;
    len -= 2;
  }
  if (len > 0) {
    sum += (u16)(buf[0] << 8);
  }

  // Fold and return one's complement
  while (sum >> 16) {
    sum = (sum & 0xFFFF) + (sum >> 16);
  }
  return (u16)~sum;
}
