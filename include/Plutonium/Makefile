
.PHONY: lib clean

lib:
	@$(MAKE) -C Plutonium/

clean:
	@$(MAKE) clean -C Plutonium/
	@$(MAKE) clean -C Example/

example: lib
	@$(MAKE) -C Example/

all: lib example