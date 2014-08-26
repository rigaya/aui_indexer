#include <Windows.h>
#include <stdio.h>
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#include "input.h"

static const char * const LSMASHINPUT_NAME[] = { "lwinput.aui", "plugins\\lwinput.aui", "lsmashinput.aui", "plugins\\lsmashinput.aui" };
static const char * AVIUTL_NAME = "Aviutl.exe";

static const int MAX_PATH_LEN = 2048;

typedef INPUT_PLUGIN_TABLE* (*func_get_aui_table)(void);

void print_help() {
	fprintf(stdout, ""
		"aui_indexer.exe [-aui <aui_path>] <filepath_1> [filepath_2] [] ...\n"
		"  options...\n"
		"    -h    print help\n"
		"    -aui  set aui path\n"
		"          if not set, this will try to use %s.\n", LSMASHINPUT_NAME[0]);
}

void check_aviutl_dir(const char *aui_path, char *aviutl_dir, size_t n_len) {
	ZeroMemory(aviutl_dir, n_len);
	char buffer[MAX_PATH_LEN] = { 0 };
	strcpy_s(buffer, _countof(buffer), aui_path);
	PathRemoveFileSpec(buffer);

	char tmp[MAX_PATH_LEN] = { 0 };
	PathCombine(tmp, buffer, AVIUTL_NAME);
	if (PathFileExists(tmp)) {
		strcpy_s(aviutl_dir, n_len, buffer);
	} else {
		PathRemoveFileSpec(buffer);
		PathCombine(tmp, buffer, AVIUTL_NAME);
		if (PathFileExists(tmp)) {
			strcpy_s(aviutl_dir, n_len, buffer);
		}
	}
}

int main(int argc, char **argv) {
	if (argc <= 1 || (0 == strcmp(argv[1], "-aui") && argc <= 3)) {
		fprintf(stdout, "invalid option(s).\n");
		print_help();
		return 1;
	}
	if (strcmp(argv[1], "-h") == 0) {
		print_help();
		return 0;
	}

	int i_arg = 1;
	char aui_path[MAX_PATH_LEN] = { 0 }; //must be fullpath
	if (0 == strcmp(argv[i_arg], "-aui")) {
		if (PathIsRelative(aui_path))
			_fullpath(aui_path, argv[i_arg+1], sizeof(aui_path));
		else
			strcpy_s(aui_path, _countof(aui_path), argv[i_arg+1]);
		i_arg += 2;
	} else {
		for (int i = 0; i < _countof(LSMASHINPUT_NAME); i++) {
			_fullpath(aui_path, LSMASHINPUT_NAME[i], sizeof(aui_path));
			if (PathFileExists(aui_path))
				break;
		}
	}
	if (!PathFileExists(aui_path)) {
		fprintf(stdout, "could not find aui file.\n");
		return 1;
	}

	char current_dir[MAX_PATH_LEN];
	char aviutl_dir[MAX_PATH_LEN];
	GetCurrentDirectory(_countof(current_dir), current_dir);
	check_aviutl_dir(aui_path, aviutl_dir, _countof(aviutl_dir));
	fclose(stderr);

	int ret = 0;
	HMODULE hmd = NULL;
	func_get_aui_table get_table = NULL;
	INPUT_PLUGIN_TABLE *ipt = NULL;
	if (   NULL != (hmd = LoadLibrary(aui_path))
		&& NULL != (get_table = (func_get_aui_table)GetProcAddress(hmd, "GetInputPluginTable"))
		&& NULL != (ipt = get_table())
		&& ipt->func_open
		&& ipt->func_close ) {
		if (ipt->func_init) ipt->func_init();
		for ( ; i_arg < argc; i_arg++) {
			char target_fullpath[MAX_PATH_LEN];
			if (PathIsRelative(argv[i_arg]))
				_fullpath(target_fullpath, argv[i_arg], sizeof(target_fullpath));
			else
				strcpy_s(target_fullpath, _countof(target_fullpath), argv[i_arg]);
			fprintf(stdout, "processing %s ...\n", PathFindFileName(target_fullpath));
			if (PathIsDirectory(aviutl_dir))
				SetCurrentDirectory(aviutl_dir);
			INPUT_HANDLE in_hnd = ipt->func_open(target_fullpath);
			if (in_hnd != NULL)
				ipt->func_close(in_hnd);
			SetCurrentDirectory(current_dir);
		}
		if (ipt->func_exit) ipt->func_exit();
	} else {
		ret = 1;
		fprintf(stdout, "failed to load aui file.\n");
	}
	if (hmd) FreeLibrary(hmd);
	return ret;
}
