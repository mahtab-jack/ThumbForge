#include <flutter/dart_project.h>
#include <flutter/flutter_view_controller.h>
#include <windows.h>

#include "flutter_window.h"
#include "utils.h"

namespace {

constexpr const wchar_t kSingleInstanceMutex[] =
    L"ThumbForge_SingleInstance_Mutex_D37F2961";

struct WindowSearchContext {
  HWND target_hwnd = nullptr;
};

BOOL CALLBACK FindExistingAppWindowCallback(HWND hwnd, LPARAM lparam) {
  auto* context = reinterpret_cast<WindowSearchContext*>(lparam);
  wchar_t class_name[256] = {0};
  if (::GetClassNameW(hwnd, class_name, 256) > 0) {
    if (::wcscmp(class_name, L"FLUTTER_RUNNER_WIN32_WINDOW") == 0) {
      wchar_t title[512] = {0};
      ::GetWindowTextW(hwnd, title, 512);
      if (::wcsstr(title, L"THUMBFORGE") != nullptr) {
        context->target_hwnd = hwnd;
        return FALSE;
      }
    }
  }
  return TRUE;
}

HWND FindExistingAppWindow() {
  WindowSearchContext context;
  ::EnumWindows(FindExistingAppWindowCallback, reinterpret_cast<LPARAM>(&context));
  if (context.target_hwnd != nullptr) {
    return context.target_hwnd;
  }
  return ::FindWindowW(L"FLUTTER_RUNNER_WIN32_WINDOW", nullptr);
}

void ActivateExistingWindow(HWND hwnd) {
  if (!hwnd) {
    return;
  }
  if (::IsIconic(hwnd)) {
    ::ShowWindow(hwnd, SW_RESTORE);
  } else {
    ::ShowWindow(hwnd, SW_SHOW);
  }

  DWORD current_thread_id = ::GetCurrentThreadId();
  DWORD window_thread_id = ::GetWindowThreadProcessId(hwnd, nullptr);
  if (current_thread_id != window_thread_id) {
    ::AttachThreadInput(current_thread_id, window_thread_id, TRUE);
    ::SetForegroundWindow(hwnd);
    ::SetFocus(hwnd);
    ::AttachThreadInput(current_thread_id, window_thread_id, FALSE);
  } else {
    ::SetForegroundWindow(hwnd);
    ::SetFocus(hwnd);
  }
}

}  // namespace

int APIENTRY wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE prev,
                      _In_ wchar_t *command_line, _In_ int show_command) {
  // Enforce single instance execution
  HANDLE single_instance_mutex = ::CreateMutexW(nullptr, TRUE, kSingleInstanceMutex);
  if (single_instance_mutex != nullptr && ::GetLastError() == ERROR_ALREADY_EXISTS) {
    HWND existing_hwnd = FindExistingAppWindow();
    if (existing_hwnd != nullptr) {
      ActivateExistingWindow(existing_hwnd);
    }
    ::CloseHandle(single_instance_mutex);
    return EXIT_SUCCESS;
  }

  // Attach to console when present (e.g., 'flutter run') or create a
  // new console when running with a debugger.
  if (!::AttachConsole(ATTACH_PARENT_PROCESS) && ::IsDebuggerPresent()) {
    CreateAndAttachConsole();
  }

  // Initialize COM, so that it is available for use in the library and/or
  // plugins.
  ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

  flutter::DartProject project(L"data");

  std::vector<std::string> command_line_arguments =
      GetCommandLineArguments();

  project.set_dart_entrypoint_arguments(std::move(command_line_arguments));

  FlutterWindow window(project);
  Win32Window::Point origin(80, 60);
  Win32Window::Size size(1320, 840);
  if (!window.Create(L"THUMBFORGE // WINDOWS SHELL THUMBNAIL MANAGER", origin, size)) {
    if (single_instance_mutex != nullptr) {
      ::ReleaseMutex(single_instance_mutex);
      ::CloseHandle(single_instance_mutex);
    }
    return EXIT_FAILURE;
  }
  window.SetQuitOnClose(true);

  ::MSG msg;
  while (::GetMessage(&msg, nullptr, 0, 0)) {
    ::TranslateMessage(&msg);
    ::DispatchMessage(&msg);
  }

  ::CoUninitialize();

  if (single_instance_mutex != nullptr) {
    ::ReleaseMutex(single_instance_mutex);
    ::CloseHandle(single_instance_mutex);
  }

  return EXIT_SUCCESS;
}
