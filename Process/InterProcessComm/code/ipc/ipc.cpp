#include "ipc.hpp"

namespace callon
{
/**
 * @name: Open
 * @brief: Create IPC path
 * @param {IPC_TYPE} type
 * @param {ROLE_TYPE} role
 * @return {*}
 */
RET_TYPE Ipc::Open(IPC_TYPE type, ROLE_TYPE role)
{
    RET_TYPE ret = FAIL;
    int32_t fd;
    ipc_info_.type = type;
    switch (type) {
    case SOCKET:
    {
        struct sockaddr_un sun;
        int bufSize = MSG_LEN;
        socklen_t len = sizeof(bufSize);
        int sock_ret;
        ipc_info_.role_type = role;
        if (SERVER == role)
            unlink(SOCKET_SERVER_NAME);
        else
            unlink(SOCKET_CLIENT_NAME);
        fd = socket(AF_UNIX, SOCK_DGRAM, 0);
        CHK_COND_IF_RET(fd < 0, ret, "socket failed, error = %d(%s).\n", errno, strerror(errno));
        sun.sun_family = AF_UNIX;
        if (SERVER == role)
            snprintf(sun.sun_path, sizeof(sun.sun_path), "%s", SOCKET_SERVER_NAME);
        else
            snprintf(sun.sun_path, sizeof(sun.sun_path), "%s", SOCKET_CLIENT_NAME);
        if (bind(fd, (struct sockaddr*)&sun, sizeof(struct sockaddr_un)) < 0) {
            close(fd);
            LOG_ERROR("bind failed, error = %d(%s).\n", errno, strerror(errno));
            return ret;
        }
        sock_ret = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &bufSize, len);
        if (sock_ret != 0) LOG_WARN("setsockopt failed, error = %d(%s).\n", errno, strerror(errno));
        sock_ret = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &bufSize, len);
        if (sock_ret != 0) LOG_WARN("setsockopt failed, error = %d(%s).\n", errno, strerror(errno));
        ipc_info_.socket = fd;
        ret = SUCCESS;
        break;
    }
    case MQUEUE:
    {
        struct mq_attr mqa;
        mqa.mq_maxmsg = MAX_MSG_NUM;
        mqa.mq_msgsize = MSG_LEN;
        if (SERVER == role)
            fd = mq_open(MQ_FILE, O_CREAT|O_EXCL|O_RDWR, 0666, &mqa);
        else
            fd = mq_open(MQ_FILE, O_RDWR);
        if (fd < 0) {
            mq_unlink(MQ_FILE);
            LOG_ERROR("mq_open failed, error = %d(%s).\n", errno, strerror(errno));
            return ret;
        }
        ipc_info_.ipc_mq = fd;
        ret = SUCCESS;
        break;
    }
    }
    return ret;
}

/**
 * @name: Read
 * @brief: Read from IPC path
 * @param {char} *buf
 * @param {uint32_t} len
 * @param {int32_t} timeout(ms)
 * @return {*}
 */
size_t Ipc::Read(char *buf, uint32_t len, int32_t timeout)
{
    size_t ret = 0;
    switch (ipc_info_.type) {
    case SOCKET:
    {
        struct sockaddr_un addr;
        socklen_t addr_len = sizeof(addr);
        addr.sun_family = AF_UNIX;
        if (SERVER == ipc_info_.role_type)
            snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", SOCKET_CLIENT_NAME);
        else
            snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", SOCKET_SERVER_NAME);
        if (timeout <= 0) {
            ret = recvfrom(ipc_info_.socket, buf, len, 0, (struct sockaddr *)&addr, &addr_len);
        } else {
            struct timeval value;
            int select_ret;
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(ipc_info_.socket, &fds);
            value.tv_sec  = timeout / 1000;
            value.tv_usec = (timeout % 1000) * 1000;
            /* more accurate than setsockopt */
            select_ret = select(ipc_info_.socket+1, &fds, nullptr, nullptr, &value);
            if (0 == select_ret) {
                return 0;
            } else if (-1 == select_ret) {
                //LOG_ERROR("select failed, error = %d(%s).\n", errno, strerror(errno));
                return 0;
            }
            ret = recvfrom(ipc_info_.socket, buf, len, 0, (struct sockaddr *)&addr, &addr_len);
        }
        break;
    }
    case MQUEUE:
    {
        if (timeout <= 0) {
            ret = mq_receive(ipc_info_.ipc_mq, buf, len, nullptr);
        } else {
            struct timespec abs_timeout;
            clock_gettime(CLOCK_REALTIME, &abs_timeout);
            abs_timeout.tv_sec += timeout / 1000;
            abs_timeout.tv_nsec += (timeout % 1000) * 1000 * 1000;
            if (abs_timeout.tv_nsec > 1000000000) {
                abs_timeout.tv_nsec -= 1000000000;
                abs_timeout.tv_sec += 1;
            }
            ret = mq_timedreceive(ipc_info_.ipc_mq, buf, len, nullptr, &abs_timeout);
        }
        break;
    }
    }
    return ret;
}

