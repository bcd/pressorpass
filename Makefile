
#PROFILE := y
#DEBUG := y

CXXFLAGS := -std=c++17 -Wall -march=core2
#CXXFLAGS += -Wextra
ifeq ($(PROFILE), y)
CXXFLAGS += -pg
endif
ifeq ($(DEBUG), y)
CXXFLAGS += -g -O0
else
CXXFLAGS += -O3
#CXXFLAGS += -Os -- did not help
endif

ifneq ($(DEBUG), y)
ifneq ($(PROFILE), y)
CXXFLAGS += -fomit-frame-pointer
endif
endif

LIB_OBJS := pyl.o pyl_search.o
APP_OBJS := test1.o test2.o test3.o
APPS := test1 test2 test3
INCLUDES := pyl.hpp pyl_search.hpp interval.hpp

OBJS := $(LIB_OBJS) $(APP_OBJS)
ASMS := $(OBJS:.o=.s)

all : $(APPS)

$(APPS) : % : %.o $(LIB_OBJS) $(INCLUDES)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LIB_OBJS)

$(OBJS) : %.o : %.cpp $(INCLUDES)
	$(CXX) $(CXXFLAGS) -o $@ -c $<

$(ASMS) : %.s : %.cpp $(INCLUDES)
	$(CXX) $(CXXFLAGS) -o $@ -S $<

#test1.s : $(OBJS)
#	objdump -S --demangle $(OBJS) > $@

clean:
	rm -f $(OBJS) $(ASMS)

diff:
	@diff base.out new.out || exit 0

run: $(APPS)
	@time nice ./$(APPS) 2>&1 | tee new.out
ifeq ($(PROFILE), y)
	gprof ./pyl > pyl.gprof
endif

