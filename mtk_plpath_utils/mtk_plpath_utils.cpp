#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <chrono>
#include <ctime>
#include <iostream>
#include <map>
#include <thread>

#include <android-base/file.h>
#include <android-base/strings.h>
#include <android-base/unique_fd.h>
#include <android-base/properties.h>
#include <log/log.h>
#include <libdm/dm.h>

using namespace std::literals::string_literals;
using namespace android::dm;
using android::base::GetProperty;
using std::string;

#define NAME_PL_A  "pl_a"
#define NAME_PL_B  "pl_b"
#define EMMC_PL_A  "/dev/block/mmcblk0boot0"
#define EMMC_PL_B  "/dev/block/mmcblk0boot1"
#define EMMC_DEV   "/sys/class/block/mmcblk0boot0/uevent"
#define UFS_PL_A   "/dev/block/sda"
#define UFS_PL_B   "/dev/block/sdb"
#define UFS_DEV    "/sys/class/block/sda/uevent"
#define LINK_PL_A  "/dev/block/by-name/preloader_raw_a"
#define LINK_PL_B  "/dev/block/by-name/preloader_raw_b"
#define LINK1_PL_A "/dev/block/platform/bootdevice/by-name/preloader_raw_a"
#define LINK1_PL_B "/dev/block/platform/bootdevice/by-name/preloader_raw_b"
#define DM_BLK_SIZE (512)

#define PLHEAD    "MMM"
#define UFSHEAD   "UFS"
#define EMMCHEAD  "EMMC"
#define COMBOHEAD "COMB"
#define EMMCHSZ   (0x800)
#define UFSHSZ    (0x1000)
#define BLKSZ     (512)

static int create_dm(const char *device, const char *name, std::string *path, int start_blk, int blk_cnt) {
    DmTable table;
    std::unique_ptr<DmTarget> target;

    if (!device || !name) {
        ALOGE("%s device or name is null\n", __func__);
        return 1;
    }

    ALOGI("create_dm dev: %s, name %s, start %d, blks %d\n", device, name, start_blk, blk_cnt);
    target = std::make_unique<DmTargetLinear>(0, blk_cnt, device, start_blk);
    if (!table.AddTarget(std::move(target))) {
        ALOGE("Add target fail(%s)", strerror(errno));
        return 1;
    }
    DeviceMapper& dm = DeviceMapper::Instance();
    if (!dm.CreateDevice(name, table, path, std::chrono::milliseconds(500))) {
        ALOGE("Create %s on %s fail(%s)", name, device, strerror(errno));
        return 1;
    }
    ALOGI("Create %s done", (*path).c_str());
    return 0;
}

static void create_pl_link(std::string link, std::string devpath)
{
    std::string link_path;

    if (android::base::Readlink(link, &link_path) && link_path != devpath) {
        ALOGE("Remove symlink %s links to: %s", link.c_str(), link_path.c_str());
        if (!android::base::RemoveFileIfExists(link))
            ALOGE("Cannot remove symlink %s", strerror(errno));
    }

    if (symlink(devpath.c_str(), link.c_str()))
        ALOGE("Failed to symlink %s to %s (%s)", devpath.c_str(), link.c_str(), strerror(errno));
}

