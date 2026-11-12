// Part of ipxx. See LICENSE.

#include "ipxx/Endpoint.hpp"

#include <cassert>
#include <cstddef>
#include <functional>
#include <unordered_set>

int main() {
	using ipxx::Address;
	using ipxx::Endpoint;

	assert(Endpoint().isNull());
	assert(Endpoint().port == 0);
	assert(Endpoint().text() == ":0");

	assert(Endpoint::anyV4().address.isV4());
	assert(Endpoint::anyV4().address.isWildcard());
	assert(Endpoint::anyV6().address.isV6());

	const Endpoint v4(Address("192.0.2.10"), 64738);
	assert(!v4.isNull());
	assert(v4.port == 64738);
	assert(v4.text() == "192.0.2.10:64738");

	const Endpoint mapped(Address("::ffff:192.0.2.10"), 64738);
	assert(mapped.text() == "192.0.2.10:64738");
	assert(mapped == v4);

	const Endpoint v6(Address("2001:db8::1"), 443);
	assert(v6.text() == "[2001:db8::1]:443");

	const Endpoint scoped(Address("fe80::1%3"), 1);
	assert(scoped.text() == "[fe80::1%3]:1");

	assert(Endpoint(Address("192.0.2.10")).port == 0);
	assert(Endpoint(std::uint16_t{ 80 }).isNull());
	assert(Endpoint(std::uint16_t{ 80 }).port == 80);

	sockaddr_in in{};
	assert(v4.toSockAddr(in));
	assert(in.sin_family == AF_INET);
	assert(ntohs(in.sin_port) == 64738);
	assert(Endpoint(in) == v4);

	sockaddr_in6 in6{};
	assert(v6.toSockAddr(in6));
	assert(in6.sin6_family == AF_INET6);
	assert(ntohs(in6.sin6_port) == 443);
	assert(Endpoint(in6) == v6);

	assert(mapped.toSockAddr(in6));
	assert(Endpoint(in6).address.isV4());
	assert(Endpoint(in6).text() == "192.0.2.10:64738");
	assert(!v6.toSockAddr(in));

	sockaddr_storage storage{};
	assert(v4.toSockAddr(storage));
	assert(storage.ss_family == AF_INET);
	assert(Endpoint(storage) == v4);
	assert(v6.toSockAddr(storage));
	assert(storage.ss_family == AF_INET6);
	assert(Endpoint(storage) == v6);
	assert(!Endpoint().toSockAddr(storage));

	assert(v4 < Endpoint(Address("192.0.2.10"), 64739));
	assert(Endpoint(Address("192.0.2.10"), 1) < Endpoint(Address("192.0.2.11"), 1));

	const Endpoint a1(Address("127.0.0.1"), 443);
	const Endpoint a2(Address("127.0.0.1"), 443);
	const Endpoint b(Address("127.0.0.1"), 64738);
	const Endpoint c(Address("10.0.0.1"), 80);
	assert(a1 == a2);
	assert(a1 != b);
	assert(a1 != c);
	assert(b != c);

	// Null first, then by address, then by port.
	const Endpoint ordered[] = {
		Endpoint(),
		Endpoint(std::uint16_t{ 1 }),
		Endpoint(std::uint16_t{ 999 }),
		Endpoint(Address("0.0.0.1"), 0),
		Endpoint(Address("0.0.0.2"), 0),
		Endpoint(Address("0.0.0.2"), 1),
		Endpoint(Address("80.0.0.1"), 0),
		Endpoint(Address("80.0.0.1"), 100),
		Endpoint(Address("80.0.0.1"), 64738),
		Endpoint(Address("80.0.0.2"), 64738),
		Endpoint(Address("255.255.255.255"), 0),
		Endpoint(Address("255.255.255.255"), 65535),
	};
	for (std::size_t i = 1; i < std::size(ordered); ++i) {
		assert(ordered[i - 1] < ordered[i]);
	}

	assert(std::hash< Endpoint >{}(a1) == std::hash< Endpoint >{}(a2));
	assert(std::hash< Endpoint >{}(a1) != std::hash< Endpoint >{}(b));
	assert(std::hash< Endpoint >{}(a1) != std::hash< Endpoint >{}(c));
	assert(std::hash< Endpoint >{}(b) != std::hash< Endpoint >{}(c));

	std::unordered_set< Endpoint > set;
	set.insert(v4);
	assert(set.contains(mapped));
}
