#pragma once
#include "plugin-main.h"
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <functional>
#include <chrono>
#include <Processing.NDI.Lib.h>

class NDIFinder {
public:
	using Callback = std::function<void(void *)>;
	static std::vector<std::string> getNDISourceList(Callback callback);

	// Remembers the URL of every NDI source seen by a process-wide finder, so that a reconnect can address a source
	// by URL when NDI discovery in this process no longer resolves its name. Entries are never removed.
	static void startUrlCache();
	static void stopUrlCache();
	static std::string lookupUrl(const char *ndi_name);

private:
	static std::vector<std::string> NDISourceList;
	static std::chrono::time_point<std::chrono::steady_clock> lastRefreshTime;
	static std::mutex listMutex;
	static bool isRefreshing;
	static void refreshNDISourceList(Callback callback);
	static void retrieveNDISourceList();
};
