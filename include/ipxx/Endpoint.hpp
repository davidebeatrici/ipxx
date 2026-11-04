// Part of ipxx. See LICENSE.

#ifndef IPXX_ENDPOINT_HPP
#define IPXX_ENDPOINT_HPP

#include "Address.hpp"

#include <cstdint>
#include <string>

namespace ipxx {
/**
 * An address plus a port.
 *
 * text() brackets a real IPv6 address so the port separator stays unambiguous:
 * `[2001:db8::1]:443`, but `192.0.2.10:64738`. A mapped address is printed as
 * native IPv4.
 */
struct Endpoint {
	Address address;         ///< The address. Null until one is set.
	std::uint16_t port = 0;  ///< The port, in host order.

	/** A null address and port 0. */
	Endpoint() = default;

	/** The IPv4 wildcard, port 0. */
	static Endpoint anyV4() { return Endpoint(Address::anyV4()); }
	/** The IPv6 wildcard, port 0. */
	static Endpoint anyV6() { return Endpoint(Address::anyV6()); }
	/** An address with port 0. */
	explicit Endpoint(const Address &address) : address(address) {}
	/** A null address with a port. */
	explicit Endpoint(std::uint16_t port) : port(port) {}
	/** An address and a port. */
	Endpoint(const Address &address, std::uint16_t port) : address(address), port(port) {}

	/** Constructs from an IPv4 socket address. The port is converted to host order. */
	explicit Endpoint(const sockaddr_in &sa) : address(sa), port(ntohs(sa.sin_port)) {}

	/**
	 * Constructs from an IPv6 socket address. A mapped address is stored as
	 * IPv4. The port is converted to host order.
	 */
	explicit Endpoint(const sockaddr_in6 &sa) : address(sa), port(ntohs(sa.sin6_port)) {}

	/**
	 * Constructs from a socket address. `AF_INET` and `AF_INET6` are read.
	 * Any other family leaves the address null and the port at zero.
	 */
	explicit Endpoint(const sockaddr_storage &sa) : address(sa) {
		if (sa.ss_family == AF_INET) {
			port = ntohs(reinterpret_cast< const sockaddr_in & >(sa).sin_port);
		} else if (sa.ss_family == AF_INET6) {
			port = ntohs(reinterpret_cast< const sockaddr_in6 & >(sa).sin6_port);
		}
	}

	/** True when the address is null. The port is ignored. */
	bool isNull() const { return address.isNull(); }

	/**
	 * Writes an IPv4 socket address, including the port. Fails unless the
	 * address is v4 or v4-mapped. On failure the address writer has zeroed
	 * @p out.
	 */
	bool toSockAddr(sockaddr_in &out) const {
		if (!address.toSockAddr(out)) {
			return false;
		}
		out.sin_port = htons(port);
		return true;
	}

	/**
	 * Writes an IPv6 socket address, including the port. A v4 address is
	 * written as `::ffff:0:0/96`. Fails when the address is null. On failure
	 * the address writer has zeroed @p out.
	 */
	bool toSockAddr(sockaddr_in6 &out) const {
		if (!address.toSockAddr(out)) {
			return false;
		}
		out.sin6_port = htons(port);
		return true;
	}

	/**
	 * Writes a socket address of the matching family, including the port.
	 * A v4 address is written as `AF_INET`. Fails when the address is null.
	 * On failure the address writer has zeroed @p out.
	 */
	bool toSockAddr(sockaddr_storage &out) const {
		if (!address.toSockAddr(out)) {
			return false;
		}
		if (out.ss_family == AF_INET) {
			reinterpret_cast< sockaddr_in & >(out).sin_port = htons(port);
		} else if (out.ss_family == AF_INET6) {
			reinterpret_cast< sockaddr_in6 & >(out).sin6_port = htons(port);
		}
		return true;
	}

	/**
	 * The textual form. Brackets a real v6 address so the port separator stays
	 * unambiguous. A null address prints as `:0`.
	 */
	std::string text() const {
		std::string out = address.text(address.isV6());
		out.push_back(':');
		out += std::to_string(port);
		return out;
	}

	/** True when the address and the port are both equal. */
	friend bool operator==(const Endpoint &lhs, const Endpoint &rhs) {
		return lhs.address == rhs.address && lhs.port == rhs.port;
	}

	/** Orders by address, then port. */
	friend std::strong_ordering operator<=>(const Endpoint &lhs, const Endpoint &rhs) {
		if (auto cmp = lhs.address <=> rhs.address; cmp != 0) {
			return cmp;
		}
		return lhs.port <=> rhs.port;
	}
};
} // namespace ipxx

/** Hashes the address and the port. Equal endpoints hash equal. */
template <> struct std::hash< ipxx::Endpoint > {
	std::size_t operator()(const ipxx::Endpoint &endpoint) const noexcept {
		return std::hash< ipxx::Address >{}(endpoint.address) ^ (static_cast< std::size_t >(endpoint.port) << 16);
	}
};

#endif
