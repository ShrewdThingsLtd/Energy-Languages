
#include <algorithm>
#include <chrono>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>
#include <curl/curl.h>

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
	std::string _topmempid;
	std::string _topmemargs;
	std::string _topmemtime;
	std::string _topmem;
	std::string _topmemcpu;
	std::string _topcpuapp;
	std::string _topcpupid;
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

class influxdbv1_client {
	
	std::string _write_url;
	std::string _username;
	std::string _password;
	CURL *_curl;
	std::string _response_str;

	static size_t get_response_chunk(const char *ptr, size_t size, size_t nmemb, influxdbv1_client *client) {
		
		client->_response_str += std::string(ptr, (size * nmemb));
		return nmemb;
	};

public:
	
	influxdbv1_client(const std::string &url, const std::pair<std::string, std::string> &credentials, const std::string &db_name) : 
		_write_url(url + "/write?db=" + db_name), _username(credentials.first), _password(credentials.second), _curl(NULL) {
		
		curl_global_init(CURL_GLOBAL_ALL);
	};
	
	void write_points(const std::string &points) {
		
		_response_str = "";
		_curl = curl_easy_init();
		if (_curl) {
			curl_easy_setopt(_curl, CURLOPT_URL, _write_url.c_str());
			curl_easy_setopt(_curl, CURLOPT_POST, 1L);
			curl_easy_setopt(_curl, CURLOPT_HTTPAUTH, (long)CURLAUTH_BASIC);
			curl_easy_setopt(_curl, CURLOPT_USERNAME, _username.c_str());
			curl_easy_setopt(_curl, CURLOPT_PASSWORD, _password.c_str());
			curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, get_response_chunk);
			curl_easy_setopt(_curl, CURLOPT_WRITEDATA, (void *)this);
			curl_easy_setopt(_curl, CURLOPT_POSTFIELDS, points.c_str());
			CURLcode res = curl_easy_perform(_curl);
			if (res != CURLE_OK) {
				fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
			};
			curl_easy_cleanup(_curl);
			printf("%s\n", _response_str.c_str());
		};
	};
};

class influxdbv2_client {
	
	std::string _write_url;
	std::string _auth_header;
	struct curl_slist *_slist;
	CURL *_curl;
	std::string _response_str;

	static size_t get_response_chunk(const char *ptr, size_t size, size_t nmemb, influxdbv2_client *client) {
		
		client->_response_str += std::string(ptr, (size * nmemb));
		return nmemb;
	};

public:
	
	influxdbv2_client(const std::string &url, const std::string &org_id, const std::string &auth_token, const std::string &bucket) : 
		_write_url(url + "/write?precision=ns&org=" + org_id + "&bucket=" + bucket), _auth_header("Authorization: Token " + auth_token), _slist(NULL), _curl(NULL) {
		
		curl_global_init(CURL_GLOBAL_ALL);
	};
	
	void write_points(const std::string &points) {
		
		_response_str = "";
		_curl = curl_easy_init();
		if (_curl) {
			curl_easy_setopt(_curl, CURLOPT_URL, _write_url.c_str());
			curl_easy_setopt(_curl, CURLOPT_POST, 1L);
			_slist = NULL;
			_slist = curl_slist_append(_slist, _auth_header.c_str());
			curl_easy_setopt(_curl, CURLOPT_HTTPHEADER, _slist);
			curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, get_response_chunk);
			curl_easy_setopt(_curl, CURLOPT_WRITEDATA, (void *)this);
			curl_easy_setopt(_curl, CURLOPT_POSTFIELDS, points.c_str());
			CURLcode res = curl_easy_perform(_curl);
			if (res != CURLE_OK) {
				fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
			};
			char *url;
			curl_easy_getinfo(_curl,CURLINFO_EFFECTIVE_URL, &url);
			curl_easy_cleanup(_curl);
			curl_slist_free_all(_slist);
			printf("%s '%s'\n", _write_url.c_str(), _auth_header.c_str());
			printf("%s\n", url);
			printf("%s\n", _response_str.c_str());
		};
	};
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
		
		exec("ps -heo %mem,%cpu,etime,ucmd,pid --sort=-%mem | head -n 1");
		std::istringstream iss(get_ret_str());
		std::vector<std::string> ret_vec {
			std::istream_iterator<std::string>(iss), {}
		};
		rec._topmem = ret_vec[0];
		rec._topmemcpu = ret_vec[1];
		rec._topmemtime = ret_vec[2];
		rec._topmemapp = ret_vec[3];
		rec._topmempid = ret_vec[4];
		rec._topmemargs = ">";
		for(uint64_t i = 5; i < ret_vec.size(); ++i) {
			if((ret_vec[i].size() > 0) && (rec._topmemargs.size() + ret_vec[i].size() < sysrecord::args_str_limit)) {
				rec._topmemargs += ret_vec[i] + ">";
			};
		};
		std::replace(rec._topmemargs.begin(), rec._topmemargs.end(), '=', ':');
	};
};

class system_top_cpu : protected system_exec {
		
public:
		
