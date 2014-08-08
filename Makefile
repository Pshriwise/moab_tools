include ${MOAB_MAKE}
include ${MK_MAKE}

MOAB_CXXFLAGS =  -Wall -pipe -pedantic -Wno-long-long
MOAB_CFLAGS = -Wall -pipe -pedantic -Wno-long-long
CXXFLAGS += ${MOAB_CXXFLAGS} ${MESHKIT_CXXFLAGS} -g 
CC = g++
LD_FLAGS = -g
CPPFLAGS += ${MOAB_INCLUDES} ${MESHKIT_INCLUDES} -g
CFLAGS   += ${MOAB_CFLAGS} ${MESHKIT_CFLAGS} -g
# add -g -pg to both CXX and LD flags to profile

all: test_cyl 

test_cyl: test_cyl.o 
	$(CC) $(LD_FLAGS) -o test_cyl test_cyl.o  \
	${MOAB_LIBS_LINK} -ldagmc ${MESHKIT_LIBS_LINK}

test_iter: test_iter.o
	$(CC) $(LD_FLAGS) -o test_iter test_iter.o \
	${MOAB_LIBS_LINK} -ldagmc ${MESHKIT_LIBS_LINK}

test_bllite: test_bllite.o 
	$(CC) $(LD_FLAGS) -o test_bllite test_bllite.o \
	${MOAB_LIBS_LINK} -ldagmc ${MESHKIT_LIBS_LINK}

test_fnsf_360: test_fnsf_360.o 
	$(CC) $(LD_FLAGS) -o test_fnsf_360 test_fnsf_360.o \
	${MOAB_LIBS_LINK} -ldagmc ${MESHKIT_LIBS_LINK}

clean:
	rm -f make_watertight.o make_watertight gen.o arc.o zip.o \
	cleanup.o post_process.o post_process cw_func.o mw_fix mw_fix.o test_cyl test_cyl.o mw_func.o \
	test_iter test_bllite test_iter.o test_bllite.o test_fnsf_360.o test_fnsf_360
