all: serv client

server: server.cpp payload.pb.cc
	g++ -o server server.cpp mensaje.pb.cc -lpthread -lprotobuf
client: client.cpp payload.pb.cc
	g++ -o server server.cpp mensaje.pb.cc -lpthread -lprotobuf
	g++ -o client client.cpp mensaje.pb.cc -lpthread -lprotobuf
payload.pb.cc: payload.proto
	protoc -I=. --cpp_output=. payload.proto