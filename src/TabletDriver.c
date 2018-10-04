
#include <mrpdi/BaseDriver.h>
#include "utils.h"
#include "hidapi.h"
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
	hid_device * handle;
	unsigned int params[32];

};

void (*pointer_callback) (driver_event);

void * thread_core(void*);
void init_driver(driver_instance_info * info);
void close_driver(driver_instance_info * info);


const char * name="Tablet Driver";
const char * version="2.0-alpha1";

driver_device_info supported_devices [] = 
{
/*vendor-product,interface,type,name*/	
{0x0b8c0083,0x00,0x00,"Smart Slate WS200"},
{0x172f0037,0x00,0x00,"Trust Flex Design"},
{0x172f0501,0x00,0x00,"Silvercrest"},
{0x55430004,0x00,0x00,"Genius Mousepen"},
{0x078c1005,0x00,0x00,"Interwrite Mobi"},
{0xffffffff,0x00,0x00,"EOL"}
};

driver_parameter_info supported_parameters [] = 
{
{0x00000000,"common.debug"},
{0x0b8c0083,"slate.pointers"},
{0x0b8c0083,"slate.pressure"},
{0x0b8c0083,"slate.mapping.key1"},
{0x0b8c0083,"slate.mapping.key2"},
{0x0b8c0083,"slate.mapping.key3"},
{0x172f0037,"flex.pointers"},
{0x172f0037,"flex.pressure"},
{0x172f0501,"silvercrest.pointers"},
{0x55430004,"mousepen.pointers"},
{0x078c1005,"mobi.pointers"},
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

struct t_slate
{
	unsigned int pressure;
	unsigned int pointers;
	unsigned int key1;
	unsigned int key2;
	unsigned int key3;
} slate ;

struct t_flex
{
	unsigned int pointers;
	unsigned int pressure;
} flex ;

struct t_silvercrest
{
	unsigned int pointers;
} silvercrest ;

struct t_mousepen
{
	unsigned int pointers;
} mousepen ;	
	
struct t_mobi
{
	unsigned int pointers;
} mobi ;	
	
/**
* global driver initialization
*/
void init()
{
	
	parameter_map["common.debug"]=&common.debug;
	parameter_map["slate.pointers"]=&slate.pointers;
	parameter_map["slate.pressure"]=&slate.pressure;
	parameter_map["slate.key1"]=&slate.key1;
	parameter_map["slate.key2"]=&slate.key2;
	parameter_map["slate.key3"]=&slate.key3;
	
	parameter_map["flex.pointers"]=&flex.pointers;
	parameter_map["flex.pressure"]=&flex.pressure;
	parameter_map["silvercrest.pointers"]=&silvercrest.pointers;
	parameter_map["mousepen.pointers"]=&mousepen.pointers;
	parameter_map["mobi.pointers"]=&mobi.pointers;
	
	//default values
	common.debug=0;
	
	slate.pointers=2;
	slate.pressure=1;
	flex.pointers=1;
	flex.pressure=1;
	silvercrest.pointers=1;
	mousepen.pointers=1;
	mobi.pointers=1;
}

/**
* global driver shutdown
*/
void shutdown()
{
	if(common.debug)
		cout<<"Shutdown:"<<name<<endl;
	
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
	int errors=0;
	unsigned char buffer[65];
	init_driver(info);
	
	int mx,my,mz;
	int button[6];
	int stylus;
	int key;
	
	while(!info->quit_request)
	{
		//read 64 bytes with a 1000ms timeout
		res = hid_read_timeout(info->handle,buffer,64,1000);
		if(res>0)
		{
			if(common.debug==1)
			{		
				cout<<"*** DATA:"<<hex<<info->id<<":"<<info->address<<" ***"<<endl;
				for(int n=0;n<res;n++)
				{
					cout<<dec<<(unsigned int)buffer[n]<<endl;
				}
				cout<<"***********"<<endl;
			}
			
			
			
			switch(info->id)
			{
				//smart slate ws200
				case 0x0b8c0083:
				
					if(buffer[0]==2)
					{
						key = buffer[7];
						//there is room for improvement here!
						/*
						driver_event event;
						event.id=info->id;
						event.address=info->address;
						event.type=EVENT_KEY;
						event.key.keycode=key;
						event.key.mod=0;
						pointer_callback(event);
						*/
						if ( (key & 0x08) ==0x08)
							cout<<"Key One"<<endl;
							
						if ( (key & 0x10) ==0x10)
							cout<<"Key Middle"<<endl;
							
						if ( (key & 0x20) ==0x20)
							cout<<"Key Two"<<endl;
						 
						
					}
					
					if(buffer[0]==2 && (buffer[1] & 0x90)==0x90)//in range test
					{
						
						mx = (int)(buffer[2]+(buffer[3]<<8));
						my = (int)(buffer[4]+(buffer[5]<<8));
						mz = (int)(buffer[6]+(buffer[7]<<8));
						
						button[0] = buffer[1] & 0x01;
						button[1] = (buffer[1] & 0x02)>>1;
						button[2] = (buffer[1] & 0x04)>>2;
						stylus = (buffer[1] & 0x20)>>5;
						
						//limits
						//17319,10819
						
						driver_event event;
						event.id=info->id;
						event.address=info->address;
						event.type=EVENT_POINTER;
						event.pointer.pointer=0;
						event.pointer.x=(float)mx/17319.0f;
						event.pointer.y=(float)my/10819.0f;
						event.pointer.z=(float)mz/512.0f;
						
						
						if(stylus==0)
						{
							event.pointer.button=button[0] | (button[1]<<1) | (button[2]<<2);
						}
						else
						{
							event.pointer.button=0;
						}					
						
						pointer_callback(event);
					}
				break;
				
				//trust flex design
				case 0x172f0037:
					
					if(buffer[0]==16)//report ID 16
					{
						mx = (int)(buffer[2]+(buffer[3]<<8));
						my = (int)(buffer[4]+(buffer[5]<<8));
						mz = (int)(buffer[6]+(buffer[7]<<8));
						
						button[0] = buffer[1] & 0x01;//tip
						button[1] = (buffer[1] & 0x02)>>1;//barrel
						button[2] = (buffer[1] & 0x04)>>2;//invert
						//todo
						
						//12288,0,9216,0,1023
						driver_event event;
						event.id=info->id;
						event.address=info->address;
						event.type=EVENT_POINTER;
						event.pointer.pointer=0;
						event.pointer.x=(float)mx/12288.0f;
						event.pointer.y=(float)my/9216.0f;
						event.pointer.z=(float)mz/1024.0f;
						
						event.pointer.button=button[0] | (button[1]<<1) | (button[2]<<2);
						
						pointer_callback(event);
						
						
					}
				break;
				
				//silvercrest
				case 0x172f0501:
					if(buffer[0]==16)
					{
						mx = (int)(buffer[2]+(buffer[3]<<8));
						my = (int)(buffer[4]+(buffer[5]<<8));
						mz = (int) ( buffer[6] + (buffer[7]<<8));
									
						//18000,0,11000,0,1023
						
						driver_event event;
						event.id=info->id;
						event.address=info->address;
						event.type=EVENT_POINTER;
						event.pointer.pointer=0;
						event.pointer.x=(float)mx/18000.0f;
						event.pointer.y=(float)my/11000.0f;
						event.pointer.z=(float)mz/1024.0f;
						
						event.pointer.button=0; //ToDo
						
						pointer_callback(event);
					}
				break;
				
				//Genius mousepen
				case 0x55430004:
					if(buffer[0]==9)
					{
						button[0] = buffer[1] & 0x01;//tip
						button[1] = (buffer[1] & 0x02)>>1;//barrel
						mx = (int)(buffer[2]+(buffer[3]<<8));
						my = (int)(buffer[4]+(buffer[5]<<8));
						mz = (int)(buffer[6]+(buffer[7]<<8));
						
						//pressure filter
						button[0]=(mz<23) ? 0 : button[0];
						
						
						//32767,0,32767,0,1023
						
						driver_event event;
						event.id=info->id;
						event.address=info->address;
						event.type=EVENT_POINTER;
						event.pointer.pointer=0;
						event.pointer.x=(float)mx/32767.0f;
						event.pointer.y=(float)my/32767.0f;
						event.pointer.z=(float)mz/1024.0f;
						
						event.pointer.button=button[0] | (button[1]<<1);
						pointer_callback(event);
						
						if(common.debug==1)
						{
							cout<<dec<<"mx:"<<mx<<endl;
							cout<<dec<<"my:"<<my<<endl;
							cout<<dec<<"mz:"<<mz<<endl;
						}
					}
				break;
				
				
				//Interwrite Mobi
				case 0x078c1005:
					//pointing messages comes from report ID 5
					if(buffer[0]==5)
					{
						mx = (int)(buffer[1]+(buffer[2]<<8));
						my = (int)(buffer[3]+(buffer[4]<<8));
						
						button[0] = buffer[5] & 0x01;
						button[1] = (buffer[5] & 0x02)>>1;
						button[2] = (buffer[5] & 0x04)>>2;
					
						
						driver_event event;
						event.id=info->id;
						event.address=info->address;
						event.type=EVENT_POINTER;
						event.pointer.pointer=0;
						event.pointer.x=(float)mx/8000.0f;
						event.pointer.y=(float)my/6000.0f;
												
						event.pointer.button=button[0] | (button[1]<<1) | (button[2]<<2);
						
						pointer_callback(event);
						
					}
				break;
			
			}
			
		}
		if(res<0)
		{
			if(errors==0)
			{
				cerr<<"Error: Failed to read from USB"<<endl;
				driver_event event;
				event.id=info->id;
				event.address=info->address;
				event.type=EVENT_STATUS;
				event.status.id=STATUS_COMMERROR;
				pointer_callback(event);
			}
			
			errors=1;
			
		}
	}
	
	close_driver(info);
	
	return NULL;	
}


/**
* Init specific devices
*/
void init_driver(driver_instance_info * info)
{
	char path[16];
	unsigned char iface;
	
	if(common.debug)
		cout<<"*** init_driver ***"<<endl;
	
	iface = get_iface(info->id,supported_devices);
	build_path(info->address,iface,path);
	info->handle = hid_open_path(path);
	if(common.debug)
		cout<<"usb path:"<<path<<endl;
	
	if(info->handle==NULL)
	{
		cerr<<"Error: Failed to open USB device"<<endl;
		info->quit_request=true;
	}
	else
	{
		
		unsigned char buffer[2];
		
		switch(info->id)
		{
			//smart slate ws200
			case 0x0b8c0083:
				
				buffer[0]=2;
				buffer[1]=2;
				hid_send_feature_report(info->handle,buffer,2);
				/*
				info->pointer = new Stylus("smart slate",info->id,0,17319,0,10819,0,511);
				info->pointer->start();
				
				info->pointer2 = new Stylus("smart slate tool",info->id,0,17319,0,10819,0,511);
				info->pointer2->start();
				* */
								
			break;
			
			//trust flex design
			case 0x172f0037:
				buffer[0]=2;//report ID=2
				buffer[1]=1;//value 1?
				hid_send_feature_report(info->handle,buffer,2);
			/*
				info->pointer = new Stylus("trust flex",info->id,0,12288,0,9216,0,1023);
				info->pointer->start();
				* */
			break;
			
			//silvercrest
			case 0x172f0501:
			/*
				info->pointer = new Stylus("silvercrest",info->id,0,18000,0,11000,0,1023);
				info->pointer->start();
				* */
			break;
			
			//Genius Mousepen
			case 0x55430004:
			/*
				info->pointer = new Stylus("genius mousepen",info->id,0,32767,0,32767,0,1023);
				info->pointer->start();
				* */
			break;
			
			//Interwrite Mobi
			case 0x078c1005:
			/*
				info->pointer = new Stylus("interwrite mobi",info->id,0,8000,0,6000,0,1);
				info->pointer->start();
				* */
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
}

/**
* close device
*/
void close_driver(driver_instance_info * info)
{
	if(common.debug)
		cout<<"*** close_driver ***"<<endl;
	
	
	if(info->handle!=NULL)
	{
		hid_close(info->handle);
		
		switch(info->id)
		{
			//smart slate ws200
			case 0x0b8c0083:
			/*
				info->pointer->stop();
				delete info->pointer;
				
				info->pointer2->stop();
				delete info->pointer2;
				* */
			break;
			
			//trust flex design
			case 0x172f0037:
			/*
				info->pointer->stop();
				delete info->pointer;
				* */
			break;
			
			//silvercrest
			case 0x172f0501:
			/*
				info->pointer->stop();
				delete info->pointer;
				* */
			break;
			
			//genius mousepen
			case 0x55430004:
			/*
				info->pointer->stop();
				delete info->pointer;
				* */
			break;
			
			//interwrite mobi
			case 0x078c1005:
			/*
				info->pointer->stop();
				delete info->pointer;
				* */
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
}


/**
* Sets device parameter value
*/
void set_parameter(const char * key,unsigned int value)
{
	if(common.debug)
		cout<<"[TabletDriver::set_parameter]:"<<value<<endl;
	
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
		cout<<"[TabletDriver::get_parameter]:"<<*value<<endl;
	
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
	if (common.debug)
		cout<<"[TabletDriver] set_callback"<<endl;
	pointer_callback = callback;
}