/**
 * @name: Write
 * @brief: Write to IPC path
 * @param {char} *buf
 * @param {uint32_t} len
 * @param {int32_t} timeout(ms)
 * @return {*}
 */
RET_TYPE Ipc::Write(char *buf, uint32_t len, int32_t timeout)
{
    int ret = 0;
    RET_TYPE ret_type = FAIL;
    switch (ipc_info_.type) {
    case SOCKET:
    {
        struct sockaddr_un addr;
        socklen_t addr_len = sizeof(addr);
        addr.sun_family = AF_UNIX;
        if (SERVER == ipc_info_.role_type)
            snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", SOCKET_CLIENT_NAME);
        else
            snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", SOCKET_SERVER_NAME);
        if (timeout <= 0) {
            ret = sendto(ipc_info_.socket, buf, len, 0, (struct sockaddr *)&addr, addr_len);
        } else {
            struct timeval value;
            socklen_t value_len = sizeof(value);
            int sock_ret;
            int sec = timeout / 1000;
            int usec = (timeout % 1000) * 1000;
            sock_ret = getsockopt(ipc_info_.socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&value, &value_len);
            if ((sock_ret != 0) || (value.tv_sec != sec)
                || (value.tv_usec != usec)) {
                value.tv_sec  = sec;
                value.tv_usec = usec;
                if (sock_ret != 0) LOG_WARN("getsockopt failed, error = %d(%s).\n", errno, strerror(errno));
                /* do not need most accurate. */
                sock_ret = setsockopt(ipc_info_.socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&value, value_len);
                if (sock_ret != 0) LOG_WARN("setsockopt failed, error = %d(%s).\n", errno, strerror(errno));
            }
            ret = sendto(ipc_info_.socket, buf, len, 0, (struct sockaddr *)&addr, addr_len);
        }
        break;
    }
    case MQUEUE:
    {
        if (timeout <= 0) {
            ret = mq_send(ipc_info_.ipc_mq, buf, len, 50);
        } else {
            struct timespec abs_timeout;
            clock_gettime(CLOCK_REALTIME, &abs_timeout);
            abs_timeout.tv_sec += timeout / 1000;
            abs_timeout.tv_nsec += (timeout % 1000) * 1000 * 1000;
            if (abs_timeout.tv_nsec > 1000000000) {
                abs_timeout.tv_nsec -= 1000000000;
                abs_timeout.tv_sec += 1;
            }
            ret = mq_timedsend(ipc_info_.ipc_mq, buf, len, 50, &abs_timeout);
        }
        if (0 == ret)
            ret_type = SUCCESS;
        break;
    }
    }
    return ret_type;
}

/**
 * @name: Close
 * @brief: Close IPC path
 * @param {*}
 * @return {*}
 */
void Ipc::Close()
{
    switch (ipc_info_.type) {
    case SOCKET:
        close(ipc_info_.socket);
        break;
    case MQUEUE:
        mq_unlink(MQ_FILE);
        mq_close(ipc_info_.ipc_mq);
        break;
    }
}

}  // namespace callon
