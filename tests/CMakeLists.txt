# Part of ipxx. See LICENSE.

list(APPEND TESTS
	"Address"
	"Endpoint"
)

foreach(TARGET IN LISTS TESTS)
	add_executable(${TARGET} "${TARGET}.cpp")

	target_link_libraries(${TARGET}
		PRIVATE
			ipxx::ipxx
	)

	add_test(
		NAME ${TARGET}
		COMMAND ${TARGET}
	)
endforeach()
