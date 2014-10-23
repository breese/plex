cmake_minimum_required(VERSION 2.8)
project(plex-example)

include_directories(".")

add_executable(udpserver
  server.cpp
)
add_dependencies(udpserver plex)
target_link_libraries(udpserver plex ${EXTRA_LIBS})

add_executable(udpclient
  client.cpp
)
add_dependencies(udpclient plex)
target_link_libraries(udpclient plex ${EXTRA_LIBS})
