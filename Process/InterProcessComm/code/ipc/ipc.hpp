#ifndef _CALLON_IPC_H_
#define _CALLON_IPC_H_

#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <mqueue.h>
#include <errno.h>
#include <sys/un.h>

#include <list>
#include <map>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>
#include <utility>
#include <iostream>

namespace callon
{

#define MQ_FILE      "/master_ipc.queue"
#define SOCKET_SERVER_NAME  "/var/master_server.sock"
#define SOCKET_CLIENT_NAME  "/var/master_client.sock"
#define MAX_MSG_NUM  (5)
#define MSG_LEN      sizeof(IPC_MSG)
#define WRITE_LOG(fp, type, ...)   {                                                            \
            char file_basename[] = __FILE__;                                                    \
            struct timeval now = { 0, 0 };                                                      \
            struct tm sys_tm;                                                                   \
            gettimeofday(&now, NULL);                                                           \
            localtime_r(&(now.tv_sec), &sys_tm);                                                \
            fprintf(fp, "%s<%04d-%02d-%02d %02d:%02d:%02d,%03d>(FILE %s, LINE %d): ",           \
                #type, sys_tm.tm_year + 1900, sys_tm.tm_mon + 1, sys_tm.tm_mday,                \
                sys_tm.tm_hour, sys_tm.tm_min, sys_tm.tm_sec, now.tv_usec / 1000,               \
                basename(file_basename), __LINE__);                                             \
            fprintf(fp, __VA_ARGS__);                                                           \
            fflush(fp);                                                                         \
            if (fp != stdout && fp != stderr)  fclose(fp);                                      \
        }
#define LOG_WARN(...)                                                                       \
    do {                                                                                    \
        FILE* out = stdout;                                                                 \
        WRITE_LOG(out, [INFO], __VA_ARGS__)                                                 \
    } while (false)

#define LOG_ERROR(...)                                                                      \
    do {                                                                                    \
        FILE* out = stderr;                                                                 \
        WRITE_LOG(out, [ERROR], __VA_ARGS__)                                                \
    } while (false)

#define CHK_COND_IF_RET(condition, ret, ...)            \
    do {                                                \
        if (condition) {                                \
            LOG_ERROR(__VA_ARGS__);                     \
            return ret;                                 \
        }                                               \
    } while (0)

#define CHK_COND_IF_NOT_RET(condition, ret, ...)        \
    do {                                                \
        if (!(condition)) {                             \
            LOG_ERROR(__VA_ARGS__);                     \
            return ret;                                 \
        }                                               \
    } while (0)

typedef enum {
    SUCCESS,
    FAIL,
    NULLPTR,
    STR_EMPTY,
    UNKNOWN_TYPE,
} RET_TYPE;

template <typename T>
class Singleton {
private:
    static std::unique_ptr<T> instance_;
public:
    template <typename... ArgTypes>
    static T* GetInstance(ArgTypes&&... args) {
        static std::once_flag of;
        std::call_once(of, [&]() {
            Singleton::instance_.reset(new T(std::forward<ArgTypes>(args)...));
        });
        return instance_.get();
    }

    explicit Singleton(T&&) = delete;
    explicit Singleton(const T&) = delete;
    void operator= (const T&) = delete;

protected:
    Singleton() = default;
    virtual ~Singleton() = default;
};

template <typename T>
std::unique_ptr<T> Singleton<T>::instance_ = nullptr;

typedef enum {
    SOCKET,
    MQUEUE,
} IPC_TYPE;

typedef enum {
    SERVER,
    CLIENT,
} ROLE_TYPE;

typedef struct {
    IPC_TYPE type;
    union {
        int32_t ipc_mq;
        struct {
            int32_t socket;
            ROLE_TYPE role_type;
        };
    };
} IPC_INFO;

typedef struct {
    int type;
    union {
        /* data */
        void *type_one_param;
        void *type_two_param;
    };
} IPC_MSG;



class Ipc : public Singleton<Ipc> {
public:
    /**
     * @name: Open
     * @brief: Create IPC path
     * @param {IPC_TYPE} type
     * @param {ROLE_TYPE} role
     * @return {*}
     */
    RET_TYPE Open(IPC_TYPE type, ROLE_TYPE role);
    /**
     * @name: Read
     * @brief: Read from IPC path
     * @param {char} *buf
     * @param {uint32_t} len
     * @param {int32_t} timeout(ms)
     * @return {*}
     */
    size_t Read(char *buf, uint32_t len, int32_t timeout);
    /**
     * @name: Write
     * @brief: Write to IPC path
     * @param {char} *buf
     * @param {uint32_t} len
     * @param {int32_t} timeout(ms)
     * @return {*}
     */
    RET_TYPE Write(char *buf, uint32_t len, int32_t timeout);
    /**
     * @name: Close
     * @brief: Close IPC path
     * @param {*}
     * @return {*}
     */
    void Close();
private:
    IPC_INFO ipc_info_;
};

}  // namespace callon

#endif  // _CALLON_IPC_H_
