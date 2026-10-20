#include "ndi-finder.h"

#include <atomic>
#include <map>

std::vector<std::string> NDIFinder::NDISourceList;
std::chrono::time_point<std::chrono::steady_clock> NDIFinder::lastRefreshTime;
std::mutex NDIFinder::listMutex;
bool NDIFinder::isRefreshing = false;

std::vector<std::string> NDIFinder::getNDISourceList(Callback callback)
{
	std::lock_guard<std::mutex> lock(listMutex);
	auto now = std::chrono::steady_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - lastRefreshTime).count();

	if (duration > 5 && !isRefreshing) {
		isRefreshing = true;
		std::thread(refreshNDISourceList, callback).detach();
	}

	return NDISourceList;
}

void NDIFinder::refreshNDISourceList(Callback callback)
{
	retrieveNDISourceList();
	std::lock_guard<std::mutex> lock(listMutex);
	lastRefreshTime = std::chrono::steady_clock::now();
	callback(&NDISourceList);
	isRefreshing = false;
}

void NDIFinder::retrieveNDISourceList()
{
	// Safety check to avoid crash if the Lib is not loaded.
	if (!ndiLib) {
		return;
	}

	NDIlib_find_create_t find_desc = {0};
	find_desc.show_local_sources = true;
	find_desc.p_groups = NULL;
	NDIlib_find_instance_t ndi_find = ndiLib->find_create_v2(&find_desc);

	if (!ndi_find) {
		return;
	}

	uint32_t n_sources = 0;
	uint32_t last_n_sources = 0;
	const NDIlib_source_t *sources = NULL;
	do {
		ndiLib->find_wait_for_sources(ndi_find, 1000);
		last_n_sources = n_sources;
		sources = ndiLib->find_get_current_sources(ndi_find, &n_sources);
	} while (n_sources > last_n_sources);

	std::vector<std::string> newList;
	for (uint32_t i = 0; i < n_sources; ++i) {
		newList.push_back(sources[i].p_ndi_name);
	}

	{
		std::lock_guard<std::mutex> lock(listMutex);
		NDISourceList = newList;
	}

	ndiLib->find_destroy(ndi_find);
}

static std::mutex url_cache_mutex;
static std::map<std::string, std::string> url_cache;
static std::thread url_cache_thread;
static std::atomic<bool> url_cache_stop{false};

void NDIFinder::startUrlCache()
{
	if (!ndiLib || url_cache_thread.joinable())
		return;
	url_cache_stop = false;
	url_cache_thread = std::thread([] {
		NDIlib_find_create_t find_desc = {0};
		find_desc.show_local_sources = true;
		find_desc.p_groups = NULL;
		NDIlib_find_instance_t ndi_find = ndiLib->find_create_v2(&find_desc);
		if (!ndi_find) {
			obs_log(LOG_WARNING, "NDI source URL cache: find_create_v2 failed");
			return;
		}
		while (!url_cache_stop.load()) {
			ndiLib->find_wait_for_sources(ndi_find, 1000);
			uint32_t n_sources = 0;
			const NDIlib_source_t *sources = ndiLib->find_get_current_sources(ndi_find, &n_sources);
			std::lock_guard<std::mutex> lock(url_cache_mutex);
			for (uint32_t i = 0; i < n_sources; ++i) {
				const char *name = sources[i].p_ndi_name;
				const char *url = sources[i].p_url_address;
				if (!name || !url || !*url)
					continue;
				auto it = url_cache.find(name);
				if (it == url_cache.end() || it->second != url) {
					obs_log(LOG_INFO, "NDI source URL remembered: '%s' at %s", name, url);
					url_cache[name] = url;
				}
			}
		}
		ndiLib->find_destroy(ndi_find);
	});
}

void NDIFinder::stopUrlCache()
{
	url_cache_stop = true;
	if (url_cache_thread.joinable())
		url_cache_thread.join();
}

std::string NDIFinder::lookupUrl(const char *ndi_name)
{
	if (!ndi_name)
		return {};
	std::lock_guard<std::mutex> lock(url_cache_mutex);
	auto it = url_cache.find(ndi_name);
	return it == url_cache.end() ? std::string() : it->second;
}
