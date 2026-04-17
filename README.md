# shared-libs-cpp17

A modern C++17 systems library for building multithreaded network servers on Linux.

Originally developed for real-time streaming servers (3G video call, multimedia RingBack Tone) in production telecom environments, this library has been modernized from legacy C++03/pthreads to C++17 standard library primitives.

## Features

| Component | Description |
|-----------|-------------|
| **Threading** | `std::thread`-based Thread/QThread, event queues, producer-consumer pattern |
| **Synchronization** | `std::mutex`, `std::condition_variable`, `std::atomic`, RAII locks |
| **Networking** | TCP/UDP server with per-connection threads, Unix/Inet/Raw sockets |
| **Database** | PostgreSQL connection pool with RAII guards (`std::optional`, move-only) |
| **IPC** | POSIX shared memory, System V message queues, semaphores |
| **Logging** | Async rotating file logger (Meyers singleton, producer-consumer queue) |
| **Utilities** | INI parser, binary archive, circular queue, timer management, ICMP monitor |

## Modern C++ Highlights

- **C++17 standard**, no compiler extensions
- **RAII everywhere** — `AutoLock` (lock_guard), `PgGuard` (scoped DB connection), `File`, `Archive`
- **Smart pointers** — `std::unique_ptr` for owned resources, move-only types
- **Standard threading** — `std::thread`, `std::mutex`, `std::condition_variable`, `std::atomic` with explicit memory ordering
- **Modern idioms** — `std::optional`, `std::function`, `[[nodiscard]]`, `noexcept`, lambda captures, move semantics
- **Code quality** — clang-format (Google-based), clang-tidy, `-Wall -Wextra -Wpedantic`

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_WITH_POSTGRESQL` | ON | Enable PostgreSQL connection pool |
| `BUILD_TESTING` | OFF | Build GoogleTest tests |

```bash
# Without PostgreSQL
cmake -B build -DBUILD_WITH_POSTGRESQL=OFF

# With tests
cmake -B build -DBUILD_TESTING=ON
cmake --build build -j$(nproc)
ctest --test-dir build
```

## Requirements

- CMake 3.16+
- GCC or Clang with C++17 support
- Linux
- PostgreSQL dev libraries (optional, for connection pool)

## Project Structure

```
libpdk/
├── include/          # Public headers (25 .hpp files)
│   ├── jthread.hpp   # Thread, Mutex, Semaphore, AutoLock
│   ├── qthread.hpp   # Event-driven thread with message queue
│   ├── sock.hpp      # Socket hierarchy (TCP, UDP, Unix, Raw)
│   ├── tcpserver.hpp # Multi-threaded TCP server
│   ├── pgpool.hpp    # PostgreSQL connection pool (RAII)
│   ├── logger.hpp    # Async rotating file logger
│   ├── sharedmemory.hpp # POSIX shared memory + IPC queues
│   └── ...
├── src/              # Implementation files (13 .cpp files)
├── test/             # GoogleTest tests
├── CMakeLists.txt
├── .clang-format     # Google-based C++17 formatting
└── .clang-tidy       # Static analysis rules
```

## License

MIT
