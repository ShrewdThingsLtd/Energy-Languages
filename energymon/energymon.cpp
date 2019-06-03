
#include <chrono>
#include <string>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <eml.h>
#ifdef __cplusplus
}
#endif

class hires_clock {

	std::chrono::high_resolution_clock _clock;
	
public:
	
	uint64_t now_nsec() {

		return std::chrono::duration_cast<std::chrono::nanoseconds>(_clock.now().time_since_epoch()).count();
	};
};

struct devrecord {

	std::string _devname;
	double _consumed;
	double _elapsed;
	
	devrecord() : _devname(""), _consumed(0), _elapsed(0) {};
};

class energymetric_eml {
	
	static const useconds_t collect_interval_usec = 1000000;
	emlDevice_t **_devs;
	emlData_t **_devdata;	
	size_t _devcount;
	devrecord *_devrecords;

	void check_error(emlError_t ret) {
		
		if (ret != EML_SUCCESS) {
			fprintf(stderr, "error: %s\n", emlErrorMessage(ret));
			exit(1);
		};
	};

protected:
	
	static const std::string metrictype() { return "eml"; };
	
	energymetric_eml() : _devcount(0) {
		
		static const std::string metrictype = "eml";
		
		check_error(emlInit());
		check_error(emlDeviceGetCount(&_devcount));
		_devs = new emlDevice_t*[_devcount];
		_devdata = new emlData_t*[_devcount];
		_devrecords = new devrecord[_devcount];
		
		for (size_t dev_idx = 0; dev_idx < _devcount; ++dev_idx) {
			const char *devname = NULL;
			emlDeviceType_t devtype;
			check_error(emlDeviceByIndex(dev_idx, &_devs[dev_idx]));
			check_error(emlDeviceGetName(_devs[dev_idx], &devname));
			check_error(emlDeviceGetType(_devs[dev_idx], &devtype));
			_devrecords[dev_idx]._devname = std::string(devname) + ":" + std::to_string(devtype);
		};
	};
	
	void collect() {
		
		check_error(emlStart());
		usleep(collect_interval_usec);
		check_error(emlStop(_devdata));
		
		for (size_t dev_idx = 0; dev_idx < _devcount; ++dev_idx) {
			check_error(emlDataGetConsumed(_devdata[dev_idx], &_devrecords[dev_idx]._consumed));
			check_error(emlDataGetElapsed(_devdata[dev_idx], &_devrecords[dev_idx]._elapsed));
			check_error(emlDataFree(_devdata[dev_idx]));
		};
	};
	
	size_t get_devcount() const { return _devcount; };
	const devrecord &get_rec(const size_t dev_idx) const { return _devrecords[dev_idx]; };
	
	~energymetric_eml() {
		
		check_error(emlShutdown());
		delete [] _devdata;
	};
};


template <typename energymetric> class energymon : protected energymetric {
	
	hires_clock _clock;
	
public:
	
	void concat_rec(std::string &rec_str, const size_t dev_idx) {
		
		const devrecord &dev_rec = energymetric::get_rec(dev_idx);
		rec_str += dev_rec._devname + ",metrictype=" + energymetric::metrictype();
		rec_str += " consumed=" + std::to_string(dev_rec._consumed) + ",elapsed=" + std::to_string(dev_rec._elapsed);
		rec_str += " " + std::to_string(_clock.now_nsec()) + "\n";
	};
	
	void poll(const uint64_t iters) {
		
		for (uint64_t it = 0; it < iters; ++it) {
			energymetric::collect();
			std::string rec_str;
			const size_t devcount = energymetric::get_devcount();
			for (size_t dev_idx = 0; dev_idx < devcount; ++dev_idx) {
				concat_rec(rec_str, dev_idx);
			};
			printf("%s", rec_str.c_str());
		};
	};
};

int main() {
	
	energymon<energymetric_eml> mon_eml;
	mon_eml.poll(3);
	return 0;
};


#if 0

/*
 * Copyright (c) 2014 Universidad de La Laguna <cap@pcg.ull.es>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <eml.h>

void check_error(emlError_t ret) {
  if (ret != EML_SUCCESS) {
    fprintf(stderr, "error: %s\n", emlErrorMessage(ret));
    exit(1);
  }
}

int main(int argc, char** argv) {
  if (argc < 2) {
    if (argc)
      printf("usage: %s command-string\n", argv[0]);
    return 1;
  }

  //initialize EML
  check_error(emlInit());

  //get total device count
  size_t count;
  check_error(emlDeviceGetCount(&count));
  emlData_t* data[count];

  //start measuring
  check_error(emlStart());

  if (system(argv[1])) {};

  //stop measuring and gather data
  check_error(emlStop(data));

  //print energy/time and free results
  char rec_str[4096];
  char *rec_ptr = rec_str;
  rec_ptr += sprintf(rec_ptr, "energy,framework=eml devs=%ld", count);
  for (size_t i = 0; i < count; i++) {
    double consumed, elapsed;
    check_error(emlDataGetConsumed(data[i], &consumed));
    check_error(emlDataGetElapsed(data[i], &elapsed));
    //FILE *fjson = fopen("/tmp/eml.json", "a");
    //check_error(emlDataDumpJSON(data[i], fjson));
    //fclose(fjson);
    check_error(emlDataFree(data[i]));

    emlDevice_t* dev;
    check_error(emlDeviceByIndex(i, &dev));
    const char* devname;
    emlDeviceType_t devtype;
    check_error(emlDeviceGetName(dev, &devname));
    check_error(emlDeviceGetType(dev, &devtype));
    rec_ptr += sprintf(rec_ptr, ",%s_type=%d,%s_jps=%g", devname, devtype, devname, (consumed / elapsed));
    //printf("%s(%d): %gJ in %gs\n", devname, devtype, consumed, elapsed);
  }
  printf("%s\n", rec_str);

  check_error(emlShutdown());
  return 0;
}

#endif

