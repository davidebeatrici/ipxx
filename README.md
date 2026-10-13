# ipxx

Header-only C++20 library for IP addresses, endpoints, and subnets.

An IPv4 address is stored as an IPv4-mapped IPv6 address, `::ffff:0:0/96`. A mapped address is an IPv4 address: `isV4()` is true, `text()` prints dotted decimal, and it compares equal to the same address constructed from four bytes. That is the form a dual-stack socket accepts, so one 16-byte value covers both families.

```cpp
ipxx::Address("192.0.2.10") == ipxx::Address("::ffff:192.0.2.10");
ipxx::Address("192.0.2.10").text(); // "192.0.2.10"
```

## Types

`Address` is a host. It parses text, raw bytes, and `sockaddr`, and writes back to all three. A failed parse leaves it null. Predicates classify it (`isLoopback`, `isMulticast`, `isLinkLocal`, `isBroadcast`, `isPrivate`, `isShared`, `isDocumentation`), and `isGlobal()` is their complement, not a claim that the address is publicly routable. `multicastScope()` reports the scope of a multicast address. A mapped address is classified as the IPv4 address it contains.

`Endpoint` is an `Address` plus a port. `text()` brackets a real IPv6 address so the port separator stays unambiguous: `[2001:db8::1]:443`, but `192.0.2.10:64738`.

`Subnet` is an `Address` plus a prefix. The caller writes the address's own width, 0 to 32 for IPv4 and 0 to 128 for IPv6. It is stored on the 16-byte form, so a v4 `/24` is 120, and `prefix()` hands 24 back. `contains` takes an address or another subnet. A subnet contains another when the other is the same network or a more specific one inside it.

```cpp
ipxx::Subnet subnet("192.0.2.0/24");
subnet.contains(ipxx::Address("192.0.2.10"));
subnet.contains(ipxx::Subnet("192.0.2.128/25"));
subnet.prefix(); // 24
```

An illegal prefix leaves the subnet null. Switching the address to the other family keeps the prefix the caller sees: a `/24` moved from IPv4 to IPv6 is stored as 24, not left as 120. A v6 `/64` cannot move to IPv4, so that call fails and the subnet is left as it was.

## Use

C++20. CMake 3.16 or newer.

```cmake
add_subdirectory(ipxx)
target_link_libraries(app PRIVATE ipxx::ipxx)
```

Or, after installing:

```cmake
find_package(ipxx REQUIRED)
target_link_libraries(app PRIVATE ipxx::ipxx)
```

```cpp
#include "ipxx/Address.hpp"
#include "ipxx/Endpoint.hpp"
#include "ipxx/Subnet.hpp"
```

On Windows the CMake target links `ws2_32`. The headers call `inet_pton` and `inet_ntop`, and MinGW does not honor the `#pragma comment(lib, "ws2_32.lib")` in the Windows SDK headers. If you include the headers without the CMake target, link `ws2_32` yourself.

## Build

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Tests are on by default. `-DIPXX_BUILD_TESTS=OFF` skips them.

## License

[MIT](LICENSE).
