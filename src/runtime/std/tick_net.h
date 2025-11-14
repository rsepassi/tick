#ifndef TICK_NET_H
#define TICK_NET_H

// Tick Network Types and Utilities
// Freestanding header for network protocol structures and endianness conversion
// Only depends on freestanding C headers

#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// ============================================================================
// Type Aliases (matching tick_runtime.h conventions)
// ============================================================================

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef size_t usz;
typedef ptrdiff_t isz;

// ============================================================================
// Endianness Detection and Conversion
// ============================================================================

// Detect byte order at compile time
// Allow manual override by defining TICK_LITTLE_ENDIAN or TICK_BIG_ENDIAN
#if !defined(TICK_LITTLE_ENDIAN) && !defined(TICK_BIG_ENDIAN)
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
    defined(__ORDER_BIG_ENDIAN__)
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define TICK_LITTLE_ENDIAN 1
#define TICK_BIG_ENDIAN 0
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define TICK_LITTLE_ENDIAN 0
#define TICK_BIG_ENDIAN 1
#else
#error "Unknown byte order"
#endif
#else
#error \
    "Cannot detect byte order - define TICK_LITTLE_ENDIAN or TICK_BIG_ENDIAN manually"
#endif
#endif

// Byte swap primitives using compiler built-ins
static inline u16 tick_bswap16(u16 x) {
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_bswap16(x);
#else
  return (u16)((x << 8) | (x >> 8));
#endif
}

static inline u32 tick_bswap32(u32 x) {
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_bswap32(x);
#else
  return ((x << 24) & 0xff000000) | ((x << 8) & 0x00ff0000) |
         ((x >> 8) & 0x0000ff00) | ((x >> 24) & 0x000000ff);
#endif
}

static inline u64 tick_bswap64(u64 x) {
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_bswap64(x);
#else
  return ((x << 56) & 0xff00000000000000ULL) |
         ((x << 40) & 0x00ff000000000000ULL) |
         ((x << 24) & 0x0000ff0000000000ULL) |
         ((x << 8) & 0x000000ff00000000ULL) |
         ((x >> 8) & 0x00000000ff000000ULL) |
         ((x >> 24) & 0x0000000000ff0000ULL) |
         ((x >> 40) & 0x000000000000ff00ULL) |
         ((x >> 56) & 0x00000000000000ffULL);
#endif
}

// Endianness conversion functions
#if TICK_LITTLE_ENDIAN

// Host to little-endian (no-op on little-endian)
static inline u16 tick_htole16(u16 x) { return x; }
static inline u32 tick_htole32(u32 x) { return x; }
static inline u64 tick_htole64(u64 x) { return x; }

// Host to big-endian (swap on little-endian)
static inline u16 tick_htobe16(u16 x) { return tick_bswap16(x); }
static inline u32 tick_htobe32(u32 x) { return tick_bswap32(x); }
static inline u64 tick_htobe64(u64 x) { return tick_bswap64(x); }

// Little-endian to host (no-op on little-endian)
static inline u16 tick_letoh16(u16 x) { return x; }
static inline u32 tick_letoh32(u32 x) { return x; }
static inline u64 tick_letoh64(u64 x) { return x; }

// Big-endian to host (swap on little-endian)
static inline u16 tick_betoh16(u16 x) { return tick_bswap16(x); }
static inline u32 tick_betoh32(u32 x) { return tick_bswap32(x); }
static inline u64 tick_betoh64(u64 x) { return tick_bswap64(x); }

#else  // TICK_BIG_ENDIAN

// Host to little-endian (swap on big-endian)
static inline u16 tick_htole16(u16 x) { return tick_bswap16(x); }
static inline u32 tick_htole32(u32 x) { return tick_bswap32(x); }
static inline u64 tick_htole64(u64 x) { return tick_bswap64(x); }

// Host to big-endian (no-op on big-endian)
static inline u16 tick_htobe16(u16 x) { return x; }
static inline u32 tick_htobe32(u32 x) { return x; }
static inline u64 tick_htobe64(u64 x) { return x; }

