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
class Address {
public:
	static constexpr std::size_t v4Size = 4;
	static constexpr std::size_t v6Size = 16;

	using V6        = std::array< std::uint8_t, v6Size >;
	using V4        = std::array< std::uint8_t, v4Size >;
	using View      = std::span< std::uint8_t >;
	using ViewConst = std::span< const std::uint8_t >;

	Address() = default;

	static Address anyV4() {
		Address address;
		address.setV4(V4{}.data());
		return address;
	}

	static Address anyV6() {
		Address address;
		address.m_null = false;
		return address;
	}

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

	explicit Address(const sockaddr_in &sa) {
		setV4(reinterpret_cast< const std::uint8_t * >(&sa.sin_addr));
	}

	explicit Address(const sockaddr_in6 &sa) {
		std::memcpy(m_bytes.data(), &sa.sin6_addr, v6Size);
		m_null = false;
		if (!isV4()) {
			m_scope = sa.sin6_scope_id;
		}
	}

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

	// True for a native v4 input and for ::ffff:0:0/96.
	bool isV4() const { return !m_null && isMappedPrefix(); }
	bool isV6() const { return !m_null && !isMappedPrefix(); }
	bool isNull() const { return m_null; }
	bool isWildcard() const {
		if (m_null) {
			return true;
		}
		if (isV4()) {
			return std::all_of(m_bytes.begin() + 12, m_bytes.end(), [](std::uint8_t b) { return b == 0; });
		}
		return std::all_of(m_bytes.begin(), m_bytes.end(), [](std::uint8_t b) { return b == 0; });
	}

	std::uint32_t scope() const { return m_scope; }

	// The 16-byte form. A v4 address occupies the last four bytes, behind the mapped prefix.
	ViewConst v6() const { return m_null ? ViewConst{} : ViewConst(m_bytes); }
	View v6() { return m_null ? View{} : View(m_bytes); }

	// The four address bytes. Empty unless this is a v4 or v4-mapped address, so the prefix cannot be edited from here.
	ViewConst v4() const { return isV4() ? ViewConst(m_bytes.data() + 12, v4Size) : ViewConst{}; }
	View v4() { return isV4() ? View(m_bytes.data() + 12, v4Size) : View{}; }

	std::uint32_t toIPv4() const {
		std::uint32_t value = 0;
		if (isV4()) {
			std::memcpy(&value, m_bytes.data() + 12, v4Size);
		}
		return value;
	}

	// A mapped address is printed as native IPv4. Brackets apply to a real v6 address only.
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

	bool toSockAddr(sockaddr_in &out) const {
		std::memset(&out, 0, sizeof(out));
		if (!isV4()) {
			return false;
		}
		out.sin_family = AF_INET;
		std::memcpy(&out.sin_addr, m_bytes.data() + 12, v4Size);
		return true;
	}

	// A v4 address is written as ::ffff:0:0/96. This is the form a dual-stack socket accepts.
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

	// Prefix match on the 16-byte form, as Mumble uses for ban masks.
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

	void reset() {
		m_bytes.fill(0);
		m_null = true;
		m_scope = 0;
	}

	friend bool operator==(const Address &lhs, const Address &rhs) {
		return lhs.m_null == rhs.m_null && lhs.m_scope == rhs.m_scope && lhs.m_bytes == rhs.m_bytes;
	}

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
	bool isMappedPrefix() const {
		return std::all_of(m_bytes.begin(), m_bytes.begin() + 10, [](std::uint8_t b) { return b == 0; }) && m_bytes[10] == 0xFF
			   && m_bytes[11] == 0xFF;
	}

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
