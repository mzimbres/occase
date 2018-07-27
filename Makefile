
all: server.cpp client.cpp

	g++ -std=c++17 -I/cygdrive/d/local/boost_1_67_0 -o server server.cpp
	g++ -std=c++17 -I/cygdrive/d/local/boost_1_67_0 -o client client.cpp