void create_pl_path(void) {
    int start_blk, blk_cnt, fd, isEmmc;
    off_t pl_size;
    char header_desc[5];
    std::string path_a, path_b, link_path, dev_path, link;
    DeviceMapper& dm = DeviceMapper::Instance();
    ssize_t sz = 0;

    if (!access(EMMC_DEV, F_OK)) {
        isEmmc = 1;
        fd = open(EMMC_PL_A, O_RDONLY);
    } else {
        isEmmc = 0;
        fd = open(UFS_PL_A, O_RDONLY);
    }
    if (fd < 0) {
        ALOGE("Cannot open %s (%s)", isEmmc ? EMMC_PL_A : UFS_PL_A, strerror(errno));
        return;
    }

    pl_size = lseek(fd, 0, SEEK_END);
    if (pl_size < 0) {
        ALOGE("lseek fail (%s)", strerror(errno));
        close(fd);
        return;
    }
    ALOGE("isEmmc = %d, pl_size: %ld\n", isEmmc, pl_size);
    blk_cnt = pl_size/DM_BLK_SIZE;

    if (lseek(fd, 0, SEEK_SET)) {
        ALOGE("lseek to head fail(%s)\n", strerror(errno));
        close(fd);
        return;
    }
    if ((sz = read(fd, header_desc, sizeof(header_desc))) < 0) {
        ALOGE("read fail(%s)", strerror(errno));
        close(fd);
        return;
    }
    if (sz != sizeof(header_desc))
        ALOGE("%s size is not header_desc\n", __func__);
    close(fd);

    header_desc[sizeof(header_desc)-1] = 0;
    if (!strncmp(header_desc, EMMCHEAD, strlen(EMMCHEAD))) {
        start_blk = EMMCHSZ/BLKSZ;
    } else if (!strncmp(header_desc, UFSHEAD, strlen(UFSHEAD))
        || !strncmp(header_desc, COMBOHEAD, strlen(COMBOHEAD))) {
        start_blk = UFSHSZ/BLKSZ;
    } else {
        ALOGE("Invalid header %s", header_desc);
        return;
    }
    blk_cnt -= start_blk;
    if (isEmmc) {
        if (create_dm(EMMC_PL_A, NAME_PL_A, &path_a, start_blk, blk_cnt) != 0) {
            ALOGE("Cannot create_dm %s %s", EMMC_PL_A, NAME_PL_A);
            return;
        }
        if (create_dm(EMMC_PL_B, NAME_PL_B, &path_b, start_blk, blk_cnt) != 0) {
            ALOGE("Cannot create_dm %s %s", EMMC_PL_B, NAME_PL_B);
            if (dm.DeleteDevice(NAME_PL_A))
                ALOGE("Cannot delete device %s (%s)", NAME_PL_A, strerror(errno));
            return;
        }
    } else {
        if (create_dm(UFS_PL_A, NAME_PL_A, &path_a, start_blk, blk_cnt) != 0) {
            ALOGE("Cannot create_dm %s %s", UFS_PL_A, NAME_PL_A);
            return;
        }
        if (create_dm(UFS_PL_B, NAME_PL_B, &path_b, start_blk, blk_cnt) != 0) {
            ALOGE("Cannot create_dm %s %s", UFS_PL_B, NAME_PL_B);
            if (dm.DeleteDevice(UFS_PL_A))
                ALOGE("Cannot delete device %s (%s)", NAME_PL_A, strerror(errno));
            return;
        }
    }

    create_pl_link(LINK_PL_A, path_a);
    create_pl_link(LINK_PL_B, path_b);
    create_pl_link(LINK1_PL_A, path_a);
    create_pl_link(LINK1_PL_B, path_b);

    return;
}

#define FIRST_BYTES   8
#define DEST_OFF   32

static void dump_bytes(unsigned char* array, int len)
{
    int i = 0;
    for (i = 0; i < len; i++) {
        ALOGI("[%x] ", *(array + i));
        if (i % FIRST_BYTES == FIRST_BYTES -1)
            ALOGI("\n");
    }
}

