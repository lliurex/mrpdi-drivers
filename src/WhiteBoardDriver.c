
#include <mrpdi/BaseDriver.h>
#include "utils.h"
#include "hidapi.h"
#include <pthread.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <cmath>

using namespace std;


struct driver_instance_info
{
	unsigned int id;
	unsigned int address;
	pthread_t thread;
	bool quit_request;
	hid_device * handle;
	
	union
	{
		struct
		{
			int right_click;
			int pen_status;
			int pen_selected;
		}smart;
		
		struct
		{
			unsigned int mx;
			unsigned int my;
			unsigned int button;
		}ebeam;
	};
};


void (*pointer_callback) (driver_event);
void * thread_core(void*);
void init_driver(driver_instance_info * info);
void close_driver(driver_instance_info * info);


const char * name="WhiteBoard Driver";
const char * version="2.1";

driver_device_info supported_devices [] = 
{
/*vendor-product,interface,type,name*/	
{0x26501311,0x00,0x01,"eBeam Classic"},
{0x0b8c0001,0x00,0x01,"Smart Board"},
{0x07dd0001,0x00,0x01,"Team Board"},
{0xffffffff,0x00,0x00,"EOL"}
};

driver_parameter_info supported_parameters [] = 
{
{0x00000000,"common.debug"},
{0x07dd0001,"teamboard.calibrate"},
{0x07dd0001,"teamboard.pointers"},
{0x26500000,"ebeam.filter"},
{0x26500000,"ebeam.fiability"},
{0x26500000,"ebeam.min_dist"},
{0x26500000,"ebeam.max_dist"},
{0x26500000,"ebeam.calibrate"},
{0x26500000,"ebeam.pointers"},
{0x0b8c0001,"smart.calibrate"},
{0x0b8c0001,"smart.pointers"},
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
} common;

struct t_ebeam
{
	unsigned int filter;
	unsigned int fiability;
	unsigned int min_dist;
	unsigned int max_dist;
	unsigned int pointers;
	unsigned int calibrate;
} ebeam;

struct t_smart
{
	unsigned int pointers;
	unsigned int calibrate;
}smart;

struct t_teamboard
{
	unsigned int pointers;
	unsigned int calibrate;
}teamboard;

struct t_panaboard
{
	unsigned int pointers;
	unsigned int calibrate;
}panaboard;

//useful vars
unsigned char smart_pen_lights[][2]={{0x00,0x2a},{0x01,0x2b},{0x02,0x28},{0x04,0x2e},{0x08,0x22},{0x10,0x3a}};



void smart_set_lights(driver_instance_info * info,unsigned char status,unsigned char lights)
{
	unsigned char buffer_out[17];
	
	//mid-hacked response
	buffer_out[0]=0x02;

	buffer_out[1]=0xd2;
	buffer_out[2]=0x07;
	buffer_out[3]=0xff;
	buffer_out[4]=status;
	
	buffer_out[5]=lights;
	buffer_out[6]=0x00;
	buffer_out[7]=0x00;
	buffer_out[8]=0x00;
	
	buffer_out[9]=0x00;
	buffer_out[10]=0x00;
	buffer_out[11]=0x00;
	buffer_out[12]=0x00;

	buffer_out[13]=0x00;
	buffer_out[14]=0x00;
	buffer_out[15]=0x00;
	buffer_out[16]=0x00;
	
	hid_write(info->handle,buffer_out,17);

}

