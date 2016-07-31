// -----------------------------------------------------------------------------------------
//     aui_indexer by rigaya
// -----------------------------------------------------------------------------------------
// The MIT License
//
// Copyright (c) 2014-2016 rigaya
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// IABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// ------------------------------------------------------------------------------------------

#include <Windows.h>
#include <stdio.h>
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#include <string>
#include "input.h"
#include "api_hook.h"

static const char * const AUI_LIST[] = {
    "lwinput.aui",
    "plugins\\lwinput.aui",
    "m2v.aui",
    "plugins\\m2v.aui",
    "lsmashinput.aui",
    "plugins\\lsmashinput.aui"
};
static const char * AVIUTL_NAME = "Aviutl.exe";

static const int MAX_PATH_LEN = 2048;

typedef INPUT_PLUGIN_TABLE* (*func_get_aui_table)(void);

unsigned int char_to_wstring(std::wstring& wstr, const char *str, uint32_t codepage = CP_THREAD_ACP) {
    if (str == nullptr) {
        wstr = L"";
        return 0;
    }
    int widechar_length = MultiByteToWideChar(codepage, 0, str, -1, nullptr, 0);
    wstr.resize(widechar_length, 0);
    if (0 == MultiByteToWideChar(codepage, 0, str, -1, &wstr[0], (int)wstr.size())) {
        wstr.clear();
        return 0;
    }
    return widechar_length;
}
std::wstring char_to_wstring(const char *str, uint32_t codepage = CP_THREAD_ACP) {
    if (str == nullptr) {
        return L"";
    }
    std::wstring wstr;
    char_to_wstring(wstr, str, codepage);
    return wstr;
}
std::wstring char_to_wstring(const std::string& str, uint32_t codepage = CP_THREAD_ACP) {
    std::wstring wstr;
    char_to_wstring(wstr, str.c_str(), codepage);
    return wstr;
}

static std::wstring g_target_fullpath_w; //対象のファイルパス
static std::wstring g_tmpdir_w;          //一時出力先
static std::wstring g_tmppath_w;         //一時出力ファイルパス
static std::wstring g_orgpath_w;         //一時出力ファイルパス
static std::string g_target_fullpath;    //対象のファイルパス
static std::string g_tmpdir;             //一時出力先
static std::string g_tmppath;            //一時出力ファイルパス
static std::string g_orgpath;            //一時出力ファイルパス

