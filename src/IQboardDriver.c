
#include <mrpdi/BaseDriver.h>
#include "utils.h"
#include <pthread.h>
#include <stdint.h>
#include <termios.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <unistd.h>

#define MIN_X	440
#define MIN_Y	632
#define MAX_X	3537
#define MAX_Y	3270

using namespace std;

struct driver_instance_info
{
	unsigned int id;
	unsigned int address;
	pthread_t thread;
	bool quit_request;
	int fd;
	int wait;
};


void (*pointer_callback) (driver_event);

void * thread_core(void*);
void init_driver(driver_instance_info * info);
void close_driver(driver_instance_info * info);



const char * name="IQBoard Driver";
const char * version="2.0-alpha1";

driver_device_info supported_devices [] = 
{
{0x10c4ea60,0x00,0x01,"IQBoard"},
{0xffffffff,0x00,0x00,"EOL"}
};

driver_parameter_info supported_parameters [] = 
{
{0x00000000,"common.debug"},
{0x10c4ea60,"iqboard.pointers"},
{0x10c4ea60,"iqboard.calibrate"},
{0x10c4ea60,"iqboard.tty"},
{0xffffffff,"EOL"}
};


vector<driver_instance_info *> driver_instances;

map<string,unsigned int *> parameter_map;


uint8_t iqboard_header[]={0xce,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

/**
 * Parameters
 */  

struct t_common
{
	unsigned int debug;
} common ;

struct t_iqboard
{
	unsigned int pointers;
	unsigned int calibrate;
	unsigned int tty;
}iqboard;


/**
* global driver initialization
*/
void init()
{
	
	parameter_map["common.debug"]=&common.debug;
	parameter_map["iqboard.pointers"]=&iqboard.pointers;
	parameter_map["iqboard.calibrate"]=&iqboard.calibrate;
	parameter_map["iqboard.tty"]=&iqboard.tty;
	
	//default values
	common.debug=0;
	iqboard.pointers=1;
	iqboard.calibrate=1;
	iqboard.tty=0;
	
	if(common.debug)
		cout<<"[IQboardDriver]: init"<<endl;
	
	
}

/**
* global driver shutdown
*/
void shutdown()
{
	if(common.debug)
		cout<<"[IQboardDriver] Shutdown:"<<name<<endl;
	
}

/**
* device start up
*/
void start(unsigned int id,unsigned int address)
{
	bool found=false;
	driver_instance_info * info;
	char path[16];
	
	
	for(int n=0;n<driver_instances.size();n++)
	{
		if(driver_instances[n]->id==id && driver_instances[n]->address==address)
		{
			found=true;
			break;
		}
	}
	
	if(!found)
	{
		if(common.debug)
			cout<<"start:"<<name<<" device:"<<hex<<id<<":"<<address<<endl;
						
		info = new driver_instance_info;
		info->id=id;
		info->address=address;
		info->quit_request=false;
		driver_instances.push_back(info);
		
		if(pthread_create(&info->thread, NULL,  thread_core, info)!=0)
		{
			cerr<<"[IQboardDriver] Failed to spawn thread"<<endl;
		}

		
		
	}
	else
	{
		cerr<<"[IQboardDriver] driver already loaded!"<<endl;
	}
	

}

/**
* device shut down
*/
void stop(unsigned int id,unsigned int address)
{
	bool found=false;
	vector<driver_instance_info *> tmp;
	driver_instance_info * info;
	
	
	for(int n=0;n<driver_instances.size();n++)
	{

		if(driver_instances[n]->id==id && driver_instances[n]->address==address)
		{
			found=true;
			info=driver_instances[n];
		}
		else
		{
			tmp.push_back(driver_instances[n]);
		}
	}
	
	if(found)
	{
		
		driver_instances=tmp;
		
		if(common.debug)
			cout<<"stop:"<<name<<" device:"<<hex<<id<<":"<<address<<endl;
		info->quit_request=true;
		if(common.debug)
			cout<<"[IQboardDriver] joining to:"<<info->address<<endl;
		pthread_join(info->thread,NULL);
		//once thread is already dead we don't need its instance reference anymore
		delete info;
	}
	else
	{
		cerr<<"[IQboardDriver] driver already unloaded!"<<endl;
	}
	

}


/**
* Thread callback function
*/
void * thread_core(void* param)
{
	driver_instance_info * info = (driver_instance_info *)param;
	int res;
	uint8_t buffer[8];
	uint8_t checksum;

	init_driver(info);
	
	if(common.debug)
		cout<<"[IQboardDriver] thread_core::enter"<<endl;
		
	while(!info->quit_request)
	{
		if(info->wait==0)
		{
			res = write(info->fd,iqboard_header,8);
			if(res<0)
				cerr<<"[IQboardDriver] failed to send data"<<endl;
			else
			{
				//cout<<"[IQboardDriver] HEADER sent"<<endl;
				info->wait=1;
			}
		}
		
		
		res = read(info->fd,buffer,8);
		if(res>0)
		{
			checksum = buffer[0] ^ buffer[1] ^ buffer[2] ^ buffer[3] ^ buffer[4] ^ buffer[5] ^ buffer[6];
			if(checksum==buffer[7])
			{
				if(buffer[0]==0xee && buffer[1]==0xee)
				{
					//cout<<"[IQboardDriver] Position received"<<endl;
					int x = ((buffer[5] & 0x3F) << 6) | (buffer[6] & 0x3F);
					int y = ((buffer[3] & 0x3F) << 6) | (buffer[4] & 0x3F);
					
					int width = MAX_X - MIN_X;
					int height = MAX_Y - MIN_Y;
					x-=MIN_X;
					y-=MIN_Y;
					//cout<<dec<<"IQDebug:"<<x<<","<<y<<endl;
					
					driver_event event;
					event.id=info->id;
					event.address=info->address;
					event.type=EVENT_POINTER;
					event.pointer.button= (buffer[2]==0x51) ? 1 : 0;
					event.pointer.pointer=0;
					event.pointer.x= (float)x/(float)width;
					event.pointer.y=(float)y/(float)height;
					pointer_callback(event);
					info->wait=0;
				}
				
				if(buffer[0]==0xc8 && buffer[1]==0xca)
				{
					//cout<<"[IQboardDriver] ACK received"<<endl;
					info->wait=0;
				}
			}
			else
			{
				cerr<<"[IQboardDriver] Bad checksum"<<endl;
				
				//sending error event
				driver_event event;
				event.id=info->id;
				event.address=info->address;
				event.type=EVENT_STATUS;
				event.status.id=STATUS_COMMERROR;
				pointer_callback(event);
			}
			
			
		}
		else
		{
			cerr<<"[IQboardDriver] failed to receive data"<<endl;
			//sending error event
			driver_event event;
			event.id=info->id;
			event.address=info->address;
			event.type=EVENT_STATUS;
			event.status.id=STATUS_COMMERROR;
			pointer_callback(event);
		}
		
		
		usleep(500);
	}
		
	if(common.debug)
		cout<<"[IQboardDriver] thread_core::exit"<<endl;
	close_driver(info);
	
	return NULL;	
}


/**
* Init specific devices
*/
void init_driver(driver_instance_info * info)
{
	char path[16];
	
	int n;
	int found=-1;
	unsigned int address; 
	struct termios options;
	stringstream ss;
	
	if(common.debug)
		cout<<"[IQboardDriver]init_driver"<<endl;
	
	ss<<"/dev/ttyUSB"<<iqboard.tty;
	info->fd = open(ss.str().c_str(),O_RDWR | O_NOCTTY | O_NONBLOCK);
	fcntl(info->fd, F_SETFL, 0);
	cout<<"status:"<<info->fd<<endl;
    tcgetattr(info->fd, &options);
    cfsetispeed(&options, B19200);
    cfsetospeed(&options, B19200);
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;
	cfmakeraw(&options);
    tcsetattr(info->fd, TCSANOW, &options);
    
	info->wait=0;
	
	//sending ready event
	driver_event event;
	event.id=info->id;
	event.address=info->address;
	event.type=EVENT_STATUS;
	event.status.id=STATUS_READY;
	pointer_callback(event);
}

/**
* close device
*/
void close_driver(driver_instance_info * info)
{
	if(common.debug)
		cout<<"[IQboardDriver] close_driver"<<endl;
		

	close(info->fd);
	
	//sending shutdown event
	driver_event event;
	event.id=info->id;
	event.address=info->address;
	event.type=EVENT_STATUS;
	event.status.id=STATUS_SHUTDOWN;
	pointer_callback(event);
}


/**
* Sets device parameter value
*/
void set_parameter(const char * key,unsigned int value)
{
	if(common.debug)
		cout<<"[IQboardDriver] set_parameter:"<<value<<endl;
	*(parameter_map[key])=value;
}

/**
* Gets device parameter value
*/
int get_parameter(const char * key,unsigned int * value)
{
	map<string,unsigned int *>::iterator it;
	
	it=parameter_map.find(key);
	
	if(it==parameter_map.end())return -1;
	
	*value=*parameter_map[key];
	
	if(common.debug)
		cout<<"[IQboardDriver] get_parameter:"<<*value<<endl;
	return 0;
}

/**
 * Gets device status 
 */ 
unsigned int get_status(unsigned int address)
{
	unsigned int ret = DEV_STATUS_STOP;
	
	for(int n=0;n<driver_instances.size();n++)
	{
		if(driver_instances[n]->address==address)
		{
			ret=DEV_STATUS_RUNNING;
			break;
		}
	}
	
	return ret;
}


void set_callback( void(*callback)(driver_event) )
{
	if(common.debug)
		cout<<"[IQboardDriver] set_callback"<<endl;
	pointer_callback = callback;
}
