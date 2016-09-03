/*
 * main.cpp
 *
 *  Created on: Oct 15, 2014
 *      Author: niccom
 */

#include "SpeedMeasure.h"
#include "Inih/INIReader.h"

int main(int argc, char* argv[])
{
	std::string iniFilename;

	if (argv[1])
	{
		iniFilename = argv[1];
	}
	else
	{
		std::string iniFilename = "3gspeed.ini";

	}
	printf("2\n");
	SpeedMeasure testKp(iniFilename);

	testKp.startMeasure();

	return 1;
}