typedef decltype(CreateFileW)* funcCreateFileW;
static funcCreateFileW origCreateFileWFunc = nullptr;
HANDLE __stdcall CreateFileWHook(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    auto ptr = lpFileName;
    //対象が一致したら、書き込みモードで開くときのみ一時出力先のファイルパスに置き換える
    if (std::wstring(lpFileName) == g_target_fullpath_w + L".lwi" && (dwDesiredAccess & GENERIC_WRITE)) {
        g_tmppath_w = g_tmpdir_w + L"\\" + PathFindFileNameW(lpFileName);
        ptr = g_tmppath_w.c_str();
        g_orgpath_w = lpFileName;
    }
    return origCreateFileWFunc(ptr, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}
typedef decltype(CreateFileA)* funcCreateFileA;
static funcCreateFileA origCreateFileAFunc = nullptr;
HANDLE __stdcall CreateFileAHook(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    auto ptr = lpFileName;
    //対象が一致したら、書き込みモードで開くときのみ一時出力先のファイルパスに置き換える
    if (std::string(lpFileName) == g_target_fullpath + ".lwi" && (dwDesiredAccess & GENERIC_WRITE)) {
        g_tmppath = g_tmpdir + "\\" + PathFindFileName(lpFileName);
        ptr = g_tmppath.c_str();
        g_orgpath = lpFileName;
    }
    return origCreateFileAFunc(ptr, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

typedef decltype(fopen)* funcfopen;
static funcfopen origFopenFunc = nullptr;
FILE* fopenHook(char const* _FileName, char const* _Mode) {
    auto ptr = _FileName;
    //対象が一致したら、書き込みモードで開くときのみ一時出力先のファイルパスに置き換える
    if (std::string(_FileName) == g_target_fullpath + ".lwi" && strstr(_Mode, "w")) {
        g_tmppath = g_tmpdir + "\\" + PathFindFileName(_FileName);
        ptr = g_tmppath.c_str();
        g_orgpath = _FileName;
    }
    return origFopenFunc(ptr, _Mode);
}

void print_help() {
    fprintf(stdout, ""
        "aui_indexer.exe [-aui <aui_path>] <filepath_1> [filepath_2] [] ...\n"
        "  options...\n"
        "    -h         print help\n"
        "    -lwtmpdir  set temp dir (only effective when aui is lwinput.aui)\n"
        "    -aui       set aui path\n"
        "                if not set, this will try to use aui below.\n");
    for (int i = 0; i < _countof(AUI_LIST); i++) {
        fprintf(stdout, "                 %s\n", AUI_LIST[i]);
    }
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
    for (int i = 0; i < _countof(AUI_LIST); i++) {
        PathCombine(aui_path, buffer, AUI_LIST[i]);
        if (PathFileExists(aui_path))
            return 0;
    }
    //次にカレントディレクトリで検索
    for (int i = 0; i < _countof(AUI_LIST); i++) {
        _fullpath(aui_path, AUI_LIST[i], size);
        if (PathFileExists(aui_path))
            return 0;
    }
    //次に普通に...
    for (int i = 0; i < _countof(AUI_LIST); i++) {
        HMODULE hModule = LoadLibrary(AUI_LIST[i]);
        if (NULL != hModule) {
            FreeLibrary(hModule);
            strcpy_s(aui_path, AUI_LIST[i]);
            return 0;
        }
    }
    return 1;
}

int main(int argc, char **argv) {
    if (argc <= 1 || (0 == _stricmp(argv[1], "-aui") || 0 == _stricmp(argv[1], "-lwtmpdir")) && argc <= 3) {
        fprintf(stdout, "invalid option(s).\n");
        print_help();
        return 1;
    }
    if (   _stricmp(argv[1], "-h") == 0
        || _stricmp(argv[1], "-?") == 0
        || _stricmp(argv[1], "--help") == 0) {
        print_help();
        return 0;
    }

    std::vector<char *> target_list;
    char aui_path[MAX_PATH_LEN] = { 0 }; //must be fullpath
    char tmpdir[MAX_PATH_LEN] = { 0 }; //must be fullpath
    for (int i_arg = 1; i_arg < argc; i_arg++) {
        if (0 == _stricmp(argv[i_arg], "-aui")) {
            if (PathIsRelative(argv[i_arg+1]))
                _fullpath(aui_path, argv[i_arg+1], sizeof(aui_path));
            else
                strcpy_s(aui_path, _countof(aui_path), argv[i_arg+1]);
            i_arg++;
        } else if (0 == _stricmp(argv[i_arg], "-lwtmpdir")) {
            if (PathIsRelative(argv[i_arg+1]))
                _fullpath(tmpdir, argv[i_arg+1], sizeof(tmpdir));
            else
                strcpy_s(tmpdir, _countof(tmpdir), argv[i_arg+1]);
            i_arg++;
        } else {
            target_list.push_back(argv[i_arg]);
        }
    }

    if (strlen(aui_path) == 0) {
        //特に指定がなければ、自動的にauiを検索する
        if (get_aui_path_auto(aui_path, argv[0])) {
            fprintf(stdout, "could not find aui file.\n");
            return 1;
        }
    }
    if (!PathIsDirectory(tmpdir)) {
        fprintf(stdout, "temp directory \"%s\" does not exist.\n", tmpdir);
        return 1;
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

        apihook hook_createfile;
        if (strlen(tmpdir)) {
            g_tmpdir = tmpdir;
            g_tmpdir_w = char_to_wstring(tmpdir);
            hook_createfile.hook(aui_path, "CreateFileW", CreateFileWHook, (void **)&origCreateFileWFunc);
            hook_createfile.hook(aui_path, "CreateFileA", CreateFileAHook, (void **)&origCreateFileAFunc);
            hook_createfile.hook(aui_path, "fopen",       fopenHook,       (void **)&origFopenFunc);
        }
        for (auto target : target_list) {
            if (!PathFileExists(target)) {
                fprintf(stdout, "Error: \"%s\" does not exist.\n", target);
                continue;
            }
            char target_fullpath[MAX_PATH_LEN] = { 0 };
            if (PathIsRelative(target))
                _fullpath(target_fullpath, target, sizeof(target_fullpath));
            else
                strcpy_s(target_fullpath, target);
            fprintf(stdout, "processing %s ...\n", PathFindFileName(target_fullpath));
            //一時出力ファイルパスをクリア
            g_tmppath_w = L"";
            g_tmppath = "";
            g_orgpath_w = L"";
            g_orgpath = "";
            //対象ファイル名を設定
            g_target_fullpath_w = char_to_wstring(target_fullpath);
            g_target_fullpath = (target_fullpath);

            //lwinput.auiの設定ファイルをきちんと反映するため、
            //できればAviutl.exeの位置に一時的にカレントディレクトリを移したい
            if (PathIsDirectory(aviutl_dir))
                SetCurrentDirectory(aviutl_dir);
            INPUT_HANDLE in_hnd = ipt->func_open(target_fullpath);
            if (in_hnd != NULL)
                ipt->func_close(in_hnd);
            //カレントディレクトリはすぐに戻しておく
            SetCurrentDirectory(current_dir);
            //一時的に出力先を変更した場合、これをもとに戻す
            if (g_tmppath_w.length() && PathFileExistsW(g_tmppath_w.c_str())) {
                MoveFileExW(g_tmppath_w.c_str(), g_orgpath_w.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING);
            }
            if (g_tmppath.length() && PathFileExists(g_tmppath.c_str())) {
                MoveFileExA(g_tmppath.c_str(), g_orgpath.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING);
            }
        }
        if (ipt->func_exit) ipt->func_exit();
        hook_createfile.fin();
    } else {
        ret = 1;
        fprintf(stdout, "failed to load aui file.\n");
    }
    if (hmd) FreeLibrary(hmd);
    return ret;
}
