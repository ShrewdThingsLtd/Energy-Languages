
#include <chrono>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <eml.h>
#ifdef __cplusplus
}
#endif

struct sysrecord {

	std::string _sysname;
	std::string _topmemapp;
	std::string _topmemtime;
	std::string _topmem;
	std::string _topmemcpu;
	std::string _topcpuapp;
	std::string _topcputime;
	std::string _topcpumem;
	std::string _topcpu;
	
	//sysrecord() : _sysname("") {};
	sysrecord(const std::string &sysname) : _sysname(sysname) {};
};

struct devrecord {

	std::string _devname;
	double _consumed;
	double _elapsed;
	
	devrecord() : _devname("") {};
};

class hires_clock {

	std::chrono::high_resolution_clock _clock;
	
public:
	
	uint64_t now_nsec() {

		return std::chrono::duration_cast<std::chrono::nanoseconds>(_clock.now().time_since_epoch()).count();
	};
};

class system_exec {

	FILE *_fpipe;
	char _pline[2048];
	std::string _ret_str;
	
protected:
	
	void exec(const char *pcmd) {
		
		_ret_str = "";
		_fpipe = (FILE *)popen(pcmd, "r");
		fflush(_fpipe);
		while(fgets(_pline, sizeof(_pline), _fpipe)) {
			_ret_str += _pline;
		};
		fflush(_fpipe);
		pclose(_fpipe);
	};
	
	const std::string &get_ret_str() const { return _ret_str; };
};

class system_top_mem : protected system_exec {
		
public:
		
	system_top_mem(sysrecord &rec) {
		
		exec("ps -heo ucmd,etime,%mem,%cpu --sort=-%mem | head -n 1");
		std::istringstream iss(get_ret_str());
		std::vector<std::string> ret_vec {
			std::istream_iterator<std::string>(iss), {}
		};
		rec._topmemapp = ret_vec[0];
		rec._topmemtime = ret_vec[1];
		rec._topmem = ret_vec[2];
		rec._topmemcpu = ret_vec[3];
	};
};

class system_top_cpu : protected system_exec {
		
public:
		
	system_top_cpu(sysrecord &rec) {
		
		exec("ps -heo ucmd,etime,%mem,%cpu --sort=-%cpu | head -n 1");
		std::istringstream iss(get_ret_str());
		std::vector<std::string> ret_vec {
			std::istream_iterator<std::string>(iss), {}
		};
		rec._topcpuapp = ret_vec[0];
		rec._topcputime = ret_vec[1];
		rec._topcpumem = ret_vec[2];
		rec._topcpu = ret_vec[3];
	};
};

class energymetric_eml {
	
	static const useconds_t collect_interval_usec = 1000000;
	emlDevice_t **_devs;
	emlData_t **_devdata;	
	size_t _devcount;
	sysrecord _sysrecord;
	devrecord *_devrecords;

	void check_error(emlError_t ret) {
		
		if (ret != EML_SUCCESS) {
			fprintf(stderr, "error: %s\n", emlErrorMessage(ret));
			exit(1);
		};
	};

protected:
	
	static const std::string metrictype() { return "eml"; };
	
	energymetric_eml(const std::string &sysname = "") : _devcount(0), _sysrecord(sysname) {
		
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
			_devrecords[dev_idx]._devname = std::string(devname) + ":" + std::to_string(devtype) + "@" + _sysrecord._sysname;
		};
	};
	
	void collect() {
		
		check_error(emlStart());
		usleep(collect_interval_usec / 2);
		system_top_mem top_mem(_sysrecord);
		system_top_cpu top_cpu(_sysrecord);
		usleep(collect_interval_usec / 2);
		check_error(emlStop(_devdata));
		
		for (size_t dev_idx = 0; dev_idx < _devcount; ++dev_idx) {
			check_error(emlDataGetConsumed(_devdata[dev_idx], &_devrecords[dev_idx]._consumed));
			check_error(emlDataGetElapsed(_devdata[dev_idx], &_devrecords[dev_idx]._elapsed));
			check_error(emlDataFree(_devdata[dev_idx]));
		};
	};
	
	size_t get_devcount() const { return _devcount; };
	const sysrecord &get_sys_rec() const { return _sysrecord; };
	const devrecord &get_dev_rec(const size_t dev_idx) const { return _devrecords[dev_idx]; };
	
	~energymetric_eml() {
		
		check_error(emlShutdown());
		delete [] _devdata;
	};
};

template <typename energymetric> class energymon : protected energymetric {
	
	hires_clock _clock;
	
	void concat_sys_rec(std::string &sys_rec_str, const uint64_t now_nsec) {
		
		const sysrecord &sys_rec = energymetric::get_sys_rec();
		sys_rec_str += sys_rec._sysname + ",metrictype=" + energymetric::metrictype();
		sys_rec_str += ",topmemapp=" + sys_rec._topmemapp + ",topmemtime=" + sys_rec._topmemtime;
		sys_rec_str += ",topcpuapp=" + sys_rec._topcpuapp + ",topcputime=" + sys_rec._topcputime;
		sys_rec_str += ",topmem=" + sys_rec._topmem + ",topmemcpu=" + sys_rec._topmemcpu;
		sys_rec_str += ",topcpumem=" + sys_rec._topcpumem + ",topcpu=" + sys_rec._topcpu;
		sys_rec_str += " " + std::to_string(now_nsec) + "\n";
	};
	
	void concat_dev_rec(std::string &dev_rec_str, const size_t dev_idx, const uint64_t now_nsec) {
		
		const devrecord &dev_rec = energymetric::get_dev_rec(dev_idx);
		dev_rec_str += dev_rec._devname + ",metrictype=" + energymetric::metrictype();
		dev_rec_str += " consumed=" + std::to_string(dev_rec._consumed) + ",elapsed=" + std::to_string(dev_rec._elapsed);
		dev_rec_str += " " + std::to_string(now_nsec) + "\n";
	};
	
public:
	
	energymon(const std::string &sysname) : energymetric(sysname) {};
	
	void poll(const uint64_t iters) {
		
		for (uint64_t it = 0; it < iters; ++it) {
			energymetric::collect();
			const uint64_t now_nsec = _clock.now_nsec();
			std::string rec_str;
			concat_sys_rec(rec_str, now_nsec);
			const size_t devcount = energymetric::get_devcount();
			for (size_t dev_idx = 0; dev_idx < devcount; ++dev_idx) {
				concat_dev_rec(rec_str, dev_idx, now_nsec);
			};
			printf("%s", rec_str.c_str());
		};
	};
};

int main() {
	
	energymon<energymetric_eml> mon_eml("sut152");
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

