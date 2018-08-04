
all: server.cpp client.cpp

	g++ -std=c++17 -Wall -Werror -I/usr/local/include/boost/ -o server server.cpp \
        -lpthread /usr/local/lib/libboost_system.a
	g++ -std=c++17 -Wall -Werror -I/usr/local/include/boost/ -o client client.cpp \
        -lpthread /usr/local/lib/libboost_system.a

server: server.cpp

	g++ -std=c++17 -Wall -Werror -I/usr/local/include/boost/ -o server server.cpp \
        -lpthread /usr/local/lib/libboost_system.a

client: client.cpp

	g++ -std=c++17 -Wall -Werror -I/usr/local/include/boost/ -o client client.cpp \
        -lpthread /usr/local/lib/libboost_system.a
