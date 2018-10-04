NO_COLOR    = \x1b[0m
COMPILE_COLOR    = \x1b[32;01m
LINK_COLOR    = \x1b[31;01m

	
all:
	@echo -e '$(LINK_COLOR)* Building Drivers[$@]$(NO_COLOR)'
	@$(MAKE) -C src/ -j2 $@

clean:
	@echo -e '$(LINK_COLOR)* Cleaning [$@]$(NO_COLOR)'
	@$(MAKE) -C src/ $@

