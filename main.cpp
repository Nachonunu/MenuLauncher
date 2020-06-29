/*! MenuLauncher v0.4 | CC0 1.0 (https://creativecommons.org/publicdomain/zero/1.0/deed) */

#include <windows.h>

#include <cstdio>
#include <string>
#include <vector>
#include <unordered_map>
using namespace std;

typedef struct _POSDATA {
	size_t file;
	size_t command;
}POSDATA;

static FILE* fp;
static string temp, temp_var, varname;
static vector<POSDATA> pos_list; //commandの位置
static unordered_map<string, string> var_list; //変数リスト
static char* cur = nullptr;

inline int _Str_End() {
	int c = fgetc(fp);
	while (c != EOF) {
		switch (c) {
			case '\"': return 0;
			case '\n': return -1;
			case '\\':
				c = fgetc(fp);
				if (c != '\"') fseek(fp, -1, SEEK_CUR);
				break;
			default: break;
		}
		c = fgetc(fp);
	}
	return -1;
}
inline int _Str_Set(string& str) {
	int c = fgetc(fp), v_char;
	while (c != EOF) {
		switch (c) {
			case '\"': return 0;
			case '\n': return -1;
			case '<':
				//変数
				varname = "";
				v_char = fgetc(fp);
				while (v_char != EOF) {
					if (v_char == '>') break;
					else varname += (char)v_char;
					v_char = fgetc(fp);
				}
				//idから文字列を取得
				if (!varname.empty()) str += var_list[varname];
				else {
					//カレントディレクトリ
					if (cur == nullptr) {
						cur = new char[MAX_PATH+1];
						DWORD s = GetCurrentDirectoryA(MAX_PATH+1, cur);
						if (s == 0) str[0] = '\0';
						else if (s > MAX_PATH+1) {
							delete cur;
							cur = new char[s];
							s = GetCurrentDirectoryA(s, cur);
							if (s == 0) str[0] = '\0';
						}
					}
					str += cur;
				}
				break;
			case '\\':
				c = fgetc(fp);
				if (c == '\"') str += "\"";
				else { str += "\\"; fseek(fp, -1, SEEK_CUR); }
				break;
			default: str += (char)c; break;
		}
		c = fgetc(fp);
	}
	return -1;
}
int CreateLaunchMenu(HMENU menu, UINT indent) {
	UINT nowindent = 0;
	HMENU pmenu;
	int state = 0; //0=start, 1=name, 2=file, 3=command, 4=var, 5=var_contents
	size_t line = ftell(fp), pos_file = 0, pos_command = 0;
	int c = fgetc(fp);
	while (c != EOF) {
		switch (c) {
			case '\t':
				if (state == 0) nowindent++;
				break;
			case '\n':
				switch (state) {
					case 1: //dir
						pmenu = CreatePopupMenu();
						InsertMenuA(menu, -1, MF_BYPOSITION | MF_POPUP, (UINT_PTR)pmenu, temp.c_str());
						if (CreateLaunchMenu(pmenu, indent+1) == -1) return -1;
						break;
					case 2: //file
						pos_list.push_back( { pos_file, 0 } ); //commandの位置
						InsertMenuA(menu, -1, MF_BYPOSITION | MF_STRING, pos_list.size() - 1 + 10000, temp.c_str());
						break;
					case 3: //file + command
						pos_list.push_back( { pos_file, pos_command } ); //commandの位置
						InsertMenuA(menu, -1, MF_BYPOSITION | MF_STRING, pos_list.size() - 1 + 10000, temp.c_str());
						break;
					case 4: var_list[temp] = ""; break; //var
					case 5: var_list[temp] = temp_var; break; //var + contents
					default: break;
				}
				line = ftell(fp);
				nowindent = 0;
				state = 0;
				break;
			case '\"':
				switch (state) {
					case 0: //name
						if (nowindent == indent) {
							//次の \" までの文字列を取得
							temp = "";
							if (_Str_Set(temp) == -1) return -1;
						} else if (nowindent < indent) {
							//ひとつ前の関数で処理
							fseek(fp, line, SEEK_SET);
							return 0;
						} else return -1;
						break;
					case 1: //file
						pos_file = ftell(fp);
						if (_Str_End() == -1) return -1;
						break;
					case 2: //file command
						pos_command = ftell(fp);
						if (_Str_End() == -1) return -1;
						break;
					case 4: //var command
						temp_var = "";
						if (_Str_Set(temp_var) == -1) return -1;
						break;
					default: return -1;
				}
				++state;
				break;
			case '$':
				if (state == 0) {
					//変数名を取得
					c = fgetc(fp);
					temp = "";
					while (c != EOF) {
						if (c == ' ') break;
						else if (c == '\n') return -1;
						else temp += (char)c;
						c = fgetc(fp);
					}
					if (c == EOF) return -1;
					state = 4;
				}
				break;
			case ';':
				//コメント
				c = fgetc(fp);
				while (c != EOF) {
					if (c == '\n') { fseek(fp, -1, SEEK_CUR); break; }
					c = fgetc(fp);
				}
				if (c == EOF) return -1;
				break;
			case '-':
				if (state == 0) {
					//区切り線
					if (nowindent == indent) {
						InsertMenuA(menu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
						c = fgetc(fp);
						while (c != EOF) {
							if (c == '\n') break;
							c = fgetc(fp);
						}
						if (c == EOF) return -1;
						line = ftell(fp);
						nowindent = 0;
						state = 0;
					} else if (nowindent < indent) {
						//ひとつ前の関数で処理
						fseek(fp, line, SEEK_SET);
						return 0;
					} else return -1;
				} else return -1;
				break;
			case ' ': break;
			default: return -1;
		}
		c = fgetc(fp);
	}
	return 0;
}
inline int RunLaunchMenu(UINT id) {
	temp = "";
	//file
	fseek(fp, pos_list[id].file, SEEK_SET);
	_Str_Set(temp); //file
	//実行ファイルのカレントディレクトリ
	string dir;
	for (size_t i = temp.size()-1; i >= 0; --i) {
		if (temp[i] == '\\') { dir = temp.substr(0, i); break; }
		else if (temp[i] == '/') { dir = temp.substr(0, i); break; }
	}
	//command
	string com;
	if (pos_list[id].command > 0) {
		fseek(fp, pos_list[id].command, SEEK_SET);
		_Str_Set(com); //file
	}
	//実行
#if defined(_WIN64)
	if ((long long int)ShellExecuteA(nullptr, nullptr, &temp[0], &com[0], &dir[0], SW_SHOW) <= 32) return -1;
#elif defined(_WIN32)
	if ((int)ShellExecuteA(nullptr, nullptr, &temp[0], &com[0], &dir[0], SW_SHOW) <= 32) return -1;
#endif
	return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
	static HMENU hPopupMenu;
	UINT id;
	switch (msg) {
		case WM_DESTROY: PostQuitMessage(0); return 0;
		case WM_CREATE:
			hPopupMenu = CreatePopupMenu();
			if (CreateLaunchMenu(hPopupMenu, 0) == 0) {
				if (SetForegroundWindow(hwnd) == 0) DestroyWindow(hwnd);
				id = TrackPopupMenu(hPopupMenu, TPM_RETURNCMD, 0, 0, 0, hwnd, nullptr);
				if (id >= 10000 && RunLaunchMenu(id-10000) == -1) MessageBoxA(hwnd, "Error", "Launcher", MB_OK | MB_ICONWARNING);
			}
			DestroyWindow(hwnd);
			break;
	}
	return DefWindowProc(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow) {
	WNDCLASSA winc;
	winc.style = CS_HREDRAW | CS_VREDRAW;
	winc.lpfnWndProc = WndProc;
	winc.cbClsExtra = 0;
	winc.cbWndExtra = 0;
	winc.hInstance = hInstance;
	winc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	winc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	winc.lpszClassName = "LauncherWindows";
	if (!RegisterClassA(&winc)) return -1;

	//カレントディレクトリを実行ファイルパスに変更
	char exepath[MAX_PATH];
	if (GetModuleFileNameA(NULL, exepath, MAX_PATH) != 0) {
		char* exepath_last = strrchr(exepath, '\\');
		if (exepath_last != NULL) {
			*exepath_last = '\0';
			SetCurrentDirectoryA(exepath);
		}
	}

	if (fopen_s(&fp, (__argc >= 2) ? __argv[1] : "MenuLauncher.conf", "r") != 0) return -1;

	HWND hwnd = CreateWindowA("LauncherWindows", "", WS_DISABLED, 0, 0, 100, 100, HWND_MESSAGE, nullptr, hInstance, nullptr);
	if (hwnd == nullptr) { fclose(fp); delete cur; return -1; }

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0)) DispatchMessage(&msg);
	delete cur;
	fclose(fp);
	return 0;
}
