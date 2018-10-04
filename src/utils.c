

#include "utils.h"
#include <iostream>
#include <sstream>
#include <cstring>

using namespace std;

unsigned char get_iface(unsigned int id,driver_device_info * supported_devices)
{
	int n = 0;
	unsigned char value=0;
	while(supported_devices[n].id!=0xffffffff)
	{
		if(supported_devices[n].id==id)
		{
			value = supported_devices[n].iface;
			break;
		}
		n++;
	}
	
	return value;
}


void build_path(unsigned int address,unsigned char iface,char * out)
{
	unsigned int bus,dev;
	ostringstream path(ostringstream::out);

	bus=(address & 0x00ff0000)>>16;
	dev=(address & 0x0000ff00)>>8;
	path.fill('0');
	path.width(4);
	path<<hex<<bus<<":";
	path.fill('0');
	path.width(4);
	path<<hex<<dev<<":";
	path.fill('0');
	path.width(2);
	path<<hex<<(int)iface;
	
	strcpy(out,path.str().c_str());
}
