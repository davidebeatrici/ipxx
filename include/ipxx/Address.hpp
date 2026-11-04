// Part of ipxx. See LICENSE.

#ifndef IPXX_ADDRESS_HPP
#define IPXX_ADDRESS_HPP

#include <algorithm>
#include <array>
#include <compare>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <span>
#include <string>
#include <string_view>

#ifdef _WIN32
#	include <winsock2.h>
#	include <ws2tcpip.h>
#else
#	include <arpa/inet.h>
#	include <netinet/in.h>
#	include <sys/socket.h>
#endif

namespace ipxx {
/**
 * An IP address.
 *
 * An IPv4 address is stored as an IPv4-mapped IPv6 address, `::ffff:0:0/96`.
 * A mapped address is an IPv4 address: isV4() is true, text() prints dotted
 * decimal, and it compares equal to the same address constructed from four
 * bytes. That is the form a dual-stack socket accepts.
 */
class Address {
public:
	/**
	 * Scope of a multicast address. None unless isMulticast(). An unassigned
	 * IPv6 scope nibble stays Unassigned so it is not reported as None.
	 */
	enum class MulticastScope : std::uint8_t {
		None,         ///< Not a multicast address.
		Interface,    ///< Interface-local, IPv6 scope 1.
		Link,         ///< Link-local. IPv4 224.0.0.0/24, or IPv6 scope 2.
		Realm,        ///< Realm-local, IPv6 scope 3.
		Admin,        ///< Admin-local. IPv4 239.0.0.0/8, or IPv6 scope 4.
		Site,         ///< Site-local, IPv6 scope 5.
		Organization, ///< Organization-local, IPv6 scope 8.
		Global,       ///< Global. Other IPv4 multicast, or IPv6 scope 14.
		Unassigned    ///< An IPv6 scope nibble with no assigned meaning.
	};

	static constexpr std::size_t v4Size = 4;  ///< Size of an IPv4 address, in bytes.
	static constexpr std::size_t v6Size = 16; ///< Size of an IPv6 address, in bytes.

	using V6        = std::array< std::uint8_t, v6Size >; ///< The 16-byte form.
	using V4        = std::array< std::uint8_t, v4Size >; ///< The four address bytes.
	using View      = std::span< std::uint8_t >;          ///< A mutable view of address bytes.
	using ViewConst = std::span< const std::uint8_t >;    ///< A const view of address bytes.

	/** A null address. isNull() and isWildcard() are true. */
	Address() = default;

	/** The IPv4 wildcard, 0.0.0.0. */
	static Address anyV4() {
		Address address;
		address.setV4(V4{}.data());
		return address;
	}

	/** The IPv6 wildcard, `::`. */
	static Address anyV6() {
		Address address;
		address.m_null = false;
		return address;
	}

	/** 127.0.0.1. */
	static Address localhostV4() {
		Address address;
		constexpr std::uint8_t bytes[v4Size] = { 127, 0, 0, 1 };
		address.setV4(bytes);
		return address;
	}

	/** `::1`. */
	static Address localhostV6() {
		Address address;
		address.m_bytes[15] = 1;
		address.m_null = false;
		return address;
	}

	/**
	 * Constructs from raw bytes.
	 *
	 * Four bytes are stored as a mapped IPv4 address. Sixteen bytes are stored
	 * as written. Any other length leaves the address null.
	 *
	 * @param bytes The address bytes.
	 * @param scope The scope id. Kept only for a real IPv6 address.
	 */
	explicit Address(std::span< const std::uint8_t > bytes, std::uint32_t scope = 0) {
		if (bytes.size() == v4Size) {
			setV4(bytes.data());
			m_scope = scope;
		} else if (bytes.size() == v6Size) {
			std::memcpy(m_bytes.data(), bytes.data(), v6Size);
			m_null = false;
			m_scope = scope;
		}
	}

	/**
	 * Parses text. A failed parse leaves the address null.
	 *
	 * Surrounding brackets are stripped. A `%` suffix is a numeric scope id.
	 * An IPv4-mapped address is stored as IPv4.
	 *
	 * @param text Dotted decimal, or an IPv6 address.
	 */
	explicit Address(std::string_view text) {
		if (text.size() >= 2 && text.front() == '[' && text.back() == ']') {
			text.remove_prefix(1);
			text.remove_suffix(1);
		}
		const auto percent = text.rfind('%');
		std::string owned(text);
		if (percent != std::string_view::npos) {
			owned = std::string(text.substr(0, percent));
			m_scope = static_cast< std::uint32_t >(std::strtoul(std::string(text.substr(percent + 1)).c_str(), nullptr, 10));
		}
		in_addr v4{};
		in6_addr v6{};
		if (inet_pton(AF_INET, owned.c_str(), &v4) == 1) {
			setV4(reinterpret_cast< const std::uint8_t * >(&v4));
		} else if (inet_pton(AF_INET6, owned.c_str(), &v6) == 1) {
			std::memcpy(m_bytes.data(), &v6, v6Size);
			m_null = false;
		}
	}

