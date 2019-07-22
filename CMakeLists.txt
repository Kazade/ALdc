project(ALdc)


add_library(
	ALdc
	AL/aldc.c
	AL/mojoal.c
	AL/private/converters.c
	AL/private/data_queue.c
	AL/private/device.c
	AL/private/errors.c
)

add_executable(basic samples/basic/main.c)
target_link_libraries(basic ALdc m)
