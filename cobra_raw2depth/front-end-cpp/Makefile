CPPFLAGS += -I../raw-to-depth-cpp/ -Wall

frontend : MockSensorHeadThread.o V4LSensorHeadThread.o SensorHeadThread.o main.o
	g++ -o $@ $^ -lpthread

clean:
	rm -f *.o frontend

.PHONY: clean