// Little-endian to host (swap on big-endian)
static inline u16 tick_letoh16(u16 x) { return tick_bswap16(x); }
static inline u32 tick_letoh32(u32 x) { return tick_bswap32(x); }
static inline u64 tick_letoh64(u64 x) { return tick_bswap64(x); }

// Big-endian to host (no-op on big-endian)
static inline u16 tick_betoh16(u16 x) { return x; }
static inline u32 tick_betoh32(u32 x) { return x; }
static inline u64 tick_betoh64(u64 x) { return x; }

#endif

// Network byte order aliases (network = big-endian)
#define tick_htons(x) tick_htobe16(x)
#define tick_htonl(x) tick_htobe32(x)
#define tick_ntohs(x) tick_betoh16(x)
#define tick_ntohl(x) tick_betoh32(x)

// ============================================================================
// Protocol Constants
// ============================================================================

// Ethernet II Ethertypes (network byte order values)
#define TICK_ETHERTYPE_IPV4 0x0800
#define TICK_ETHERTYPE_ARP 0x0806
#define TICK_ETHERTYPE_IPV6 0x86DD
#define TICK_ETHERTYPE_VLAN 0x8100

// IP Protocol Numbers
#define TICK_IPPROTO_ICMP 1
#define TICK_IPPROTO_TCP 6
#define TICK_IPPROTO_UDP 17
#define TICK_IPPROTO_ICMPV6 58

// ARP Operation Codes
#define TICK_ARP_OP_REQUEST 1
#define TICK_ARP_OP_REPLY 2

// ARP Hardware Types
#define TICK_ARP_HW_ETHERNET 1

// ICMP Types
#define TICK_ICMP_ECHO_REPLY 0
#define TICK_ICMP_DEST_UNREACH 3
#define TICK_ICMP_SOURCE_QUENCH 4
#define TICK_ICMP_REDIRECT 5
#define TICK_ICMP_ECHO_REQUEST 8
#define TICK_ICMP_TIME_EXCEEDED 11
#define TICK_ICMP_PARAM_PROBLEM 12
#define TICK_ICMP_TIMESTAMP 13
#define TICK_ICMP_TIMESTAMP_REPLY 14

// ICMPv6 Types
#define TICK_ICMPV6_ECHO_REQUEST 128
#define TICK_ICMPV6_ECHO_REPLY 129
#define TICK_ICMPV6_ROUTER_SOLICIT 133
#define TICK_ICMPV6_ROUTER_ADVERT 134
#define TICK_ICMPV6_NEIGHBOR_SOLICIT 135
#define TICK_ICMPV6_NEIGHBOR_ADVERT 136

// ICMPv6 NDP Option Types (RFC 4861)
#define TICK_ICMPV6_OPT_SRC_LINK_ADDR 1
#define TICK_ICMPV6_OPT_TGT_LINK_ADDR 2
#define TICK_ICMPV6_OPT_PREFIX_INFO 3
#define TICK_ICMPV6_OPT_REDIRECTED_HDR 4
#define TICK_ICMPV6_OPT_MTU 5

// ICMPv6 Neighbor Advertisement Flags
#define TICK_ICMPV6_ND_ROUTER 0x80000000     // Router flag
#define TICK_ICMPV6_ND_SOLICITED 0x40000000  // Solicited flag
#define TICK_ICMPV6_ND_OVERRIDE 0x20000000   // Override flag

// DNS Resource Record Types
#define TICK_DNS_TYPE_A 1      // IPv4 address
#define TICK_DNS_TYPE_NS 2     // Name server
#define TICK_DNS_TYPE_CNAME 5  // Canonical name
#define TICK_DNS_TYPE_SOA 6    // Start of authority
#define TICK_DNS_TYPE_PTR 12   // Pointer
#define TICK_DNS_TYPE_MX 15    // Mail exchange
#define TICK_DNS_TYPE_TXT 16   // Text
#define TICK_DNS_TYPE_AAAA 28  // IPv6 address

// DNS Classes
#define TICK_DNS_CLASS_IN 1  // Internet

