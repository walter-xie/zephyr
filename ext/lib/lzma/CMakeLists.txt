zephyr_interface_library_named(LZMA)

target_include_directories(LZMA INTERFACE src)

zephyr_library()
zephyr_library_sources(
    src/LzmaDec.c
	src/LzmaUtil.c
)

zephyr_library_link_libraries(LZMA)
target_link_libraries(LZMA INTERFACE zephyr_interface)
