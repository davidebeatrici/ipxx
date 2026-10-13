// Part of ipxx. See LICENSE.

#include "ipxx/Subnet.hpp"

#include <cassert>
#include <unordered_set>

int main() {
	using ipxx::Address;
	using ipxx::Subnet;

	assert(Subnet().isNull());
	assert(Subnet().prefix() == 0);
	assert(!Subnet().contains(Address("192.0.2.1")));
	assert(Subnet().text().empty());

	const Subnet v4(Address("192.0.2.0"), 24);
	assert(!v4.isNull());
	assert(v4.prefix() == 24);
	assert(v4.address() == Address("192.0.2.0"));
	assert(v4.text() == "192.0.2.0/24");
	assert(v4.contains(Address("192.0.2.10")));
	assert(v4.contains(Address("::ffff:192.0.2.10")));
	assert(!v4.contains(Address("192.0.3.1")));
	assert(!v4.contains(Address("2001:db8::1")));

	const Subnet host(Address("192.0.2.10"), 32);
	assert(host.contains(Address("192.0.2.10")));
	assert(!host.contains(Address("192.0.2.11")));

	const Subnet v6(Address("2001:db8::"), 32);
	assert(v6.prefix() == 32);
	assert(v6.text() == "[2001:db8::]/32");
	assert(v6.contains(Address("2001:db8::1")));
	assert(!v6.contains(Address("2001:db9::1")));

	const Subnet v6Host(Address("2001:db8::1"), 128);
	assert(v6Host.contains(Address("2001:db8::1")));
	assert(!v6Host.contains(Address("2001:db8::2")));

	assert(Subnet(Address("192.0.2.0"), 33).isNull());
	assert(Subnet(Address("2001:db8::"), 129).isNull());
	assert(Subnet(Address(), 24).isNull());

	assert(Subnet("192.0.2.0/24") == v4);
	assert(Subnet("2001:db8::/32") == v6);
	assert(Subnet("[2001:db8::1]/128") == v6Host);
	assert(Subnet("192.0.2.0").isNull());
	assert(Subnet("192.0.2.0/").isNull());
	assert(Subnet("192.0.2.0/24/").isNull());
	assert(Subnet("/24").isNull());

	Subnet switched(Address("192.0.2.0"), 24);
	assert(switched.setAddress(Address("2001:db8::")));
	assert(switched.prefix() == 24);
	assert(switched.address() == Address("2001:db8::"));
	assert(switched.contains(Address("2001:db8::1")));
	assert(!switched.contains(Address("192.0.2.1")));

	Subnet tooWide(Address("2001:db8::"), 64);
	assert(!tooWide.setAddress(Address("192.0.2.0")));
	assert(tooWide.prefix() == 64);
	assert(tooWide.address() == Address("2001:db8::"));

	Subnet pending;
	assert(!pending.setPrefix(24));
	assert(pending.isNull());
	assert(pending.setAddress(Address("10.0.0.0")));
	assert(pending.isNull());
	assert(pending.setPrefix(24));
	assert(pending.prefix() == 24);
	assert(pending.contains(Address("::ffff:10.0.0.1")));

	Subnet edited(Address("192.0.2.0"), 24);
	assert(edited.setPrefix(16));
	assert(edited.prefix() == 16);
	assert(edited.contains(Address("192.0.3.1")));
	assert(!edited.setPrefix(33));
	assert(edited.prefix() == 16);
	assert(!edited.setAddress(Address()));
	assert(edited.prefix() == 16);

	assert(Subnet(Address("192.0.2.0"), 0).contains(Address("198.51.100.1")));
	assert(!Subnet(Address("192.0.2.0"), 0).contains(Address("2001:db8::1")));
	assert(Subnet(Address("2001:db8::"), 0).contains(Address("ff02::1")));

	assert(v4 < Subnet(Address("192.0.2.0"), 25));
	std::unordered_set< Subnet > set;
	set.insert(v4);
	assert(set.contains(Subnet("192.0.2.0/24")));
}

