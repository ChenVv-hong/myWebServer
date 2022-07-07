#include "server/server.h"


int main() {
	char ip[] = "127.0.0.1" ;
	server s(ip, 9999, "localhost", 3306, "root", "ChenJiaHong123!!", "webServer");
	s.start();
	return 0;
}
