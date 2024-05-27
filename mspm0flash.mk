ALL_TARGETS += $(o)mspm0flash
CLEAN_TARGETS += clean-mspm0flash
INSTALL_TARGETS += install-mspm0flash

mspm0flash_SOURCES := $(wildcard *.c)
mspm0flash_OBJECTS := $(addprefix $(o),$(mspm0flash_SOURCES:.c=.o))

$(o)%.o: %.c
	$(call compile_tgt,mspm0flash)

$(o)mspm0flash: $(mspm0flash_OBJECTS)
	$(call link_tgt,mspm0flash)

clean-mspm0flash:
	rm -f $(mspm0flash_OBJECTS) $(o)lpcflash

install-mspm0flash: $(o)mspm0flash
	$(INSTALL) -d -m 0755 $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 $(o)mspm0flash $(DESTDIR)$(BINDIR)/
