LOCALBASE = ../
include $(LOCALBASE)Make.env
include $(BASEDIR)Make.env

SUBDIR = daemon plugin oiad conf test oiad

all: $(SUBDIR)
	@$(MULTIMAKE) $(SUBDIR)

install:
	@$(MULTIMAKE) -m install $(SUBDIR)

clean:
	@$(MULTIMAKE) -m clean $(SUBDIR)