	system_top_cpu(sysrecord &rec) {
		
		exec("ps -heo %mem,%cpu,etime,ucmd,pid --sort=-%cpu | head -n 1");
		std::istringstream iss(get_ret_str());
		std::vector<std::string> ret_vec {
			std::istream_iterator<std::string>(iss), {}
		};
		rec._topcpumem = ret_vec[0];
		rec._topcpu = ret_vec[1];
		rec._topcputime = ret_vec[2];
		rec._topcpuapp = ret_vec[3];
		rec._topcpupid = ret_vec[4];
		rec._topcpuargs = ">";
		for(uint64_t i = 5; i < ret_vec.size(); ++i) {
			if((ret_vec[i].size() > 0) && (rec._topcpuargs.size() + ret_vec[i].size() < sysrecord::args_str_limit)) {
				rec._topcpuargs += ret_vec[i] + ">";
			};
		};
		std::replace(rec._topcpuargs.begin(), rec._topcpuargs.end(), '=', ':');
	};
};

class energymetric_eml {
	
	static const char *sleep_expr;
	
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
	
	int32_t do_sleep() {
		
		return system(sleep_expr);
	};

protected:
	
	static const std::string metrictype() { return "eml"; };
	
	energymetric_eml(const std::string &sysname = "") : 
		_devcount(0), 
		_sysrecord(sysname) {
		
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
		do_sleep();
		check_error(emlStop(_devdata));
		system_top_mem top_mem(_sysrecord);
		system_top_cpu top_cpu(_sysrecord);
		
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
const char *energymetric_eml::sleep_expr = "sleep 1";

template <typename energymetric, typename influxdb_client> class energymon : protected energymetric {
	
	hires_clock _clock;
	influxdb_client _influxdb;
	
	/*
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
	*/
	
	void concat_dev_rec(std::string &dev_rec_str, const sysrecord &sys_rec, const devrecord &dev_rec, const uint64_t now_nsec) {
		
		dev_rec_str += dev_rec._devname + ",metrictype=" + energymetric::metrictype();
		/*
		dev_rec_str += ",topmemtime=" + sys_rec._topmemtime;
		dev_rec_str += ",topcputime=" + sys_rec._topcputime;
		*/
		dev_rec_str += " topmem=" + sys_rec._topmem + ",topmemcpu=" + sys_rec._topmemcpu;
		dev_rec_str += ",topmemapp=\"" + sys_rec._topmemapp + "\"";
		dev_rec_str += ",topcpuapp=\"" + sys_rec._topcpuapp + "\"";
		dev_rec_str += ",topmempid=" + sys_rec._topmempid;
		dev_rec_str += ",topcpupid=" + sys_rec._topcpupid;
		dev_rec_str += ",topcpumem=" + sys_rec._topcpumem + ",topcpu=" + sys_rec._topcpu;
		dev_rec_str += ",consumed=" + std::to_string(dev_rec._consumed) + ",elapsed=" + std::to_string(dev_rec._elapsed) + ",jps=" + std::to_string(dev_rec._consumed / dev_rec._elapsed);
		dev_rec_str += " " + std::to_string(now_nsec) + "\n";
	};
	
public:
	
	energymon(const std::string &sysname, const std::string &url, const std::pair<std::string, std::string> &credentials, const std::string &db_name) : 
		energymetric(sysname), _influxdb(url, credentials, db_name) {};
		
	energymon(const std::string &system_name, const std::string &db_url, const std::string &db_org_id, const std::string &db_auth_token, const std::string &db_bucket) : 
		energymetric(system_name), _influxdb(db_url, db_org_id, db_auth_token, db_bucket) {};
	
	void poll() {
			
		while (true) {
			energymetric::collect();
			const uint64_t now_nsec = _clock.now_nsec();
			std::string rec_str;
			//concat_sys_rec(rec_str, now_nsec);
			const size_t devcount = energymetric::get_devcount();
			const sysrecord &sys_rec = energymetric::get_sys_rec();
			for (size_t dev_idx = 0; dev_idx < devcount; ++dev_idx) {
				const devrecord &dev_rec = energymetric::get_dev_rec(dev_idx);
				concat_dev_rec(rec_str, sys_rec, dev_rec, now_nsec);
			};
			_influxdb.write_points(rec_str);
			printf("%s", rec_str.c_str());
		};
	};
};

int main() {
	
	//energymon<energymetric_eml, influxdbv1_client> mon_eml("sut152", "https://localhost:8086/write?db=energydb0", "root", "root");
	energymon<energymetric_eml, influxdbv1_client> mon_eml(
		getenv("MON_SYSTEM_NAME"), 
		getenv("MON_DB_URL"), 
		std::pair<std::string, std::string>(getenv("MON_DB_USERNAME"), getenv("MON_DB_PASSWORD")),
		getenv("MON_DB_NAME"));
	/*
	energymon<energymetric_eml, influxdbv2_client> mon_eml(
		getenv("MON_SYSTEM_NAME"), 
		getenv("MON_DB_URL"), 
		getenv("MON_DB_ORG_ID"), 
		getenv("MON_DB_AUTH_TOKEN"), 
		getenv("MON_DB_BUCKET"));
	*/
	mon_eml.poll();
	return 0;
};
