#include "Main.h"

#include <iostream>

int main(int argc, char** argv) {
	printf("Starting...\n");

	int code = InitService();

	if (code != 0) {
		printf("Error while starting.");
		FinalizeService();
		return -1;
	}

	printf("Started. Accepting connection...\n");

	AppMain();

	FinalizeService();

	return 0;
}