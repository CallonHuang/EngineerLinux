#include <sys/sendfile.h>
#include <dirent.h>
#include <sys/types.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

int move_dir(char *srcDir, char *dstDir)
{
    DIR *srcDirp = NULL;
    struct dirent *srcdir = NULL;
    char srcPath[512] = {0}, dstPath[512] = {0}, linkPath[128] = {0};
    struct stat scrStat, dstStat;
    int ret = 0;
    if (NULL == srcDir || NULL == dstDir) 
        return -1;
    srcDirp = opendir(srcDir);
    while ((srcdir = readdir(srcDirp)) != NULL) {
        if (0 == strcmp(srcdir->d_name, ".") || 0 == strcmp(srcdir->d_name, "..")) 
            continue;
        memset(&scrStat, 0, sizeof(scrStat));
        memset(&dstStat, 0, sizeof(dstStat));
        if (snprintf(srcPath, sizeof(srcPath), "%s/%s", srcDir, srcdir->d_name) >= sizeof(srcPath)
            || snprintf(dstPath, sizeof(dstPath), "%s/%s", dstDir, srcdir->d_name) >= sizeof(srcPath)
            || lstat(srcPath, &scrStat) != 0) {
            ret = -1;
            goto SAFE_EXIT;
        }
        if (S_ISLNK(scrStat.st_mode)) {
            if (0 == access(dstPath, F_OK)) {
                lstat(dstPath, &dstStat);
                if (S_ISLNK(dstStat.st_mode)) {
                    unlink(srcPath);
                    continue;
                }
                unlink(dstPath);
            }
            if (0 != rename(srcPath, dstPath)) {
                memset(linkPath, 0, sizeof(linkPath));
                readlink(srcPath, linkPath, sizeof(linkPath));
                symlink(linkPath, dstPath);
                unlink(srcPath);
            }
        } else if (S_ISDIR(scrStat.st_mode)) {
            if ((0 != mkdir(dstPath, S_IRWXU) && EEXIST != errno)
                || 0 != move_dir(srcPath, dstPath)) {
                ret = -1;
                goto SAFE_EXIT;
            }
        } else if (S_ISREG(scrStat.st_mode)) {
            if (0 == access(dstPath, F_OK)) {
                stat(dstPath, &dstStat);
                if (dstStat.st_size == scrStat.st_size) {
                    unlink(srcPath);
                    continue;
                }
                unlink(dstPath);
            }
            if (0 != rename(srcPath, dstPath)) {
                int srcFd = -1, dstFd = -1;
                srcFd = open(srcPath, O_RDONLY);
                dstFd = open(dstPath, O_CREAT | O_WRONLY, 0777);
                while (sendfile(dstFd, srcFd, NULL, 0x8000) > 0);
                close(srcFd);
                close(dstFd);
                chmod(dstPath, scrStat.st_mode);
                unlink(srcPath);
            }
        }
    }
SAFE_EXIT:
    closedir(srcDirp);
    rmdir(srcDir);
    return ret;
}
