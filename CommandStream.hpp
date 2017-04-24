#ifndef COMMANDSTREAM_HPP
#define COMMANDSTREAM_HPP

#include <cstdint>
#include <map>
#include <mutex>
#include <vector>
#include <memory>

namespace Imperative
{
	class CommandStream
	{
	public:
		// If you use more than 4 billion commands for some insane reason, you can up this.
		using Id = uint32_t;

		// Default uint32_t for cross-architecture sending commands via sockets (coming soon). 
		// You can up this to uint64_t if you are 64 bit only.
		// Or size_t if you don't use sockets at all.
		using Size = uint32_t;
		static const constexpr Size AnySize = (uint64_t)(-1);

		struct HandlerFunction
		{
			using Function = void(*)(void*);

			Size sizeMin;
			Size sizeMax;
			Function ptr;
			Id id;
		};

		struct Command
		{
			Size size;
			void* data;
			Id id;
		};

		// Serialized Command
		// Id Size Data ...
		// No separation
		using SerializedCommand = char*;

		static inline Command DeserializeCommand(const SerializedCommand cmd)
		{
			Command command;
			command.id = *(cmd);
			command.size = *(cmd + sizeof(Id));

			command.data = new char[command.size];

			memcpy(command.data, cmd + sizeof(Id) + sizeof(Size), command.size);

			return command;
		}

		static inline SerializedCommand SerializeCommand(const Command& command)
		{
			SerializedCommand serialized = new char[sizeof(Id) + sizeof(Size) + command.size];
			if (!serialized)
				return nullptr;

			memcpy(serialized, &(command.id), sizeof(Id));
			memcpy(serialized + sizeof(Id), &(command.size), sizeof(Size));
			memcpy(serialized + sizeof(Id) + sizeof(Size), command.data, command.size);

			return serialized;
		}

		inline void ProcessSerializedCommand(SerializedCommand command)
		{
			SendCommand(DeserializeCommand(command));
		}

		inline void RegisterHandler(Id id, HandlerFunction::Function functionPtr, Size size)
		{
			RegisterHandler(id, functionPtr, size, size);
		}

		inline void RegisterHandler(Id id, HandlerFunction::Function functionPtr, Size sizeMin, Size sizeMax)
		{
			HandlerFunction handler;
			handler.id = id;
			handler.sizeMin = sizeMin;
			handler.sizeMax = sizeMax;
			handler.ptr = functionPtr;

			RegisterHandler(handler);
		}

		inline void RegisterHandler(HandlerFunction func)
		{
			std::lock_guard<std::mutex> guard(m_functionHandlerMutex);
			m_functionHandlers.insert(std::pair<Id, HandlerFunction>(func.id, func));
		}

		inline void SendCommand(Command command)
		{
			std::lock_guard<std::mutex> guard(m_commandMutex);
			m_commands.push_back(command);
		}

		inline static Command CreateCommand(Id id, Size size, void* data)
		{
			Command command;
			command.id = id;
			command.size = size;
			command.data = new char[size];

			memcpy(command.data, data, size);

			return command;
		}

		inline void SendCommand(Id id, Size size, void* data)
		{
			SendCommand(CreateCommand(id, size, data));
		}

		template <typename T>
		inline static Command CreateCommand(Id id, T& data)
		{
			Command command;
			command.id = id;
			command.size = sizeof(T);
			command.data = malloc(sizeof(T));

			*((T*)data) = data;

			return command;
		}

		template <typename T>
		inline void SendCommand(Id id, T& data)
		{
			SendCommand(CreateCommand(id, data));
		}

		/*template <typename ... Args>
		inline static Command CreateCommand(Id id, Args... args)
		{
			Command command;
			command.id = id;
			command.size = sizeof...(args);
			command.data = malloc(sizeof...(args));

			memcpy(command.data, make_array(args...).begin(), sizeof...(args));

			return command;
		}

		template <typename ... Args>
		inline void SendCommand(Id id, Args... args)
		{
			SendCommand(CreateCommand(id, args...));
		}*/

		inline void Update()
		{
			std::lock_guard<std::mutex> guard(m_commandMutex);
			{
				std::lock_guard<std::mutex> guard(m_functionHandlerMutex);
				for (auto command : m_commands)
				{
					if (m_functionHandlers.find(command.id) != m_functionHandlers.end())
					{
						auto function = m_functionHandlers[command.id];
						if (function.sizeMin == AnySize || !(function.sizeMin > command.size || function.sizeMax < command.size))
							function.ptr(command.data);
					}
					delete command.data;
				}
			}
			m_commands.clear();
		}

	private:

		std::mutex m_functionHandlerMutex;
		std::map<Id, HandlerFunction> m_functionHandlers;

		std::mutex m_commandMutex;
		std::vector<Command> m_commands;
	};
}

#endif