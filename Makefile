LOCALBASE = ./
include $(LOCALBASE)Make.env
include $(BASEDIR)Make.env

SUBDIR = mbase client server

all: $(SUBDIR)
	@$(MULTIMAKE) $(SUBDIR)

clean:
	@$(MULTIMAKE) -m clean $(SUBDIR)

install:
	@$(MULTIMAKE) -m install $(SUBDIR)
