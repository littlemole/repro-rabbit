DEBUG = -g -DMOL_PROMISE_DEBUG
release : override DEBUG = -O3

LIBEVENT=$(shell pkg-config libevent_pthreads --libs)
PKG_INC=$(shell pkg-config priocpp openssl --cflags)
PKG_LIB=$(shell pkg-config priocpp openssl  --libs)

# if not g++ we assume clang++
DEFAULT_LIBS = $(TS) -stdlib=libc++ -fcoroutines-ts  -lc++abi -std=c++17 -fpic
DEFAULT_OPTS = $(TS) -std=c++17 -stdlib=libc++ -fcoroutines-ts -D_RESUMABLE_FUNCTIONS_SUPPORTED -fpic

ifeq ($(CXX),g++)
DEFAULT_OPTS = -std=c++17 -fpic
DEFAULT_LIBS = -fpic 
endif

#backend switch libevent/boost_asio
PROMISE_IMPL = -DPROMISE_USE_LIBEVENT
PROMISE_3DPARTY = $(LIBEVENT) $(DEFAULT_LIBS) -lamqpcpp -ldl 

ifeq ($(BACKEND),boost_asio)
        PROMISE_IMPL = -DPROMISE_USE_BOOST_ASIO
        PROMISE_3DPARTY = -lboost_system $(DEFAULT_LIBS)
endif

#final cpp options
OPTIONS =  -pthread -D_REENTRANT $(DEFAULT_OPTS) $(PKG_INC)
CPPFLAGS = -Wall -I../include $(DEBUG) $(OPTIONS) $(PROMISE_IMPL)

# target
# ../src/build/libreprorabbit.a
LIB = -lpriocppd ../src/build/libreprorabbitd.a
release : override LIB = -lpriocpp ../src/build/libreprorabbit.a

TEST_SRC   = .
TEST_BUILD = ./build
TEST_LIBS  = $(LIB) /usr/lib/libgtest.a -lpthread  $(PKG_LIB) $(PROMISE_3DPARTY)


# Objects for the test executable
TEST_SRCFILESABS = $(shell ls $(TEST_SRC)/*.cpp)
TEST_SRCFILES =  $(notdir $(TEST_SRCFILESABS))
TEST_OBJFILES = $(TEST_SRCFILES:%.cpp=$(TEST_BUILD)/%.o)
TEST_BINS = $(TEST_SRCFILES:%.cpp=$(TEST_BUILD)/%.bin)

#################################################
# rule to compile all (default rule)
#################################################

all: $(TEST_BINS)

#################################################
# rules to compile .o files from .cpp source
#################################################

$(TEST_BUILD)/%.o: $(TEST_SRC)/%.cpp 
	-mkdir -p $(TEST_BUILD)
	$(CXX) -c $^ -o $@ $(CPPFLAGS) 


#################################################
# rules to compile the artifacts from .o files
#################################################

$(TEST_BUILD)/%.bin: $(TEST_BUILD)/%.o 
	$(CXX) $(LIB_OBJFILES) $^ $(TEST_LIBS) -o $@ 
			
#################################################
# make clean
#################################################

clean:
	-find -name "*.o" -exec rm {} \;
	-find -name "*.bin" -exec rm {} \;
	-find -name "*~" -exec rm {} \;


release : all