// DNS Header Flags
#define TICK_DNS_FLAG_QR 0x8000       // Query/Response
#define TICK_DNS_FLAG_AA 0x0400       // Authoritative Answer
#define TICK_DNS_FLAG_TC 0x0200       // Truncated
#define TICK_DNS_FLAG_RD 0x0100       // Recursion Desired
#define TICK_DNS_FLAG_RA 0x0080       // Recursion Available
#define TICK_DNS_OPCODE_QUERY 0x0000  // Standard query

// Well-known Ports
#define TICK_PORT_DNS 53
#define TICK_PORT_HTTP 80
#define TICK_PORT_HTTPS 443
#define TICK_PORT_NTP 123

// NTP Constants
#define TICK_NTP_VERSION 4
#define TICK_NTP_MODE_CLIENT 3
#define TICK_NTP_MODE_SERVER 4

// ============================================================================
// MAC Address and Ethernet
// ============================================================================

// MAC address (6 bytes)
typedef struct {
  u8 addr[6];
} __attribute__((packed)) tick_mac_addr_t;

// Ethernet II frame header
typedef struct {
  tick_mac_addr_t dst;  // Destination MAC address
  tick_mac_addr_t src;  // Source MAC address
  u16 ethertype;        // Ethertype (network byte order)
} __attribute__((packed)) tick_eth_hdr_t;

// ============================================================================
// IPv4
// ============================================================================

// IPv4 address (4 bytes)
typedef struct {
  u8 addr[4];
} __attribute__((packed)) tick_ipv4_addr_t;

// IPv4 header (RFC 791)
typedef struct {
#if TICK_LITTLE_ENDIAN
  u8 ihl : 4;      // Internet Header Length (4-bit)
  u8 version : 4;  // Version (4-bit)
#else
  u8 version : 4;  // Version (4-bit)
  u8 ihl : 4;      // Internet Header Length (4-bit)
#endif
  u8 tos;                // Type of Service
  u16 total_len;         // Total Length (network byte order)
  u16 id;                // Identification (network byte order)
  u16 frag_off;          // Fragment Offset (network byte order)
  u8 ttl;                // Time to Live
  u8 protocol;           // Protocol
  u16 check;             // Header Checksum (network byte order)
  tick_ipv4_addr_t src;  // Source Address
  tick_ipv4_addr_t dst;  // Destination Address
} __attribute__((packed)) tick_ipv4_hdr_t;

// IPv4 socket address
typedef struct {
  u16 family;             // Address family (AF_INET = 2)
  u16 port;               // Port number (network byte order)
  tick_ipv4_addr_t addr;  // IPv4 address
  u8 zero[8];             // Padding to match sockaddr size
} __attribute__((packed)) tick_sockaddr_in_t;

// ============================================================================
// IPv6
// ============================================================================

// IPv6 address (16 bytes)
typedef struct {
  u8 addr[16];
} __attribute__((packed)) tick_ipv6_addr_t;

// IPv6 header (RFC 2460)
typedef struct {
#if TICK_LITTLE_ENDIAN
  u32 flow_label : 20;    // Flow Label (20-bit)
  u32 traffic_class : 8;  // Traffic Class (8-bit)
  u32 version : 4;        // Version (4-bit)
#else
  u32 version : 4;        // Version (4-bit)
  u32 traffic_class : 8;  // Traffic Class (8-bit)
  u32 flow_label : 20;    // Flow Label (20-bit)
#endif
  u16 payload_len;       // Payload Length (network byte order)
  u8 next_hdr;           // Next Header (protocol)
  u8 hop_limit;          // Hop Limit
  tick_ipv6_addr_t src;  // Source Address
  tick_ipv6_addr_t dst;  // Destination Address
} __attribute__((packed)) tick_ipv6_hdr_t;

// IPv6 socket address
typedef struct {
  u16 family;             // Address family (AF_INET6 = 10)
  u16 port;               // Port number (network byte order)
  u32 flowinfo;           // Flow information (network byte order)
  tick_ipv6_addr_t addr;  // IPv6 address
  u32 scope_id;           // Scope ID
} __attribute__((packed)) tick_sockaddr_in6_t;

// ============================================================================
// ARP (Address Resolution Protocol)
// ============================================================================

