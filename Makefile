
server: server.cpp

	g++ -std=c++17 -Wall -Werror -g -ggdb3 \
        -I/opt/boost_1_67_0/include/ \
        -I/opt/nlohmann_3_1_2/ \
        -o server server.cpp \
        -lpthread /opt/boost_1_67_0/lib/libboost_system.a

client: client.cpp

	g++ -std=c++17 -Wall -Werror -g -ggdb3 \
        -I/opt/boost_1_67_0/include/ \
        -I/opt/nlohmann_3_1_2/ \
        -o client client.cpp \
        -lpthread /opt/boost_1_67_0/lib/libboost_system.a

all: client server

