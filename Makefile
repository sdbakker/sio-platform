JUNK	= *~

all: platform modules

platform:
	$(MAKE) -C platform sios

modules: platform
	$(MAKE) -C modules modules

uninstall:
	find $(MODPATH) -name "sios" | xargs rm -rf

clean:
	$(MAKE) -C platform clean
	$(MAKE) -C modules clean

.PHONY: platform modules uninstall clean