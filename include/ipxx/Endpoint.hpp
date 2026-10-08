// Part of ipxx. See LICENSE.

#ifndef IPXX_ENDPOINT_HPP
#define IPXX_ENDPOINT_HPP

#include "Address.hpp"

#include <cstdint>
#include <string>

namespace ipxx {
struct Endpoint {
	Address address;
	std::uint16_t port = 0;

	Endpoint() = default;

	static Endpoint anyV4() { return Endpoint(Address::anyV4()); }
	static Endpoint anyV6() { return Endpoint(Address::anyV6()); }
	explicit Endpoint(const Address &address) : address(address) {}
	explicit Endpoint(std::uint16_t port) : port(port) {}
	Endpoint(const Address &address, std::uint16_t port) : address(address), port(port) {}

	explicit Endpoint(const sockaddr_in &sa) : address(sa), port(ntohs(sa.sin_port)) {}

	explicit Endpoint(const sockaddr_in6 &sa) : address(sa), port(ntohs(sa.sin6_port)) {}

	explicit Endpoint(const sockaddr_storage &sa) : address(sa) {
		if (sa.ss_family == AF_INET) {
			port = ntohs(reinterpret_cast< const sockaddr_in & >(sa).sin_port);
		} else if (sa.ss_family == AF_INET6) {
			port = ntohs(reinterpret_cast< const sockaddr_in6 & >(sa).sin6_port);
		}
	}

	bool isNull() const { return address.isNull(); }

	bool toSockAddr(sockaddr_in &out) const {
		if (!address.toSockAddr(out)) {
			return false;
		}
		out.sin_port = htons(port);
		return true;
	}

	bool toSockAddr(sockaddr_in6 &out) const {
		if (!address.toSockAddr(out)) {
			return false;
		}
		out.sin6_port = htons(port);
		return true;
	}

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

	// Brackets a v6 address so the port separator stays unambiguous.
	std::string text() const {
		std::string out = address.text(address.isV6());
		out.push_back(':');
		out += std::to_string(port);
		return out;
	}

	friend bool operator==(const Endpoint &lhs, const Endpoint &rhs) {
		return lhs.address == rhs.address && lhs.port == rhs.port;
	}

	friend std::strong_ordering operator<=>(const Endpoint &lhs, const Endpoint &rhs) {
		if (auto cmp = lhs.address <=> rhs.address; cmp != 0) {
			return cmp;
		}
		return lhs.port <=> rhs.port;
	}
};
} // namespace ipxx

template <> struct std::hash< ipxx::Endpoint > {
	std::size_t operator()(const ipxx::Endpoint &endpoint) const noexcept {
		return std::hash< ipxx::Address >{}(endpoint.address) ^ (static_cast< std::size_t >(endpoint.port) << 16);
	}
};

#endif
