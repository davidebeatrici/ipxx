// Part of ipxx. See LICENSE.

#include "ipxx/Address.hpp"

#include <cassert>
#include <unordered_set>

int main() {
	using ipxx::Address;

	assert(Address().isNull());
	assert(Address().isWildcard());
	assert(!Address().isV4());
	assert(!Address().isV6());

	assert(Address::anyV4().isV4());
	assert(Address::anyV4().isWildcard());
	assert(Address::anyV4().text() == "0.0.0.0");
	assert(Address::anyV6().isV6());
	assert(Address::anyV6().isWildcard());
	assert(Address::anyV6().text() == "::");

	assert(Address::localhostV4().text() == "127.0.0.1");
	assert(Address::localhostV4().isLoopback());
	assert(Address::localhostV6().text() == "::1");
	assert(Address::localhostV6().isLoopback());

	const Address mapped("::ffff:192.0.2.10");
	assert(mapped.isV4());
	assert(!mapped.isV6());
	assert(mapped.text() == "192.0.2.10");
	assert(mapped == Address("192.0.2.10"));
	assert(mapped.text(true) == "192.0.2.10");

	const Address v6("2001:db8::1");
	assert(v6.isV6());
	assert(v6.text() == "2001:db8::1");
	assert(v6.text(true) == "[2001:db8::1]");

	const Address scoped("fe80::1%12");
	assert(scoped.scope() == 12);
	assert(scoped.text() == "fe80::1%12");
	assert(Address("[fe80::1%12]").scope() == 12);
	assert(Address("not-an-address").isNull());

	const std::uint8_t rawV4[] = { 192, 0, 2, 10 };
	assert(Address(std::span< const std::uint8_t >(rawV4)) == Address("192.0.2.10"));

	sockaddr_in in{};
	assert(Address("192.0.2.10").toSockAddr(in));
	assert(in.sin_family == AF_INET);
	assert(Address(in) == Address("192.0.2.10"));

	sockaddr_in6 in6{};
	assert(Address("2001:db8::1").toSockAddr(in6));
	assert(in6.sin6_family == AF_INET6);
	assert(Address(in6) == Address("2001:db8::1"));
	assert(mapped.toSockAddr(in6));
	assert(Address(in6).isV4());
	assert(Address(in6).text() == "192.0.2.10");

	sockaddr_storage storage{};
	assert(Address("192.0.2.10").toSockAddr(storage));
	assert(storage.ss_family == AF_INET);
	assert(!Address("2001:db8::1").toSockAddr(in));

	assert(Address("192.0.2.10").match(Address("192.0.2.0"), 120));
	assert(!Address("192.0.3.1").match(Address("192.0.2.0"), 120));
	assert(!Address().match(Address("192.0.2.10"), 128));

	assert(Address("::ffff:127.0.0.1").isLoopback());
	assert(Address("10.1.2.3").isPrivate());
	assert(Address("172.16.0.1").isPrivate());
	assert(Address("172.31.255.255").isPrivate());
	assert(!Address("172.32.0.1").isPrivate());
	assert(Address("192.168.1.1").isPrivate());
	assert(Address("fc00::1").isPrivate());
	assert(Address("fd12::1").isPrivate());
	assert(Address("100.64.0.1").isShared());
	assert(Address("100.127.255.255").isShared());
	assert(!Address("100.128.0.1").isShared());
	assert(Address("169.254.1.1").isLinkLocal());
	assert(Address("fe80::1").isLinkLocal());
	assert(Address("255.255.255.255").isBroadcast());
	assert(!Address("192.168.1.255").isBroadcast());
	assert(Address("192.0.2.10").isDocumentation());
	assert(Address("198.51.100.1").isDocumentation());
	assert(Address("203.0.113.1").isDocumentation());
	assert(Address("2001:db8::1").isDocumentation());

	assert(Address("224.0.0.1").isMulticast());
	assert(Address("224.0.0.1").multicastScope() == Address::MulticastScope::Link);
	assert(Address("224.0.1.1").multicastScope() == Address::MulticastScope::Global);
	assert(Address("239.1.2.3").multicastScope() == Address::MulticastScope::Admin);
	assert(Address("::ffff:224.0.0.1").multicastScope() == Address::MulticastScope::Link);
	assert(Address("ff01::1").multicastScope() == Address::MulticastScope::Interface);
	assert(Address("ff02::1").multicastScope() == Address::MulticastScope::Link);
	assert(!Address("ff02::1").isLinkLocal());
	assert(Address("ff03::1").multicastScope() == Address::MulticastScope::Realm);
	assert(Address("ff04::1").multicastScope() == Address::MulticastScope::Admin);
	assert(Address("ff05::1").multicastScope() == Address::MulticastScope::Site);
	assert(Address("ff08::1").multicastScope() == Address::MulticastScope::Organization);
	assert(Address("ff0e::1").multicastScope() == Address::MulticastScope::Global);
	assert(Address("ff06::1").multicastScope() == Address::MulticastScope::Unassigned);
	assert(Address("8.8.8.8").multicastScope() == Address::MulticastScope::None);

	assert(Address("8.8.8.8").isGlobal());
	assert(Address("::ffff:8.8.8.8").isGlobal());
	assert(Address("2001:4860:4860::8888").isGlobal());
	assert(!Address::anyV4().isGlobal());
	assert(!Address::localhostV4().isGlobal());
	assert(!Address("192.0.2.1").isGlobal());

	assert(Address("192.0.2.1") < Address("192.0.2.2"));
	std::unordered_set< Address > set;
	set.insert(Address("192.0.2.10"));
	assert(set.contains(Address("::ffff:192.0.2.10")));
}
