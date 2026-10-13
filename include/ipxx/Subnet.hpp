// Part of ipxx. See LICENSE.

#ifndef IPXX_SUBNET_HPP
#define IPXX_SUBNET_HPP

#include "Address.hpp"

#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

namespace ipxx {
// A network: an Address plus the prefix that makes it one.
// The prefix the caller writes is the address's own width, 0 to 32 for v4 and
// 0 to 128 for v6. It is stored on the 16-byte form match() already takes, so
// a v4 /24 is 120 and a v6 /64 is 64. The getter undoes that translation.
class Subnet {
public:
	Subnet() = default;

	// An illegal pair leaves the subnet null, the same way a failed parse does.
	Subnet(const Address &address, std::uint8_t prefix) { set(address, prefix); }

	// A failed parse leaves the subnet null, the same way Address does.
	explicit Subnet(std::string_view text) { setText(text); }

	bool set(const Address &address, std::uint8_t prefix) {
		if (!prefixFits(address, prefix)) {
			return false;
		}
		m_address = address;
		store(address, prefix);
		return true;
	}

	// Keeps the prefix the caller sees, and rewrites the stored form for the new family.
	// 192.0.2.0/24 stores 120; switching the address to v6 stores 24, so prefix() still returns 24.
	// A v6 /64 cannot move to v4, so that call fails and the subnet is left as it was.
	bool setAddress(const Address &address) {
		if (address.isNull()) {
			return false;
		}
		if (m_null) {
			m_address = address;
			return true;
		}
		const auto callerPrefix = prefix();
		if (!prefixFits(address, callerPrefix)) {
			return false;
		}
		m_address = address;
		store(address, callerPrefix);
		return true;
	}

	// Fails on a null address or a prefix the current family cannot hold.
	// Either failure leaves the subnet unchanged.
	bool setPrefix(std::uint8_t prefix) {
		if (!prefixFits(m_address, prefix)) {
			return false;
		}
		store(m_address, prefix);
		return true;
	}

	bool setText(std::string_view text) {
		const auto slash = text.rfind('/');
		if (slash == std::string_view::npos || slash == 0 || slash + 1 >= text.size()) {
			return false;
		}
		char *end = nullptr;
		const auto prefixText = std::string(text.substr(slash + 1));
		const auto parsed = std::strtoul(prefixText.c_str(), &end, 10);
		if (end == prefixText.c_str() || *end != '\0' || parsed > 128) {
			return false;
		}
		return set(Address(text.substr(0, slash)), static_cast< std::uint8_t >(parsed));
	}

	bool isNull() const { return m_null; }

	Address address() const { return m_null ? Address() : m_address; }

	// The width the caller wrote. A v4 /24 reads back as 24; the 96 never leaks.
	std::uint8_t prefix() const {
		if (m_null) {
			return 0;
		}
		if (m_address.isV4()) {
			return static_cast< std::uint8_t >(m_prefix - v4MappedBits);
		}
		return m_prefix;
	}

	// match(), with the subnet owning the prefix. 192.0.2.0/24 contains 192.0.2.10.
	bool contains(const Address &other) const {
		if (m_null) {
			return false;
		}
		return m_address.match(other, m_prefix);
	}

	// True when other is this network or a more specific one inside it.
	// 192.0.2.0/24 contains 192.0.2.0/25. It does not contain 192.0.2.0/16.
	bool contains(const Subnet &other) const {
		if (m_null || other.m_null || other.m_prefix < m_prefix) {
			return false;
		}
		return m_address.match(other.m_address, m_prefix);
	}

	std::string text() const {
		if (m_null) {
			return {};
		}
		std::string out = m_address.text(m_address.isV6());
		out.push_back('/');
		out += std::to_string(prefix());
		return out;
	}

	friend bool operator==(const Subnet &lhs, const Subnet &rhs) {
		if (lhs.m_null != rhs.m_null) {
			return false;
		}
		if (lhs.m_null) {
			return true;
		}
		return lhs.m_address == rhs.m_address && lhs.m_prefix == rhs.m_prefix;
	}

	friend std::strong_ordering operator<=>(const Subnet &lhs, const Subnet &rhs) {
		if (auto cmp = rhs.m_null <=> lhs.m_null; cmp != 0) {
			return cmp;
		}
		if (lhs.m_null) {
			return std::strong_ordering::equal;
		}
		if (auto cmp = lhs.m_address <=> rhs.m_address; cmp != 0) {
			return cmp;
		}
		return lhs.m_prefix <=> rhs.m_prefix;
	}

private:
	static constexpr std::uint8_t v4MappedBits = 96;

	static bool prefixFits(const Address &address, std::uint8_t prefix) {
		if (address.isNull()) {
			return false;
		}
		if (address.isV4()) {
			return prefix <= 32;
		}
		return address.isV6() && prefix <= 128;
	}

	void store(const Address &address, std::uint8_t prefix) {
		m_prefix = address.isV4() ? static_cast< std::uint8_t >(prefix + v4MappedBits) : prefix;
		m_null = false;
	}

	Address m_address;
	std::uint8_t m_prefix = 0;
	bool m_null = true;
};
} // namespace ipxx

template <> struct std::hash< ipxx::Subnet > {
	std::size_t operator()(const ipxx::Subnet &subnet) const noexcept {
		if (subnet.isNull()) {
			return 0;
		}
		return std::hash< ipxx::Address >{}(subnet.address()) ^ (static_cast< std::size_t >(subnet.prefix()) << 1);
	}
};

#endif