// ARP packet (RFC 826)
typedef struct {
  u16 hw_type;                    // Hardware type (network byte order)
  u16 proto_type;                 // Protocol type (network byte order)
  u8 hw_addr_len;                 // Hardware address length
  u8 proto_addr_len;              // Protocol address length
  u16 op;                         // Operation code (network byte order)
  tick_mac_addr_t sender_hw;      // Sender hardware address
  tick_ipv4_addr_t sender_proto;  // Sender protocol address
  tick_mac_addr_t target_hw;      // Target hardware address
  tick_ipv4_addr_t target_proto;  // Target protocol address
} __attribute__((packed)) tick_arp_pkt_t;

// ============================================================================
// ICMP (Internet Control Message Protocol)
// ============================================================================

// ICMPv4 header (RFC 792)
typedef struct {
  u8 type;       // ICMP type
  u8 code;       // ICMP code
  u16 checksum;  // Checksum (network byte order)
  union {
    struct {
      u16 id;   // Identifier (network byte order)
      u16 seq;  // Sequence number (network byte order)
    } echo;
    u32 gateway;  // Gateway address (for redirect)
    struct {
      u16 unused;
      u16 mtu;  // Next-hop MTU (network byte order)
    } frag;
  } data;
} __attribute__((packed)) tick_icmp_hdr_t;

// ICMPv6 header (RFC 4443)
typedef struct {
  u8 type;       // ICMPv6 type
  u8 code;       // ICMPv6 code
  u16 checksum;  // Checksum (network byte order)
  union {
    struct {
      u16 id;   // Identifier (network byte order)
      u16 seq;  // Sequence number (network byte order)
    } echo;
    u32 mtu;      // MTU (network byte order)
    u32 pointer;  // Pointer (network byte order)
  } data;
} __attribute__((packed)) tick_icmpv6_hdr_t;

// ============================================================================
// ICMPv6 Neighbor Discovery Protocol (NDP)
// ============================================================================

// ICMPv6 NDP Option (RFC 4861)
typedef struct {
  u8 type;    // Option type
  u8 length;  // Length in units of 8 octets
  // Followed by option-specific data
} __attribute__((packed)) tick_icmpv6_ndp_opt_t;

// ICMPv6 NDP Link-Layer Address Option
typedef struct {
  u8 type;               // Option type (1=source, 2=target)
  u8 length;             // Length (1 for 6-byte MAC)
  tick_mac_addr_t addr;  // Link-layer address (MAC)
} __attribute__((packed)) tick_icmpv6_ndp_ll_addr_opt_t;

// ICMPv6 Neighbor Solicitation (RFC 4861)
typedef struct {
  u8 type;                  // Type (135)
  u8 code;                  // Code (0)
  u16 checksum;             // Checksum (network byte order)
  u32 reserved;             // Reserved (must be zero)
  tick_ipv6_addr_t target;  // Target address
  // Followed by options (e.g., source link-layer address)
} __attribute__((packed)) tick_icmpv6_neighbor_solicit_t;

// ICMPv6 Neighbor Advertisement (RFC 4861)
typedef struct {
  u8 type;                  // Type (136)
  u8 code;                  // Code (0)
  u16 checksum;             // Checksum (network byte order)
  u32 flags;                // Flags: Router(R), Solicited(S), Override(O)
  tick_ipv6_addr_t target;  // Target address
  // Followed by options (e.g., target link-layer address)
} __attribute__((packed)) tick_icmpv6_neighbor_advert_t;

// ICMPv6 Router Solicitation (RFC 4861)
typedef struct {
  u8 type;       // Type (133)
  u8 code;       // Code (0)
  u16 checksum;  // Checksum (network byte order)
  u32 reserved;  // Reserved (must be zero)
  // Followed by options (e.g., source link-layer address)
} __attribute__((packed)) tick_icmpv6_router_solicit_t;

// ICMPv6 Router Advertisement (RFC 4861)
typedef struct {
  u8 type;              // Type (134)
  u8 code;              // Code (0)
  u16 checksum;         // Checksum (network byte order)
  u8 cur_hop_limit;     // Current hop limit
  u8 flags;             // Flags: Managed(M), Other(O)
  u16 router_lifetime;  // Router lifetime (network byte order)
  u32 reachable_time;   // Reachable time (network byte order)
  u32 retrans_timer;    // Retransmit timer (network byte order)
  // Followed by options
} __attribute__((packed)) tick_icmpv6_router_advert_t;

