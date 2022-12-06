bin=httpserver
cgi=test_cgi
cc=g++
LD_FLAGS=-std=c++11 -lpthread
curr=$(shell pwd)	# 获取当前路径
src=main.cc

ALL:$(bin) $(cgi)
.PHONY:ALL

$(bin):$(src)
	$(cc) -o $@ $^ $(LD_FLAGS)

$(cgi):cgi/test_cgi.cc	# %.c表示当前文件夹下所有的.c文件
	$(cc) -o $@ $^

.PHONY:clean
clean:
	rm -f $(bin) $(cgi)
	rm -rf output

.PHONY:output	# 做发布用，将所有需要的资源放到一个文件夹下
output:
	mkdir output
	cp $(bin) output
	cp -rf wwwroot output
	cp $(cgi) output/wwwroot