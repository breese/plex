cmake_minimum_required(VERSION 2.8)
project(plex-example)

include_directories(".")

# Async examples

add_executable(async_echo_server
  async_echo_server.cpp
)
add_dependencies(async_echo_server plex)
target_link_libraries(async_echo_server plex ${EXTRA_LIBS})

add_executable(async_echo_client
  async_echo_client.cpp
)
add_dependencies(async_echo_client plex)
target_link_libraries(async_echo_client plex ${EXTRA_LIBS})

# Future examples

add_executable(future_echo_client
  future_echo_client.cpp
)
add_dependencies(future_echo_client plex)
target_link_libraries(future_echo_client plex ${EXTRA_LIBS})
