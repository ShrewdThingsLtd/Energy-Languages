
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
	
	static const uint64_t args_str_limit = 128;

	std::string _sysname;
	std::string _topmemapp;
	std::string _topmemargs;
	std::string _topmemtime;
	std::string _topmem;
	std::string _topmemcpu;
	std::string _topcpuapp;
	std::string _topcpuargs;
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
		
		exec("ps -heo %mem,%cpu,etime,ucmd,cmd --sort=-%mem | head -n 1");
		std::istringstream iss(get_ret_str());
		std::vector<std::string> ret_vec {
			std::istream_iterator<std::string>(iss), {}
		};
		rec._topmem = ret_vec[0];
		rec._topmemcpu = ret_vec[1];
		rec._topmemtime = ret_vec[2];
		rec._topmemapp = ret_vec[3];
		rec._topmemargs = ">";
		for(uint64_t i = 5; i < ret_vec.size(); ++i) {
			if((ret_vec[i].size() > 0) && (rec._topmemargs.size() + ret_vec[i].size() < sysrecord::args_str_limit)) {
				rec._topmemargs += ret_vec[i] + ">";
			};
		};
	};
};

class system_top_cpu : protected system_exec {
		
public:
		
	system_top_cpu(sysrecord &rec) {
		
		exec("ps -heo %mem,%cpu,etime,ucmd,cmd --sort=-%cpu | head -n 1");
		std::istringstream iss(get_ret_str());
		std::vector<std::string> ret_vec {
			std::istream_iterator<std::string>(iss), {}
		};
		rec._topcpumem = ret_vec[0];
		rec._topcpu = ret_vec[1];
		rec._topcputime = ret_vec[2];
		rec._topcpuapp = ret_vec[3];
		rec._topcpuargs = ">";
		for(uint64_t i = 5; i < ret_vec.size(); ++i) {
			if((ret_vec[i].size() > 0) && (rec._topcpuargs.size() + ret_vec[i].size() < sysrecord::args_str_limit)) {
				rec._topcpuargs += ret_vec[i] + ">";
			};
		};
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
		sys_rec_str += ",topmemtime=" + sys_rec._topmemtime + ",topmemapp=" + sys_rec._topmemapp;
		sys_rec_str += ",topcputime=" + sys_rec._topcputime + ",topcpuapp=" + sys_rec._topcpuapp;
		sys_rec_str += ",topmemargs=" + sys_rec._topmemargs;
		sys_rec_str += ",topcpuargs=" + sys_rec._topcpuargs;
		sys_rec_str += " topmem=" + sys_rec._topmem + ",topmemcpu=" + sys_rec._topmemcpu;
		sys_rec_str += ",topcpumem=" + sys_rec._topcpumem + ",topcpu=" + sys_rec._topcpu;
		sys_rec_str += " " + std::to_string(now_nsec) + "\n";
	};
	
	void concat_dev_rec(std::string &dev_rec_str, const size_t dev_idx, const uint64_t now_nsec) {
		
		const devrecord &dev_rec = energymetric::get_dev_rec(dev_idx);
		dev_rec_str += dev_rec._devname + ",metrictype=" + energymetric::metrictype();
		dev_rec_str += " consumed=" + std::to_string(dev_rec._consumed) + ",elapsed=" + std::to_string(dev_rec._elapsed) + ",jps=" + std::to_string(dev_rec._consumed / dev_rec._elapsed);
		dev_rec_str += " " + std::to_string(now_nsec) + "\n";
	};
	
public:
	
	energymon(const std::string &sysname) : energymetric(sysname) {};
	
	void poll() {
			
		while (true) {
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
	mon_eml.poll();
	return 0;
};