/**
* global driver initialization
*/
void init()
{
	
	parameter_map["common.debug"]=&common.debug;
	parameter_map["ebeam.filter"]=&ebeam.filter;
	parameter_map["ebeam.fiability"]=&ebeam.fiability;
	parameter_map["ebeam.min_dist"]=&ebeam.min_dist;
	parameter_map["ebeam.max_dist"]=&ebeam.max_dist;
	parameter_map["ebeam.pointers"]=&ebeam.pointers;
	parameter_map["ebeam.calibrate"]=&ebeam.calibrate;
	
	
	parameter_map["smart.calibrate"]=&smart.calibrate;
	parameter_map["smart.pointers"]=&smart.pointers;
	
	
	parameter_map["teamboard.calibrate"]=&teamboard.calibrate;
	parameter_map["teamboard.pointers"]=&teamboard.pointers;
	
	parameter_map["panaboard.pointers"]=&panaboard.pointers;
	parameter_map["panaboard.calibrate"]=&panaboard.calibrate;
	
	//default values
	common.debug=0;
	
	ebeam.pointers=1;
	ebeam.calibrate=1;
	ebeam.filter=1;
	ebeam.fiability=100;
	ebeam.min_dist=8;
	ebeam.max_dist=40;
	
	smart.pointers=6;
	smart.calibrate=1;
	
	teamboard.pointers=1;
	teamboard.calibrate=1;
	
	panaboard.pointers=1;
	panaboard.calibrate=0;
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
	unsigned char buffer_out[32];
	int mx,my;
	int button[6];
	
	init_driver(info);
	
	
	while(!info->quit_request)
	{
		//read 64 bytes with a 1000ms timeout
		res = hid_read_timeout(info->handle,buffer,64,1000);
		if(res>0)
		{
			if(common.debug==2)
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
				
				
				
				//eBeam Classic
				case 0x26501311:
					if(buffer[0]==0x03 && buffer[5]>ebeam.fiability)
					{
						mx = (int)(buffer[1]+(buffer[2]<<8));
						my = (int)(buffer[3]+(buffer[4]<<8));
												
						
						button[0] = ((~buffer[6]) & 0x01);
						button[1] = (buffer[6] & 0x08)>>3;
						button[2] = (buffer[6] & 0x04)>>2;
						
						bool init_press = (button[0]==1 && info->ebeam.button==0) ? true : false;
						
						if(init_press)
						{
							info->ebeam.mx=mx;
							info->ebeam.my=my;
						}
						
						int vx = info->ebeam.mx - mx;
						int vy = info->ebeam.my - my;
						
						float dist = sqrtf( (vx*vx) + (vy*vy) );
												
						
						
						info->ebeam.button = button[0];
						
						
						
						if(dist<ebeam.max_dist)
						{
							
								if(ebeam.filter==1)
								{
									mx = (info->ebeam.mx + mx)*0.5f;
									my = (info->ebeam.my + my)*0.5f;
								}
								
								driver_event event;
								event.id=info->id;
								event.address=info->address;
								event.type=EVENT_POINTER;
								event.pointer.pointer=0;
								event.pointer.x=(float)mx/16384.0f;
								event.pointer.y=(float)my/16384.0f;
														
								event.pointer.button=button[0] | (button[1]<<1) | (button[2]<<2);
								
								pointer_callback(event);
							
						}
						else
						{
							if(common.debug)
								cout<<"Distance too long, aborting movement. Check battery."<<endl;
						}
						
						info->ebeam.mx=mx;
						info->ebeam.my=my;
						
						if(common.debug)
						{
							cout<<dec<<"fiability: "<<(int)buffer[5]<<endl;
							cout<<dec<<"pointer id: "<<(int)( (buffer[6] & 0xf0)>>4 )<<endl;
							cout<<dec<<"buffer 7: "<<(int)buffer[7]<<endl;
						}
					}
					else
					{
						if(common.debug)
							cout<<dec<<"fiability: "<<(int)buffer[5]<<endl;
						
					}						
					
					
				break;
				
				
				//Smart Board
				case 0x0b8c0001:
					
					//seems that smart uses report id 02
					if(buffer[0]==0x02)
					{
						//command
						switch(buffer[1])
						{
							//watch dog and status
							case 0xd2:
								if(common.debug)
									cout<<"-> cmd: 0xd2:"<<hex<<(int)buffer[2]<<endl;
								
								if(buffer[2]==0)
								{
									//We don't fully understand this command
									//so we answer in a very hacked way
									buffer_out[0]=0x02;
									
									buffer_out[1]=0xe1;
									buffer_out[2]=0x00;
									buffer_out[3]=0x01;
									buffer_out[4]=0xe0;
									
									hid_write(info->handle,buffer_out,17);
									
								}else 
								{
									if(common.debug)
									{
										cout<<"Unknown D2 param:"<<hex<<(int)buffer[2]<<endl;
										for(int n=0;n<res;n++)
											cout<<hex<<(int)buffer[n]<<" ";
										cout<<endl;
									}
								}
							break;
							
							//input coords
							case 0xb4:
								if(common.debug)
									cout<<"-> cmd: 0xb4:"<<hex<<(int)buffer[2]<<endl;
								
								if(buffer[2]==1)
								{
									if(common.debug)
									{
										cout<<"Unknown b4 param:"<<hex<<(int)buffer[2]<<endl;
										for(int n=0;n<res;n++)
											cout<<hex<<(int)buffer[n]<<" ";
										cout<<endl;
									}
								}
								
								//XY datagram
								if(buffer[2]==4)
								{
									button[0]=(buffer[3] & 0x80)>>7;
									
									//cout<<"input:";
									//cout<<hex<<(int)buffer[4]<<","<<(int)buffer[5]<<","<<(int)buffer[6]<<","<<(int)buffer[7]<<endl;
									//cout<<"Click:"<<button[0]<<endl;
									mx = (int)(buffer[4]+( (buffer[5] & 0xF0 )<<4));
									my = (int)(buffer[6]+( (buffer[5] & 0x0F )<<8));
										
													
									driver_event event;
									event.id=info->id;
									event.address=info->address;
									event.type=EVENT_POINTER;
									event.pointer.pointer=info->smart.pen_selected;
									event.pointer.x=(float)mx/4096.0f;
									event.pointer.y=(float)my/4096.0f;
									
									if(info->smart.right_click==1 && button[0]==1)
										event.pointer.button=0x02; 
									else 
										event.pointer.button=button[0]; 
									
									pointer_callback(event);																				
									
										
								}
								
								
							break;
							
							//pen board status
							case 0xe1:
								if(common.debug)
									cout<<"-> cmd: 0xe1:"<<hex<<(int)buffer[2]<<endl;
								
								//unknown datagram
								if(buffer[2]==0x14)
								{
									if(common.debug)
									{
										cout<<"Unknown b4 param:"<<hex<<(int)buffer[2]<<endl;
										for(int n=0;n<res;n++)
											cout<<hex<<(int)buffer[n]<<" ";
										cout<<endl;
									}
								}
								
								
								//pen status datagram
								if(buffer[2]==5)
								{
									info->smart.pen_status=buffer[3];
									
									if(common.debug)
										cout<<"status:"<<hex<<(int)buffer[3]<<endl;
																		
									for(int n=0;n<6;n++)
									{
											if(smart_pen_lights[n][0]==buffer[3])
											{
												info->smart.pen_selected=n;
												//buffer_out[5]=smart_pen_lights[n][1];
											}
									}
									
									if(common.debug)
										cout<<"lighting:"<<hex<<(int)smart_pen_lights[info->smart.pen_selected][1]<<endl;
									
									smart_set_lights(info,buffer[3],smart_pen_lights[info->smart.pen_selected][1]);
									
									driver_event event;
									event.id=info->id;
									event.address=info->address;
									event.type=EVENT_DATA;
									event.data.type=1;//pen selected
									*((unsigned int *)event.data.buffer)=(unsigned int)info->smart.pen_selected;
									pointer_callback(event);
									
								}
								
								//key datagram
								if(buffer[2]==6)
								{
									/*
									cout<<"button click"<<endl;
									for(int n=0;n<res;n++)
										cout<<hex<<(int)buffer[n]<<" ";
									cout<<endl;
									*/
									cout<<"* key press: "<<hex<<(int)buffer[3]<<endl;
									
									//right click
									info->smart.right_click=((buffer[3] & 0x02)>>1);
								}
							
							break;
							
							default:
								if(common.debug)
									cout<<"Unknown command:"<<hex<<(int)buffer[1]<<endl;
								
							break;
						}
						
					}
					
					if(buffer[0]!=2)
					{
						if(common.debug)
							cout<<"Unknown report!:"<<dec<<(int)buffer[0]<<endl;
						
					}
				
				break;
				
				//Team board
				case 0x07dd0001:
						mx = (int)(buffer[1]+(buffer[2]<<8));
						my = (int)(buffer[3]+(buffer[4]<<8));
						button[0] = buffer[0];
						
						driver_event event;
						event.id=info->id;
						event.address=info->address;
						event.type=EVENT_POINTER;
						event.pointer.pointer=0;
						event.pointer.x=(float)mx/4096.0f;
						event.pointer.y=(float)my/4096.0f;
												
						event.pointer.button=button[0];
						
						pointer_callback(event);
						
						if(common.debug)
							cout<<dec<<"pos: "<<mx<<","<<my<<endl;
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
	unsigned char buffer[64];
	
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
		//common default params setting
		
		/* uinput init goes here*/
		switch(info->id)
		{
			case 0x04da104d:
				/*
				 * report id:2
				 * 	input/output 8 bytes
				 * 
				 * report id:4 
				 * 	feature 8 bytes
				 * 
				 * report id:8
				 * 	feature 8 bytes
				 */
				cout<<"init: panaboard ub-t880"<<endl;
				buffer[0]=8;
				hid_get_feature_report(info->handle,buffer,8);
				cout<<"Max Contact Number:"<<(int)buffer[1]<<endl;
				
				buffer[0]=4;
				hid_get_feature_report(info->handle,buffer,8);
				cout<<"Report ID 4"<<endl;
				for(int n=0;n<9;n++)
				{
					cout<<hex<<(int)buffer[n]<<" ";
				}
				cout<<endl;
				
				buffer[0]=4;
				buffer[1]=1;
				hid_send_feature_report(info->handle,buffer,8);
			break;
			
			//ebeam 3
			case 0x26501311:
				/*
				info->pointer = new AbsolutePointer("ebeam 3",info->id,0,32767,0,32767);
				info->pointer->start();
				*/
			
				info->ebeam.mx=0;
				info->ebeam.my=0;
				info->ebeam.button=0;
			break;
			
			//smart board
			case 0x0b8c0001:
				info->smart.right_click=0;
				info->smart.pen_selected=0;
				info->smart.pen_status=0;
					
				/*
				info->aux_pointer[0] = new AbsolutePointer("smart Blue",info->id,0,32767,0,32767);
				info->aux_pointer[0]->start();
				info->aux_pointer[1] = new AbsolutePointer("smart Green",info->id,0,32767,0,32767);
				info->aux_pointer[1]->start();
				info->aux_pointer[2] = new AbsolutePointer("smart Eraser",info->id,0,32767,0,32767);
				info->aux_pointer[2]->start();
				info->aux_pointer[3] = new AbsolutePointer("smart Red",info->id,0,32767,0,32767);
				info->aux_pointer[3]->start();
				info->aux_pointer[4] = new AbsolutePointer("smart Black",info->id,0,32767,0,32767);
				info->aux_pointer[4]->start();
				*/
	
			break;
			
			//TeamBoard
			case 0x07dd0001:
				/*
				info->pointer = new AbsolutePointer("TeamBoard",info->id,0,32767,0,32767);
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
			//ebeam 3
			case 0x26501311:
		
			break;
			
			
			//smart board
			case 0x0b8c0001:
		
			break;
			
			//Team board
			case 0x07dd0001:
			
			break;

		}
		
		//Sending shutdown signal
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
	*(parameter_map[key])=value;
	
	//catching new states
	for(int n=0;n<driver_instances.size();n++)
	{
		switch(driver_instances[n]->id)
		{
			//team board
			case 0x07dd0001:
		
			break;
		}
	}
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
		cout<<"[WhiteBoardDriver] set_callback"<<endl;
	pointer_callback = callback;
}
