
all: server.cpp client.cpp

	g++ -std=c++17 -Wall -Werror -I/opt/boost_1_67_0/include/boost/ -o server server.cpp \
        -lpthread /usr/local/lib/libboost_system.a
	g++ -std=c++17 -Wall -Werror -I/opt/boost_1_67_0/include/boost/ -o client client.cpp \
        -lpthread /opt/boost_1_67_0/lib/libboost_system.a

server: server.cpp

	g++ -std=c++17 -Wall -Werror -I/opt/boost_1_67_0/include/boost/ -o server server.cpp \
        -lpthread /opt/boost_1_67_0/lib/libboost_system.a

client: client.cpp

	g++ -std=c++17 -Wall -Werror -I/opt/boost_1_67_0/include/boost/ -o client client.cpp \
        -lpthread /opt/boost_1_67_0/lib/libboost_system.a
