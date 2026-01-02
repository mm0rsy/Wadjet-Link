//! Common types used throughout Wadjet.

use std::fmt;
use std::net::Ipv4Addr;

/// MAC address (6 bytes)
#[derive(Clone, Copy, PartialEq, Eq, Hash, Default)]
pub struct MacAddress(pub [u8; 6]);

impl MacAddress {
    /// Create a new MAC address from bytes
    pub const fn new(bytes: [u8; 6]) -> Self {
        Self(bytes)
    }

    /// Create a broadcast MAC address
    pub const fn broadcast() -> Self {
        Self([0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF])
    }

    /// Check if this is a broadcast address
    pub fn is_broadcast(&self) -> bool {
        self.0 == [0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF]
    }

    /// Check if this is a multicast address
    pub fn is_multicast(&self) -> bool {
        self.0[0] & 0x01 != 0
    }

    /// Get the raw bytes
    pub fn as_bytes(&self) -> &[u8; 6] {
        &self.0
    }

    /// Convert from C type
    pub(crate) fn from_c(c: &wadjet_sys::wadjet_mac_address_t) -> Self {
        Self(c.bytes)
    }

    /// Convert to C type
    pub(crate) fn to_c(&self) -> wadjet_sys::wadjet_mac_address_t {
        wadjet_sys::wadjet_mac_address_t { bytes: self.0 }
    }
}

impl fmt::Debug for MacAddress {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "MacAddress({})", self)
    }
}

impl fmt::Display for MacAddress {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
            self.0[0], self.0[1], self.0[2], self.0[3], self.0[4], self.0[5]
        )
    }
}

impl From<[u8; 6]> for MacAddress {
    fn from(bytes: [u8; 6]) -> Self {
        Self(bytes)
    }
}

impl From<MacAddress> for [u8; 6] {
    fn from(mac: MacAddress) -> Self {
        mac.0
    }
}

/// IPv4 address wrapper
#[derive(Clone, Copy, PartialEq, Eq, Hash, Default)]
pub struct Ipv4Address(pub [u8; 4]);

impl Ipv4Address {
    /// Create a new IPv4 address from bytes
    pub const fn new(bytes: [u8; 4]) -> Self {
        Self(bytes)
    }

    /// Create from standard library type
    pub fn from_std(addr: Ipv4Addr) -> Self {
        Self(addr.octets())
    }

    /// Convert to standard library type
    pub fn to_std(&self) -> Ipv4Addr {
        Ipv4Addr::from(self.0)
    }

    /// Get the raw bytes
    pub fn as_bytes(&self) -> &[u8; 4] {
        &self.0
    }

    /// Convert from C type
    pub(crate) fn from_c(c: &wadjet_sys::wadjet_ipv4_address_t) -> Self {
        Self(c.bytes)
    }

    /// Convert to C type
    pub(crate) fn to_c(&self) -> wadjet_sys::wadjet_ipv4_address_t {
        wadjet_sys::wadjet_ipv4_address_t { bytes: self.0 }
    }
}

impl fmt::Debug for Ipv4Address {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Ipv4Address({})", self)
    }
}

impl fmt::Display for Ipv4Address {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}.{}.{}.{}", self.0[0], self.0[1], self.0[2], self.0[3])
    }
}

impl From<[u8; 4]> for Ipv4Address {
    fn from(bytes: [u8; 4]) -> Self {
        Self(bytes)
    }
}

impl From<Ipv4Addr> for Ipv4Address {
    fn from(addr: Ipv4Addr) -> Self {
        Self::from_std(addr)
    }
}

impl From<Ipv4Address> for Ipv4Addr {
    fn from(addr: Ipv4Address) -> Self {
        addr.to_std()
    }
}

/// Timestamp with nanosecond precision
#[derive(Clone, Copy, PartialEq, Eq, Default)]
pub struct Timestamp {
    /// Seconds since Unix epoch
    pub seconds: i64,
    /// Nanoseconds within second
    pub nanoseconds: i64,
}

impl Timestamp {
    /// Create a new timestamp
    pub const fn new(seconds: i64, nanoseconds: i64) -> Self {
        Self { seconds, nanoseconds }
    }

