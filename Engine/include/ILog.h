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

	class ILog
	{
	public:
		static INVENT_API void Init(const ILogInitInfo& info = {});

		static INVENT_API void Info(const std::string& log);
		static INVENT_API void Warning(const std::string& log);
		static INVENT_API void Debug(const std::string& log);
		static INVENT_API void Error(const std::string& log);
		static INVENT_API void Trace(const std::string& log);
		static INVENT_API void Fatal(const std::string& log);

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
	INVENT_API void INVENT_DLL InventLogWarning(const char* log);
	INVENT_API void INVENT_DLL InventLogDebug(const char* log);
	INVENT_API void INVENT_DLL InventLogError(const char* log);
	INVENT_API void INVENT_DLL InventLogTrace(const char* log);
	INVENT_API void INVENT_DLL InventLogFatal(const char* log);
#ifdef __cplusplus
}
#endif // _cplusplus


#ifdef SHOW_LOGS
#define INVENT_LOG_INFO(X)		INVENT::ILog::Info(X)
#define INVENT_LOG_WARNING(X)	INVENT::ILog::Warning(X)
#define INVENT_LOG_DEBUG(X)		INVENT::ILog::Debug(X)
#define INVENT_LOG_ERROR(X)		INVENT::ILog::Error(X)
#define INVENT_LOG_TRACE(X)		INVENT::ILog::Trace(X)
#define INVENT_LOG_FATAL(X)		INVENT::ILog::Fatal(X)
#else
#define INVENT_LOG_INFO(X)
#define INVENT_LOG_WARNING(X)
#define INVENT_LOG_DEBUG(X)
#define INVENT_LOG_ERROR(X)
#define INVENT_LOG_TRACE(X)
#define INVENT_LOG_FATAL(X)
#endif // SHOW_LOGS

