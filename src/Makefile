NO_COLOR    = \x1b[0m
COMPILE_COLOR    = \x1b[32;01m
LINK_COLOR    = \x1b[31;01m

LIBUSB_LINK=`pkg-config libusb-1.0 --libs`
PTHREAD_LINK=-lpthread
LIBUSB_COMPILE=`pkg-config libusb-1.0 --cflags`

COMPILER_FLAGS=-O3 -march=native -I ../include/


all: drivers tablet board promethean iqboard multiclass

multiclass: MulticlassDriver.o
	@echo -e '$(LINK_COLOR)* Building [$@]$(NO_COLOR)'
	g++  -shared -o drivers/MulticlassDriver.so MulticlassDriver.o  $(PTHREAD_LINK)

iqboard: IQboardDriver.o
	@echo -e '$(LINK_COLOR)* Building [$@]$(NO_COLOR)'
	g++  -shared -o drivers/IQboardDriver.so IQboardDriver.o  $(PTHREAD_LINK)

tablet: TabletDriver.o	utils.o hidapi.o
	@echo -e '$(LINK_COLOR)* Building [$@]$(NO_COLOR)'
	g++  -shared -o drivers/TabletDriver.so TabletDriver.o utils.o hidapi.o $(PTHREAD_LINK) $(LIBUSB_LINK)

board: WhiteBoardDriver.o utils.o hidapi.o
	@echo -e '$(LINK_COLOR)* Building [$@]$(NO_COLOR)'
	g++  -shared -o drivers/WhiteBoardDriver.so WhiteBoardDriver.o  utils.o hidapi.o $(PTHREAD_LINK) $(LIBUSB_LINK)
	
promethean: PrometheanDriver.o utils.o
	@echo -e '$(LINK_COLOR)* Building [$@]$(NO_COLOR)'
	g++  -shared -o drivers/PrometheanDriver.so PrometheanDriver.o utils.o $(PTHREAD_LINK) $(LIBUSB_LINK)

dvit: SmartDViTDriver.o utils.o libcam.o
	@echo -e '$(LINK_COLOR)* Building [$@]$(NO_COLOR)'
	g++  -shared -o drivers/SmartDViTDriver.so SmartDViTDriver.o utils.o libcam.o $(PTHREAD_LINK)
	
drivers: 
	@echo -e '$(LINK_COLOR)* Building Drivers$(NO_COLOR)'
	mkdir -p drivers
	
	
mrpdi.o: Core.cpp
	@echo -e '$(COMPILE_COLOR)* Compiling [$@]$(NO_COLOR)'
	g++ $(COMPILER_FLAGS) -c -fPIC Core.cpp -o $@ $(LIBUSB_COMPILE)

WhiteBoardDriver.o: WhiteBoardDriver.c
	@echo -e '$(COMPILE_COLOR)* Compiling [$@]$(NO_COLOR)'
	g++ $(COMPILER_FLAGS) -c -fPIC WhiteBoardDriver.c
	
TabletDriver.o: TabletDriver.c
	@echo -e '$(COMPILE_COLOR)* Compiling [$@]$(NO_COLOR)'
	g++ $(COMPILER_FLAGS) -c -fPIC TabletDriver.c 

SmartDViTDriver.o: SmartDViTDriver.c
	@echo -e '$(COMPILE_COLOR)* Compiling [$@]$(NO_COLOR)'
	g++ $(COMPILER_FLAGS) $(OPENCV_COMPILE) -c -fPIC SmartDViTDriver.c

PrometheanDriver.o: PrometheanDriver.c
	@echo -e '$(COMPILE_COLOR)* Compiling [$@]$(NO_COLOR)'
	g++ $(COMPILER_FLAGS) $(LIBUSB_COMPILE) -c -fPIC PrometheanDriver.c
	
IQboardDriver.o: IQboardDriver.c
	@echo -e '$(COMPILE_COLOR)* Compiling [$@]$(NO_COLOR)'
	g++ $(COMPILER_FLAGS) -c -fPIC IQboardDriver.c

MulticlassDriver.o: MulticlassDriver.c
	@echo -e '$(COMPILE_COLOR)* Compiling [$@]$(NO_COLOR)'
	g++ $(COMPILER_FLAGS) -c -fPIC MulticlassDriver.c

hidapi.o: hid-libusb.c
	@echo -e '$(COMPILE_COLOR)* Compiling [$@]$(NO_COLOR)'
	gcc  $(COMPILER_FLAGS) -fPIC $(LIBUSB_COMPILE) -c hid-libusb.c -o hidapi.o

utils.o: utils.c
	@echo -e '$(COMPILE_COLOR)* Compiling [$@]$(NO_COLOR)'
	g++ $(COMPILER_FLAGS) -c -fPIC utils.c 

libcam.o: libcam.cpp
	@echo -e '$(COMPILE_COLOR)* Compiling [$@]$(NO_COLOR)'
	g++ $(COMPILER_FLAGS) -c -fPIC libcam.cpp 
	
clean:
	@echo -e '$(LINK_COLOR)* Cleaning$(NO_COLOR)'
	rm -f *.o
	rm -f *.so
	rm -rf drivers


install: all
	cp drivers/*.so /usr/lib/mrpdi/
