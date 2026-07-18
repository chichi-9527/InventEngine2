#pragma once

#include "EngineAPI.h"

#include <cstdint>
#include <vector>
#include <string>
#include <atomic>
#include <shared_mutex>
#include <filesystem>
#include <thread>
#include <stop_token>

namespace INVENT
{
	struct ILogInitInfo
	{
		std::string LogfilePath;
		std::string LogfileName;
	};

	class INVENT_API ILog
	{
	public:
		static void Init(const ILogInitInfo& info = {});

		static void Info(const std::string& log);
		static void Warning(const std::string& log);
		static void Debug(const std::string& log);
		static void Error(const std::string& log);
		static void Trace(const std::string& log);
		static void Fatal(const std::string& log);

	private:
		static std::string _get_current_time();
		static void _process_logs(std::stop_token stop_token);
		static void _flush_queue();
		static void _resize_queue();
		static void _add_log(std::string& final_log);
	private:
		static inline std::vector<std::string> _logs;
		static inline std::atomic_uint64_t _log_start{ 0 };
		static inline std::atomic_uint64_t _log_end{ 0 };
		static inline size_t _log_queue_size{ 100 };
		static inline std::jthread _work_thread;
		static inline std::atomic_int _notifier{ 0 };
		static inline std::shared_mutex _resize_mutex;
		static inline std::filesystem::path _current_path;
		static inline bool _write_file{ false };
	};
}


#ifdef __cplusplus
extern "C"
{
#endif // _cplusplus
	INVENT_API void INVENT_DLL InventLogInit(const char* file_path, const char* file_name);
	INVENT_API void INVENT_DLL InventLogInfo(const char* log);
#ifdef __cplusplus
}
#endif // _cplusplus
