
#include "fsck.h"
#include <libgen.h>
#include <ctype.h>
#include <time.h>
#include <getopt.h>
#include <stdbool.h>
#include "quotaio.h"
#include "compress.h"
#include "f2fs_fs.h"
#ifdef WITH_INJECT
#include "inject.h"
#else
static void inject_usage(void)
{
	MSG(0, "\ninject.f2fs not supported\n");
	exit(1);
}
#endif

struct f2fs_fsck gfsck;

INIT_FEATURE_TABLE;

static char *absolute_path(const char *file)
{
	char *ret;
	char cwd[PATH_MAX];

	if (file[0] != '/') {
		if (getcwd(cwd, PATH_MAX) == NULL) {
			fprintf(stderr, "Failed to getcwd\n");
			exit(EXIT_FAILURE);
		}
		ret = malloc(strlen(cwd) + 1 + strlen(file) + 1);
		if (ret)
			sprintf(ret, "%s/%s", cwd, file);
	} else
		ret = strdup(file);
	return ret;
}

void dump_usage()
{
	MSG(0, "\nUsage: dump.f2fs [options] device\n");
	MSG(0, "[options]:\n");
	MSG(0, "  -d debug level [default:0]\n");
	MSG(0, "  -i inode no (hex)\n");
	MSG(0, "  -I inode no (hex) scan full disk\n");
	MSG(0, "  -n [NAT dump nid from #1~#2 (decimal), for all 0~-1]\n");
	MSG(0, "  -M show a block map\n");
	MSG(0, "  -s [SIT dump segno from #1~#2 (decimal), for all 0~-1]\n");
	MSG(0, "  -S sparse_mode\n");
	MSG(0, "  -a [SSA dump segno from #1~#2 (decimal), for all 0~-1]\n");
	MSG(0, "  -b blk_addr (in 4KB)\n");
	MSG(0, "  -r dump out from the root inode\n");
	MSG(0, "  -f do not prompt before dumping\n");
	MSG(0, "  -H support write hint\n");
	MSG(0, "  -y alias for -f\n");
	MSG(0, "  -o dump inodes to the given path\n");
	MSG(0, "  -P preserve mode/owner/group for dumped inode\n");
	MSG(0, "  -L Preserves symlinks. Otherwise symlinks are dumped as regular files.\n");
	MSG(0, "  -V print the version number and exit\n");

	exit(1);
}

int is_digits(char *optarg)
{
	unsigned int i;

	for (i = 0; i < strlen(optarg); i++)
		if (!isdigit(optarg[i]))
			break;
	return i == strlen(optarg);
}

static void error_out(char *prog)
{
	 if (!strcmp("inject.f2fs", prog))
		inject_usage();
	else
		MSG(0, "\nWrong program.\n");
}

static void __add_fsck_options(void)
{
	/* -a */
	c.auto_fix = 1;
}

static void add_default_options(void)
{
	switch (c.defset) {
	case CONF_ANDROID:
		__add_fsck_options();
	}
	c.quota_fix = 1;
}

