#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

COMPONENT_ADD_LDFLAGS := $(COMPONENT_ADD_LDFLAGS) -L$(COMPONENT_PATH) -lalgobsec
