
#include <mrpdi/BaseDriver.h>
#include "utils.h"
#include <pthread.h>
#include <stdint.h>
#include <termios.h>
#include <fcntl.h>
#include <cstring>

#include <sstream>
#include <vector>
#include <map>
#include <iostream>
#include <unistd.h>

using namespace std;

struct driver_instance_info
{
	unsigned int id;
	unsigned int address;
	pthread_t thread;
	pthread_t keep_alive_thread;
	bool quit_request;
	bool keep_alive_quit_request;
	int fd;
	int header;
};


void (*pointer_callback) (driver_event);

void * thread_core(void*);
void * keep_alive(void*);

void init_driver(driver_instance_info * info);
void close_driver(driver_instance_info * info);

uint8_t cmd_init=0xCA;
uint8_t cmd_keep_alive=0xCC;

const char * name="MultiClass Driver";
const char * version="2.0-alpha1";

driver_device_info supported_devices [] = 
{
{0x10c4ea60,0x00,0x01,"Multiclass"},
{0xffffffff,0x00,0x00,"EOL"}
};

driver_parameter_info supported_parameters [] = 
{
{0x00000000,"common.debug"},
{0x10c4ea60,"multiclass.pointers"},
{0x10c4ea60,"multiclass.calibrate"},
{0x10c4ea60,"multiclass.tty"},
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

struct t_multiclass
{
	unsigned int pointers;
	unsigned int calibrate;
	unsigned int tty;
}multiclass;


/**
* global driver initialization
*/
void init()
{
	
	parameter_map["common.debug"]=&common.debug;
	parameter_map["multiclass.pointers"]=&multiclass.pointers;
	parameter_map["multiclass.calibrate"]=&multiclass.calibrate;
	parameter_map["multiclass.tty"]=&multiclass.tty;
	
	//default values
	common.debug=0;
	multiclass.pointers=1;
	multiclass.calibrate=1;
	multiclass.tty=0;
	
	if(common.debug)
		cout<<"[MultiClassDriver]: init"<<endl;
	
	
}

/**
* global driver shutdown
*/
void shutdown()
{
	if(common.debug)
		cout<<"[MultiClassDriver] Shutdown:"<<name<<endl;
	
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
		info->keep_alive_quit_request=false;
		driver_instances.push_back(info);
		
		if(pthread_create(&info->thread, NULL,  thread_core, info)!=0)
		{
			cerr<<"[MultiClassDriver] Failed to spawn thread"<<endl;
		}
		
		if(pthread_create(&info->keep_alive_thread, NULL, keep_alive, info)!=0)
		{
			cerr<<"[MultiClassDriver] Failed to spawn keep alive thread"<<endl;
		}
		
		
	}
	else
	{
		cerr<<"[MultiClassDriver] driver already loaded!"<<endl;
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
			cout<<"[MultiClassDriver] joining to:"<<info->address<<endl;
		pthread_join(info->thread,NULL);
		pthread_join(info->keep_alive_thread,NULL);
		
		//once thread is already dead we don't need its instance reference anymore
		delete info;
	}
	else
	{
		cerr<<"[MultiClassDriver] driver already unloaded!"<<endl;
	}
	

}

/**
 * Keep alive thread
 */ 
void * keep_alive(void* param)
{
	driver_instance_info * info = (driver_instance_info *)param;
	int res;
	
	if(common.debug)
		cout<<"[MultiClassDriver] keep_alive enter"<<endl;
	
	while(!info->keep_alive_quit_request)
	{
		if(info->header==1)
		{
			res=write(info->fd,&cmd_keep_alive,1);
			if(res<=0)cout<<"[MultiClassDriver]: Error sending keep alive message!"<<endl;
		}	
	
		sleep(1);
	}
	
	if(common.debug)
		cout<<"[MultiClassDriver] keep_alive exit"<<endl;
}

