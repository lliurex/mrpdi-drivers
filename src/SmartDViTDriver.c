

#include <mrpdi/BaseDriver.h>
#include <pthread.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <cmath>
#include <stdint.h>
#include "libcam.h"

using namespace std;

struct driver_instance_info
{
	unsigned int id;
	unsigned int address;
	pthread_t thread;
	bool quit_request;
	Camera * video0;
	Camera * video1;
	uint8_t * buffer0;
	uint8_t * buffer1;
	int click;
	float px;
	float py;
};

void (*pointer_callback) (driver_event);
void * thread_core(void*);
void * thread_aux(void*);
void init_driver(driver_instance_info * info);
void close_driver(driver_instance_info * info);

void binarize(uint8_t * src,uint8_t * dest);
void get_center(uint8_t * img,int * cx,int * cy,int * area);

void method1(int c1,int c2,float * ox,float * oy);
void method2(int c1,int c2,float * ox,float * oy);
void method3(int c1,int c2,float * ox,float * oy);
void method4(int c1,int c2,float * ox,float * oy);

void method5(int c1,int c2,float * ox,float * oy);
void method6(int c1,int c2,float * ox,float * oy);

int cmap [] = {555,568,96,552,75,541,69,82,				
	559,496,286,456,183,350,141,80,					
	559,428,348,367,271,253,210,80,
	557,371,434,304,334,205,267,82
};

const char * name="Smart DViT Driver";
const char * version="2.0-alpha1";

driver_device_info supported_devices [] = 
{
{0x0b8c000e,0x00,0x01,"Smart DViT v280"},
{0xffffffff,0x00,0x00,"EOL"}
};

driver_parameter_info supported_parameters [] = 
{
{0x00000000,"common.debug"},
{0x0b8c000e,"dvit.calibrate"},
{0x0b8c000e,"dvit.pointers"},
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
	
	//used for debugging, not mapped
	unsigned int address;
	unsigned int id;
	
} common ;

struct t_dvit
{
	unsigned int calibrate;
	unsigned int pointers;
	unsigned int method;
}dvit;


/**
* global driver initialization
*/
void init()
{
	
	parameter_map["common.debug"]=&common.debug;
	parameter_map["dvit.calibrate"]=&dvit.calibrate;
	parameter_map["dvit.pointers"]=&dvit.pointers;
	parameter_map["dvit.method"]=&dvit.method;
	
	//default values
	common.debug=1;
	dvit.calibrate=1;
	dvit.pointers=1;
	dvit.method=5;
}

