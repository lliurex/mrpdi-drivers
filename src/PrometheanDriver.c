

#include <mrpdi/BaseDriver.h>
#include <libusb-1.0/libusb.h>
#include "utils.h"
#include <pthread.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>
#include <map>

using namespace std;

struct driver_instance_info
{
	unsigned int id;
	unsigned int address;
	pthread_t thread;
	bool quit_request;
	libusb_device_handle * handle;
};

void (*pointer_callback) (driver_event);
void * thread_core(void*);
void init_driver(driver_instance_info * info);
void close_driver(driver_instance_info * info);



const char * name="Promethean Driver";
const char * version="2.0-alpha1";

driver_device_info supported_devices [] = 
{
{0x0d480001,0x00,0x01,"Promethean ActiveBoard"},
{0xffffffff,0x00,0x00,"EOL"}
};

driver_parameter_info supported_parameters [] = 
{
{0x00000000,"common.debug"},
{0x0d480001,"activeboard.calibrate"},
{0x0d480001,"activeboard.pointers"},
{0xffffffff,"EOL"}
};


vector<driver_instance_info *> driver_instances;


map<string,unsigned int *> parameter_map;

/**
 * Parameters
 */  

struct t_common
{
	unsigned int debug;
} common ;

struct t_activeboard
{
	unsigned int calibrate;
	unsigned int pointers;
}activeboard;

libusb_context *ctx = NULL;

/**
* global driver initialization
*/
void init()
{
	
	parameter_map["common.debug"]=&common.debug;
	parameter_map["activeboard.calibrate"]=&activeboard.calibrate;
	parameter_map["activeboard.pointers"]=&activeboard.pointers;
	
	//default values
	common.debug=0;
	activeboard.calibrate=1;
	activeboard.pointers=1;
	
	if(libusb_init(&ctx)<0)
	{
		cerr<<"[PrometheanDriver]: Failed to init libusb"<<endl;
	}
	
}

/**
* global driver shutdown
*/
void shutdown()
{
	if(common.debug)
		cout<<"[Promethean] Shutdown:"<<name<<endl;
	libusb_exit(ctx);
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
			cerr<<"Failed to spawn thread"<<endl;
		}

		
		
	}
	else
	{
		cerr<<"driver already loaded!"<<endl;
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
			cout<<"joining to:"<<info->address<<endl;
		pthread_join(info->thread,NULL);
		//once thread is already dead we don't need its instance reference anymore
		delete info;
	}
	else
	{
		cerr<<"driver already unloaded!"<<endl;
	}
	

}


/**
* Thread callback function
*/
void * thread_core(void* param)
{
	driver_instance_info * info = (driver_instance_info *)param;
	int res;
	int length;
	unsigned char buffer[65];
	int mx,my;
	int in_range;
	int button[6];

	init_driver(info);
	
	if(common.debug)
		cout<<"thread_core::enter"<<endl;
		
	while(!info->quit_request)
	{
		res=libusb_interrupt_transfer(info->handle,(1 | LIBUSB_ENDPOINT_IN),buffer,64,&length,1000);
		switch(res)
		{
		
			case 0:
				switch(info->id)
				{
					case 0x0d480001:
						mx = (int)(buffer[3]+(buffer[4]<<8));
						my = (int)(buffer[5]+(buffer[6]<<8));
						in_range = (buffer[7] & 0x04)>>2;
						button[0] = buffer[7] & 0x01;
						button[1] = (buffer[7] & 0x02)>>1;						
						
						if(common.debug==1)
						{
							for(int n=0;n<(length-1);n++)
								cout<<hex<<(int)buffer[n]<<",";
							
							cout<<hex<<(int)buffer[length-1]<<endl;
							
						}
						
						driver_event event;
						event.id=info->id;
						event.address=info->address;
						event.type=EVENT_POINTER;
						event.pointer.pointer=0;
						event.pointer.x=(float)mx/32767.0f;
						event.pointer.y=(float)my/32767.0f;
												
						event.pointer.button=button[0] | (button[1]<<1);
						pointer_callback(event);
						
						
					break;
				}
			break;
			
			case LIBUSB_ERROR_TIMEOUT:
				/*cout<<"[PrometheanDriver]: USB Timeout"<<endl;*/
			
			break;
			
			default:
				cerr<<"[PrometheanDriver]: Unkown USB error"<<endl;
				driver_event event;
				event.id=info->id;
				event.address=info->address;
				event.type=EVENT_STATUS;
				event.status.id=STATUS_COMMERROR;
				pointer_callback(event);
		}
		
		
		
	}
	if(common.debug)
		cout<<"thread_core::exit"<<endl;
	close_driver(info);
	
	return NULL;	
}


/**
* Init specific devices
*/
void init_driver(driver_instance_info * info)
{
	char path[16];
	libusb_device ** list ;
	int n;
	int found=-1;
	unsigned int address; 
	
	if(common.debug)
		cout<<"*** init_driver ***"<<endl;
	
	
	n=libusb_get_device_list(ctx,&list);
	for(int i=0;i<n;i++)
	{
		address = libusb_get_device_address(list[i]);
		address = address | (libusb_get_bus_number(list[i]))<<8;
		address = address<<8;
		
		if(address==info->address)
		{
			found=i;
			break;
		}
	}
	
	if(found>0)
	{
		libusb_open(list[found],&info->handle);
		if(libusb_kernel_driver_active(info->handle, 0) == 1) 
		{
			if(common.debug)
				cout<<"[PrometheanDriver]: Kernel Driver Active"<<endl;
				
			if(libusb_detach_kernel_driver(info->handle, 0) == 0)
				if(common.debug) 
					cout<<"[PrometheanDriver]: Kernel Driver Detached!"<<endl;
		}
		
		if(libusb_claim_interface(info->handle, 0))
		{
			cerr<<"[PrometheanDriver]: Cannot Claim Interface"<<endl;
		}
		else
		{
			if(common.debug)
				cout<<"[PrometheanDriver]: Claimed Interface"<<endl;
		}
	}
	
	libusb_free_device_list(list,1);	
		
	switch(info->id)
	{
		//Promethean Activeboard
		case 0x0d480001:
		/*
				info->pointer = new AbsolutePointer("ActiveBoard",info->id,0,32767,0,32767);
				info->pointer->start();		
				*/ 
		break;
		
		
	
	}

	//Sending ready signal
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
		cout<<"*** close_driver ***"<<endl;
	
	libusb_close(info->handle);
		
	switch(info->id)
	{
		//Promethean Activeboard
		case 0x0d480001:
		/*
			info->pointer->stop();
			delete info->pointer;
			*/ 
		break;
		
		
	}

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
		cout<<"[PrometheanDriver::set_parameter]:"<<value<<endl;
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
		cout<<"[PrometheanDriver::get_parameter]:"<<*value<<endl;
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
		cout<<"[PrometheanDriver] set_callback"<<endl;
		
	pointer_callback = callback;
}
