Intel TBB:	4.3 (20140724oss)

	* include 복사
	* bin, lib 는 intel64 / vc12 만 복사

	* 리눅스는 tbb43_20140724oss_lin.tar 를 압축풀어 tbb 로 rename

boost:	1.54


C++ REST SDK:	2.2.0

	see https://casablanca.codeplex.com/wikipage?title=Setup%20and%20Build%20on%20Linux

	(prerequisite) apt-get install libboost-all-dev libssl-dev

	git clone https://git01.codeplex.com/casablanca
	cd casablanca/Release
	mkdir build.release
	cd build.release
	CXX=g++ cmake .. -DCMAKE_BUILD_TYPE=Release
	make



unzip:	1.01h
	http://www.winimage.com/zLibDll/minizip.html

zlib:	1.2.8
	http://www.zlib.net/