/**
* global driver shutdown
*/
void shutdown()
{
	if(common.debug)
		cout<<"[SmartDViTDriver] Shutdown:"<<name<<endl;
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
	int mx,my;
	
			
	if(common.debug)
		cout<<"thread_core::enter"<<endl;
	
	cout<<"info:"<<(int)info->quit_request<<endl;
		
	while(!info->quit_request)
	{
		
		switch(info->id)
		{
			case 0x0b8c000e:
				//IplImage * img0;
				//IplImage * img1;
				int c1,c2,cy,area0,area1;
				double timestamp0,timestamp1;
				float px,py;
				
				//cout<<"p1"<<endl;
				
				//step 1: video aquisition
				/*
				img0=cvQueryFrame(info->video0);
				img1=cvQueryFrame(info->video1);
				*/
				//cvGrabFrame(info->video0);
				//cvGrabFrame(info->video1);
				//img0=cvRetrieveFrame(info->video0);
				//img1=cvRetrieveFrame(info->video1); 
				/*
				timestamp0=cvGetCaptureProperty(info->video0,CV_CAP_PROP_POS_FRAMES);
				timestamp1=cvGetCaptureProperty(info->video1,CV_CAP_PROP_POS_FRAMES);
				*/ 
				//cout<<"Fetched frames:"<<timestamp0<<","<<timestamp1<<endl;
				
				//step 2: video post-processing
				//cvThreshold(img0,img0,200,255,CV_THRESH_BINARY);
				//cvThreshold(img1,img1,200,255,CV_THRESH_BINARY);
				
				//step 3: video analysis & computation
				//get_center(img0,&c1,&cy,&area0);
				
				//get_center(img1,&c2,&cy,&area1);
			
				
				//info->video0->Update(100,24);
				//info->video1->Update(100,24);
				info->video0->Update(info->video1,100,24);			
				
				binarize(info->video0->data,info->buffer0);
				binarize(info->video1->data,info->buffer1);
				
				get_center(info->buffer0,&c1,&cy,&area0);
				get_center(info->buffer1,&c2,&cy,&area1);
				
				
				if(c1>0 && c2>0)
				{
					info->click=1;
					info->px=px;
					info->py=py;
									
					common.id=info->id;
					common.address=info->address;
					
					switch(dvit.method)
					{
						case 5:
							method5(c1,c2,&px,&py);	
						break;
						
						case 6:
							method6(c1,c2,&px,&py);
						break;
					}
					
					
					
					if(common.debug)
					{
						//cout<<"dvit:"<<dec<<c1<<","<<c2<<endl;
						cout<<"pos:"<<px<<","<<py<<endl;
						cout<<"timestamp 0:"<<dec<<info->video0->timestamp.tv_sec<<"."<<(info->video0->timestamp.tv_usec/1000)<<endl;
						cout<<"timestamp 1:"<<info->video1->timestamp.tv_sec<<"."<<(info->video1->timestamp.tv_usec/1000)<<endl;
						//cout<<"width:"<<area0<<","<<area1<<endl;
						//float io_time = (info->video0->p2.tv_sec + (1.0f/info->video0->p2.tv_usec)) - (info->video0->p1.tv_sec + (1.0f/info->video0->p1.tv_usec));
					
						
						cout<<"p1 "<<info->video0->p1.tv_sec<<":"<<info->video0->p1.tv_usec/1000<<endl;
						cout<<"p2 "<<info->video0->p2.tv_sec<<":"<<info->video0->p2.tv_usec/1000<<endl;
						cout<<"p3 "<<info->video0->p3.tv_sec<<":"<<info->video0->p3.tv_usec/1000<<endl;
						cout<<"p4 "<<info->video0->p4.tv_sec<<":"<<info->video0->p4.tv_usec/1000<<endl;
						
						cout<<"p1 "<<info->video1->p1.tv_sec<<":"<<info->video1->p1.tv_usec/1000<<endl;
						cout<<"p2 "<<info->video1->p2.tv_sec<<":"<<info->video1->p2.tv_usec/1000<<endl;
						cout<<"p3 "<<info->video1->p3.tv_sec<<":"<<info->video1->p3.tv_usec/1000<<endl;
						cout<<"p4 "<<info->video1->p4.tv_sec<<":"<<info->video1->p4.tv_usec/1000<<endl;
						
						driver_event event;
						event.id=info->id;
						event.address=info->address;
						event.type=EVENT_DATA;
						event.data.type=1;//c1 and c2 positions
						*((unsigned int *)(event.data.buffer))=(unsigned int) c1;
						*((unsigned int *)(event.data.buffer)+1) =(unsigned int) c2;
						pointer_callback(event);
						
						event.data.type=3;//c1 and c2 areas
						*((unsigned int *)(event.data.buffer))=(unsigned int) area0;
						*((unsigned int *)(event.data.buffer)+1) =(unsigned int) area1;
						pointer_callback(event);
					}
					
					driver_event event;
					event.id=info->id;
					event.address=info->address;
					event.type=EVENT_POINTER;
					event.pointer.pointer=0;
					event.pointer.x=px;
					event.pointer.y=py;
					event.pointer.button=1;//hack					
					
					pointer_callback(event);
				}
				else
				{
					if(info->click==1)
					{
						driver_event event;
						event.id=info->id;
						event.address=info->address;
						event.type=EVENT_POINTER;
						event.pointer.pointer=0;
						event.pointer.x=info->px;
						event.pointer.y=info->py;
						event.pointer.button=0;					
						
						pointer_callback(event);
						
						info->click=0;
					}
				}
											
				
				
				
				
				
			break;
		}
		
		//sleep(0.0015);
		//sleep(0.01);
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
	
	if(common.debug)
		cout<<"*** init_driver ***"<<endl;
		
		
	switch(info->id)
	{
		//smart DViT
		case 0x0b8c000e:
		/*
			info->video0 = cvCaptureFromCAM(0);
			info->video1 = cvCaptureFromCAM(1);
			
			double width = cvGetCaptureProperty(info->video0,CV_CAP_PROP_FRAME_WIDTH);
			double height = cvGetCaptureProperty(info->video0,CV_CAP_PROP_FRAME_HEIGHT);
		
			
			if(common.debug)
				cout<<"* DViT camera0 size: "<<width<<"x"<<height<<endl;
			
			width = cvGetCaptureProperty(info->video1,CV_CAP_PROP_FRAME_WIDTH);
			height = cvGetCaptureProperty(info->video1,CV_CAP_PROP_FRAME_HEIGHT);
			
			if(common.debug)
				cout<<"* DViT camera1 size: "<<width<<"x"<<height<<endl;
			*/
			
			/*
			 * You know what? there is some room for improvement here
			 */ 
			info->video0 = new Camera("/dev/video0",640,480,30);
			info->video1 = new Camera("/dev/video1",640,480,30);
			
			info->buffer0 = new uint8_t[640*480];
			info->buffer1 = new uint8_t[640*480];
			
			info->click=0;
			
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
		
	switch(info->id)
	{
		//smart DViT
		case 0x0b8c000e:
			/*
			cvReleaseCapture(&info->video0);
			cvReleaseCapture(&info->video1);
			*/
			delete info->video0;
			delete info->video1;
			 
			delete [] info->buffer0;
			delete [] info->buffer1;
			
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



/**
* Sets device parameter value
*/
void set_parameter(const char * key,unsigned int value)
{
	if(common.debug)
		cout<<"[SmartDViTDriver::set_parameter]:"<<value<<endl;
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
		cout<<"[SmartDViTDriver::get_parameter]:"<<*value<<endl;
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
		cout<<"[SmartDViTDriver] set_callback"<<endl;
	pointer_callback = callback;
}


void binarize(uint8_t * src,uint8_t * dest)
{
	uint16_t hi,low;
	int Y1,Y2;
	
	/*
	 * Assuming YUV2 input pixel format 
	 * and 8-bit level output 
	 **/
	
	for(int i=0;i<640;i+=2)
	{
		for(int j=0;j<480;j++)
		{
			hi=((uint16_t * )src)[i+j*640];
			low=((uint16_t * )src)[(i+1)+j*640];
			 
			Y1=(hi & 0x00FF);
			Y2=(low & 0x00FF);
			
			//binarize
			Y1=(Y1>200) ? 0xFF : 0x00;
			Y2=(Y2>200) ? 0xFF : 0x00;
			
			dest[i+j*640]=Y1;
			dest[(i+1)+j*640]=Y2;
		}
	}
}

/**
* Computes the geometrical center of a binarized image
*/
void get_center(uint8_t * img,int * cx,int * cy,int * area)
{
	int left=10000;
	int right=-10000;
	int up=10000;
	int down=-10000;
	uint8_t value;
	
	*cx=-1;
	*cy=-1;

	//HACK HACK HACK
	for(int i=0;i<640;i++)
	{
		for(int j=0;j<480;j++)
		{
			value = img[i+j*640];
			
			if(value==0xFF)
			{
				if(i<left)left=i;
				if(i>right)right=i;
				if(j<up)up=j;
				if(j>down)down=j;

			}

		}
	}


	*cx = (left+right)/2;
	*cy = (up+down)/2;
	*area = (right-left);


}


void method1(int c1,int c2,float * ox,float * oy)
{
	double x1,x1p,y1,x2,x2p,y2;
	double a,b,c;
	
	
	
	x1p=c1;
	x2p = c2 - 320.0;
	
	x2=sqrt(x1p*x2p);
	x1=x1p*x2;
	y1=x2;
	y2=x1;
	
	b=-x2p+320.0;
	a=1.0;
	c=-x1p*x2p;
	
	//x2 = ( b+sqrt( (b*b) - 4*a*c ) )/ (2.0*a);
	//cout<<"x2: "<<x2<<endl;
	
	x2 = ( b-sqrt( (b*b) - 4*a*c ) )/ (2.0*a);
	//cout<<"x2: "<<x2<<endl;
	y2=x2/x2p;
	
	*ox=x2;
	*oy=y2;
	
}

void method2(int c1,int c2,float * ox,float * oy)
{
	double B[2];
	double P[2];
	double modB,modP;
	double cosAlpha,alpha;
	double cosBeta,beta;
	double gamma;
	double radius;
	double factor;
	
	
	/*	STEP 1: compute C1 angle */
	B[0] = 0.0;
	B[1]=452.548;
	
	P[0]=452.548;
	P[1]=452.548;
	
	factor = c1/640.0;
	
	P[0]*=factor;
	P[1]*=factor;
	P[0]-=B[0];
	P[1]-=B[1];
		
	
	modB = sqrt( (B[0]*B[0]) + (B[1]*B[1]) );
	modP = sqrt( (P[0]*P[0]) + (P[1]*P[1]) );
		
	cosAlpha = ((B[0]*P[0]) + (B[1]*P[1])) / (modB * modP);
	
	alpha = M_PI - acos(cosAlpha) ;
	
	/*	STEP 2: compute C2 angle */
	B[0] = 0.0;
	B[1]=452.548;
	
	P[0]=452.548;
	P[1]=452.548;
	
	factor = c2/640.0;
	
	P[0]*=factor;
	P[1]*=factor;
	P[0]-=B[0];
	P[1]-=B[1];
	
	modB = sqrt( (B[0]*B[0]) + (B[1]*B[1]) );
	modP = sqrt( (P[0]*P[0]) + (P[1]*P[1]) );
		
	cosBeta = ((B[0]*P[0]) + (B[1]*P[1])) / (modB * modP);
	
	beta = acos(cosBeta) - (M_PI/2.0) ;
	
	
	/* 	STEP 3: Triangulation */
	
	gamma = M_PI - alpha - beta;
	
	radius = (sin(beta)/sin(gamma));
		
	
	
	*ox=radius * cos(alpha);
	*oy=radius * sin(alpha);
	
	//cout<<"pos: "<<(int)*ox<<","<<(int)*oy<<endl;
	
}

void method3(int c1,int c2,float * ox,float * oy)
{
	double O[2];
	double B[2];
	double P[2];
	double mod;
	double VA[2];
	double VB[2];
	double factor;
	double mA;
	double mB;
	double Ix,Iy;
	double BP[2];
	
	//compute C1 vector
	O[0]=0.0;
	O[1]=0.0;
	
	B[0]=452.548;
	B[1] = 0.0;
	
	P[0]= 0.0;
	P[1]=452.548;
	
	factor = c1/640.0;
	
	BP[0]=P[0]-B[0];
	BP[1]=P[1]-B[1];
	
	BP[0]*=factor;
	BP[1]*=factor;
	
	P[0]=B[0]+BP[0];
	P[1]=B[1]+BP[1];
	
	VA[0] = P[0] - O[0];
	VA[1] = P[1] - O[1];
		
	//convert to unitary vector
	mod = sqrt( (VA[0]*VA[0]) + (VA[1]*VA[1]) );
	VA[0] = VA[0]/mod;
	VA[1] = VA[1]/mod;	
	
	//slope
	mA = VA[1]/VA[0];
	
	//compute C2 vector
	O[0]=0.0;
	O[1]=0.0;
		
	B[0] = 0.0;
	B[1]=452.548;
	
	P[0]=-452.548;
	P[1]= 0.0;
	
	
	factor = c2/640.0;
	
	BP[0]=P[0]-B[0];
	BP[1]=P[1]-B[1];
	
	BP[0]*=factor;
	BP[1]*=factor;
	
	P[0]=B[0]+BP[0];
	P[1]=B[1]+BP[1];
	
	VB[0] = P[0] - O[0];
	VB[1] = P[1] - O[1];
	
	mod = sqrt( (VB[0]*VB[0]) + (VB[1]*VB[1]) );
	VB[0] = VB[0]/mod;
	VB[1] = VB[1]/mod;
	
	mB = VB[1]/VB[0];	

	/*
	* line point-slope equation
	* y=m(x-Px)+Py
	* lets assume C1 laying on coordinates 0.0,0.0 and C2 1.0,0.0 so:
	* mA(x-0.0)+0.0 = mB(x-1.0)+0.0
	* mAx-mBx=-mB
	* x = -mB / (mA-mB)
	*/
	
	Ix = -mB / (mA-mB);
	Iy = mA * Ix;
	
		
	*ox=Ix;
	*oy=Iy;
	
	cout<<"VA: "<<VA[0]<<","<<VA[1]<<endl;
	cout<<"VB: "<<VB[0]<<","<<VB[1]<<endl;
}


void method4(int c1,int c2,float *ox,float * oy)
{
	double dist;
	int tx,ty;
	
	double minA=100000.0;
	double minB=100000.0;
	double minC=100000.0;
	
	int A=-1;
	int B=-1;
	int C=-1;
	
	for(int i=0;i<4;i++)
	{
		for(int j=0;j<4;j++)
		{
				tx=cmap[(i*2)+j*8];
				ty=cmap[1+((i*2)+j*8)];
				
				tx-=c1;
				ty-=c2;
				
				dist=sqrt((tx*tx)+(ty*ty));
				if(dist<minA)
				{
					minA=dist;
					A=i+j*8;
					*ox=i/3.0;
					*oy=j/3.0;
				}
				
		}
	}
	
	cout<<"A sector:"<<(A+1)<<endl;
	
}


void method5(int c1,int c2,float * ox,float *oy)
{
	double anglec1;
	double anglec2;
	double alpha,beta,gamma,radius;
	
	anglec1 = (c1 - 67.0)*87.431 / 489.0;
	anglec2 = (c2 - 85.0)*87.431 / 487.0;
		
	alpha= anglec1;
	beta= 90.0 - anglec2;
	
	//cout<<"m5 angles:"<<alpha<<","<<beta<<endl;
	
	alpha=alpha*M_PI/180.0;
	beta=beta*M_PI/180.0;
	gamma = M_PI - alpha - beta;
	
	radius = (sin(beta)/sin(gamma));
		
	*ox=radius * cos(alpha);
	*oy = radius* sin(alpha);
	
	/*
	cout<<"ox:"<<*ox<<endl;
	cout<<"radius:"<<radius<<endl;
	*/
	if(common.debug)
	{
		driver_event event;
		event.id=common.id;
		event.address=common.address;
		event.type=EVENT_DATA;
		event.data.type=2;//angle1 and angle2 positions
		*((float *)(event.data.buffer))=(float) (alpha*180.0/M_PI);
		*((float *)(event.data.buffer)+1) =(float) (beta*180.0/M_PI);
		pointer_callback(event);
	}					
}



void method6(int c1,int c2,float * ox,float *oy)
{
	double cosc1,cosc2;
	double alpha,beta,gamma,radius;
	
	alpha = (M_PI/2.0)*(c1/640.0);
	beta = (M_PI/2) - ((M_PI/2.0)*(c2/640.0));
	
	
	cout<<"m6 angles:"<<(alpha*180.0/M_PI)<<","<<(beta*180.0/M_PI)<<endl;
	
	gamma = M_PI - alpha - beta;
	
	radius = (sin(beta)/sin(gamma));
		
	*ox=radius * cos(alpha);
	*oy=radius * sin(alpha);	
	
	if(common.debug)
	{
		driver_event event;
		event.id=common.id;
		event.address=common.address;
		event.type=EVENT_DATA;
		event.data.type=2;//angle1 and angle2 positions
		*((float *)(event.data.buffer))=(float) (alpha*180.0/M_PI);
		*((float *)(event.data.buffer)+1) =(float) (beta*180.0/M_PI);
		pointer_callback(event);
	}
	
}