	/** Constructs from an IPv4 socket address. The port is ignored. */
	explicit Address(const sockaddr_in &sa) {
		setV4(reinterpret_cast< const std::uint8_t * >(&sa.sin_addr));
	}

	/**
	 * Constructs from an IPv6 socket address. A mapped address is stored as
	 * IPv4, and its scope id is dropped. The port is ignored.
	 */
	explicit Address(const sockaddr_in6 &sa) {
		std::memcpy(m_bytes.data(), &sa.sin6_addr, v6Size);
		m_null = false;
		if (!isV4()) {
			m_scope = sa.sin6_scope_id;
		}
	}

	/**
	 * Constructs from a socket address. `AF_INET` and `AF_INET6` are read.
	 * Any other family leaves the address null.
	 */
	explicit Address(const sockaddr_storage &sa) {
		if (sa.ss_family == AF_INET) {
			const auto &in = reinterpret_cast< const sockaddr_in & >(sa);
			setV4(reinterpret_cast< const std::uint8_t * >(&in.sin_addr));
		} else if (sa.ss_family == AF_INET6) {
			const auto &in6 = reinterpret_cast< const sockaddr_in6 & >(sa);
			std::memcpy(m_bytes.data(), &in6.sin6_addr, v6Size);
			m_null = false;
			m_scope = in6.sin6_scope_id;
		}
	}

	/** True for a native v4 input and for `::ffff:0:0/96`. */
	bool isV4() const { return !m_null && isMappedPrefix(); }
	/** True for a real IPv6 address. A mapped address is not v6. */
	bool isV6() const { return !m_null && !isMappedPrefix(); }
	/** True when no address has been set, or after reset(). */
	bool isNull() const { return m_null; }
	/**
	 * True for a null address, for 0.0.0.0, and for `::`.
	 */
	bool isWildcard() const {
		if (m_null) {
			return true;
		}
		if (isV4()) {
			return std::all_of(m_bytes.begin() + 12, m_bytes.end(), [](std::uint8_t b) { return b == 0; });
		}
		return std::all_of(m_bytes.begin(), m_bytes.end(), [](std::uint8_t b) { return b == 0; });
	}

	/**
	 * True for 127.0.0.0/8 and for `::1`. A mapped address is classified as the
	 * IPv4 address it contains.
	 */
	bool isLoopback() const {
		if (isV4()) {
			return m_bytes[12] == 127;
		}
		return isV6() && std::all_of(m_bytes.begin(), m_bytes.end() - 1, [](std::uint8_t b) { return b == 0; }) && m_bytes[15] == 1;
	}

	/** True for 224.0.0.0/4 and for `ff00::/8`. */
	bool isMulticast() const {
		if (isV4()) {
			return (m_bytes[12] & 0xF0) == 224;
		}
		return isV6() && m_bytes[0] == 0xFF;
	}

	/**
	 * Unicast link-local only: 169.254.0.0/16, or `fe80::/10`.
	 * Multicast scope is multicastScope(), not this.
	 */
	bool isLinkLocal() const {
		if (isV4()) {
			return m_bytes[12] == 169 && m_bytes[13] == 254;
		}
		return isV6() && m_bytes[0] == 0xFE && (m_bytes[1] & 0xC0) == 0x80;
	}

	/**
	 * The limited broadcast, 255.255.255.255. A subnet-directed broadcast needs
	 * the subnet.
	 */
	bool isBroadcast() const { return isV4() && std::all_of(m_bytes.begin() + 12, m_bytes.end(), [](std::uint8_t b) { return b == 0xFF; }); }

	/** 10/8, 172.16/12, 192.168/16, and `fc00::/7`. */
	bool isPrivate() const {
		if (isV4()) {
			return m_bytes[12] == 10 || (m_bytes[12] == 172 && (m_bytes[13] & 0xF0) == 16) || (m_bytes[12] == 192 && m_bytes[13] == 168);
		}
		return isV6() && (m_bytes[0] & 0xFE) == 0xFC;
	}

	/** 100.64/10, carrier-grade NAT. Not private. */
	bool isShared() const { return isV4() && m_bytes[12] == 100 && (m_bytes[13] & 0xC0) == 64; }