// ============================================================================
// UDP (User Datagram Protocol)
// ============================================================================

// UDP header (RFC 768)
typedef struct {
  u16 src_port;  // Source port (network byte order)
  u16 dst_port;  // Destination port (network byte order)
  u16 len;       // Length (network byte order)
  u16 checksum;  // Checksum (network byte order)
} __attribute__((packed)) tick_udp_hdr_t;

// ============================================================================
// DNS (Domain Name System)
// ============================================================================

// DNS header (RFC 1035)
typedef struct {
  u16 id;        // Identification (network byte order)
  u16 flags;     // Flags (network byte order)
  u16 qd_count;  // Question count (network byte order)
  u16 an_count;  // Answer count (network byte order)
  u16 ns_count;  // Authority count (network byte order)
  u16 ar_count;  // Additional count (network byte order)
} __attribute__((packed)) tick_dns_hdr_t;

// DNS question section (variable length due to name)
typedef struct {
  // Note: Name field is variable length (DNS name format)
  // This struct represents only the fixed fields after the name
  u16 type;   // Query type (network byte order)
  u16 class;  // Query class (network byte order)
} __attribute__((packed)) tick_dns_question_tail_t;

// DNS resource record (variable length)
typedef struct {
  // Note: Name field is variable length (DNS name format)
  // This struct represents only the fixed fields after the name
  u16 type;    // Resource record type (network byte order)
  u16 class;   // Resource record class (network byte order)
  u32 ttl;     // Time to live (network byte order)
  u16 rd_len;  // Resource data length (network byte order)
  // Followed by rd_len bytes of resource data
} __attribute__((packed)) tick_dns_rr_tail_t;

// ============================================================================
// NTP (Network Time Protocol)
// ============================================================================

// NTP timestamp (64-bit: 32-bit seconds, 32-bit fraction)
typedef struct {
  u32 seconds;   // Seconds since 1900-01-01 (network byte order)
  u32 fraction;  // Fractional seconds (network byte order)
} __attribute__((packed)) tick_ntp_timestamp_t;

// NTP packet (RFC 5905)
typedef struct {
#if TICK_LITTLE_ENDIAN
  u8 mode : 3;     // Mode (3-bit)
  u8 version : 3;  // Version (3-bit)
  u8 leap : 2;     // Leap Indicator (2-bit)
#else
  u8 leap : 2;     // Leap Indicator (2-bit)
  u8 version : 3;  // Version (3-bit)
  u8 mode : 3;     // Mode (3-bit)
#endif
  u8 stratum;                      // Stratum level
  u8 poll;                         // Poll interval
  i8 precision;                    // Precision
  u32 root_delay;                  // Root delay (network byte order)
  u32 root_dispersion;             // Root dispersion (network byte order)
  u32 ref_id;                      // Reference ID (network byte order)
  tick_ntp_timestamp_t ref_time;   // Reference timestamp
  tick_ntp_timestamp_t orig_time;  // Origin timestamp
  tick_ntp_timestamp_t rx_time;    // Receive timestamp
  tick_ntp_timestamp_t tx_time;    // Transmit timestamp
} __attribute__((packed)) tick_ntp_pkt_t;

// ============================================================================
// Utility Functions
// ============================================================================

// IPv4 address manipulation
static inline u32 tick_ipv4_to_u32(tick_ipv4_addr_t addr) {
  return ((u32)addr.addr[0] << 24) | ((u32)addr.addr[1] << 16) |
         ((u32)addr.addr[2] << 8) | (u32)addr.addr[3];
}

static inline tick_ipv4_addr_t tick_u32_to_ipv4(u32 val) {
  tick_ipv4_addr_t addr;
  addr.addr[0] = (u8)(val >> 24);
  addr.addr[1] = (u8)(val >> 16);
  addr.addr[2] = (u8)(val >> 8);
  addr.addr[3] = (u8)val;
  return addr;
}

