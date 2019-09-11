#
# Advertising Space For Rent
#

PROGRAM := a.out

SOURCES := $(wildcard ../*.c)
OBJECTS := $(patsubst ../%.c, %.o, $(SOURCES))
DEPENDENCIES := $(patsubst %.o, %.d, $(OBJECTS))

CFLAGS := -Wall -O2
LDFLAGS :=

# Some explanations on C preprocessor options:
#
# -MMD automatically generates makefile denpendencies in %.d file
# -MP creates phony targets with no prerequisites and no commands for .h files
# So that GNU make won't report the missing of the no longer used .h files
CPPFLAGS := -MMD -MP

# Search prerequisites and targets in ../ if it isn't here
VPATH := ../

$(PROGRAM): $(OBJECTS)
	$(CC) $(LDFLAGS) $^ -o $@

# The minus sign is added to suppress any error occurred in the procedure
# as the dependencies are inevitably not available at the first time make runs
-include $(DEPENDENCIES)

# Then fat otaku can have a rest and let make deduce everything else

.PHONY: clean help

clean:
	rm -rf *.[do] $(PROGRAM)

help:
	@echo "No, I'm just not gonna help you" 1>&2
