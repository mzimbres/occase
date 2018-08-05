
all: server.cpp client.cpp

	g++ -std=c++17 -Wall -Werror \
        -I/opt/boost_1_67_0/include/ \
        -I/opt/nlohmann_3_1_2/ \
        -o server server.cpp \
        -lpthread /opt/boost_1_67_0/lib/libboost_system.a
	g++ -std=c++17 -Wall -Werror \
        -I/opt/boost_1_67_0/include/ \
        -I/opt/nlohmann_3_1_2/ \
        -o client client.cpp \
        -lpthread /opt/boost_1_67_0/lib/libboost_system.a

server: server.cpp

	g++ -std=c++17 -Wall -Werror \
        -I/opt/boost_1_67_0/include/ \
        -I/opt/nlohmann_3_1_2/ \
        -lpthread /opt/boost_1_67_0/lib/libboost_system.a \
        -o server server.cpp

client: client.cpp

	g++ -std=c++17 -Wall -Werror \
        -I/opt/boost_1_67_0/include/ \
        -I/opt/nlohmann_3_1_2/ \
        -lpthread /opt/boost_1_67_0/lib/libboost_system.a \
        -o client client.cpp