    /// Get current time
    pub fn now() -> Self {
        let mut ts = wadjet_sys::wadjet_timestamp_t::default();
        unsafe { wadjet_sys::wadjet_timestamp_now(&mut ts) };
        Self::from_c(&ts)
    }

    /// Convert to total nanoseconds
    pub fn as_nanos(&self) -> i128 {
        (self.seconds as i128) * 1_000_000_000 + (self.nanoseconds as i128)
    }

    /// Convert to total microseconds
    pub fn as_micros(&self) -> i128 {
        (self.seconds as i128) * 1_000_000 + (self.nanoseconds as i128) / 1000
    }

    /// Convert from C type
    pub(crate) fn from_c(c: &wadjet_sys::wadjet_timestamp_t) -> Self {
        Self {
            seconds: c.seconds,
            nanoseconds: c.nanoseconds,
        }
    }

    /// Convert to C type
    pub(crate) fn to_c(&self) -> wadjet_sys::wadjet_timestamp_t {
        wadjet_sys::wadjet_timestamp_t {
            seconds: self.seconds,
            nanoseconds: self.nanoseconds,
        }
    }
}

impl fmt::Debug for Timestamp {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Timestamp({}.{:09})", self.seconds, self.nanoseconds)
    }
}

impl fmt::Display for Timestamp {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}.{:09}", self.seconds, self.nanoseconds)
    }
}

/// Ethernet frame types
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[repr(u16)]
pub enum EtherType {
    /// IPv4
    Ipv4 = 0x0800,
    /// ARP
    Arp = 0x0806,
    /// VLAN Tagged
    VlanTagged = 0x8100,
    /// IPv6
    Ipv6 = 0x86DD,
    /// Unknown type
    Unknown(u16),
}

impl From<u16> for EtherType {
    fn from(value: u16) -> Self {
        match value {
            0x0800 => EtherType::Ipv4,
            0x0806 => EtherType::Arp,
            0x8100 => EtherType::VlanTagged,
            0x86DD => EtherType::Ipv6,
            other => EtherType::Unknown(other),
        }
    }
}

impl From<EtherType> for u16 {
    fn from(et: EtherType) -> Self {
        match et {
            EtherType::Ipv4 => 0x0800,
            EtherType::Arp => 0x0806,
            EtherType::VlanTagged => 0x8100,
            EtherType::Ipv6 => 0x86DD,
            EtherType::Unknown(v) => v,
        }
    }
}

/// IP protocol numbers
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[repr(u8)]
pub enum IpProtocol {
    /// ICMP
    Icmp = 1,
    /// TCP
    Tcp = 6,
    /// UDP
    Udp = 17,
    /// Unknown protocol
    Unknown(u8),
}

impl From<u8> for IpProtocol {
    fn from(value: u8) -> Self {
        match value {
            1 => IpProtocol::Icmp,
            6 => IpProtocol::Tcp,
            17 => IpProtocol::Udp,
            other => IpProtocol::Unknown(other),
        }
    }
}

impl From<IpProtocol> for u8 {
    fn from(proto: IpProtocol) -> Self {
        match proto {
            IpProtocol::Icmp => 1,
            IpProtocol::Tcp => 6,
            IpProtocol::Udp => 17,
            IpProtocol::Unknown(v) => v,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_mac_address() {
        let mac = MacAddress::new([0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF]);
        assert_eq!(mac.to_string(), "aa:bb:cc:dd:ee:ff");
        assert!(!mac.is_broadcast());
        assert!(!mac.is_multicast());
        
        let bcast = MacAddress::broadcast();
        assert!(bcast.is_broadcast());
        assert!(bcast.is_multicast());
    }

    #[test]
    fn test_ipv4_address() {
        let ip = Ipv4Address::new([192, 168, 1, 100]);
        assert_eq!(ip.to_string(), "192.168.1.100");
        
        let std_ip: Ipv4Addr = ip.into();
        assert_eq!(std_ip, Ipv4Addr::new(192, 168, 1, 100));
    }

    #[test]
    fn test_timestamp() {
        let ts = Timestamp::new(1234567890, 123456);
        assert_eq!(ts.as_micros(), 1234567890_123456);
    }

    #[test]
    fn test_ether_type() {
        assert_eq!(EtherType::from(0x0800), EtherType::Ipv4);
        assert_eq!(u16::from(EtherType::Ipv4), 0x0800);
    }
}