void f2fs_parse_options(int argc, char *argv[])
{
	int option = 0;
	char *prog = basename(argv[0]);
	int err = NOERROR;
	int i;

	/* Allow prog names (e.g, sload_f2fs, fsck_f2fs, etc) */
	for (i = 0; i < strlen(prog); i++) {
		if (prog[i] == '_')
			prog[i] = '.';
	}

	if (argc < 2) {
		MSG(0, "\tError: Device not specified\n");
		error_out(prog);
	}

	if (!strcmp("dump.f2fs", prog)) {
		const char *option_string = "d:fi:I:n:LMo:Prs:Sa:b:Vy";
		static struct dump_option dump_opt = {
			.nid = 0,	/* default root ino */
			.start_nat = -1,
			.end_nat = -1,
			.start_sit = -1,
			.end_sit = -1,
			.start_ssa = -1,
			.end_ssa = -1,
			.blk_addr = -1,
			.scan_nid = 0,
			.use_root_nid = 0,
			.base_path = NULL,
		};
		c.func = DUMP;
		while ((option = getopt(argc, argv, option_string)) != EOF) {
			int ret = 0;

			switch (option) {
			case 'd':
				if (!is_digits(optarg)) {
					err = EWRONG_OPT;
					break;
				}
				c.dbg_lv = atoi(optarg);
				MSG(0, "Info: Debug level = %d\n",
							c.dbg_lv);
				break;
			case 'i':
				if (strncmp(optarg, "0x", 2))
					ret = sscanf(optarg, "%d",
							&dump_opt.nid);
				else
					ret = sscanf(optarg, "%x",
							&dump_opt.nid);
				break;
			case 'I':
				if (strncmp(optarg, "0x", 2))
					ret = sscanf(optarg, "%d",
							&dump_opt.scan_nid);
				else
					ret = sscanf(optarg, "%x",
							&dump_opt.scan_nid);
				break;
			case 'n':
				ret = sscanf(optarg, "%d~%d",
							&dump_opt.start_nat,
							&dump_opt.end_nat);
				break;
			case 'M':
				c.show_file_map = 1;
				break;
			case 's':
				ret = sscanf(optarg, "%d~%d",
							&dump_opt.start_sit,
							&dump_opt.end_sit);
				break;
			case 'S':
				c.sparse_mode = 1;
				break;
			case 'a':
				ret = sscanf(optarg, "%d~%d",
							&dump_opt.start_ssa,
							&dump_opt.end_ssa);
				break;
			case 'b':
				if (strncmp(optarg, "0x", 2))
					ret = sscanf(optarg, "%d",
							&dump_opt.blk_addr);
				else
					ret = sscanf(optarg, "%x",
							&dump_opt.blk_addr);
				break;
			case 'y':
			case 'f':
				c.force = 1;
				break;
			case 'r':
				dump_opt.use_root_nid = 1;
				break;
			case 'o':
				dump_opt.base_path = absolute_path(optarg);
				break;
			case 'P':
				c.preserve_perms = 1;
				break;
			case 'L':
				c.preserve_symlinks = 1;
				break;
			case 'V':
				show_version(prog);
				exit(0);
			default:
				err = EUNKNOWN_OPT;
				break;
			}
			ASSERT(ret >= 0);
			if (err != NOERROR)
				break;
		}
		c.private = &dump_opt;
	}

	if (err == NOERROR) {
		add_default_options();

		if (optind >= argc) {
			MSG(0, "\tError: Device not specified\n");
			error_out(prog);
		}

		c.devices[0].path = strdup(argv[optind]);
		if (argc > (optind + 1)) {
			c.dbg_lv = 0;
			err = EUNKNOWN_ARG;
		}
		if (err == NOERROR)
			return;
	}

	check_block_struct_sizes();
	/* print out error */
	switch (err) {
	case EWRONG_OPT:
		MSG(0, "\tError: Wrong option -%c %s\n", option, optarg);
		break;
	case ENEED_ARG:
		MSG(0, "\tError: Need argument for -%c\n", option);
		break;
	case EUNKNOWN_OPT:
		MSG(0, "\tError: Unknown option %c\n", option);
		break;
	case EUNKNOWN_ARG:
		MSG(0, "\tError: Unknown argument %s\n", argv[optind]);
		break;
	}
	error_out(prog);
}

static void do_dump(struct f2fs_sb_info *sbi)
{
	struct dump_option *opt = (struct dump_option *)c.private;
	struct f2fs_checkpoint *ckpt = F2FS_CKPT(sbi);
	u32 flag = le32_to_cpu(ckpt->ckpt_flags);

	if (opt->use_root_nid)
		opt->nid = sbi->root_ino_num;

	if (opt->end_nat == -1)
		opt->end_nat = NM_I(sbi)->max_nid;
	if (opt->end_sit == -1)
		opt->end_sit = SM_I(sbi)->main_segments;
	if (opt->end_ssa == -1)
		opt->end_ssa = SM_I(sbi)->main_segments;
	if (opt->start_nat != -1)
		nat_dump(sbi, opt->start_nat, opt->end_nat);
	if (opt->start_sit != -1)
		sit_dump(sbi, opt->start_sit, opt->end_sit);
	if (opt->start_ssa != -1)
		ssa_dump(sbi, opt->start_ssa, opt->end_ssa);
	if (opt->blk_addr != -1)
		dump_info_from_blkaddr(sbi, opt->blk_addr);
	if (opt->nid)
		dump_node(sbi, opt->nid, c.force, opt->base_path, 1, 1, NULL);
	if (opt->scan_nid)
		dump_node_scan_disk(sbi, opt->scan_nid);

	print_cp_state(flag);

}

