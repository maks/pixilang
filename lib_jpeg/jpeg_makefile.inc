FINAL_CFLAGS += -I$(JPEG)

LIB_JPEG = \
    jpgd.o \
    jpge.o

jpgd.o: $(JPEG)jpgd.cpp $(JPEG)jpgd.h $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(JPEG)jpgd.cpp

jpge.o: $(JPEG)jpge.cpp $(JPEG)jpge.h $(STD_INCS)
	$(CXX) $(FINAL_CFLAGS) -c $(JPEG)jpge.cpp