// MAC address comparison
static inline bool tick_mac_equal(tick_mac_addr_t a, tick_mac_addr_t b) {
  return a.addr[0] == b.addr[0] && a.addr[1] == b.addr[1] &&
         a.addr[2] == b.addr[2] && a.addr[3] == b.addr[3] &&
         a.addr[4] == b.addr[4] && a.addr[5] == b.addr[5];
}

// IPv4 address comparison
static inline bool tick_ipv4_equal(tick_ipv4_addr_t a, tick_ipv4_addr_t b) {
  return a.addr[0] == b.addr[0] && a.addr[1] == b.addr[1] &&
         a.addr[2] == b.addr[2] && a.addr[3] == b.addr[3];
}

// IPv6 address comparison
static inline bool tick_ipv6_equal(tick_ipv6_addr_t a, tick_ipv6_addr_t b) {
  for (int i = 0; i < 16; i++) {
    if (a.addr[i] != b.addr[i]) {
      return false;
    }
  }
  return true;
}

// ============================================================================
// Checksum Functions (RFC 1071)
// ============================================================================

// Internet checksum algorithm (RFC 1071)
// Used for IP, ICMP, ICMPv6, UDP, TCP checksums
// Returns checksum in network byte order
u16 tick_inet_checksum(const void *data, usz len);

// IPv4 header checksum
// Calculates checksum for IPv4 header (assumes checksum field is zero)
u16 tick_ipv4_checksum(const tick_ipv4_hdr_t *hdr);

// IPv4 pseudo-header for UDP/TCP checksum calculation
typedef struct {
  tick_ipv4_addr_t src;
  tick_ipv4_addr_t dst;
  u8 zero;
  u8 protocol;
  u16 length;
} __attribute__((packed)) tick_ipv4_pseudo_hdr_t;

// IPv6 pseudo-header for UDP/TCP checksum calculation
typedef struct {
  tick_ipv6_addr_t src;
  tick_ipv6_addr_t dst;
  u32 length;
  u8 zero[3];
  u8 next_hdr;
} __attribute__((packed)) tick_ipv6_pseudo_hdr_t;

// UDP checksum for IPv4
// Parameters:
//   udp_hdr: UDP header (checksum field should be zero)
//   payload: UDP payload data
//   payload_len: Length of payload in bytes
//   src_ip: Source IPv4 address
//   dst_ip: Destination IPv4 address
u16 tick_udp_checksum_ipv4(const tick_udp_hdr_t *udp_hdr, const void *payload,
                           usz payload_len, tick_ipv4_addr_t src_ip,
                           tick_ipv4_addr_t dst_ip);

// UDP checksum for IPv6
// Parameters:
//   udp_hdr: UDP header (checksum field should be zero)
//   payload: UDP payload data
//   payload_len: Length of payload in bytes
//   src_ip: Source IPv6 address
//   dst_ip: Destination IPv6 address
u16 tick_udp_checksum_ipv6(const tick_udp_hdr_t *udp_hdr, const void *payload,
                           usz payload_len, tick_ipv6_addr_t src_ip,
                           tick_ipv6_addr_t dst_ip);

// ICMP checksum
// Calculates checksum for ICMP message (header + payload)
// icmp_hdr: ICMP header (checksum field should be zero)
// payload: ICMP payload data
// payload_len: Length of payload in bytes
u16 tick_icmp_checksum(const tick_icmp_hdr_t *icmp_hdr, const void *payload,
                       usz payload_len);

// ICMPv6 checksum
// Calculates checksum for ICMPv6 message with IPv6 pseudo-header
// icmpv6_hdr: ICMPv6 header (checksum field should be zero)
// payload: ICMPv6 payload data
// payload_len: Length of payload in bytes
// src_ip: Source IPv6 address
// dst_ip: Destination IPv6 address
u16 tick_icmpv6_checksum(const tick_icmpv6_hdr_t *icmpv6_hdr,
                         const void *payload, usz payload_len,
                         tick_ipv6_addr_t src_ip, tick_ipv6_addr_t dst_ip);

#endif  // TICK_NET_H
