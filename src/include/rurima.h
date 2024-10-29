// SPDX-License-Identifier: MIT
/*
 *
 * This file is part of rurima, with ABSOLUTELY NO WARRANTY.
 *
 * MIT License
 *
 * Copyright (c) 2024 Moe-hacker
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 */
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <limits.h>
#include <dirent.h>
// This program need be linked with -lcap.
#include <sys/capability.h>
// Bool!!!
#if __STDC_VERSION__ < 202000L
#ifndef bool
#define bool _Bool
#define true ((_Bool) + 1u)
#define false ((_Bool) + 0u)
#endif
#endif
#ifndef _Nullable
#define _Nullable
#endif
#ifndef _Nonnull
#define _Nonnull
#endif
#include "version.h"
#include "k2v.h"
#include "cprintf.h"
#include "jsonv.h"
// Ruri container config.
#define INIT_VALUE (-114)
#define MAX_COMMANDS (1024)
#define MAX_ENVS (128 * 2)
#define MAX_MOUNTPOINTS (128 * 2)
struct __attribute__((aligned(128))) RURI_CONTAINER {
	/*
	 * This is a subset of CONTAINER struct in ruri.
	 * It defines all configs for running ruri container.
	 */
	// Container directory.
	char *container_dir;
	// Capabilities to drop.
	cap_value_t drop_caplist[CAP_LAST_CAP + 1];
	// Command for exec(2).
	char *command[MAX_COMMANDS];
	// Extra mountpoints.
	char *extra_mountpoint[MAX_MOUNTPOINTS];
	// Extra read-only mountpoints.
	char *extra_ro_mountpoint[MAX_MOUNTPOINTS];
	// Environment variables.
	char *env[MAX_ENVS];
	// Set NO_NEW_PRIV bit.
	bool no_new_privs;
	// Enable built-in seccomp profile.
	bool enable_seccomp;
	// Do not show warnings.
	bool no_warnings;
	// Unshare container.
	bool enable_unshare;
	// Useless rootless container support.
	bool rootless;
	// Mount host runtime.
	bool mount_host_runtime;
	// Arch of multi-architecture container.
	char *cross_arch;
	// Path of QEMU binary.
	char *qemu_path;
	// Do not store .rurienv file.
	bool use_rurienv;
	// Mount / as read-only.
	bool ro_root;
	// Cpuset.
	char *cpuset;
	// Memory.
	char *memory;
};
struct __attribute__((aligned(128))) RURIMA {
	/*
	 * This is full rurima config.
	 */
	// rootfs source: docker, lxc, rootfs or hostdir.
	char *_Nonnull rootfs_source;
	// For docker.
	char *_Nullable rootfs_image;
	// For docker.
	char *_Nullable rootfs_tag;
	// For docker/lxc.
	char *_Nullable rootfs_arch;
	// For lxc.
	char *_Nullable rootfs_mirror;
	// For lxc.
	char *_Nullable rootfs_os;
	// For lxc.
	char *_Nullable rootfs_version;
	// For lxc.
	char *_Nullable rootfs_type;
	// For rootfs.
	char *_Nullable rootfs_path;
	// For rootfs.
	char *_Nullable host_dir;
	// Hook script, will be copied into /tmp.
	char *_Nullable hook_script;
	// Hook command.
	char *_Nullable hook_command[MAX_COMMANDS];
	// Full ruri container config.
	struct RURI_CONTAINER container;
};
// Warnings.
#define warning(...) cfprintf(stderr, ##__VA_ARGS__)
// Show error msg and exit.
#define error(...)                                                                                           \
	{                                                                                                    \
		cfprintf(stderr, ##__VA_ARGS__);                                                             \
		cfprintf(stderr, "{base}%s{clear}\n", "  .^.   .^.");                                        \
		cfprintf(stderr, "{base}%s{clear}\n", "  /⋀\\_ﾉ_/⋀\\");                                      \
		cfprintf(stderr, "{base}%s{clear}\n", " /ﾉｿﾉ\\ﾉｿ丶)|");                                      \
		cfprintf(stderr, "{base}%s{clear}\n", " ﾙﾘﾘ >  x )ﾘ");                                       \
		cfprintf(stderr, "{base}%s{clear}\n", "ﾉノ㇏  ^ ﾉ|ﾉ");                                       \
		cfprintf(stderr, "{base}%s{clear}\n", "      ⠁⠁");                                           \
		cfprintf(stderr, "{base}%s{clear}\n", "RURI ERROR MESSAGE");                                 \
		cfprintf(stderr, "{base}%s{clear}\n", "If you think something is wrong, please report at:"); \
		cfprintf(stderr, "\033[4m{base}%s{clear}\n", "https://github.com/Moe-hacker/ruri/issues");   \
		exit(EXIT_FAILURE);                                                                          \
	}
// Log system.
#if defined(RURIMA_DEBUG)
#define log(...)                                                                                                                      \
	{                                                                                                                             \
		struct timeval tv;                                                                                                    \
		gettimeofday(&tv, NULL);                                                                                              \
		cfprintf(stdout, "{green}[%ld.%06ld] in %s() in %s line %d:\n", tv.tv_sec, tv.tv_usec, __func__, __FILE__, __LINE__); \
		cfprintf(stdout, ##__VA_ARGS__)                                                                                       \
	}
#else
#define log(...) \
	{        \
	}
#endif
// Functions.
int fork_execvp(const char *argv[]);
char *fork_execvp_get_stdout(const char *argv[]);
int extract_archive(const char *file, const char *dir);
off_t get_file_size(const char *file);
char *get_tmpdir(void);
char *get_prefix(void);
int mkdirs(const char *path, mode_t mode);
bool run_with_root(void);
int docker_search(const char *image, const char *page_size);
int docker_search_tag(const char *image, const char *page_size, const char *_Nullable architecture);
char **docker_pull(const char *image, const char *tag, const char *_Nullable architecture, const char *savedir);
void register_signal(void);
char *get_host_arch(void);
void lxc_pull_image(const char *_Nullable mirror, const char *os, const char *version, const char *_Nullable architecture, const char *_Nullable type, const char *savedir);
void lxc_get_image_list(const char *_Nullable mirror, const char *_Nullable architecture);
void lxc_search_image(const char *_Nullable mirror, const char *os, const char *_Nullable architecture);
void docker(int argc, char **argv);
void lxc(int argc, char **argv);
void unpack(int argc, char **argv);
struct RURIMA *init_config(void);