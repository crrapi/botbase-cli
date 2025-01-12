OBJS = experiment.o socket.o

experiment: $(OBJS)
	c++ -std=c++20 -Wall -O2 -o $@ $(OBJS)

%.o: %.cpp
	c++ -std=c++20 -Wall -O2 -c $< -o $@
