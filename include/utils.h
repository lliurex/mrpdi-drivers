
#ifndef _UTILS_
#define _UTILS_

#include <mrpdi/BaseDriver.h>

unsigned char get_iface(unsigned int id,driver_device_info * supported_devices);
void build_path(unsigned int address,unsigned char iface,char * out);


#endif
