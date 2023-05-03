.PHONY: all

all: .DEFAULT

.DEFAULT:
	$(MAKE) -C build $(MAKECMDGOALS)