int mt_pl_handler_ab(const char * pl)
{
    int fd = 0, nBytes, dest_sz, pl_sz;
    int res = 0;
    unsigned short type, size;
    unsigned long off, tgt_off;
    unsigned char HDR[FIRST_BYTES];
    unsigned char *buf = NULL;
    ALOGI("%s\n", __func__);


    fd = open(pl, O_RDWR | O_SYNC);
    if (fd <= 0) {
        ALOGE("%s error reading commandline\n", __func__);
        res = -11;
        goto error;
    }

    pl_sz = lseek(fd, 0, SEEK_END);
    ALOGI("pl_size %x(%d)\n", pl_sz, pl_sz);

    if(lseek(fd, 0x220, SEEK_SET) == -1){
		ALOGE("lseek error\n");
        res = -12;
        goto error;
	};

    nBytes = read(fd, &HDR[0], 4);

    tgt_off = (unsigned long)((HDR[3] << 24) | (HDR[2] << 16) | (HDR[1] << 8) | HDR[0]);

    ALOGI("tgt_off %lx\n", tgt_off);

    if(lseek(fd, EMMCHSZ, SEEK_SET) == -1){
        ALOGE("lseek error\n");
        res = -12;
        goto error;
    };

    do {
        nBytes = read(fd, &HDR[0], FIRST_BYTES);
        dump_bytes(HDR, FIRST_BYTES);
        type = HDR[7] << 8 | HDR[6];
        size = HDR[5] << 8 | HDR[4];

        ALOGI("type = %x, size = %x(%d)\n", type, size, size);

        if (type == 0x12)
            break;

        if(lseek(fd, size - FIRST_BYTES, SEEK_CUR) == -1){
            ALOGE("lseek error\n");
            res = -13;
            goto error;
        }
    } while(HDR[0] == 0x4D && HDR[1] == 0x4D && HDR[2] == 0x4D && HDR[3] == 0x01);

    if(lseek(fd, FIRST_BYTES, SEEK_CUR) == -1){
        ALOGE("lseek error\n");
        res = -14;
        goto error;
	};

    nBytes = read(fd, &HDR[0], FIRST_BYTES - 4);
    off = (unsigned long)((HDR[3] << 24) | (HDR[2] << 16) | (HDR[1] << 8) | HDR[0]);

    ALOGI("off = %lx\n", off);

    if(lseek(fd, EMMCHSZ + off + DEST_OFF, SEEK_SET) == -1){
	    ALOGE("lseek error\n");
        res = -15;
        goto error;
	};

    nBytes = read(fd, &HDR[0], 4);
    dest_sz = (HDR[3] << 24) | (HDR[2] << 16) | (HDR[1] << 8) | HDR[0];

    ALOGI("dest_sz = %x(%d)\n", dest_sz, dest_sz);

    if(lseek(fd, EMMCHSZ + off, SEEK_SET) == -1){
        ALOGE("lseek error\n");
        res = -16;
        goto error;
	};

    ALOGI("read = %lx(%ld)\n", EMMCHSZ + off, EMMCHSZ + off);
    if(dest_sz > 0){
        buf = (unsigned char *)malloc(dest_sz);
        if(buf == NULL){
            ALOGE("malloc error %d\n", dest_sz);
            res = -17;
            goto error;
        }
    }
    nBytes = read(fd, buf, dest_sz);
    if(nBytes > 4)
        ALOGI("read = %x %x %x %x\n", *buf, *(buf +1), *(buf +2), *(buf +3));

    if(lseek(fd, tgt_off, SEEK_SET) == -1){
        ALOGE("lseek error\n");
        res = -18;
        goto error;
    }
    nBytes = write(fd, buf, dest_sz);
    if (nBytes != dest_sz){
        ALOGE("write %d bytes fail %d\n", dest_sz, nBytes);
        res = -19;
        goto error;
    }

error:
    if(buf != NULL)
        free(buf);
    if(fd >= 0)
        close(fd);

    return res;
}

int setrw_blockdev(const char *p){
    char cmdbuf[128] = {0,};
    int res = 0;
    snprintf(cmdbuf, sizeof(cmdbuf), "blockdev --setrw  %s", p);
    res = system(cmdbuf);
    if (res < 0){
        ALOGE("Error run %s :", cmdbuf);
        return -3;
    }
    return 0;
}

int main(void) {
    int res = 0;
    
    create_pl_path();
    
    string platform = GetProperty("ro.board.platform", "");
    printf("\n%s\n", platform.c_str());
    if (platform != "mt6739"){
        return -1;
    }
    
    res = setrw_blockdev(EMMC_PL_A);
    if (res != 0){
        printf("Error setrw_blockdev %s : %d\n", EMMC_PL_A, res);
    }
    res = setrw_blockdev(EMMC_PL_B);
    if (res != 0){
        printf("Error setrw_blockdev %s : %d\n", EMMC_PL_B, res);
    }
    res = mt_pl_handler_ab(EMMC_PL_A);
    printf("mtk_plpath_utils_a : %d\n", res);
    ALOGI("mtk_plpath_utils_a : %d\n", res);
    res = mt_pl_handler_ab(EMMC_PL_B);
    printf("mtk_plpath_utils_b : %d\n", res);
    ALOGI("mtk_plpath_utils_b : %d\n", res);

    return 0;
}