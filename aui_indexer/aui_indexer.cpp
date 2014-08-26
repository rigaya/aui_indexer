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

//PathRemoveFileSpecFixedがVistaでは5C問題を発生させるため、その回避策
static BOOL PathRemoveFileSpecFixed(char *path) {
	char *ptr = PathFindFileNameA(path);
	if (path == ptr)
		return FALSE;
	*(ptr - 1) = '\0';
	return TRUE;
}

//aui_pathと同じ場所、あるいはその一つ上の階層にAviutlがあれば、
//そのディレクトリをaviutl_dirに格納する
template<int size>
void check_aviutl_dir(char(& aviutl_dir)[size], const char *aui_path) {
	ZeroMemory(aviutl_dir, size * sizeof(aviutl_dir[0]));
	char buffer[MAX_PATH_LEN] = { 0 };
	strcpy_s(buffer, aui_path);
	PathRemoveFileSpecFixed(buffer);

	char tmp[MAX_PATH_LEN] = { 0 };
	PathCombine(tmp, buffer, AVIUTL_NAME);
	if (PathFileExists(tmp)) {
		strcpy_s(aviutl_dir, buffer);
	} else {
		PathRemoveFileSpecFixed(buffer);
		PathCombine(tmp, buffer, AVIUTL_NAME);
		if (PathFileExists(tmp)) {
			strcpy_s(aviutl_dir, buffer);
		}
	}
}

//lwinput.auiの設定ファイルをきちんと反映するため、
//できればAviutl.exeの位置に一時的にカレントディレクトリを移したいので、
//lwinput.auiのフルパスがほしいので、それをaui_pathに格納する
//だめなら、素直にLoadLibraryを試みる
template<int size>
int get_aui_path_auto(char(& aui_path)[size], const char *exe_path) {
	char buffer[MAX_PATH_LEN] = { 0 };
	strcpy_s(buffer, exe_path);
	PathRemoveFileSpecFixed(buffer);

	//実行ファイルがある場所で検索
	for (int i = 0; i < _countof(LSMASHINPUT_NAME); i++) {
		PathCombine(aui_path, buffer, LSMASHINPUT_NAME[i]);
		if (PathFileExists(aui_path))
			return 0;
	}
	//次にカレントディレクトリで検索
	for (int i = 0; i < _countof(LSMASHINPUT_NAME); i++) {
		_fullpath(aui_path, LSMASHINPUT_NAME[i], size);
		if (PathFileExists(aui_path))
			return 0;
	}
	//次に普通に...
	for (int i = 0; i < _countof(LSMASHINPUT_NAME); i++) {
		HMODULE hModule = LoadLibrary(LSMASHINPUT_NAME[i]);
		if (NULL != hModule) {
			FreeLibrary(hModule);
			strcpy_s(aui_path, LSMASHINPUT_NAME[i]);
			return 0;
		}
	}
	return 1;
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
		if (get_aui_path_auto(aui_path, argv[0])) {
			fprintf(stdout, "could not find aui file.\n");
			return 1;
		}
	}

	//lwinput.auiの設定ファイルをきちんと反映するため、
	//できればAviutl.exeの位置に一時的にカレントディレクトリを移したいが、
	//そのためにはlwinput.auiのフルパスが必要
	char current_dir[MAX_PATH_LEN] = { 0 };
	char aviutl_dir[MAX_PATH_LEN] = { 0 };
	if (!PathIsRelative(aui_path)) {
		check_aviutl_dir(aviutl_dir, aui_path);
	}
	
	GetCurrentDirectory(_countof(current_dir), current_dir);
	fclose(stderr); //ffmpegのエラー出力すると遅い( & うるさい) ので殺す

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
			char target_fullpath[MAX_PATH_LEN] = { 0 };
			if (PathIsRelative(argv[i_arg]))
				_fullpath(target_fullpath, argv[i_arg], sizeof(target_fullpath));
			else
				strcpy_s(target_fullpath, argv[i_arg]);
			fprintf(stdout, "processing %s ...\n", PathFindFileName(target_fullpath));

			//lwinput.auiの設定ファイルをきちんと反映するため、
			//できればAviutl.exeの位置に一時的にカレントディレクトリを移したい
			if (PathIsDirectory(aviutl_dir))
				SetCurrentDirectory(aviutl_dir);
			INPUT_HANDLE in_hnd = ipt->func_open(target_fullpath);
			if (in_hnd != NULL)
				ipt->func_close(in_hnd);
			//カレントディレクトリはすぐに戻しておく
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
