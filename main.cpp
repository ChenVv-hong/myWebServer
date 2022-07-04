#include "server/server.h"


int main() {
	server s("127.0.0.1", 9999);
	s.start();
	return 0;
}