	/** 192.0.2/24, 198.51.100/24, 203.0.113/24, and `2001:db8::/32`. */
	bool isDocumentation() const {
		if (isV4()) {
			return (m_bytes[12] == 192 && m_bytes[13] == 0 && m_bytes[14] == 2) || (m_bytes[12] == 198 && m_bytes[13] == 51 && m_bytes[14] == 100)
				   || (m_bytes[12] == 203 && m_bytes[13] == 0 && m_bytes[14] == 113);
		}
		return isV6() && m_bytes[0] == 0x20 && m_bytes[1] == 0x01 && m_bytes[2] == 0x0D && m_bytes[3] == 0xB8;
	}

	/**
	 * Complement of the predicates above, not a claim that the address is
	 * publicly routable. Benchmarking, 6to4 and Teredo still report global.
	 */
	bool isGlobal() const {
		return !isNull() && !isWildcard() && !isLoopback() && !isMulticast() && !isLinkLocal() && !isBroadcast() && !isPrivate() && !isShared()
			   && !isDocumentation();
	}

	/**
	 * The multicast scope. None when this is not a multicast address.
	 * An IPv4 224.0.0.0/24 is Link, 239.0.0.0/8 is Admin, and the rest is Global.
	 */
	MulticastScope multicastScope() const {
		if (!isMulticast()) {
			return MulticastScope::None;
		}
		if (isV4()) {
			if (m_bytes[12] == 224 && m_bytes[13] == 0 && m_bytes[14] == 0) {
				return MulticastScope::Link;
			}
			if (m_bytes[12] == 239) {
				return MulticastScope::Admin;
			}
			return MulticastScope::Global;
		}
		switch (m_bytes[1] & 0x0F) {
		case 1:
			return MulticastScope::Interface;
		case 2:
			return MulticastScope::Link;
		case 3:
			return MulticastScope::Realm;
		case 4:
			return MulticastScope::Admin;
		case 5:
			return MulticastScope::Site;
		case 8:
			return MulticastScope::Organization;
		case 0xE:
			return MulticastScope::Global;
		default:
			return MulticastScope::Unassigned;
		}
	}

	/** The scope id. Zero unless a real IPv6 address carried one. */
	std::uint32_t scope() const { return m_scope; }

	/**
	 * The 16-byte form. A v4 address occupies the last four bytes, behind the
	 * mapped prefix. Empty when null.
	 */
	ViewConst v6() const { return m_null ? ViewConst{} : ViewConst(m_bytes); }
	/** @copydoc v6() const */
	View v6() { return m_null ? View{} : View(m_bytes); }

	/**
	 * The four address bytes. Empty unless this is a v4 or v4-mapped address,
	 * so the mapped prefix cannot be edited from here.
	 */
	ViewConst v4() const { return isV4() ? ViewConst(m_bytes.data() + 12, v4Size) : ViewConst{}; }
	/** @copydoc v4() const */
	View v4() { return isV4() ? View(m_bytes.data() + 12, v4Size) : View{}; }

	/**
	 * The four address bytes copied into an integer, as stored. Zero unless
	 * this is a v4 or v4-mapped address. The byte order is the machine's.
	 */
	std::uint32_t toIPv4() const {
		std::uint32_t value = 0;
		if (isV4()) {
			std::memcpy(&value, m_bytes.data() + 12, v4Size);
		}
		return value;
	}

	/**
	 * The textual form. Empty when null.
	 *
	 * A mapped address is printed as native IPv4. Brackets apply to a real v6
	 * address only: an IPv4 address has no colon, so it needs none.
	 *
	 * @param bracketEnclosed Bracket a real IPv6 address.
	 */
	std::string text(bool bracketEnclosed = false) const {
		if (m_null) {
			return {};
		}
		char buffer[INET6_ADDRSTRLEN] = {};
		if (isV4()) {
			inet_ntop(AF_INET, m_bytes.data() + 12, buffer, sizeof(buffer));
			return buffer;
		}
		inet_ntop(AF_INET6, m_bytes.data(), buffer, sizeof(buffer));
		std::string out = buffer;
		if (m_scope != 0) {
			out.push_back('%');
			out += std::to_string(m_scope);
		}
		if (bracketEnclosed) {
			out.insert(out.begin(), '[');
			out.push_back(']');
		}
		return out;
	}

	/**
	 * Writes an IPv4 socket address. Fails, and leaves @p out zeroed, unless
	 * this is a v4 or v4-mapped address. The port is left at zero.
	 */
	bool toSockAddr(sockaddr_in &out) const {
		std::memset(&out, 0, sizeof(out));
		if (!isV4()) {
			return false;
		}
		out.sin_family = AF_INET;
		std::memcpy(&out.sin_addr, m_bytes.data() + 12, v4Size);
		return true;
	}

