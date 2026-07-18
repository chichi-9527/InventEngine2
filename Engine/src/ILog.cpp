#include "ILog.h"

#include "IEngineTools.h"

#include <vector>
#include <string>
#include <fstream>
#include <format>
#include <chrono>
#include <clocale>
#include <iostream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "Windows.h"
#endif // _WIN32


namespace INVENT
{
	constexpr const char* out_color_red = "\033[31m";
	constexpr const char* out_color_green = "\033[32m";
	constexpr const char* out_color_yellow = "\033[33m";
	constexpr const char* out_color_blue = "\033[34m";
	constexpr const char* out_color_magenta = "\033[35m";
	constexpr const char* out_color_cyan = "\033[36m";
	constexpr const char* out_color_white = "\033[37m";
	constexpr const char* out_color_reset = "\033[0m";

	void ILog::Init(const ILogInitInfo& info)
	{
		//std::setlocale(LC_ALL, "");
		// init windows
#ifdef _WIN32
		// 启用 ANSI 转义序列支持
		auto enableVTMode = []() {
			HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
			if (hOut == INVALID_HANDLE_VALUE)
			{
				return;
			}

			DWORD dwMode = 0;
			if (!GetConsoleMode(hOut, &dwMode))
			{
				return;
			}

			dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
			if (!SetConsoleMode(hOut, dwMode))
			{
				return;
			}
			};
		enableVTMode();

		SetConsoleOutputCP(CP_UTF8);
#endif // !_WIN32

		// init log
		ILog::_logs.resize(ILog::_log_queue_size);

		ILog::_work_thread = std::jthread{ ILog::_process_logs };

		if (info.LogfilePath.empty() ||
			info.LogfileName.empty())
		{
			return;
		}

		ILog::_write_file = true;
		ILog::_current_path = std::filesystem::path{ info.LogfilePath } / (info.LogfileName + ".txt");
		if (auto path = ILog::_current_path.parent_path(); !path.empty())
		{
			std::filesystem::create_directories(path);
		}
		std::ofstream out(ILog::_current_path, std::ios::out | std::ios::trunc);
		if (out.is_open())
		{
			out << "\xEF\xBB\xBF";
			out << "Log start\n";
		}
		
	}
	void ILog::Info(const std::string& log)
	{
		std::string formattedLog = std::format("{}[{}][{}]:{}{}\n",
			out_color_green, ILog::_get_current_time(), "INFO", log, out_color_reset);

		ILog::_add_log(formattedLog);
	}

	void ILog::Warning(const std::string & log)
	{
		std::string formattedLog = std::format("{}[{}][{}]:{}{}\n",
			out_color_yellow, ILog::_get_current_time(), "WARNING", log, out_color_reset);

		ILog::_add_log(formattedLog);
	}

	void ILog::Debug(const std::string & log)
	{
		std::string formattedLog = std::format("{}[{}][{}]:{}{}\n",
			out_color_blue, ILog::_get_current_time(), "DEBUG", log, out_color_reset);

		ILog::_add_log(formattedLog);
	}

	void ILog::Error(const std::string & log)
	{
		std::string formattedLog = std::format("{}[{}][{}]:{}{}\n",
			out_color_red, ILog::_get_current_time(), "ERROR", log, out_color_reset);

		ILog::_add_log(formattedLog);
	}

	void ILog::Trace(const std::string & log)
	{
		std::string formattedLog = std::format("{}[{}][{}]:{}{}\n",
			out_color_cyan, ILog::_get_current_time(), "TRACE", log, out_color_reset);

		ILog::_add_log(formattedLog);
	}

	void ILog::Fatal(const std::string & log)
	{
		std::string formattedLog = std::format("{}[{}][{}]:{}{}\n",
			out_color_magenta, ILog::_get_current_time(), "FATAL", log, out_color_reset);

		ILog::_add_log(formattedLog);
	}

	std::string ILog::_get_current_time()
	{
		return std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::system_clock::now());
	}

	void ILog::_process_logs(std::stop_token stop_token)
	{
		while (!stop_token.stop_requested())
		{
			uint64_t currentStart = _log_start.load(std::memory_order_relaxed);
			uint64_t currentEnd = _log_end.load(std::memory_order_relaxed);
			if (currentStart == currentEnd)
			{
				_notifier.store(0, std::memory_order_release);
				if (_log_start.load(std::memory_order_relaxed) == _log_end.load(std::memory_order_relaxed))
				{
					_notifier.wait(0, std::memory_order_acquire);
				}
				// 被唤醒
				ILog::_flush_queue();
			}
			ILog::_flush_queue();
		}
	}

	void ILog::_flush_queue()
	{
		std::shared_lock<std::shared_mutex> lock(ILog::_resize_mutex);
		uint64_t currentStart = _log_start.load(std::memory_order_relaxed);
		uint64_t currentEnd = _log_end.load(std::memory_order_acquire);
		if (currentStart == currentEnd)
		{
			return;
		}
		std::ofstream out;
		if (ILog::_write_file)
		{
			out = std::ofstream(ILog::_current_path, std::ios::out | std::ios::app);
		}
		while (currentStart != currentEnd)
		{
			const auto& logMsg = _logs[currentStart];
			std::cout << logMsg;
			if (ILog::_write_file && out.is_open())
			{
				out << logMsg;
			}
			currentStart = (currentStart + 1) % _log_queue_size;
		}
		if (out.is_open()) out.flush();
		_log_start.store(currentEnd, std::memory_order_release);
	}

	void ILog::_resize_queue()
	{
		std::unique_lock<std::shared_mutex> writeLock(_resize_mutex);
		if ((_log_end.load() + 1) % _log_queue_size == _log_start.load())
		{
			size_t oldSize = _log_queue_size;
			_log_queue_size *= 2;
			std::vector<std::string> newLogs(_log_queue_size);
			size_t i = 0;
			uint64_t s = _log_start.load();
			uint64_t e = _log_end.load();
			while (s != e)
			{
				newLogs[i++] = std::move(_logs[s]);
				s = (s + 1) % oldSize;
			}
			_logs = std::move(newLogs);
			_log_start.store(0, std::memory_order_release);
			_log_end.store(i, std::memory_order_release);
		}
	}

	void ILog::_add_log(std::string& final_log)
	{
		while (true)
		{
			std::shared_lock<std::shared_mutex> lock(_resize_mutex);
			uint64_t currentStart = _log_start.load(std::memory_order_relaxed);
			uint64_t currentEnd = _log_end.load(std::memory_order_acquire);
			uint64_t nextEnd = (currentEnd + 1) % _log_queue_size;
			if (nextEnd == currentStart)
			{
				lock.unlock();
				ILog::_resize_queue();
				continue;
			}
			if (_log_end.compare_exchange_weak(currentEnd, nextEnd, std::memory_order_release, std::memory_order_relaxed))
			{
				_logs[currentEnd] = std::move(final_log);
				break;
			}
		}
		if (_notifier.exchange(1, std::memory_order_release) == 0)
		{
			_notifier.notify_one();
		}
	}
}

void INVENT_DLL InventLogInit(const char* file_path, const char* file_name)
{
	INVENT::ILog::Init({ file_path,file_name });
}

void INVENT_DLL InventLogInfo(const char* log)
{
	
}