/**
* Thread callback function
*/
void * thread_core(void* param)
{
	driver_instance_info * info = (driver_instance_info *)param;
	int res;
	uint8_t buffer[32];
	uint8_t pBuffer=0;
	uint8_t checksum;
	
	int x,y,lx,ly;

	init_driver(info);
	
	if(common.debug)
		cout<<"[MultiClassDriver] thread_core::enter"<<endl;
		
	while(!info->quit_request)
	{
		
		res = read(info->fd,&(buffer[pBuffer]),1);
		if(res>0)
		{
			//reached 8-bytes
			if(pBuffer==7)
			{
				if(buffer[0]==0xA8)
				{
					if(common.debug)
						cout<<"* header message, welcome Multiclass! ^_^"<<endl;
					info->header=1;//mark header status as received
				}
				
				if(buffer[0]==0xAA && buffer[1]==0xAA)
				{
					
					y = (buffer[3]<<6) | buffer[4];
					x = (buffer[5]<<6) | buffer[6];
					
					checksum = buffer[2] ^ buffer[3] ^ buffer[4] ^ buffer[5] ^ buffer[6];
					
					//checksum validation
					if(buffer[7]==checksum)
					{
						//Press
						if(buffer[2]==0x41)
						{						
						
							driver_event event;
							event.id=info->id;
							event.address=info->address;
							event.type=EVENT_POINTER;
							event.pointer.button= 1;
							event.pointer.pointer=0;
							event.pointer.x=(float)x/4096.0f;
							event.pointer.y=(float)y/4096.0f;
							pointer_callback(event);
							
							//last good known coords
							lx=x;
							ly=y;
							
						}
						else
						{
							//Release
							driver_event event;
							event.id=info->id;
							event.address=info->address;
							event.type=EVENT_POINTER;
							event.pointer.button=0;
							event.pointer.pointer=0;
							event.pointer.x=(float)lx/4096.0f;
							event.pointer.y=(float)ly/4096.0f;
							pointer_callback(event);
							
						}
					}
					else
					{
						cout<<"[MultiClassBoard]: Checksum error"<<endl;
					}	
			
				}
				
				pBuffer=0;
			}
			else
			{
				pBuffer++;
			}
		}
		else
		{
			/*
			 * If there is no data we just wait for a 1/10 second in order to avoid a full idle loop
			 * This cause a small latency after each idle state. There is some room for improvement here
			 */
			if(res==0)
			{ 
				usleep(100000);//100ms
				cout<<"res==0"<<endl;
			} 
			else
			{
				usleep(10000);//10ms
				//sending error event
				/*
				driver_event event;
				event.id=info->id;
				event.address=info->address;
				event.type=EVENT_STATUS;
				event.status.id=STATUS_COMMERROR;
				pointer_callback(event);
				*/ 
			}
			//cout<<"Error reading..."<<endl;
		}

	}
	
	//turn off keep_alive thread
	info->keep_alive_quit_request=true;
		
	if(common.debug)
		cout<<"[MultiClassDriver] thread_core::exit"<<endl;
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
	int res;
	stringstream ss;
	
	if(common.debug)
		cout<<"[MultiClassDriver]init_driver"<<endl;
	
	ss<<"/dev/ttyUSB"<<multiclass.tty;
	info->fd = open(ss.str().c_str(),O_RDWR | O_NOCTTY | O_NONBLOCK);
	fcntl(info->fd, F_SETFL, FNDELAY); //set non-blocking reads
	if(common.debug)
		cout<<"status:"<<info->fd<<endl;
		
    tcgetattr(info->fd, &options);
    cfsetispeed(&options, B9600);
    cfsetospeed(&options, B9600);
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;
	cfmakeraw(&options);
    tcsetattr(info->fd, TCSANOW, &options);
    	
	
	//from what I tested, I should wait at least one second before start reading the board
	if(common.debug)
		cout<<"[MultiClassDriver] Waiting"<<endl;
	sleep(1);
	if(common.debug)
		cout<<"[MultiClassDriver] Initialiazing...";
	
	res=write(info->fd,&cmd_init,1);
	if(common.debug)
	{
		if(res>0)cout<<"done"<<endl;
			else cout<<"fail"<<endl;
	}
		
	//sending ready event
	driver_event event;
	event.id=info->id;
	event.address=info->address;
	event.type=EVENT_STATUS;
	event.status.id=STATUS_READY;
	pointer_callback(event);
	

	info->header=0;
}

/**
* close device
*/
void close_driver(driver_instance_info * info)
{
	if(common.debug)
		cout<<"[MultiClassDriver] close_driver"<<endl;
		

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
		cout<<"[MultiClassDriver] set_parameter:"<<value<<endl;
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
		cout<<"[MultiClassDriver] get_parameter:"<<*value<<endl;
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
		cout<<"[MultiClassDriver] set_callback"<<endl;
	pointer_callback = callback;
}