	/**
	 * Writes an IPv6 socket address. A v4 address is written as `::ffff:0:0/96`,
	 * the form a dual-stack socket accepts. Fails, and leaves @p out zeroed,
	 * when null. The port is left at zero.
	 */
	bool toSockAddr(sockaddr_in6 &out) const {
		std::memset(&out, 0, sizeof(out));
		if (m_null) {
			return false;
		}
		out.sin6_family = AF_INET6;
		std::memcpy(&out.sin6_addr, m_bytes.data(), v6Size);
		if (!isV4()) {
			out.sin6_scope_id = m_scope;
		}
		return true;
	}

	/**
	 * Writes a socket address of the matching family. A v4 address is written
	 * as `AF_INET`. Fails, and leaves @p out zeroed, when null.
	 */
	bool toSockAddr(sockaddr_storage &out) const {
		std::memset(&out, 0, sizeof(out));
		if (isV4()) {
			return toSockAddr(reinterpret_cast< sockaddr_in & >(out));
		}
		if (isV6()) {
			return toSockAddr(reinterpret_cast< sockaddr_in6 & >(out));
		}
		return false;
	}

	/**
	 * Prefix match on the 16-byte form.
	 *
	 * A v4 /24 is 120 bits here, because the mapped prefix occupies the first
	 * 96. False when either address is null. @p bits above 128 is clamped.
	 *
	 * @param other The address to compare against.
	 * @param bits How many leading bits must match.
	 */
	bool match(const Address &other, unsigned int bits) const {
		if (m_null || other.m_null) {
			return false;
		}
		if (bits > v6Size * 8) {
			bits = static_cast< unsigned int >(v6Size * 8);
		}
		for (std::size_t i = 0; i < v6Size; ++i) {
			if (bits >= 8) {
				if (m_bytes[i] != other.m_bytes[i]) {
					return false;
				}
				bits -= 8;
			} else {
				const auto mask = static_cast< std::uint8_t >(0xFFu << (8 - bits));
				if ((m_bytes[i] & mask) != (other.m_bytes[i] & mask)) {
					return false;
				}
				break;
			}
		}
		return true;
	}

	/** Clears the address and the scope, and marks the address null. */
	void reset() {
		m_bytes.fill(0);
		m_null = true;
		m_scope = 0;
	}

	/**
	 * True when both are null, or both hold the same bytes and scope.
	 * A mapped address compares equal to the same native IPv4 address.
	 */
	friend bool operator==(const Address &lhs, const Address &rhs) {
		return lhs.m_null == rhs.m_null && lhs.m_scope == rhs.m_scope && lhs.m_bytes == rhs.m_bytes;
	}

	/**
	 * Orders by null, then bytes, then scope. A null address sorts first.
	 * A mapped address orders with the same native IPv4 address.
	 */
	friend std::strong_ordering operator<=>(const Address &lhs, const Address &rhs) {
		if (auto cmp = rhs.m_null <=> lhs.m_null; cmp != 0) {
			return cmp;
		}
		if (auto cmp = lhs.m_bytes <=> rhs.m_bytes; cmp != 0) {
			return cmp;
		}
		return lhs.m_scope <=> rhs.m_scope;
	}

private:
	/** True when the first twelve bytes are the mapped prefix. */
	bool isMappedPrefix() const {
		return std::all_of(m_bytes.begin(), m_bytes.begin() + 10, [](std::uint8_t b) { return b == 0; }) && m_bytes[10] == 0xFF
			   && m_bytes[11] == 0xFF;
	}

	/** Stores four bytes behind the mapped prefix and clears the null flag. */
	void setV4(const std::uint8_t *bytes) {
		m_bytes.fill(0);
		m_bytes[10] = 0xFF;
		m_bytes[11] = 0xFF;
		std::memcpy(m_bytes.data() + 12, bytes, v4Size);
		m_null = false;
	}

	V6 m_bytes{};
	bool m_null = true;
	std::uint32_t m_scope = 0;
};
} // namespace ipxx

/** Hashes the scope and the 16-byte form. Equal addresses hash equal. */
template <> struct std::hash< ipxx::Address > {
	std::size_t operator()(const ipxx::Address &address) const noexcept {
		std::size_t value = address.scope();
		for (const std::uint8_t byte : address.v6()) {
			value = value * 131 + byte;
		}
		return value;
	}
};

#endif