#ifdef HAVE_MACH_TIME_H
static u64 get_boottime_ns()
{
	return mach_absolute_time();
}
#elif defined(HAVE_CLOCK_GETTIME) && defined(HAVE_CLOCK_BOOTTIME)
static u64 get_boottime_ns()
{
	struct timespec t;
	t.tv_sec = t.tv_nsec = 0;
	clock_gettime(CLOCK_BOOTTIME, &t);
	return (u64)t.tv_sec * 1000000000LL + t.tv_nsec;
}
#else
static u64 get_boottime_ns()
{
	return 0;
}
#endif

int main(int argc, char **argv)
{
	struct f2fs_sb_info *sbi;
	int ret = 0, ret2;
	u64 start = get_boottime_ns();

	f2fs_init_configuration();

	f2fs_parse_options(argc, argv);

	if (c.func != DUMP && (ret = f2fs_devs_are_umounted()) < 0) {
		if (ret == -EBUSY) {
			ret = -1;
			if (c.func == FSCK)
				ret = FSCK_OPERATIONAL_ERROR;
			goto quick_err;
		}
		if (!c.ro || c.func == DEFRAG) {
			MSG(0, "\tError: Not available on mounted device!\n");
			ret = -1;
			if (c.func == FSCK)
				ret = FSCK_OPERATIONAL_ERROR;
			goto quick_err;
		}

		/* allow ro-mounted partition */
		if (c.force) {
			MSG(0, "Info: Force to check/repair FS on RO mounted device\n");
		} else {
			MSG(0, "Info: Check FS only on RO mounted device\n");
			c.fix_on = 0;
			c.auto_fix = 0;
		}
	}

	/* Get device */
	if (f2fs_get_device_info() < 0 || f2fs_get_f2fs_info() < 0) {
		ret = -1;
		if (c.func == FSCK)
			ret = FSCK_OPERATIONAL_ERROR;
		goto quick_err;
	}

fsck_again:
	memset(&gfsck, 0, sizeof(gfsck));
	gfsck.sbi.fsck = &gfsck;
	sbi = &gfsck.sbi;

	ret = f2fs_do_mount(sbi);
	if (ret != 0) {
		if (ret == 1) {
			MSG(0, "Info: No error was reported\n");
			ret = 0;
		}
		goto out_err;
	}

	switch (c.func) {
	case DUMP:
		do_dump(sbi);
		break;
	default:
		ERR_MSG("Wrong program name\n");
		ASSERT(0);
	}

	f2fs_do_umount(sbi);

	if (c.func == FSCK && c.bug_on) {
		if (!c.ro && c.fix_on == 0 && c.auto_fix == 0 && !c.dry_run) {
			char ans[255] = {0};
retry:
			printf("Do you want to fix this partition? [Y/N] ");
			ret2 = scanf("%s", ans);
			ASSERT(ret2 >= 0);
			if (!strcasecmp(ans, "y"))
				c.fix_on = 1;
			else if (!strcasecmp(ans, "n"))
				c.fix_on = 0;
			else
				goto retry;

			if (c.fix_on)
				goto fsck_again;
		}
	}
	ret2 = f2fs_finalize_device();

	if (!c.show_file_map)
		printf("\nDone: %lf secs\n", (get_boottime_ns() - start) / 1000000000.0);
	return ret;

out_err:
	if (sbi->ckpt)
		free(sbi->ckpt);
	if (sbi->raw_super)
		free(sbi->raw_super);
quick_err:
	f2fs_release_sparse_resource();
	return ret;
}